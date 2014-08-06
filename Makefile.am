# Makefile.am - master Makefile for miredo

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

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = 1.11 \
	-Wall \
	check-news \
	subdir-objects \
	dist-xz \
	no-dist-gzip \
	no-exeext

confdir = $(sysconfdir)/miredo

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir) -D_REENTRANT \
	-DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DLOCALSTATEDIR=\"$(localstatedir)\" \
	-DPKGLIBEXECDIR=\"$(pkglibexecdir)\"

SUBDIRS = po
noinst_HEADERS = include/gettext.h
noinst_LTLIBRARIES =
EXTRA_DIST = autogen.sh Doxyfile.in

lib_LTLIBRARIES =

check_PROGRAMS =
TESTS = $(check_PROGRAMS)

DISTCLEANFILES = ChangeLog
CLEANFILES =
MOSTLYCLEANFILES =

include compat/Makefile.am
include doc/Makefile.am
include misc/Makefile.am
include libtun6/Makefile.am
include libteredo/Makefile.am
include src/Makefile.am

doc: Doxyfile
	doxygen $<

if CONF_SAMPLE
# For user's convenience, we install miredo.conf
# iif it does not already exist.
# Packagers will probably use --disable-sample-conf
# and copy their own default (or those in misc/) to @sysconfdir@/miredo/

example = $(examplesdir)/miredo.conf
conf = $(confdir)/miredo.conf

install-data-hook:
	@echo "************************"
	@echo "* Miredo configuration *"
	@echo "************************"
	@echo " "
	@echo "An example configuration file has been installed at:"
	@echo "  $(examplesdir)/miredo.conf"
	@echo " "
	@if test ! -e "$(DESTDIR)$(conf)"; then \
		mkdir -p -- "$(DESTDIR)$(confdir)" || exit $$? ; \
		cp -- "$(DESTDIR)$(example)" "$(DESTDIR)$(conf)" || exit $$?; \
		echo "No pre-existing configuration file for Miredo" ; \
		echo "could be found, so the example as been copied as" ; \
		echo "  $(conf) too." ; \
	else \
		echo "There was already a configuration file at:" ; \
		echo "  $(conf) - it was not modified." ; \
	fi
	@echo " "
	@echo "Please take time to review the settings and adjust them"
	@echo "if necessary. Once finished, save the file as:"
	@echo "  $(conf) and run miredo (as root)."

uninstall-hook:
	rm -f -- "$(DESTDIR)$(conf)"
endif

# ChangeLog
ChangeLog:
	printf "" > ChangeLog
	if test -d "$(srcdir)/.git"; then \
		(cd "$(srcdir)" && git log) >> ChangeLog ; \
	fi
	if test -f "$(srcdir)/ChangeLog.old"; then \
		cat "$(srcdir)/ChangeLog.old" >> ChangeLog ; \
	fi

.PHONY: ChangeLog

# m4 macros
EXTRA_DIST += m4/codeset.m4 m4/fcntl-o.m4 m4/flags.m4 m4/getopt.m4 \
	m4/gettext.m4 m4/glibc21.m4 m4/glibc2.m4 m4/hostname.m4 m4/iconv.m4 \
	m4/intdiv0.m4 m4/intldir.m4 m4/intlmacosx.m4 m4/intl.m4 m4/intmax.m4 \
	m4/inttypes_h.m4 m4/inttypes-pri.m4 m4/lcmessage.m4 m4/lib-ld.m4 \
	m4/lib-link.m4 m4/lib-prefix.m4 m4/libtool.m4 m4/lock.m4 \
	m4/longlong.m4 m4/lt~obsolete.m4 m4/ltoptions.m4 m4/ltsugar.m4 \
	m4/ltversion.m4 m4/nls.m4 m4/po.m4 m4/printf-posix.m4 m4/progtest.m4 \
	m4/size_max.m4 m4/sockaddr.m4 m4/stdint_h.m4 m4/threadlib.m4 \
	m4/uintmax_t.m4 m4/visibility.m4 m4/wchar_t.m4 m4/wint_t.m4 m4/xsize.m4

# Test coverage
lcov.info:
	find -name '*.gcda' -delete
	$(MAKE) $(AM_MAKEFLAGS) check
	lcov --directory . --capture --output $@.tmp
	lcov --remove $@.tmp '*test*' '*include*' --output $@
	rm -f $@.tmp

lcov-report: lcov.info
	rm -Rf lcov
	mkdir -p lcov
	genhtml $< -o lcov

lcov:
	rm -f lcov.info
	$(MAKE) $(AM_MAKEFLAGS) lcov-report

.PHONY: distcheck-hook stamp-svn lcov doc

