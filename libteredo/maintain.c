/*
 * maintain.c - Teredo client qualification & maintenance
 *
 * See "Teredo: Tunneling IPv6 over UDP through NATs"
 * for more information
 */

/***********************************************************************
 *  Copyright © 2004-2007 Rémi Denis-Courmont.                         *
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

#include <gettext.h>

#include <string.h> /* memcmp() */
#include <assert.h>

#include <stdbool.h>
#include <inttypes.h>

#include <sys/types.h>
#include <unistd.h> /* sysconf() */
#include <sys/socket.h> /* AF_INET */
#include <netinet/in.h> /* struct in6_addr */
#include <netinet/ip6.h> /* struct ip6_hdr */
#include <netdb.h> /* getaddrinfo(), gai_strerror() */
#include <syslog.h>
#include <stdlib.h> /* malloc(), free() */
#include <errno.h> /* EINTR */
#include <pthread.h>

#include "clock.h"
#include "teredo.h"
#include "teredo-udp.h"
#include "packets.h"

#include "security.h"
#include "maintain.h"
#include "v4global.h" // is_ipv4_global_unicast()
#include "debug.h"

struct teredo_maintenance
{
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t received;

	int fd;
	struct
	{
		teredo_state state;
		teredo_state_cb cb;
		void *opaque;
	} state;
	char *server;

	uint32_t server_ip;
	unsigned char nonce[8];

	unsigned qualification_delay;
	unsigned qualification_retries;
	unsigned refresh_delay;
	unsigned restart_delay;
};


/**
 * Resolves an IPv4 address (thread-safe).
 *
 * @return 0 on success, or an error value as defined for getaddrinfo().
 */
static int getipv4byname (const char *restrict name, uint32_t *restrict ipv4)
{
	struct addrinfo hints =
	{
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM
	}, *res;

	int val = getaddrinfo (name, NULL, &hints, &res);
	if (val)
		return val;

	*ipv4 = ((const struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
	freeaddrinfo (res);

	return 0;
}


/**
 * Checks and parses a received Router Advertisement.
 *
 * @return 0 if successful.
 */
int teredo_maintenance_process (teredo_maintenance *restrict m,
                                const teredo_packet *restrict packet)
{
	teredo_state state;
	int ret = 0;

	state.mtu = 1280;
	state.up = true;

	/*
	 * We don't accept router advertisement without nonce.
	 * It is far too easy to spoof such packets.
	 */
	if ((packet->source_port != htons (IPPORT_TEREDO))
	    /* TODO: check for primary or secondary server address */
	 || !packet->auth_present
	 || !IN6_ARE_ADDR_EQUAL (&packet->ip6->ip6_dst, &teredo_restrict))
		return EINVAL;

	/* TODO: fail instead of ignoring the packet? */
	if (packet->auth_fail)
	{
		syslog (LOG_ERR, _("Authentication with server failed."));
		return EACCES;
	}

	pthread_mutex_lock(&m->lock);
	if (m->state.state.up /* Already up, not expecting message */
	 || m->server_ip == 0 /* Server not resolved yet */
	 || memcmp (packet->auth_nonce, m->nonce, 8) /* Nonce mismatch */)
		ret = EPERM;
	else
	if (teredo_parse_ra (packet, &state.addr, false /*cone*/, &state.mtu)
	/* TODO: try to work-around incorrect server IP */
	 || state.addr.teredo.server_ip != m->server_ip)
		ret = EINVAL;
	else
	{	/* Valid router advertisement received! */
		state.ipv4 = packet->dest_ipv4;

		m->state.state = state;
		pthread_cond_signal(&m->received);
	}
	pthread_mutex_unlock(&m->lock);
	return ret;
}


/**
 * Make sure ts is in the future. If not, set it to the current time.
 * @return false if (*ts) was changed, true otherwise.
 */
static bool
checkTimeDrift (struct timespec *ts)
{
	struct timespec now;
	teredo_gettime (&now);

	if ((now.tv_sec > ts->tv_sec)
	 || ((now.tv_sec == ts->tv_sec) && (now.tv_nsec > ts->tv_nsec)))
	{
		/* process stopped, CPU starved or system suspended */
		syslog (LOG_WARNING, _("Too much time drift. Resynchronizing."));
		*ts = now;
		return false;
	}
	return true;
}


/*
 * Implementation notes:
 * - Optional Teredo interval determination procedure was never implemented.
 *   It adds NAT binding maintenance brittleness in addition to implementation
 *   complexity, and is not necessary for RFC4380 compliance.
 *   Also STUN RFC3489bis deprecates this type of behavior.
 * - NAT cone type probing was removed in Miredo version 0.9.5. This violated
 *   RFC4380. Since then, draft-krishnan-v6ops-teredo-update has nevertheless
 *   confirmed that the cone type should be dropped.
 * - NAT symmetric probing was removed in Miredo version 1.1.0, which deepens
 *   the gap between Miredo and RFC4380. Still, this is fairly consistent with
 *   RFC3489bis.
 */

/*
 * Teredo client maintenance procedure
 */
static inline LIBTEREDO_NORETURN
void maintenance_thread (teredo_maintenance *m)
{
	struct timespec deadline = { 0, 0 };
	unsigned retries = 0;
	enum
	{
		TERR_NONE,
		TERR_BLACKHOLE
	} last_error = TERR_NONE;

	/*
	 * Qualification/maintenance procedure
	 */
	for (;;)
	{
		int canc;
		uint32_t server_ip = m->server_ip;

		/* Resolve server IPv4 addresses */
		while (server_ip == 0)
		{
			int val = getipv4byname (m->server, &server_ip);
			teredo_gettime (&deadline);

			pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &canc);

			if (val != 0)
			{
				/* DNS resolution failed */
				syslog (LOG_ERR,
				        _("Cannot resolve Teredo server address \"%s\": %s"),
				        m->server, gai_strerror (val));
			}
			else
			if (!is_ipv4_global_unicast (server_ip))
			{
				syslog (LOG_ERR,
				        _("Teredo server has a non global IPv4 address."));
				server_ip = 0;
			}
			else
			{
				/* DNS resolution succeeded */
				/* Tells Teredo client about the new server's IP */
				assert (!m->state.state.up);
				m->state.state.addr.teredo.server_ip = m->server_ip;
				m->state.cb (&m->state.state, m->state.opaque);
			}

			pthread_setcancelstate (canc, NULL);

			if (server_ip != 0)
				break;

			/* wait some time before next resolution attempt */
			deadline.tv_sec += m->restart_delay;
			teredo_wait (&deadline);
		}

		/* SEND ROUTER SOLICATION */
		do
			deadline.tv_sec += m->qualification_delay;
		while (!checkTimeDrift (&deadline));

		teredo_state *state = &m->state.state, ostate;

		pthread_mutex_lock(&m->lock);
		teredo_get_nonce (deadline.tv_sec, server_ip, htons (IPPORT_TEREDO),
		                  m->nonce);
		teredo_send_rs (m->fd, server_ip, m->nonce, false);
		m->server_ip = server_ip;
		ostate = *state;

		/* RECEIVE ROUTER ADVERTISEMENT */
		state->up = false;
		while (pthread_cond_timedwait (&m->received, &m->lock, &deadline) == 0
		    && !state->up);

		unsigned delay = 0;

		pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &canc);

		/* UPDATE FINITE STATE MACHINE */
		if (state->up)
		{	/* Router Advertisement received and parsed succesfully */
			retries = 0;

			/* 12-bits Teredo flags randomization */
			state->addr.teredo.flags = ostate.addr.teredo.flags;
			if (!IN6_ARE_ADDR_EQUAL (&state->addr.ip6, &ostate.addr.ip6))
			{
				uint16_t f = teredo_get_flbits (deadline.tv_sec);
				state->addr.teredo.flags = f & htons (TEREDO_RANDOM_MASK);
			}

			if (!ostate.up
			 || !IN6_ARE_ADDR_EQUAL (&ostate.addr.ip6, &state->addr.ip6)
			 || ostate.mtu != state->mtu)
			{
				syslog (LOG_NOTICE, _("New Teredo address/MTU"));
				m->state.cb (state, m->state.opaque);
			}

			/* Success: schedule next NAT binding maintenance */
			last_error = TERR_NONE;
			delay = m->refresh_delay;
		}
		else
		{	/* No response */
			if (++retries >= m->qualification_retries)
			{
				retries = 0;

				/* No response from server */
				if (last_error != TERR_BLACKHOLE)
				{
					syslog (LOG_INFO, _("No reply from Teredo server"));
					last_error = TERR_BLACKHOLE;
				}

				if (ostate.up)
				{
					syslog (LOG_NOTICE, _("Lost Teredo connectivity"));
					m->state.cb (state, m->state.opaque);
					m->server_ip = 0;
				}

				/* Wait some time before retrying */
				delay = m->restart_delay;
			}
		}

		pthread_setcancelstate (canc, NULL);
		pthread_mutex_unlock (&m->lock);

		/* WAIT UNTIL NEXT SOLICITATION */
		/* TODO: watch for new interface events
		 * (netlink on Linux, PF_ROUTE on BSD) */
		if (delay)
		{
			deadline.tv_sec -= m->qualification_delay;
			deadline.tv_sec += delay;
			teredo_wait (&deadline);
		}
	}
	/* dead code */
}


static LIBTEREDO_NORETURN void *do_maintenance (void *opaque)
{
	maintenance_thread ((teredo_maintenance *)opaque);
}


static const unsigned QualificationDelay = 4; // seconds
static const unsigned QualificationRetries = 3;

static const unsigned RefreshDelay = 30; // seconds
static const unsigned RestartDelay = 100; // seconds

teredo_maintenance *
teredo_maintenance_start (int fd, teredo_state_cb cb, void *opaque,
                          const char *s1, const char *s2,
                          unsigned q_sec, unsigned q_retries,
                          unsigned refresh_sec, unsigned restart_sec)
{
	teredo_maintenance *m = malloc (sizeof (*m));

	if (m == NULL)
		return NULL;

	memset (m, 0, sizeof (*m));
	m->fd = fd;
	m->state.cb = cb;
	m->state.opaque = opaque;

	assert (s1 != NULL);
	m->server = strdup (s1);
	(void)s2;

	m->qualification_delay = q_sec ?: QualificationDelay;
	m->qualification_retries = q_retries ?: QualificationRetries;
	m->refresh_delay = refresh_sec ?: RefreshDelay;
	m->restart_delay = restart_sec ?: RestartDelay;

	if (m->server == NULL)
	{
		free (m);
		return NULL;
	}
	else
	{
		pthread_condattr_t attr;

		pthread_condattr_init (&attr);
#if (_POSIX_CLOCK_SELECTION > 0) && (_POSIX_MONOTONIC_CLOCK >= 0)
		pthread_condattr_setclock (&attr, CLOCK_MONOTONIC);
#endif
		pthread_cond_init (&m->received, &attr);
		pthread_condattr_destroy (&attr);
	}

	pthread_mutex_init (&m->lock, NULL);

	int err = pthread_create (&m->thread, NULL, do_maintenance, m);
	if (err == 0)
		return m;

	errno = err;
	syslog (LOG_ALERT, _("Error (%s): %m"), "pthread_create");

	pthread_cond_destroy (&m->received);
	pthread_mutex_destroy (&m->lock);

	free (m->server);
	free (m);
	return NULL;
}


void teredo_maintenance_stop (teredo_maintenance *m)
{
	pthread_cancel (m->thread);
	pthread_join (m->thread, NULL);

	pthread_cond_destroy (&m->received);
	pthread_mutex_destroy (&m->lock);

	free (m->server);
	free (m);
}
