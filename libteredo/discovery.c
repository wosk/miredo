/*
 * discovery.c - Teredo local client discovery procedure
 *
 * See "Teredo: Tunneling IPv6 over UDP through NATs"
 * for more information
 */

/***********************************************************************
 *  Copyright © 2009 Jérémie Koenig.                                   *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license, or (at  *
 *  your option) any later version.                                    *
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h> // malloc()
#include <string.h> // mem???()
#include <assert.h>

#include <sys/types.h>
#include <netinet/in.h> // struct in6_addr
#include <netinet/ip6.h> // struct ip6_hdr (for packets.h)
#include <arpa/inet.h> // inet_ntop()
#include <pthread.h>
#include <ifaddrs.h> // getifaddrs()
#include <sys/socket.h>
#include <net/if.h> // IFF_MULTICAST

#include "teredo.h"
#include "teredo-udp.h"
#include "packets.h"
#include "v4global.h"
#include "security.h"
#include "clock.h"
#include "debug.h"
#include "thread.h"
#include "tunnel.h"
#include "discovery.h"


struct teredo_discovery
{
	int refcnt;
	void (*proc)(void *, int);
	void *opaque;
	int fd;
	int mcast_fd;
	struct teredo_discovery_interface
	{
		uint32_t addr;
	} *ifaces;
	struct in6_addr src;
	teredo_thread *recv;
	pthread_t send;
};


static const struct in6_addr in6addr_allnodes =
{ { { 0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1 } } };


static int teredo_discovery_send_bubble (int fd, const struct in6_addr *src)
{
	return teredo_send_bubble (fd, htonl (TEREDO_DISCOVERY_IPV4),
	                           htons (IPPORT_TEREDO), src, &in6addr_allnodes);
}

void teredo_discovery_send_bubbles (teredo_discovery *d, int fd)
{
	const struct in6_addr *src = &d->src;
#ifdef IP_MULTICAST_IF
	/* Neither IETF nor POSIX standardized selecting the outgoing interface.
     * But if we can, try to send a packet on each interface. */
	struct if_nameindex *idx = if_nameindex();
	if (idx != NULL)
	{
		struct ip_mreqn mreq;

		memset (&mreq, 0, sizeof (mreq));

		for (unsigned i = 0; idx[i].if_index != 0; i++)
		{
			mreq.imr_ifindex = idx[i].if_index;

			if (setsockopt (fd, SOL_IP, IP_MULTICAST_IF, &mreq, sizeof (mreq)))
				continue;

			teredo_discovery_send_bubble (fd, src);
		}

		if_freenameindex (idx);
		/* No need to clear the multicast interface, as the socket should not
		 * send multicast packets in any other circumstance. */
		return;
	}
#endif
	/* Fallback to default multicast interface only */
	teredo_discovery_send_bubble (fd, src);
}


bool IsDiscoveryBubble (const teredo_packet *restrict packet)
{
	return IsBubble(packet->ip6)
	 && packet->dest_ipv4 == htonl (TEREDO_DISCOVERY_IPV4)
	 && memcmp(&packet->ip6->ip6_dst, &in6addr_allnodes, 16) == 0;
}


// 5.2.8  Optional Local Client Discovery Procedure
static LIBTEREDO_NORETURN void *teredo_mcast_thread (void *opaque)
{
	teredo_discovery *d = opaque;
	int fd = d->mcast_fd;

	for (;;)
	{
		teredo_discovery_send_bubble (fd, &d->src);

		int interval = 200 + teredo_get_flbits (teredo_clock ()) % 100;

		struct timespec delay = { .tv_sec = interval };
		teredo_sleep (&delay);
	}
}


/* Join the Teredo local discovery multicast group on a given interface */
static void teredo_discovery_joinmcast(int sk, uint32_t ifaddr)
{
	struct ip_mreqn mreq;
	int r;
	char addr[20];

	memset (&mreq, 0, sizeof mreq);
	mreq.imr_address.s_addr = ifaddr;
	mreq.imr_multiaddr.s_addr = htonl (TEREDO_DISCOVERY_IPV4);
	r = setsockopt (sk, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);

	debug (r < 0 ? "Could not join the Teredo local discovery "
	               "multicast group on interface %.20s"
	             : "Listening for Teredo local discovery bubbles "
	               "on interface %.20s",
	       inet_ntop(AF_INET, &ifaddr, addr, sizeof addr));
}


static LIBTEREDO_NORETURN void *teredo_discovery_thread (void *data)
{
	teredo_discovery *d = data;

	d->proc (d->opaque, d->fd);
}

teredo_discovery *
teredo_discovery_start (const teredo_discovery_params *params,
                        int fd, const struct in6_addr *src,
                        void (*proc)(void *, int fd), void *opaque)
{
	struct ifaddrs *ifaddrs, *ifa;
	int r, ifno;

	teredo_discovery *d = malloc (sizeof (teredo_discovery));
	if (d == NULL)
	{
		return NULL;
	}

	d->refcnt = 1;

	/* Get a list of the suitable interfaces */

	r = getifaddrs(&ifaddrs);
	if (r < 0)
	{
		debug ("Could not enumerate interfaces for local discovery");
		free (d);
		return NULL;
	}

	d->ifaces = NULL;
	ifno = 0;

	for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next)
	{
		struct teredo_discovery_interface *list = d->ifaces;
		struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;

		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (!(ifa->ifa_flags & IFF_MULTICAST))
			continue;

		if (!params->forced
		 && is_ipv4_global_unicast (sa->sin_addr.s_addr))
			continue;
		if (params->ifname_re
		 && regexec(params->ifname_re, ifa->ifa_name, 0, NULL, 0) != 0)
			continue;

		list = realloc (list, (ifno + 2) * sizeof (*d->ifaces));
		if(list == NULL)
		{
			debug ("Out of memory.");
			break; // memory error
		}

		d->ifaces = list;
		d->ifaces[ifno].addr = sa->sin_addr.s_addr;
		ifno++;
	}

	freeifaddrs(ifaddrs);

	if (d->ifaces == NULL)
	{
		debug ("No suitable interfaces found for local discovery");
		free (d);
		return NULL;
	}
	d->ifaces[ifno].addr = 0;

	/* Setup the multicast-receiving socket */

	d->fd = teredo_socket (0, htons (IPPORT_TEREDO));
	if (d->fd == -1)
	{
		debug ("Could not create the local discovery socket");
		free (d->ifaces);
		free (d);
		return NULL;
	}

	for (ifno = 0; d->ifaces[ifno].addr; ifno++)
		teredo_discovery_joinmcast (d->fd, d->ifaces[ifno].addr);

	d->opaque = opaque;
	d->proc = proc;
	d->recv = teredo_thread_start (teredo_discovery_thread, d);

	/* Start the discovery procedure thread */

	memcpy (&d->src, src, sizeof d->src);
	d->mcast_fd = fd;
	if (pthread_create (&d->send, NULL, teredo_mcast_thread, d))
	{
		teredo_close (d->mcast_fd);
		teredo_discovery_release (d);
		d = NULL;
	}
	return d;
}


struct teredo_discovery *teredo_discovery_grab (teredo_discovery *d)
{
	assert (d->refcnt);

	d->refcnt++;
	return d;
}


void teredo_discovery_release (teredo_discovery *d)
{
	assert (d->refcnt);

	if (--d->refcnt)
		return;

	pthread_cancel (d->send);
	pthread_join (d->send, NULL);

	assert (d->recv == NULL);

	free (d->ifaces);
	free (d);
}


void teredo_discovery_stop (teredo_discovery *d)
{
	if (d->recv)
	{
		teredo_thread_stop (d->recv);
		d->recv = NULL;
	}

	teredo_close(d->fd);
	teredo_discovery_release(d);
}
