/**
 * @file clock.h
 * @brief libteredo internal low-precision (1 Hz) clock
 *
 * This is way faster than calling time() for every packet transmitted or
 * received. The first implementation was using POSIX timers, but it might
 * be a bit overkill to spawn a thread every second to simply increment an
 * integer. Also, POSIX timers with thread event delivery has a terrible
 * portability at the time of writing (June 2006). Basically, recent
 * GNU/Linux have it, and that's about it... no uClibc support, only in
 * -current for FreeBSD...
 */

/***********************************************************************
 *  Copyright © 2006 Rémi Denis-Courmont.                              *
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

#ifndef LIBTEREDO_CLOCK_H
# define LIBTEREDO_CLOCK_H

/**
 * Low-precision clock time value
 */
typedef unsigned long teredo_clock_t;

/**
 * @return current clock value; undefined if the clock is not running.
 */
teredo_clock_t teredo_clock (void);

void teredo_clock_init (void);

# include <time.h>

extern clockid_t teredo_clock_id;

static inline void teredo_gettime (struct timespec *now)
{
	clock_gettime (teredo_clock_id, now);
}

static inline void teredo_wait (const struct timespec *dl)
{
	while (clock_nanosleep (teredo_clock_id, TIMER_ABSTIME, dl, NULL));
}

static inline void teredo_sleep (const struct timespec *d)
{
	struct timespec duration = *d;

	while (clock_nanosleep (teredo_clock_id, 0, &duration, &duration));
}

#endif /* ifndef LIBTEREDO_CLOCK_H */
