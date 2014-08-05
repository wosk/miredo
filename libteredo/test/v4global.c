/*
 * v4global.c - Global IPv4 test
 */

/***********************************************************************
 *  Copyright © 2007 Rémi Denis-Courmont.                              *
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

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>


#include "v4global.h"

static void check (const char *str, bool glob, bool priv)
{
	uint32_t ip = inet_addr (str);

	if (is_ipv4_global_unicast (ip) != glob)
	{
		fprintf (stderr, "Error with %s\n", str);
		exit (1);
	}

	if (is_ipv4_private_unicast (ip) != priv)
	{
		fprintf (stderr, "Error with %s\n", str);
		exit (1);
	}
}

static void check_glob (const char *str)
{
	check (str, true, false);
}

static void check_priv (const char *str)
{
	check (str, false, true);
}

static void check_misc (const char *str)
{
	check (str, false, false);
}

int main (void)
{
	check_misc ("0.1.2.3");
	check_glob ("9.8.7.6");
	check_priv ("10.11.12.133");
	check_glob ("11.12.13.14");
	check_glob ("126.127.128.129");
	check_misc ("127.0.0.1");
	check_priv ("169.254.12.42");
	check_priv ("172.20.123.45");
	check_glob ("192.0.2.10");
	check_glob ("192.167.255.255");
	check_priv ("192.168.234.123");
	check_glob ("223.255.255.254");
	check_misc ("232.11.22.33");
	check_misc ("255.255.255.255");
	return 0;
}
