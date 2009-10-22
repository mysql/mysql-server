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

## Makefile.am for semi-synchronous replication

pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include \
			-I$(top_srcdir)/sql \
			-I$(top_srcdir)/regex \
			-I$(srcdir)

noinst_HEADERS = semisync.h semisync_master.h semisync_slave.h

pkgplugin_LTLIBRARIES =	libsemisync_master.la libsemisync_slave.la

libsemisync_master_la_LDFLAGS =	-module
libsemisync_master_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libsemisync_master_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libsemisync_master_la_SOURCES = semisync.cc semisync_master.cc semisync_master_plugin.cc

libsemisync_slave_la_LDFLAGS =	-module
libsemisync_slave_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libsemisync_slave_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libsemisync_slave_la_SOURCES = semisync.cc semisync_slave.cc semisync_slave_plugin.cc
