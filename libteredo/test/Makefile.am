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

check_PROGRAMS = \
	libteredo-list \
	libteredo-stresslist \
	libteredo-test \
	libteredo-clock \
	libteredo-v4global \
	libteredo-addrcmp \
	md5test
TESTS = $(check_PROGRAMS)

if TEREDO_CLIENT
check_PROGRAMS += libteredo-hmac
endif

# libteredo-list
libteredo_list_SOURCES = test/list.c
libteredo_list_LDFLAGS = -static
libteredo_list_LDADD = libteredo.la

# libteredo-stresslist
libteredo_stresslist_SOURCES = test/stresslist.c
libteredo_stresslist_LDFLAGS = -static
libteredo_stresslist_LDADD = libteredo.la

# libteredo-hmac
libteredo_hmac_SOURCES = test/hmac.c
libteredo_hmac_LDFLAGS = -static
libteredo_hmac_LDADD = libteredo.la

# libteredo-test
libteredo_test_SOURCES = test/teredo.c
libteredo_test_LDFLAGS = -static
libteredo_test_LDADD = libteredo.la

# libteredo-clock
libteredo_clock_SOURCES = test/clock.c
libteredo_clock_LDFLAGS = -static
libteredo_clock_LDADD = libteredo.la

# libteredo-v4global
libteredo_v4global_SOURCES = test/v4global.c
libteredo_v4global_LDFLAGS = -static
libteredo_v4global_LDADD = libteredo.la

# libteredo-addrcmp
libteredo_addrcmp_SOURCES = test/addrcmp.c
libteredo_addrcmp_LDFLAGS = -static
libteredo_addrcmp_LDADD = libteredo.la

# md5main
md5test_SOURCES = test/md5test.c
md5test_LDFLAGS = -static
md5test_LDADD = libteredo.la
