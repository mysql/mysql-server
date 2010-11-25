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

noinst_HEADERS =	ha_example.h

EXTRA_LTLIBRARIES =	ha_example.la
pkgplugin_LTLIBRARIES =	@plugin_example_shared_target@
ha_example_la_LDFLAGS =	-module -rpath $(pkgplugindir) -L$(top_builddir)/libservices -lmysqlservices
ha_example_la_CXXFLAGS=	-shared $(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_example_la_SOURCES =	ha_example.cc

EXTRA_LIBRARIES =	libexample.a
noinst_LIBRARIES =	@plugin_example_static_target@
libexample_a_CXXFLAGS =	$(AM_CXXFLAGS)
libexample_a_SOURCES=	ha_example.cc


EXTRA_DIST =		CMakeLists.txt plug.in

if HAVE_DTRACE_DASH_G
# The object for static and dynamic linking of example differ
# For static linkage of example to mysqld
# That's actually not needed as example is only dynamic loadable, but for completion
libexample_a_LIBADD = probes_mysql.o
libexample_a_DEPENDENCIES = probes_mysql.o dtrace_files dtrace_providers
# For example as shared library
ha_example_la_LIBADD = probes_sh_mysql.o
# Hack: We "depend" on ".libs/" but have no rule for it,
# but it is created as a byproduct of the ".lo"
DTRACESHAREDDEPS = ha_example_la-ha_example.lo
DTRACESHAREDFILES = ha_example_la-ha_example.o
DTRACEPROVIDER = probes_mysql.d
ha_example_la_DEPENDENCIES = probes_sh_mysql.o $(DTRACESHAREDDEPS) dtrace_providers

CLEANFILES = $(DTRACEPROVIDER) dtrace_files dtrace_providers $(DTRACESHAREDFILES)
DTRACEFILES = libexample_a-ha_example.o

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
