# Copyright (c) 2004-2007 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

EXTRA_DIST = CMakeLists.txt

noinst_LTLIBRARIES = libndbmgmclient.la
ndbtools_PROGRAMS = ndb_mgm

libndbmgmclient_la_SOURCES = CommandInterpreter.cpp
libndbmgmclient_la_LIBADD = ../mgmapi/libmgmapi.la \
			    ../common/logger/liblogger.la \
			    ../common/portlib/libportlib.la \
			    ../common/util/libgeneral.la \
			    ../common/portlib/libportlib.la \
			    ../common/debugger/libtrace.la


ndb_mgm_SOURCES = main.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am

INCLUDES += -I$(top_srcdir)/storage/ndb/include/mgmapi \
            -I$(top_srcdir)/storage/ndb/src/common/mgmcommon

LDADD_LOC = $(noinst_LTLIBRARIES) \
	    ../common/portlib/libportlib.la \
            @readline_link@ \
            $(top_builddir)/storage/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbuglt.la \
            $(top_builddir)/mysys/libmysyslt.la \
            $(top_builddir)/strings/libmystringslt.la \
            @TERMCAP_LIB@ @NDB_SCI_LIBS@

ndb_mgm_LDFLAGS = @ndb_bin_am_ldflags@

