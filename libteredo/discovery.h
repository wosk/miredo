/*
 * @file discovery.h
 * @brief Local client discovery procedure
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

#ifndef LIBTEREDO_TEREDO_DISCOVERY_H
# define LIBTEREDO_TEREDO_DISCOVERY_H

/**
 * Teredo local client discovery procedure internal state.
 */
typedef struct teredo_discovery teredo_discovery;

/**
 * Sends discovery bubbles to all possible interfaces.
 *
 * @param fd socket to send the bubbles from.
 */
void teredo_discovery_send_bubbles (teredo_discovery *d, int fd);

/**
 * Returns true if the given @p packet looks like a discovery bubble.
 */
bool IsDiscoveryBubble (const teredo_packet *restrict packet);

/**
 * Creates and starts threads for the Teredo local client discovery procedure.
 * A list of interfaces suitable for the exchange of multicast local discovery
 * bubbles will be assembled for later use by SendDiscoveryBubble().
 *
 * @param params local discovery configuration parameters.
 * @param fd socket used for sending the discovery bubbles.
 * @param src source Teredo IPv6 address for the discovery bubbles.
 * @param proc IO procedure to use for receiving multicast traffic
 * @param opaque pointer passed to @p proc
 */
teredo_discovery *
teredo_discovery_start (const teredo_discovery_params *params,
                        int fd, const struct in6_addr *src,
                        void (*proc)(void *, int fd), void *opaque);

/**
 * Protects a @c teredo_discovery object from destruction until
 * teredo_discovery_release() is called.
 *
 * @param d the discovery object to grab
 * @return @c d
 */
struct teredo_discovery *teredo_discovery_grab (teredo_discovery *d);

/**
 * Release a previously grabbed @c teredo_discovery object.
 * A reference counter is used: this function must be called exactly as many
 * times as teredo_discovery_grab() has been used.
 */
void teredo_discovery_release (teredo_discovery *d);

/**
 * Stops and destroys discovery threads created by teredo_discovery_start().
 *
 * @param d non-NULL pointer from teredo_discovery_start().
 */
void teredo_discovery_stop (teredo_discovery *d);

#endif /* ifndef LIBTEREDO_TEREDO_DISCOVERY_H */
