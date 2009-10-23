# Copyright (C) 2005-2006 MySQL AB
# Copyright (C) 2009 SUN Microsystems
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
                        -I$(srcdir) @ZLIB_INCLUDES@
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

noinst_HEADERS =	ha_archive.h azlib.h
noinst_PROGRAMS	=	archive_test archive_reader

EXTRA_LTLIBRARIES =	ha_archive.la
pkgplugin_LTLIBRARIES =	@plugin_archive_shared_target@
ha_archive_la_LDFLAGS =	-module -rpath $(pkgplugindir)
ha_archive_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_archive_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_archive_la_SOURCES =	ha_archive.cc azio.c


EXTRA_LIBRARIES =	libarchive.a
noinst_LIBRARIES =	@plugin_archive_static_target@
libarchive_a_CXXFLAGS =	$(AM_CFLAGS)
libarchive_a_CFLAGS =	$(AM_CFLAGS)
libarchive_a_SOURCES =	ha_archive.cc azio.c


archive_test_SOURCES =	archive_test.c azio.c
archive_test_CFLAGS =	$(AM_CFLAGS)
archive_test_LDADD =	$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a \
			@ZLIB_LIBS@
archive_test_LDFLAGS = @NOINST_LDFLAGS@

archive_reader_SOURCES = archive_reader.c azio.c
archive_reader_CFLAGS =	$(AM_CFLAGS)
archive_reader_LDADD =	$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a \
			@ZLIB_LIBS@
archive_reader_LDFLAGS = @NOINST_LDFLAGS@


EXTRA_DIST =		CMakeLists.txt plug.in

if HAVE_DTRACE_DASH_G
# The object for static and dynamic linking of archive differ 
# For static linkage of archive to mysqld 
libarchive_a_LIBADD = probes_mysql.o
libarchive_a_DEPENDENCIES = probes_mysql.o dtrace_files dtrace_providers
# For archive as shared library 
ha_archive_la_LIBADD = probes_sh_mysql.o
ha_archive_la_DEPENDENCIES = probes_sh_mysql.o dtrace_shared_files dtrace_providers

CLEANFILES = $(DTRACEPROVIDER) dtrace_files dtrace_providers dtrace_shared_files
DTRACEFILES = libarchive_a-ha_archive.o
DTRACESHAREDFILES = .libs/ha_archive_la-ha_archive.o
DTRACEPROVIDER = probes_mysql.d

dtrace_files:
	echo $(DTRACEFILES) > $@
dtrace_shared_files:
	echo $(DTRACESHAREDFILES) > $@
dtrace_providers: probes_mysql.d
	echo $(DTRACEPROVIDER) > $@
probes_mysql.d:
	-$(RM) -f probes_mysql.d
	$(CP) $(top_srcdir)/include/probes_mysql.d.base probes_mysql.d
	echo timestamp > dtrace_sources

probes_sh_mysql.o: $(DTRACEPROVIDER) $(DTRACESHAREDFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $(DTRACEPROVIDER) $(DTRACESHAREDFILES) -o $@

probes_mysql.o: $(DTRACEPROVIDER) $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $(DTRACEPROVIDER) $(DTRACEFILES) -o $@
endif

# Don't update the files from bitkeeper
%::SCCS/s.%
