/*
 * server_pkt.cpp - Handling of a single Teredo datagram (server-side).
 * $Id$
 */

/***********************************************************************
 *  Copyright (C) 2004 Remi Denis-Courmont.                            *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license.         *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *  See the GNU General Public License for more details.               *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, you can get it from:              *
 *  http://www.gnu.org/copyleft/gpl.html                               *
 ***********************************************************************/

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <string.h> /* memcpy(), memset() */
#include <sys/types.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include "teredo-udp.h"
#include "common_pkt.h"
#include "conf.h" // conf

#include <arpa/inet.h>
#include <syslog.h> // DEBUG

/*
 * TODO: no longer use the global conf structure, which is a big dirty hack.
 * Make this similar to <relay.cpp>
 */

static uint16_t
sum16 (const uint8_t *data, size_t length, uint32_t sum32 = 0)
{
	size_t wordc = length / 2;

	for (size_t i = 0; i < wordc; i++)
		sum32 += ((uint16_t *)data)[i];
	if (length & 1) // trailing byte if length is odd
		sum32 += ntohs(((uint16_t)(data[length - 1])) << 8);

	while (sum32 > 0xffff)
		sum32 = (sum32 & 0xffff) + (sum32 >> 16);
	
	return sum32;
}

/*
 * Computes an IPv6 16-bits checksum
 */
static uint16_t 
ipv6_sum (const struct ip6_hdr *ip6)
{
	uint32_t sum32 = 0;

	/* Pseudo-header sum */
	for (size_t i = 0; i < 16; i += 2)
		sum32 += *(uint16_t *)(&ip6->ip6_src.s6_addr[i]);
	for (size_t i = 0; i < 16; i += 2)
		sum32 += *(uint16_t *)(&ip6->ip6_dst.s6_addr[i]);

	sum32 += ip6->ip6_plen + ntohs (ip6->ip6_nxt);

	while (sum32 > 0xffff)
		sum32 = (sum32 & 0xffff) + (sum32 >> 16);

	return sum32;
}


static uint16_t
icmp6_checksum (const struct ip6_hdr *ip6, const struct icmp6_hdr *icmp6)
{
	return ~sum16 ((uint8_t *)icmp6, ntohs (ip6->ip6_plen),
			ipv6_sum (ip6));
}


/*
 * Sends a Teredo-encapsulated Router Advertisement.
 * Returns -1 on error, 0 on success.
 */
static int
teredo_send_ra (const MiredoServerUDP *sock, const struct in6_addr *dest_ip6)
{
	uint8_t packet[13 + 8 + 40 + sizeof (struct nd_router_advert)
			+ sizeof (struct nd_opt_prefix_info)],
		*ptr = packet;

	// Authentification header
	const uint8_t *nonce = sock->GetAuthNonce ();
	if (nonce != NULL)
	{
		struct teredo_simple_auth *auth =
			(struct teredo_simple_auth *)ptr;

		auth->hdr.hdr.zero = 0;
		auth->hdr.hdr.code = teredo_auth_hdr;
		auth->hdr.id_len = auth->hdr.au_len = 0;
		memcpy (auth->nonce, nonce, 8);
		auth->confirmation = 0;

		ptr += 13;
	}

	// Origin indication header
	{
		struct teredo_orig_ind *orig = (struct teredo_orig_ind *)ptr;

		orig->hdr.zero = 0;
		orig->hdr.code = teredo_orig_ind;
		orig->orig_port = ~sock->GetClientPort (); // obfuscate
		orig->orig_addr = ~sock->GetClientIP (); // obfuscate
		ptr += 8;
	}


	// IPv6 header
	struct ip6_hdr *ip6 = (struct ip6_hdr *)ptr;

	ip6->ip6_flow = htonl (0x60000000);
	ip6->ip6_plen = htons (sizeof (struct nd_router_advert)
					+ sizeof (struct nd_opt_prefix_info));
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	ip6->ip6_hlim = 255;
	memcpy (&ip6->ip6_src, "\xfe\x80\x00\x00\x00\x00\x00\x00"
		"\x80\x00\xf2\x27\xbf\xfb\xe6\xad", 16);
	memcpy (&ip6->ip6_dst, dest_ip6, sizeof (struct in6_addr));
	ptr += 40;

	// ICMPv6: Router Advertisement
	struct nd_router_advert *ra = (struct nd_router_advert *)ptr;

	ra->nd_ra_type = ND_ROUTER_ADVERT;
	ra->nd_ra_code = 0;
	ra->nd_ra_cksum = 0;
	ra->nd_ra_curhoplimit = 0;
	ra->nd_ra_flags_reserved = 0;
	ra->nd_ra_router_lifetime = 0;
	ra->nd_ra_reachable = 0;
	ra->nd_ra_retransmit = htonl (2000);

	ptr += sizeof (struct nd_router_advert);

	// ICMPv6 option: Prefix information
	struct nd_opt_prefix_info *pref = (struct nd_opt_prefix_info *)ptr;

	pref->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
	pref->nd_opt_pi_len = sizeof (struct nd_opt_prefix_info) / 8;
	pref->nd_opt_pi_prefix_len = 64;
	pref->nd_opt_pi_flags_reserved = ND_OPT_PI_FLAG_AUTO;
	pref->nd_opt_pi_valid_time = 0xffffffff;
	pref->nd_opt_pi_preferred_time = 0xffffffff;
	{
		union teredo_addr *prefix = (union teredo_addr *)&pref->nd_opt_pi_prefix;

		prefix->teredo.prefix = htonl (TEREDO_PREFIX);
		prefix->teredo.server_ip = conf.server_ip;
		memset (&prefix->ip6.s6_addr[8], 0, 8);
	}

	ptr += sizeof (struct nd_opt_prefix_info);

	ra->nd_ra_cksum = icmp6_checksum (ip6, (struct icmp6_hdr *)ra);

	bool use_secondary_ip = sock->WasSecondaryIP ();
	if (IN6_IS_ADDR_TEREDO_CONE (dest_ip6))
		use_secondary_ip = !use_secondary_ip;

	if (!sock->ReplyPacket (packet, ptr - packet, use_secondary_ip)) 
	{
		struct in_addr inp;

		inp.s_addr = sock->GetClientIP ();
		syslog (LOG_DEBUG,
			_("Router Advertisement sent to %s (%s)\n"),
			inet_ntoa (inp), IN6_IS_ADDR_TEREDO_CONE(dest_ip6)
				? _("cone flag set")
				: _("cone flag not set"));

		return 0;
	}

	return -1;
}

/*
 * Forwards a Teredo packet to a client
 */
static int
ForwardPacket (const MiredoServerUDP *sock)
{
	size_t length;
	const struct ip6_hdr *p = sock->GetIPv6Header (length);

	if ((p == NULL) || (length > 65507))
		return -1;

	union teredo_addr dst;
	memcpy (&dst, &p->ip6_dst, sizeof (dst));
	uint32_t dest_ip = ~dst.teredo.client_ip;

	{
		struct in_addr addr;

		addr.s_addr = dest_ip;
		syslog (LOG_DEBUG, "DEBUG: Forwarding packet to %s:%u\n",
			inet_ntoa (addr), ntohs (~dst.teredo.client_port));
	}

	if (!is_ipv4_global_unicast (dest_ip))
		return 0; // ignore invalid client IP

	uint8_t buf[65515];
	unsigned offset;

	// Origin indication header
	// if the Teredo server's address is ours
	if (dst.teredo.server_ip == conf.server_ip)
	{
		struct teredo_orig_ind orig;
		offset = 8;
		
		orig.hdr.zero = 0;
		orig.hdr.code = teredo_orig_ind;
		orig.orig_port = ~sock->GetClientPort (); // obfuscate
		orig.orig_addr = ~sock->GetClientIP (); // obfuscate
		memcpy (buf, &orig, offset);
	}
	else
		offset = 0;

	memcpy (buf + offset, p, length);
	return sock->SendPacket (buf, length + offset, dest_ip,
					~dst.teredo.client_port);
}


/*
 * Checks and handles an Teredo-encapsulated packet.
 */
int
handle_server_packet (const MiredoServerUDP *sock)
{
	// Teredo server check number 3
	if (!is_ipv4_global_unicast (sock->GetClientIP ()))
		return 0;

	// Check IPv6 packet (Teredo server check number 1)
	// TODO: really check header (as per the authoritative RFC)
	// FIXME: byte alignment
	size_t ip6len;
	const struct ip6_hdr *ip6 = sock->GetIPv6Header (ip6len);
	if (ip6len < 40)
		return 0; // too small
	ip6len -= 40;

	if (((ip6->ip6_vfc >> 4) != 6)
	 || (ntohs (ip6->ip6_plen) != ip6len))
		return 0; // not an IPv6 packet

	uint8_t *buf = ((uint8_t *)ip6) + 40;

	// Teredo server check number 2
	uint8_t proto = ip6->ip6_nxt;
	if (((proto != IPPROTO_NONE) || (ip6len > 0))	// neither a bubble...
	 && (proto != IPPROTO_ICMPV6))	// nor an ICMPv6 message
		return 0; // packet not allowed through server

	// Teredo server check number 4
	if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_src)
	 && IN6_ARE_ADDR_EQUAL (&_in6addr_allrouters, &ip6->ip6_dst)
	 && (proto == IPPROTO_ICMPV6)
	 && (ip6len > sizeof (nd_router_solicit))
	 && (((struct icmp6_hdr *)buf)->icmp6_type == ND_ROUTER_SOLICIT))
		// sends a Router Advertisement
		return teredo_send_ra (sock, &ip6->ip6_src);

	// Teredo server check number 5
	if (!IN6_MATCHES_TEREDO_CLIENT (&ip6->ip6_src, sock->GetClientIP (),
					sock->GetClientPort ()))
	{
		// Teredo server check number 6
		if (IN6_IS_ADDR_TEREDO (&ip6->ip6_src)
		 ||!IN6_MATCHES_TEREDO_SERVER (&ip6->ip6_dst, conf.server_ip))
		{
			// Teredo server check number 7
			return 0; // packet not allowed through server
		}
	}
	
	// Accepts packet:
	if (!IN6_IS_ADDR_TEREDO(&ip6->ip6_dst))
		// forwards packet to native IPv6:
		return ForwardPacket (sock, conf.tunnel);

	// forwards packet over Teredo:
	return ForwardPacket (sock);
}
