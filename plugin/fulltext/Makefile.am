# Copyright (C) 2005-2006 MySQL AB
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

#Makefile.am example for a plugin

pkgplugindir=$(pkglibdir)/plugin
INCLUDES= -I$(top_builddir)/include -I$(top_srcdir)/include
#noinst_LTLIBRARIES= mypluglib.la
pkgplugin_LTLIBRARIES= mypluglib.la
mypluglib_la_SOURCES= plugin_example.c
mypluglib_la_LDFLAGS= -module -rpath $(pkgplugindir)
mypluglib_la_CFLAGS= -DMYSQL_DYNAMIC_PLUGIN

# Don't update the files from bitkeeper
%::SCCS/s.%
