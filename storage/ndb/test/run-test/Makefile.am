# Copyright (C) 2004-2006 MySQL AB
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

testdir=$(prefix)/mysql-test/ndb

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_util.mk.am
include $(top_srcdir)/storage/ndb/config/type_mgmapiclient.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapitools.mk.am

test_PROGRAMS = atrt
test_DATA=daily-basic-tests.txt daily-devel-tests.txt 16node-tests.txt \
          conf-ndbmaster.cnf \
          conf-fimafeng08.cnf conf-dl145a.cnf test-tests.txt conf-test.cnf db.sql
#          conf-dl145a.cnf test-tests.txt conf-test.cnf db.sql

test_SCRIPTS=atrt-analyze-result.sh atrt-gather-result.sh atrt-setup.sh \
          autotest-run.sh atrt-backtrace.sh

atrt_SOURCES = main.cpp setup.cpp files.cpp db.cpp command.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/test/include
LDADD_LOC = $(top_builddir)/storage/ndb/test/src/libNDBT.a \
	    $(top_builddir)/storage/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbuglt.la \
            $(top_builddir)/mysys/libmysyslt.la \
            $(top_builddir)/strings/libmystringslt.la @NDB_SCI_LIBS@ \
            $(top_builddir)/libmysql_r/libmysqlclient_r.la

atrt_CXXFLAGS = -I$(top_srcdir)/ndb/src/mgmapi \
                      -I$(top_srcdir)/ndb/src/mgmsrv \
                      -I$(top_srcdir)/ndb/include/mgmcommon \
                      -DMYSQLCLUSTERDIR="\"\"" \
                 -DDEFAULT_PREFIX="\"$(prefix)\""

atrt_LDFLAGS = -static @ndb_bin_am_ldflags@

wrappersdir=$(prefix)/bin
wrappers_SCRIPTS=atrt-testBackup atrt-mysql-test-run

EXTRA_DIST = $(test_DATA) $(test_SCRIPTS) $(wrappers_SCRIPTS) README.ATRT atrt.hpp

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp:
