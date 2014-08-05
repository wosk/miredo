# Makefile.am - misc Makefile for miredo

# ***********************************************************************
# *  Copyright © 2004-2009 Rémi Denis-Courmont.                         *
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

systemddir = $(libdir)/systemd/system

MOSTLYCLEANFILES += $(examples_DATA) $(conf_SCRIPTS) $(systemd_DATA)

cat_verbose = $(cat_verbose_$(V))
cat_verbose_ = $(cat_verbose_$(AM_DEFAULT_VERBOSITY))
cat_verbose_0 = @echo "  CAT      $@";

# Configuration files
dist_noinst_DATA = misc/miredo.conf-in misc/miredo-server.conf-in
examples_DATA = misc/miredo.conf misc/miredo-server.conf

.conf-in.conf:
	$(AM_V_at)rm -f -- $@
	$(sed_verbose)sed -e 's,@sbindir\@,$(sbindir),g' < $< > $@

# Hook scripts
dist_noinst_SCRIPTS = misc/client-hook.iproute misc/client-hook.bsd
conf_SCRIPTS = client-hook

client-hook: misc/client-hook.$(hook_suffix)
	$(cat_verbose)cat $< > client-hook.tmp
	$(AM_V_at)mv -f -- client-hook.tmp client-hook

# systemd service
dist_noinst_DATA += misc/miredo.service-in misc/miredo-server.service-in
systemd_DATA = misc/miredo.service misc/miredo-server.service

.service-in.service:
	$(AM_V_at)$(mkdir_p) misc
	$(AM_V_at)rm -f -- $@
	$(sed_verbose)sed \
		-e 's,@sbindir\@,$(sbindir),g' \
		-e 's,@sysconfdir\@,$(sysconfdir),g' \
		< $< > $@
