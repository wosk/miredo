/*
 * relay.h - Teredo relay peers list declaration
 * $Id: relay.h,v 1.20 2004/08/27 14:54:52 rdenisc Exp $
 *
 * See "Teredo: Tunneling IPv6 over UDP through NATs"
 * for more information
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

#ifndef LIBTEREDO_RELAY_H
# define LIBTEREDO_RELAY_H

# include <inttypes.h>
# include <sys/time.h> // struct timeval

# include <libteredo/teredo.h>
# include <libteredo/teredo-udp.h>

struct ip6_hdr;
struct in6_addr;
union teredo_addr;

struct __TeredoRelay_peer;


// big TODO: make all functions re-entrant safe
//           make all functions thread-safe
class TeredoRelay
{
	private:
		/*** Internal stuff ***/
		union teredo_addr addr;
		struct
		{
			struct timeval next, serv;
			unsigned char nonce[8];
			unsigned probe:2;
			unsigned count:3;
		} state;

		struct __TeredoRelay_peer *head;

		TeredoRelayUDP sock;

		struct __TeredoRelay_peer *AllocatePeer (void);
		struct __TeredoRelay_peer *FindPeer (const struct in6_addr *addr);

		/*** Callbacks ***/
		/*
		 * Sends an IPv6 packet from Teredo toward the IPv6 Internet.
		 *
		 * Returns 0 on success, -1 on error.
		 */
		virtual int SendIPv6Packet (const void *packet,
						size_t length) = 0;

		/*
		 * Tries to define the Teredo client IPv6 address. This is an
		 * indication that the Teredo tunneling interface is ready.
		 * The default implementation in base class TeredoRelay does
		 * nothing.
		 *
		 * Returns 0 on success, -1 on error.
		 * TODO: handle error in calling function.
		 */
		virtual int NotifyUp (const struct in6_addr *addr);

		/*
		 * Indicates that the Teredo tunneling interface is no longer
		 * ready to process packets.
		 * Any packet sent when the relay/client is down will be
		 * ignored.
		 */
		virtual int NotifyDown (void);

	protected:
		/*
		 * Creates a Teredo relay manually (ie. one that does not
		 * qualify with a Teredo server and has no Teredo IPv6
		 * address). The prefix must therefore be specified.
		 *
		 * If port is nul, the OS will choose an available UDP port
		 * for communication. This is NOT a good idea if you are
		 * behind a fascist firewall, as the port might be blocked.
		 *
		 * TODO: allow the caller to specify an IPv4 address to bind
		 * to.
		 */
		TeredoRelay (uint32_t pref, uint16_t port /*= 0*/,
				bool cone /*= true*/);

		/*
		 * Creates a Teredo client/relay automatically. The client
		 * will try to qualify and get a Teredo IPv6 address from the
		 * server.
		 *
		 * TODO: support for secure qualification
		 */
		TeredoRelay (uint32_t server_ip, uint16_t port = 0);

	public:
		virtual ~TeredoRelay ();

		int operator! (void) const
		{
			return !sock;
		}

		/*
		 * Transmits a packet from IPv6 Internet via Teredo,
		 * i.e. performs "Packet transmission".
		 * This function will not block because normal IPv4 stacks do
		 * not block when sending UDP packets.
		 * Not thread-safe yet.
		 */
		int SendPacket (const void *packet, size_t len);

		/*
		 * Receives a packet from Teredo to IPv6 Internet, i.e.
		 * performs "Packet reception". This function will block until
		 * a Teredo packet is received.
		 * Not thread-safe yet.
		 */
		int ReceivePacket (void);

		/*
		 * Sends pending queued UDP packets (Teredo bubbles,
		 * Teredo pings, Teredo router solicitation) if any.
		 *
		 * Call this function as frequently as possible.
		 * Not thread-safe yet.
		 */
		int Process (void);

		/*
		 * Returns true if the relay/client is behind a cone NAT.
		 * The result is not meaningful if the client is not fully
		 * qualified.
		 */
		uint32_t GetPrefix (void) const
		{
			return IN6_TEREDO_PREFIX (&addr);
		}

		uint32_t GetServerIP (void) const
		{
			return IN6_TEREDO_SERVER (&addr);
		}

		bool IsCone (void) const
		{
			return IN6_IS_TEREDO_ADDR_CONE (&addr);
		}

		uint16_t GetMappedPort (void) const
		{
			return IN6_TEREDO_PORT (&addr);
		}
		
		uint32_t GetMappedIP (void) const
		{
			return IN6_TEREDO_IPV4 (&addr);
		}

		bool IsClient (void) const
		{
			return GetServerIP () != 0;
		}

		bool IsRelay (void) const
		{
			return GetServerIP () == 0;
		}

		bool IsRunning (void) const
		{
			return is_valid_teredo_prefix (GetPrefix ());
		}


		int RegisterReadSet (fd_set *rs) const
		{
			return sock.RegisterReadSet (rs);
		}
};

#endif /* ifndef MIREDO_RELAY_H */

