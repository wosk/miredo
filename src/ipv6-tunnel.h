/*
 * ipv6-tunnel.h - IPv6 interface class declaration
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

#ifndef MIREDO_IPV6_TUNNEL_H
# define MIREDO_IPV6_TUNNEL_H

# ifndef __cplusplus
#  error C++ only header
# endif

# include <stddef.h>
# include <stdint.h>
# include <sys/types.h>
# include <sys/select.h>

struct ip6_hdr;

class IPv6Tunnel
{
	private:
		int fd;
		uint8_t pbuf[65535 + 4];
		size_t plen;

	public:
		IPv6Tunnel (const char *ifname,
				const char *tundev = "/dev/net/tun");
		~IPv6Tunnel ();

		int operator! (void)
		{
			return fd == -1;
		}

		/*
		 * Registers file descriptors in an fd_set for use with
		 * select(). Returns the "biggest" file descriptor
		 * registered (useful as the first parameter to selcet()).
		 */
		int RegisterReadSet (fd_set *readset) const;

		/*
		 * Checks an fd_set, receives a packet.
		 *
		 * Returns 0 on success, -1 if no packet were to be received.
		 *
		 * In case of success, one can use GetBuffer, GetIPv6Header,
		 * etc. functions. Otherwise, these functions will return
		 * bogus values.
		 */
		int ReceivePacket (const fd_set *readset);

		/*
		 * Sends an IPv6 packet at <packet>, of length <len>.
		 */
		int SendPacket (const void *packet, size_t len) const;

		/*
		 * Returns a pointer to the last received packet
		 * (by ReceivePacket()).
		 */
		const uint8_t *GetPacket (size_t& length) const
		{
			length = plen - 4;
			return pbuf + 4;
		}
};


#endif /* ifndef MIREDO_IPV6TUNNEL_H */
