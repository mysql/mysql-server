# Copyright (C) 2005-2006 MySQL AB, 2009 Sun Microsystems, Inc.
# All rights reserved.
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#called from the top level Makefile

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

noinst_HEADERS =	ha_blackhole.h

EXTRA_LTLIBRARIES =	ha_blackhole.la
pkgplugin_LTLIBRARIES =	@plugin_blackhole_shared_target@
ha_blackhole_la_LDFLAGS=-module -rpath $(pkgplugindir)
ha_blackhole_la_CXXFLAGS=-shared $(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_blackhole_la_SOURCES=ha_blackhole.cc


EXTRA_LIBRARIES =	libblackhole.a
noinst_LIBRARIES =	@plugin_blackhole_static_target@
libblackhole_a_CXXFLAGS=$(AM_CXXFLAGS)
libblackhole_a_SOURCES=	ha_blackhole.cc


EXTRA_DIST =		CMakeLists.txt plug.in

if HAVE_DTRACE_DASH_G
# The object for static and dynamic linking of blackhole differ
# For static linkage of blackhole to mysqld

libblackhole_a_LIBADD = probes_mysql.o
libblackhole_a_DEPENDENCIES = probes_mysql.o dtrace_files dtrace_providers
# For blackhole as shared library
ha_blackhole_la_LIBADD = probes_sh_mysql.o
# Hack: We "depend" on ".libs/" but have no rule for it,
# but it is created as a byproduct of the ".lo"
DTRACESHAREDDEPS = ha_blackhole_la-ha_blackhole.lo
DTRACESHAREDFILES = ha_blackhole_la-ha_blackhole.o
DTRACEPROVIDER = probes_mysql.d
ha_blackhole_la_DEPENDENCIES = probes_sh_mysql.o $(DTRACESHAREDDEPS) dtrace_providers

CLEANFILES = $(DTRACEPROVIDERS) dtrace_files dtrace_providers $(DTRACESHAREDDEPS)
DTRACEFILES = libblackhole_a-ha_blackhole.o

dtrace_files:
	echo $(DTRACEFILES) > $@
dtrace_providers: probes_mysql.d
	echo $(DTRACEPROVIDER) > $@
probes_mysql.d:
	-$(RM) -f probes_mysql.d
	$(CP) $(top_srcdir)/include/probes_mysql.d.base probes_mysql.d
	echo timestamp > dtrace_sources

probes_sh_mysql.o: $(DTRACEPROVIDER) $(DTRACESHAREDDEPS)
	$(DTRACE) $(DTRACEFLAGS) -G -s $(DTRACEPROVIDER) $(DTRACESHAREDFILES) -o $@
	$(CP) $(DTRACESHAREDFILES) .libs
probes_mysql.o: $(DTRACEPROVIDER) $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $(DTRACEPROVIDER) $(DTRACEFILES) -o $@

endif
