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
	void (*proc)(void *, int);
	void *opaque;
	int send_fd;
	int recv_fd;
	struct in6_addr src;
	teredo_thread *recv_thread;
	pthread_t send_thread;
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

	for (;;)
	{
		teredo_discovery_send_bubble (d->send_fd, &d->src);

		int interval = 200 + teredo_get_flbits (teredo_clock ()) % 100;

		struct timespec delay = { .tv_sec = interval };
		teredo_sleep (&delay);
	}
}

static void *teredo_discovery_thread (void *data)
{
	teredo_discovery *d = data;

	d->proc (d->opaque, d->recv_fd);
	return NULL; /* dead */
}

teredo_discovery *
teredo_discovery_start (const teredo_discovery_params *params,
                        int fd, const struct in6_addr *src,
                        void (*proc)(void *, int fd), void *opaque)
{
	teredo_discovery *d = malloc (sizeof (*d));
	if (d == NULL)
		return NULL;

	/* Setup the multicast-receiving socket */

	d->recv_fd = teredo_socket (0, htons (IPPORT_TEREDO));
	if (d->recv_fd == -1)
	{
		debug ("Could not create the local discovery socket");
		free (d);
		return NULL;
	}

	struct ip_mreq mreq =
	{
		.imr_multiaddr = { .s_addr = htonl (TEREDO_DISCOVERY_IPV4) },
	};

	if (setsockopt (d->recv_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
	                &mreq, sizeof mreq))
		debug ("Local discovery multicast subscription failure: %m");

	d->opaque = opaque;
	d->proc = proc;
	d->send_fd = fd;
	d->src = *src;

	d->recv_thread = teredo_thread_start (teredo_discovery_thread, d);

	/* Start the discovery procedure thread */

	if (pthread_create (&d->send_thread, NULL, teredo_mcast_thread, d))
	{
		if (d->recv_thread != NULL)
			teredo_thread_stop (d->recv_thread);

		teredo_close (d->recv_fd);
		free (d);
		return NULL;
	}
	return d;
}

void teredo_discovery_stop (teredo_discovery *d)
{
	if (d->recv_thread != NULL)
		teredo_thread_stop (d->recv_thread);

	pthread_cancel (d->send_thread);
	pthread_join (d->send_thread, NULL);

	teredo_close(d->recv_fd);
	free (d);
}
