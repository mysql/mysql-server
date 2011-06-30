# Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
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
INCLUDES =              -I$(top_builddir)/include \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)
LDADD =

DEFS =	@DEFS@
noinst_HEADERS	  =	ha_tina.h transparent_file.h

EXTRA_LTLIBRARIES =	ha_csv.la
pkglib_LTLIBRARIES =	@plugin_csv_shared_target@
ha_csv_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir)
ha_csv_la_CXXFLAGS =	$(AM_CXXFLAGS) -DMYSQL_PLUGIN
ha_csv_la_SOURCES =	transparent_file.cc ha_tina.cc 

EXTRA_LIBRARIES =	libcsv.a
noinst_LIBRARIES =	@plugin_csv_static_target@
libcsv_a_CXXFLAGS =	$(AM_CXXFLAGS)
libcsv_a_SOURCES =	transparent_file.cc ha_tina.cc

EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
