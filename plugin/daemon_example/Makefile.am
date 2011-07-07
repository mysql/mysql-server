# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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

#Makefile.am example for a daemon
MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir) @ZLIB_INCLUDES@

EXTRA_LTLIBRARIES =	libdaemon_example.la
pkgplugin_LTLIBRARIES =	@plugin_daemon_example_shared_target@
libdaemon_example_la_LDFLAGS =	-module -rpath $(pkgplugindir)
libdaemon_example_la_CXXFLAGS=	$(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libdaemon_example_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libdaemon_example_la_SOURCES =	daemon_example.cc


EXTRA_LIBRARIES =	libdaemon_example.a
noinst_LIBRARIES =	@plugin_daemon_example_static_target@
libdaemon_example_a_CXXFLAGS =	$(AM_CXXFLAGS)
libdaemon_example_a_CFLAGS =	$(AM_CFLAGS)
libdaemon_example_a_SOURCES=	daemon_example.cc

# Don't update the files from bitkeeper
%::SCCS/s.%
