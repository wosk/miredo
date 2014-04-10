/**
 * @file thread.h
 * @brief IO thread management
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

#ifndef LIBTEREDO_IOTHREAD_H
# define LIBTEREDO_IOTHREAD_H

typedef pthread_t teredo_thread;

/**
 * Start a new IO thread.
 *
 * @param proc callback function to be run as the new thread.
 * @param opaque opaque pointer passed to @p proc.
 *
 * @return the new IO thread on success, NULL on error.
 */
static inline
teredo_thread *teredo_thread_start (void *(*proc)(void *), void *opaque)
{
	teredo_thread *th = malloc (sizeof (*th));
	if (th == NULL)
		return NULL;

	if (pthread_create (th, NULL, proc, opaque))
	{
		free (th);
		th = NULL;
	}

	return th;
}

static inline
void teredo_thread_stop (teredo_thread *th)
{
	pthread_cancel (*th);
	pthread_join (*th, NULL);
	free (th);
}
#endif /* ifndef LIBTEREDO_THREAD_H */
