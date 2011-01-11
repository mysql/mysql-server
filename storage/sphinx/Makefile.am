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
pkgplugindir =          $(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)

DEFS= @DEFS@ -D_REENTRANT -D_PTHREADS -DMYSQL_SERVER

noinst_HEADERS =	ha_sphinx.h

EXTRA_LTLIBRARIES =	libsphinx.la ha_sphinx.la
pkgplugin_LTLIBRARIES = @plugin_sphinx_shared_target@ sphinx.la

ha_sphinx_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir) \
			-L$(top_builddir)/libservices -lmysqlservices
ha_sphinx_la_CXXFLAGS=	$(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_sphinx_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_sphinx_la_SOURCES =	ha_sphinx.cc

sphinx_la_LDFLAGS = -module
sphinx_la_CXXFLAGS = $(AM_CXXFLAGS)
sphinx_la_CFLAGS = $(AM_CFLAGS)
sphinx_la_SOURCES = snippets_udf.cc

noinst_LTLIBRARIES =	@plugin_sphinx_static_target@
libsphinx_la_CXXFLAGS =	$(AM_CXXFLAGS)
libsphinx_la_CFLAGS =	$(AM_CFLAGS)
libsphinx_la_SOURCES=	ha_sphinx.cc

EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
