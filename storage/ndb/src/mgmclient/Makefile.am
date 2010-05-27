# Copyright (C) 2004 MySQL AB
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
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a \
            @TERMCAP_LIB@ @NDB_SCI_LIBS@

ndb_mgm_LDFLAGS = @ndb_bin_am_ldflags@

windoze-dsp: ndb_mgm.dsp libndbmgmclient.dsp

ndb_mgm.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-prg.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-prg.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(ndbtools_PROGRAMS)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(ndb_mgm_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LINK $(LDADD)

libndbmgmclient.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libndbmgmclient_la_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB
