/*
 * iothread.c - IO thread management for Teredo tunnels
 */

/***********************************************************************
 *  Copyright © 2009 Jérémie Koenig                                    *
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
#include <stdlib.h> // malloc()
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>

#include "iothread.h"
#include "teredo-udp.h" // teredo_close()
#include "debug.h"


struct teredo_iothread
{
	pthread_t thread;
	void *(*proc)(void *);
	void *opaque;
};


static void *teredo_iothread_run (void *data)
{
	teredo_iothread *io = data;
	return io->proc (io->opaque);
}


teredo_iothread *teredo_iothread_start (void *(*proc)(void *), void *opaque)
{
	teredo_iothread *io = malloc (sizeof *io);
	if (io == NULL)
		return NULL;

	io->proc = proc;
	io->opaque = opaque;

	if (pthread_create (&io->thread, NULL, teredo_iothread_run, io))
	{
		debug ("Could not create IO thread");
		free (io);
		return NULL;
	}

#ifndef NDEBUG
	debug ("IO thread started (%p, %p, %p)", io, proc, opaque);
#endif

	return io;
}


void teredo_iothread_stop (teredo_iothread *io)
{
	pthread_cancel (io->thread);
	pthread_join (io->thread, NULL);

#ifndef NDEBUG
	debug ("IO thread stopped (%p)", io);
#endif

	free (io);
}


