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
ha_csv_la_CXXFLAGS =	$(AM_CFLAGS) -DMYSQL_PLUGIN
ha_csv_la_SOURCES =	transparent_file.cc ha_tina.cc 

EXTRA_LIBRARIES =	libcsv.a
noinst_LIBRARIES =	@plugin_csv_static_target@
libcsv_a_CXXFLAGS =	$(AM_CFLAGS)
libcsv_a_SOURCES =	transparent_file.cc ha_tina.cc

EXTRA_DIST =		CMakeLists.txt plug.in

if HAVE_DTRACE_DASH_G
libcsv_a_LIBADD = probes_mysql.o
libcsv_a_DEPENDENCIES = probes_mysql.o dtrace_files dtrace_providers
CLEANFILES = probes_mysql.o dtrace_files dtrace_providers
DTRACEFILES = libcsv_a-ha_tina.o
DTRACEPROVIDER = probes_mysql.d
CLEANFILES += $(DTRACEPROVIDER) dtrace_sources

dtrace_files:
	echo $(DTRACEFILES) > $@
dtrace_providers: probes_mysql.d
	echo $(DTRACEPROVIDER) > $@
probes_mysql.d:
	-$(RM) -f probes_mysql.d
	$(CP) $(top_srcdir)/include/probes_mysql.d.base probes_mysql.d
	echo timestamp > dtrace_sources

probes_mysql.o: $(DTRACEPROVIDER) $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $(DTRACEPROVIDER) $(DTRACEFILES) -o $@
endif

# Don't update the files from bitkeeper
%::SCCS/s.%
