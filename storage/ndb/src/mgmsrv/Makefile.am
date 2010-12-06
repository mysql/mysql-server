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

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
#MYSQLCLUSTERdir=        $(prefix)/mysql-cluster
MYSQLCLUSTERdir=        .

ndbbin_PROGRAMS = ndb_mgmd

ndb_mgmd_SOURCES = \
	MgmtSrvr.cpp \
	MgmtSrvrGeneralSignalHandling.cpp \
	main.cpp \
	Services.cpp \
	convertStrToInt.cpp \
	SignalQueue.cpp \
	MgmtSrvrConfig.cpp \
	ConfigInfo.cpp \
	InitConfigFileParser.cpp \
	Config.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/ndbapi \
               -I$(top_srcdir)/storage/ndb/src/mgmapi \
               -I$(top_srcdir)/storage/ndb/src/common/mgmcommon \
               -I$(top_srcdir)/storage/ndb/src/mgmclient

LDADD_LOC = $(top_builddir)/storage/ndb/src/mgmclient/CommandInterpreter.lo \
            $(top_builddir)/storage/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a \
            @readline_link@ \
            @NDB_SCI_LIBS@ \
	    @TERMCAP_LIB@

DEFS_LOC =		-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DMYSQL_DATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DMYSQLCLUSTERDIR="\"$(MYSQLCLUSTERdir)\""

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am

ndb_mgmd_LDFLAGS = @ndb_bin_am_ldflags@

windoze-dsp: ndb_mgmd.dsp

ndb_mgmd.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-prg.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-prg.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(ndbbin_PROGRAMS)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(ndb_mgmd_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LINK $(LDADD)
