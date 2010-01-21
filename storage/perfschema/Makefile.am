# Copyright (C) 2008-2009 Sun Microsystems, Inc
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

SUBDIRS = . unittest

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

DEFS =                  -DMYSQL_SERVER @DEFS@


noinst_HEADERS = ha_perfschema.h pfs_engine_table.h pfs.h pfs_server.h \
		pfs_global.h pfs_instr_class.h pfs_instr.h \
                pfs_column_types.h pfs_column_values.h \
                table_setup_instruments.h table_performance_timers.h \
                table_setup_timers.h \
                table_setup_consumers.h table_events_waits.h \
		pfs_events_waits.h pfs_timer.h table_processlist.h \
		table_sync_instances.h \
		table_events_waits_summary.h pfs_stat.h \
		table_all_instr.h \
		table_file_instances.h table_file_summary.h \
		table_setup_objects.h pfs_lock.h pfs_atomic.h

PSE_SOURCES = ha_perfschema.cc pfs_engine_table.cc pfs.cc pfs_server.cc \
		pfs_global.cc pfs_instr_class.cc pfs_instr.cc \
                pfs_column_values.cc \
                table_setup_instruments.cc table_performance_timers.cc \
                table_setup_timers.cc \
                table_setup_consumers.cc table_events_waits.cc \
		pfs_events_waits.cc pfs_timer.cc table_processlist.cc \
		table_sync_instances.cc \
		table_events_waits_summary.cc \
		table_all_instr.cc \
		table_file_instances.cc table_file_summary.cc \
		table_setup_objects.cc pfs_atomic.cc pfs_check.cc

EXTRA_LIBRARIES = libperfschema.a
noinst_LIBRARIES = @plugin_perfschema_static_target@

libperfschema_a_SOURCES= $(PSE_SOURCES)

EXTRA_DIST = plug.in CMakeLists.txt

unittests = unittest

test:
	perl $(top_srcdir)/unittest/unit.pl run $(unittests)

test-verbose:
	HARNESS_VERBOSE=1 perl $(top_srcdir)/unittest/unit.pl run $(unittests)

