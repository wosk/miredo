/*
 * clock.c - Fast-lookup 1Hz clock
 */

/***********************************************************************
 *  Copyright © 2006-2014 Rémi Denis-Courmont.                         *
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

#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h> // _POSIX_*
#include <pthread.h>

#include "clock.h"
#include "debug.h"

typedef struct
{
	time_t            value;
#if (_POSIX_MONOTONIC_CLOCK == 0)
	clockid_t         id;
#endif
	bool              fresh;

	uintptr_t         refs;
	pthread_mutex_t   lock;
	pthread_cond_t    wait;
	pthread_t         thread;
} teredo_clockdata_t;

static void cleanup_unlock (void *mutex)
{
	pthread_mutex_unlock (mutex);
}

static inline clockid_t teredo_clock_id(teredo_clockdata_t *ctx)
{
#if (_POSIX_MONOTONIC_CLOCK > 0)
	(void) ctx;
	return CLOCK_MONOTONIC;
#elif (_POSIX_MONOTONIC_CLOCK == 0)
	return ctx->id;
#else
	(void) ctx;
	return CLOCK_REALTIME;
#endif
}

static LIBTEREDO_NORETURN void *teredo_clock_thread (void *data)
{
	teredo_clockdata_t *ctx = data;
	clockid_t id = teredo_clock_id (ctx);
	struct timespec ts = { 0, 0 };

	pthread_mutex_lock (&ctx->lock);
	for (;;)
	{
		pthread_cleanup_push (cleanup_unlock, &ctx->lock);
		while (!ctx->fresh)
			pthread_cond_wait (&ctx->wait, &ctx->lock);

		ts.tv_sec = ctx->value + 1;
		pthread_cleanup_pop (1);

		while (clock_nanosleep (id, TIMER_ABSTIME, &ts, &ts));

		pthread_mutex_lock (&ctx->lock);
		ctx->fresh = false;
	}
}

static teredo_clockdata_t instance =
{
	.fresh = false,
	.refs = 0,
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.wait = PTHREAD_COND_INITIALIZER,
};

unsigned long teredo_clock (void)
{
	teredo_clockdata_t *ctx = &instance;
	clockid_t id = teredo_clock_id (ctx);
	struct timespec ts;

	pthread_mutex_lock (&ctx->lock);
	if (ctx->fresh)
		ts.tv_sec = ctx->value;
	else
	{
		ctx->fresh = ctx->refs != 0;

		clock_gettime (id, &ts);
		ctx->value = ts.tv_sec;

		pthread_cond_signal (&ctx->wait);
	}
	pthread_mutex_unlock (&ctx->lock);

	return ts.tv_sec;
}

void teredo_clock_init (void)
{
	teredo_clockdata_t *ctx = &instance;

	pthread_mutex_lock (&ctx->lock);
	if (ctx->refs == 0)
	{
#if (_POSIX_MONOTONIC_CLOCK == 0)
		/* Run-time POSIX monotonic clock detection */
		ctx->id = (sysconf (_SC_MONOTONIC_CLOCK) > 0) ? CLOCK_MONOTONIC
		                                              : CLOCK_REALTIME;
#endif

		if (pthread_create (&ctx->thread, NULL,
		                    teredo_clock_thread, ctx) == 0)
			ctx->refs++;
	}
	else
		ctx->refs++;
	pthread_mutex_unlock (&ctx->lock);
}

void teredo_clock_deinit (void)
{
	teredo_clockdata_t *ctx = &instance;

	pthread_mutex_lock (&ctx->lock);
	if (ctx->refs > 0 && --ctx->refs > 0)
		ctx = NULL;
	pthread_mutex_unlock (&instance.lock);

	if (ctx != NULL)
	{
		pthread_cancel (ctx->thread);
		pthread_join (ctx->thread, NULL);
	}
}
