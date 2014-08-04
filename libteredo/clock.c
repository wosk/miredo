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

static clockid_t coarse_clock_id; /* Coarse clock */
clockid_t teredo_clock_id; /* Precise clock */

static void teredo_clock_select (void)
{
#if (_POSIX_MONOTONIC_CLOCK > 0)
	teredo_clock_id = CLOCK_MONOTONIC;
#elif (_POSIX_MONOTONIC_CLOCK == 0)
	/* Run-time POSIX monotonic clock detection */
	if (sysconf (_SC_MONOTONIC_CLOCK) > 0)
		teredo_clock_id = CLOCK_MONOTONIC;
	else
		teredo_clock_id = CLOCK_REALTIME;
#else
	teredo_clock_id = CLOCK_REALTIME;
#endif
	coarse_clock_id = teredo_clock_id;

#ifdef CLOCK_MONOTONIC_COARSE
	struct timespec coarseness;

	if (clock_getres(CLOCK_MONOTONIC_COARSE, &coarseness) == 0
	 && (coarseness.tv_sec <= 0
	  || (coarseness.tv_sec == 1 && coarseness.tv_nsec == 0)))
		coarse_clock_id = CLOCK_MONOTONIC_COARSE;
#endif
}

unsigned long teredo_clock (void)
{
	struct timespec ts;

	clock_gettime (coarse_clock_id, &ts);
	return ts.tv_sec;
}

void teredo_clock_init (void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;

	pthread_once(&once, teredo_clock_select);
}
