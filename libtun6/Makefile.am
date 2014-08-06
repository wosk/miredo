# Makefile.am - libtun6/ directory Makefile for miredo

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

lib_LTLIBRARIES += libtun6.la
check_PROGRAMS += libtun6-diagnose

include_libtun6dir = $(includedir)/libtun6
include_libtun6_HEADERS = libtun6/tun6.h

# libtun6.a
libtun6_la_SOURCES = libtun6/tun6.c
libtun6_la_LIBADD = @LTLIBINTL@ compat/libcompat.la
libtun6_la_LDFLAGS = -no-undefined -export-symbols-regex tun6_.* \
	-version-info 2:0:0

# libtun6 versions:
# 0) First stable shared release (0.8.2)
# 1) tun_wait_recv() (0.9.x)
# -- backward compatibility break --
# 2) libtun6_diagnose() removed

# libtun6-diagnose
libtun6_diagnose_SOURCES = libtun6/test_diag.c
libtun6_diagnose_LDADD = libtun6.la
