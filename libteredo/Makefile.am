# Makefile.am - libteredo/ directory Makefile for miredo

# ***********************************************************************
# *  Copyright © 2004-2006 Rémi Denis-Courmont.                         *
# *  This program is free software; you can redistribute and/or modify  *
# *  it under the terms of the GNU General Public License as published  *
# *  by the Free Software Foundation; version 2 of the license, or (at  *
# *  your option) any later version.                                    *
# *                                                                     *
# *  This program is distributed in the hope that it will be useful,    *
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
# *  See the GNU General Public License for more details.               *
# *                                                                     *
# *  You should have received a copy of the GNU General Public License  *
# *  along with this program; if not, you can get it from:              *
# *  http://www.gnu.org/copyleft/gpl.html                               *
# ***********************************************************************

noinst_LTLIBRARIES += libteredo-common.la libteredo-server.la
lib_LTLIBRARIES += libteredo.la
bin_PROGRAMS = teredo-mire
EXTRA_DIST += libteredo/libteredo.sym

include_libteredodir = $(includedir)/libteredo
include_libteredo_HEADERS = \
	libteredo/teredo.h \
	libteredo/teredo-udp.h \
	libteredo/tunnel.h

# libteredo-common.la
libteredo_common_la_SOURCES = \
	libteredo/teredo.c \
	libteredo/v4global.c libteredo/v4global.h \
	libteredo/checksum.h libteredo/debug.h
libteredo_common_la_LDFLAGS = -no-undefined

# libteredo.la
libteredo_la_SOURCES = \
	libteredo/security.c libteredo/security.h \
	libteredo/md5.c libteredo/md5.h \
	libteredo/packets.c libteredo/packets.h \
	libteredo/peerlist.c libteredo/peerlist.h \
	libteredo/clock.c libteredo/clock.h \
	libteredo/thread.h libteredo/stub.c \
	libteredo/relay.c
if TEREDO_CLIENT
libteredo_la_SOURCES += \
	libteredo/maintain.c libteredo/maintain.h \
	libteredo/discovery.c libteredo/discovery.h
endif
libteredo_la_DEPENDENCIES = libteredo/libteredo.sym libteredo-common.la \
	libcompat.la
libteredo_la_LIBADD = \
	libteredo-common.la libcompat.la \
	$(LIBJUDY) $(LIBRT) $(LTLIBINTL)
libteredo_la_LDFLAGS = \
	-no-undefined \
	-export-symbols $(srcdir)/libteredo/libteredo.sym \
	-version-info 6:0:0

# libteredo versions:
# 0) First stable shared release (0.8.2)
# 1) teredo_run_async() added, teredo_register_readset() deprecated (0.9.2)
# 2) teredo_set_relay_mode() added, teredo_set_cone_*() deprecated (0.9.5)
# -- backward compatibility break --
# 3) remove teredo_register_readset() (1.0.0)
# 4) added internal teredo_send_bubble, teredo_cksum (1.1.0)
# -- backward compatibility break --
# 5) added teredo_packet.dest_ipv4, removed teredo_set_cone_ignore() (1.1.7)
# -- backward compatibility break --
# 6) teredo_run(), teredo_set_prefix(), teredo_startup(), teredo_cleanup()
#    removed (1.3.0)

# libteredo-server.la
libteredo_server_la_SOURCES = libteredo/server.c libteredo/server.h
libteredo_server_la_LIBADD = \
	libteredo-common.la libcompat.la \
	$(LTLIBINTL)
libteredo_server_la_LDFLAGS = -no-undefined -static
# libteredo-server is static given it hardly make sense to reuse it
# outside miredo-server (which is itself way less commonly used
# than miredo)

# teredo-mire
teredo_mire_SOURCES = libteredo/mire.c
teredo_mire_LDADD = libteredo.la

include libteredo/test/Makefile.am
