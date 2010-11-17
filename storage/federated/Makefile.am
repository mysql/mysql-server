# Copyright (C) 2006 MySQL AB
#
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

EXTRA_LTLIBRARIES =	libfederated.la libfederated_embedded.la ha_federated.la
pkgplugin_LTLIBRARIES =	@plugin_federated_shared_target@
ha_federated_la_LDFLAGS =	-module -rpath $(pkgplugindir)
ha_federated_la_CXXFLAGS=	-shared $(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federated_la_CFLAGS =	-shared $(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federated_la_SOURCES =	ha_federated.cc $(top_srcdir)/mysys/string.c


noinst_LTLIBRARIES =	@plugin_federated_static_target@ @plugin_federated_embedded_static_target@
libfederated_la_CXXFLAGS =	$(AM_CXXFLAGS)
libfederated_la_SOURCES=	ha_federated.cc

libfederated_embedded_la_CXXFLAGS =	$(AM_CXXFLAGS) @plugin_embedded_defs@
libfederated_embedded_la_CFLAGS =	$(AM_CFLAGS) @plugin_embedded_defs@
libfederated_embedded_la_SOURCES=	ha_federated.cc


EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
