# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#called from the top level Makefile

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

noinst_HEADERS =	ha_federated.h

EXTRA_LTLIBRARIES =	ha_federated.la
pkglib_LTLIBRARIES =	@plugin_federated_shared_target@
ha_federated_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir)
ha_federated_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federated_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federated_la_SOURCES =	ha_federated.cc


EXTRA_LIBRARIES =	libfederated.a
noinst_LIBRARIES =	@plugin_federated_static_target@
libfederated_a_CXXFLAGS =	$(AM_CFLAGS)
libfederated_a_CFLAGS =	$(AM_CFLAGS)
libfederated_a_SOURCES=	ha_federated.cc


EXTRA_DIST =		CMakeLists.txt
# Don't update the files from bitkeeper
%::SCCS/s.%
