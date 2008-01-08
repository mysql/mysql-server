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

LDADD =

DEFS =                  @DEFS@

noinst_HEADERS =	ha_blackhole.h

EXTRA_LTLIBRARIES =	ha_blackhole.la
pkgplugin_LTLIBRARIES =	@plugin_blackhole_shared_target@
ha_blackhole_la_LDFLAGS=-module -rpath $(pkgplugindir)
ha_blackhole_la_CXXFLAGS=$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_blackhole_la_CFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_blackhole_la_SOURCES=ha_blackhole.cc


EXTRA_LIBRARIES =	libblackhole.a
noinst_LIBRARIES =	@plugin_blackhole_static_target@
libblackhole_a_CXXFLAGS=$(AM_CFLAGS)
libblackhole_a_CFLAGS =	$(AM_CFLAGS)
libblackhole_a_SOURCES=	ha_blackhole.cc


EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
