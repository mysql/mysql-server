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

DEFS =                  @DEFS@

noinst_HEADERS =	ha_federated.h

EXTRA_LTLIBRARIES =	ha_federated.la
pkgplugin_LTLIBRARIES =	@plugin_federated_shared_target@
ha_federated_la_LDFLAGS =	-module -rpath $(pkgplugindir) \
				-L$(top_builddir)/libservices -lmysqlservices
ha_federated_la_CXXFLAGS=	-shared $(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federated_la_CFLAGS =	-shared $(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federated_la_SOURCES =	ha_federated.cc $(top_srcdir)/mysys/string.c


EXTRA_LIBRARIES =	libfederated.a
noinst_LIBRARIES =	@plugin_federated_static_target@
libfederated_a_CXXFLAGS =	$(AM_CXXFLAGS)
libfederated_a_SOURCES=	ha_federated.cc


EXTRA_DIST =		CMakeLists.txt plug.in

if HAVE_DTRACE_DASH_G
# The object for static and dynamic linking of federated differ
# For static linkage of federated to mysqld

libfederated_a_LIBADD = probes_mysql.o
libfederated_a_DEPENDENCIES = probes_mysql.o dtrace_files dtrace_providers
# For federated as shared library
ha_federated_la_LIBADD =	probes_sh_mysql.o
# Hack: We "depend" on ".libs/" but have no rule for it,
# but it is created as a byproduct of the ".lo"
DTRACESHAREDDEPS = ha_federated_la-ha_federated.lo
DTRACESHAREDFILES = ha_federated_la-ha_federated.o
DTRACEPROVIDER = probes_mysql.d
ha_federated_la_DEPENDENCIES = probes_sh_mysql.o $(DTRACESHAREDDEPS) dtrace_providers

CLEANFILES = $(DTRACEPROVIDER) dtrace_files dtrace_providers $(DTRACESHAREDFILES)
DTRACEFILES = libfederated_a-ha_federated.o

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
