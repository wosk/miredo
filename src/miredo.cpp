/*
 * miredo.cpp - Unix Teredo server & relay implementation
 *              core functions
 * $Id: miredo.cpp,v 1.36 2004/08/27 15:04:32 rdenisc Exp $
 *
 * See "Teredo: Tunneling IPv6 over UDP through NATs"
 * for more information
 */

/***********************************************************************
 *  Copyright (C) 2004 Remi Denis-Courmont.                            *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license.         *
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h> // memset(), strsignal()
#include <stdlib.h> // daemon() on FreeBSD
#include <inttypes.h>
#include <signal.h> // sigaction()

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h> // struct sockaddr_in
#include <syslog.h>
#include <unistd.h> // uid_t
#include <sys/wait.h> // wait()

#ifndef LOG_PERROR
# define LOG_PERROR 0
#endif

#include "miredo.h"

#include <libtun6/ipv6-tunnel.h>

#include <libteredo/teredo.h>
#include "server.h"
#include "relay.h"
#include <privproc.h>

/*
 * Signal handlers
 *
 * We block all signals when one of those we catch is being handled.
 * SECURITY NOTE: these signal handlers might be called as root or not,
 * in the context of the privileged child process or in that of the main
 * unprivileged worker process. They must not compromise the child's security.
 */
static int should_exit;
static int should_reload;

/* 
 * The value of rootpid is defined to the PID of the permanent parent process
 * that reads the configuration before any signal handler is set.
 */
static pid_t rootpid;


static void
exit_handler (int signum)
{
	if (should_exit)
		return; // avoid infinite signal loop

	if (rootpid == getpid ())
		/* Signal handler run from the parent that loads configuration
		 * and respawns miredo */
		kill (0, signum);

	should_exit = signum;
}


static void
reload_handler (int signum)
{
	if (should_reload)
		return; // avoid infinite signal loop

	if (rootpid == getpid ())
		/* Signal handler run from the parent that loads configuration
		 * and respawns miredo */
		kill (0, signum);

	should_reload = signum;
}


/*
 * Main server function, with UDP datagrams receive loop.
 */
static int
teredo_server_relay (IPv6Tunnel& tunnel, TeredoRelay *relay = NULL,
			TeredoServer *server = NULL)
{
	/* Main loop */
	int exitcode = 0;

	while (exitcode == 0 && !should_exit && !should_reload)
	{
		/* Registers file descriptors */
		fd_set readset;
		struct timeval tv;
		FD_ZERO (&readset);

		int maxfd = -1;

		if (server != NULL)
		{
			int val = server->RegisterReadSet (&readset);
			if (val > maxfd)
				maxfd = val;
		}

		if (relay != NULL)
		{
			int val = tunnel.RegisterReadSet (&readset);
			if (val > maxfd)
				maxfd = val;

			val = relay->RegisterReadSet (&readset);
			if (val > maxfd)
				maxfd = val;
		}

		/*
		 * Short time-out to call relay->Proces () quite often.
		 */
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		/* Wait until one of them is ready for read */
		maxfd = select (maxfd + 1, &readset, NULL, NULL, &tv);
		if (maxfd <= 0) // interrupted by signal
			continue;

		/* Handle incoming data */
		if (server != NULL)
			server->ProcessTunnelPacket ();

		if (relay != NULL)
		{
			char pbuf[65535];
			int len;

			relay->Process ();

			/* Forwards IPv6 packet to Teredo
			 * (Packet transmission) */
			len = tunnel.ReceivePacket (&readset, pbuf,
							sizeof (pbuf));
			if (len > 0)
				relay->SendPacket (pbuf, len);

			/* Forwards Teredo packet to IPv6
			 * (Packet reception) */
			relay->ReceivePacket ();
		}
	}

	/* Termination */
	if (exitcode)
		return exitcode;
	if (should_reload)
		return -2;
	return 0;
}



/*
 * Returns an IPv4 address (network byte order) associated with hostname
 * <name>. Returns -1 on error.
 */
static int
getipv4byname (const char *name, uint32_t *ipv4)
{
	struct addrinfo help, *res;

	memset (&help, 0, sizeof (help));
	help.ai_family = AF_INET;
	help.ai_socktype = SOCK_DGRAM;
	help.ai_protocol = IPPROTO_UDP;

	int check = getaddrinfo (name, NULL, &help, &res);

	if (check)
	{
		syslog (LOG_ERR, _("Invalid hostname `%s': %s"),
			name, gai_strerror (check));
		return -1;
	}

	*ipv4 = ((const struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
	freeaddrinfo (res);
	return 0;
}


static int
getipv6byname (const char *name, struct in6_addr *ipv6)
{
	struct addrinfo help, *res;

	memset (&help, 0, sizeof (help));
	help.ai_family = PF_INET6;
	help.ai_socktype = SOCK_DGRAM;
	help.ai_protocol = IPPROTO_UDP;

	int check = getaddrinfo (name, NULL, &help, &res);

	if (check)
	{
		syslog (LOG_ERR, _("Invalid hostname '%s': %s"),
			name, gai_strerror (check));
		return -1;
	}

	memcpy (ipv6,
		&((const struct sockaddr_in6*)(res->ai_addr))->sin6_addr,
		sizeof (struct in6_addr));

	freeaddrinfo (res);
	return 0;
}


/*
 * Initialization stuff
 * (client_port is is host byte order)
 */
uid_t unpriv_uid = 0;


// TODO: support for client
#define MIREDO_CLIENT 2
#define MIREDO_CONE   1

static int
miredo_run (uint16_t client_port, const char *server_name,
		const char *prefix_name, const char *ifname, int mode)
{
	seteuid (unpriv_uid);

	/* default values */
	if (client_port == 0)
		/*
		 * We use 3545 as a Teredo service port.
		 * It is better to use a fixed port number for the
		 * purpose of firewalling, rather than a pseudo-random
		 * one (all the more as it might be a "dangerous"
		 * often firewalled port, such as 1214 as it happened
		 * to me once).
		 */
		client_port = IPPORT_TEREDO + 1;

	// server_name may be NULL, this is legal

	if (ifname == NULL)
		ifname = "teredo";
	if (prefix_name == NULL)
		prefix_name = DEFAULT_TEREDO_PREFIX_STR":";


	union teredo_addr prefix;
	if (getipv6byname (prefix_name, &prefix.ip6))
	{
		syslog (LOG_ALERT,
			_("Teredo IPv6 prefix not properly set."));
		return -1;
	}

	if (!is_valid_teredo_prefix (prefix.teredo.prefix))
	{
		syslog (LOG_ALERT,
			_("Invalid Teredo IPv6 prefix: %s."), prefix_name);
		return -1;
	}

	MiredoRelay *relay = NULL;
	MiredoServer *server = NULL;
	int fd = -1, retval = -1;

	if (seteuid (0))
		syslog (LOG_WARNING, _("SetUID to root failed: %m"));

	/* 
	 * Tunneling interface initialization
	 *
	 * NOTE: The Linux kernel does not allow setting up an address
	 * before the interface is up, and it tends to complain about its
	 * inability to set a link-scope address for the interface, as it
	 * lacks an hardware layer address.
	 */

	/*
	 * Must likely be root (unless the user was granted access to the
	 * device file).
	 */
	IPv6Tunnel tunnel (ifname);

	/*
	 * Must be root to do that. It's best to set MTU now to make sure we
	 * are root, rather than do it in the child privileged process.
	 * TODO: move BringUp() to privileged process, not for security, but
	 * to let the kernel know when the interface is really ready to
	 * reveive packets (ie. after qualification in case of Teredo client).
	 */
	if (!tunnel || tunnel.SetMTU (1280) || tunnel.BringUp ())
	{
		syslog (LOG_ALERT, _("Teredo tunnel setup failed."
					" You should be root to do that."));
		goto abort;
	}

	if (seteuid (unpriv_uid)
	 || ((fd = miredo_privileged_process (tunnel, &prefix.ip6)) == -1))
	{
		syslog (LOG_ALERT,
			_("Privileged process initialization failed: %m"));
		goto abort;
	}

	// Definitely drops privileges
	if (setuid (unpriv_uid))
	{
		syslog (LOG_ALERT, _("setuid failed: %m"));
		goto abort;
	}

	// Sets up server sockets
	if (((mode & MIREDO_CLIENT) == 0) && (server_name != NULL))
	{
		uint32_t ipv4;
		 
		if (getipv4byname (server_name, &ipv4))
		{
			syslog (LOG_ALERT, _("Fatal configuration error"));
			goto abort;
		}

		try
		{
			server = new MiredoServer (ipv4, htonl (ntohl (
							ipv4) + 1));
		}
		catch (...)
		{
			server = NULL;
			syslog (LOG_ALERT, _("Teredo server failure"));
			goto abort;
		}

		/*
		 * NOTE:
		 * While it is nowhere in the draft Teredo
		 * specification, it really seems that the secondary
		 * server IPv4 address has to be the one just after
		 * the primary server IPv4 address.
		 */
		if (!*server)
		{
			syslog (LOG_ALERT, _("Teredo UDP port failure"));
			syslog (LOG_NOTICE, _("Make sure another instance "
				"of the program is not already running."));
			goto abort;
		}

		server->SetPrefix (prefix.teredo.prefix);
	}

	// Sets up relay or client
	// TODO: ability to not be a relay at all
	client_port = htons (client_port);

	if (mode & MIREDO_CLIENT)
	{
		// Sets up client
		uint32_t ipv4;

		if (getipv4byname (server_name, &ipv4))
		{
			syslog (LOG_ALERT, _("Fatal configuration error"));
			goto abort;
		}

		try
		{
			relay = new MiredoRelay (fd, &tunnel, ipv4,
						 client_port);
		}
		catch (...)
		{
			relay = NULL;
		}
	}
	else
	{
		// Sets up relay
		try
		{
			relay = new MiredoRelay (&tunnel,
						 prefix.teredo.prefix,
						 client_port,
						 mode & MIREDO_CONE != 0);
		}
		catch (...)
		{
			relay = NULL;
		}

		/*
		 * In this case, the privileged process is useless, since we won't
		 * get an IPv6 Teredo address, and won't change our tunnel interface
		 * IPv6 address.
		 * FIXME: should (try to?) use the privileged process to set our
		 * addres and _then_ close our pipe.
		 */
		close (fd); // privileged process will exit
		wait (NULL); // privileged process exited
		fd = -1;
	}

	if (relay == NULL)
	{
		syslog (LOG_ALERT, _("Teredo service failure"));
		goto abort;
	}

	if (!*relay)
	{
		syslog (LOG_ALERT,
			_("Teredo service port failure: "
			"cannot open UDP port %u\n"), (unsigned)client_port);
		syslog (LOG_NOTICE, _("Make sure another instance "
			"of the program is not already running."));
		goto abort;
	}

	retval = teredo_server_relay (tunnel, relay, server);

abort:
	if (fd != -1)
		close (fd);
	if (relay != NULL)
		delete relay;
	if (server != NULL)
		delete server;
	if (fd != -1)
		wait (NULL); // wait for privsep process

	return retval;
}


// Defines signal handlers
static void
init_signals (void)
{
	struct sigaction sa;

	rootpid = getpid ();
	should_exit = 0;
	should_reload = 0;

	memset (&sa, 0, sizeof (sa));
	sigemptyset (&sa.sa_mask); // -- can that really fail ?!?

	sa.sa_handler = exit_handler;
	sigaction (SIGINT, &sa, NULL);
	sigaction (SIGQUIT, &sa, NULL);
	sigaction (SIGTERM, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	// We check for EPIPE in errno instead:
	sigaction (SIGPIPE, &sa, NULL);
	// might use these for other purpose in later versions:
	sigaction (SIGUSR1, &sa, NULL);
	sigaction (SIGUSR2, &sa, NULL);
	
	sa.sa_handler = reload_handler;
	sigaction (SIGHUP, &sa, NULL);
}


/*
 * Configuration and respawning stuff
 * TODO: really implement reloading
 */
static const char *const daemon_ident = "miredo";

extern "C" int
miredo_main (uint16_t client_port, const char *server_name,
		const char *prefix_name, const char *ifname,
		int mode)
{
	int facility = LOG_DAEMON;
	openlog (daemon_ident, LOG_PID, facility);

	int retval;

	init_signals ();

	do
	{
		if (should_reload)
		{
			syslog (LOG_NOTICE, _(
				"Reloading configuration on signal %d (%s)"),
				should_reload, strsignal (should_reload));
			should_reload = 0;
		}

		/* TODO: really implement configuration parsing */

		// Apply syslog facility change if needed
		int newfacility = LOG_DAEMON;

		if (newfacility != facility)
		{
			closelog ();
			facility = newfacility;
			openlog (daemon_ident, LOG_PID, facility);
		}

		// Starts the main miredo process
		pid_t pid = fork ();

		switch (pid)
		{
			case -1:
				syslog (LOG_ALERT, _("fork failed: %m"));
				break;

			case 0:
			{
				retval = miredo_run (client_port, server_name,
							prefix_name, ifname,
							mode);
				closelog ();
				exit (-retval);
			}
		}

		// Waits until the miredo process terminates
		while (waitpid (pid, &retval, 0) == -1)
		{
			if (should_exit)
			{
				syslog (LOG_NOTICE,
					_("Exiting on signal %d (%s)"),
					should_exit, strsignal (should_exit));
				wait (NULL); // child exited

				closelog ();
				return 0;
			}
		}

		if (WIFEXITED (retval))
			retval = -WEXITSTATUS (retval);
		else
		{
			if (WIFSIGNALED (retval))
			{
				retval = WTERMSIG (retval);
				syslog (LOG_INFO, _(
					"Child %d killed by signal %d (%s)"),
					(int)pid, retval, strsignal (retval));
			}
			retval = -2;
		}
	}
	while (retval == -2);

	closelog ();
	return retval;
}


extern "C" int
miredo (uint16_t client_port, const char *server_name,
	const char *prefix_name, const char *ifname, int cone)
{
	return miredo_main (client_port, server_name, prefix_name, ifname,
				cone ? MIREDO_CONE : 0);
}


extern "C" int
miredo_client (const char *server_name, uint16_t client_port,
		const char *ifname)
{
	return miredo_main (client_port, server_name, NULL, ifname,
				MIREDO_CLIENT);
}
