/*
 * diagnose.c - Libtun6 sanity test
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <syslog.h> /* TODO: do not use syslog within the library */
#include "tun6.h"
#include <sys/socket.h> /* OpenBSD wants that for <net/if.h> */
#include <net/if.h>
#include <errno.h>

static const char *invalid_name =
	"Overly-long-interface-name-that-will-not-work";

int main (void)
{
	openlog ("libtun6-diagnose", LOG_PERROR, LOG_USER);
	tun6 *t = tun6_create (invalid_name);
	if (t != NULL)
	{
		tun6_destroy (t);
		return 1;
	}

	t = tun6_create (NULL);
	if (t == NULL)
	{
		if ((errno == EPERM) || (errno == EACCES))
		{
			puts ("Warning: cannot perform full libtun6 test");
			return 77;
		}
		return 1;
	}
	tun6_destroy (t);

	/* TODO: further testing */
	t = tun6_create ("diagnose");
	if (t == NULL)
	{
		if (errno == ENOSYS)
		{
			puts ("Warning: cannot rename tunnel interface.");
			return 0;
		}
		return 1;
	}
	unsigned id = tun6_getId (t);
	if ((id == 0) || (if_nametoindex ("diagnose") != id))
		goto fail;

	tun6_destroy (t);
	closelog ();
	return 0;
fail:
	tun6_destroy (t);
	return 1;
}
