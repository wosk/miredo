/*
 * v4global.c - Check whether an IPv4 address is global
 */

/***********************************************************************
 *  Copyright © 2004-2005 Rémi Denis-Courmont.                         *
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

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <netinet/in.h> // ntohl()

#include "v4global.h"

bool is_ipv4_global_unicast (uint32_t ip)
{
	/*
	 * NOTE (FIXME)
	 *
	 * Additionnaly, the specification forbids broadcast addresses of
	 * subnets the node is attached to. It would be quite complex to check
	 * that. We don't set the SO_BROADCAST socket option, which should be
	 * sufficient to avoid security issues.
	 */

	if ((ip & htonl (0x80000000)) == 0)
	{
		// Forbidden classes A:
		// 0.0.0.0/8, 10.0.0.0/8 and 127.0.0.0/8
		ip &= htonl (0xff000000);
		return ( ip                       != htonl (0x0a000000)) &&
		       ( ip                       != htonl (0x7f000000)) &&
		       ( ip                       != htonl (0x00000000));
	}

	if ((ip & htonl (0x40000000)) == 0)
	{
		// Forbidden classes B:
		// 169.254.0.0/16, 172.16.0.0/12
		return ((ip & htonl (0xffff0000)) != htonl (0xa9fe0000)) &&
		       ((ip & htonl (0xfff00000)) != htonl (0xac100000)); 
	}

	if ((ip & htonl (0x20000000)) == 0)
	{
		// Forbidden classes C:
		// 192.168.0.0/16, 192.88.99.0/24
		return ((ip & htonl (0xffff0000)) != htonl (0xc0a80000)) &&
		       ((ip & htonl (0xffffff00)) != htonl (0xc0586200));
	}

	if ((ip & htonl (0x10000000)) == 0)
		// Whole class D space (multicast) is forbidden:
		return false;

	return ip != htonl (0xffffffff);
}

bool is_ipv4_private_unicast (uint32_t ip)
{
	if ((ip & htonl (0xffff0000)) == htonl (0xc0a80000))
		return true; // 192.168.0.0/16
	if ((ip & htonl (0xfff00000)) == htonl (0xac100000))
		return true; // 172.16.0.0/12
	if ((ip & htonl (0xff000000)) == htonl (0x0a000000))
		return true; // 10.0.0.0/8
	if ((ip & htonl (0xffff0000)) == htonl (0xa9fe0000))
		return true; // 169.254.0.0/16
	return false;
}
