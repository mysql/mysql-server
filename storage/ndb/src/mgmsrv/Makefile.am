# Copyright (C) 2004-2008 MySQL AB, 2008 Sun Microsystems, Inc.
#  All rights reserved. Use is subject to license terms.
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

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
MYSQLCLUSTERdir=        $(prefix)/mysql-cluster

ndbbin_PROGRAMS = ndb_mgmd

ndb_mgmd_SOURCES = \
	MgmtSrvr.cpp \
	main.cpp \
	Services.cpp \
	ConfigInfo.cpp \
	InitConfigFileParser.cpp \
	Config.cpp \
	ConfigManager.cpp

noinst_PROGRAMS = MgmConfig-t

MgmConfig_t_SOURCES = \
	testConfig.cpp \
	InitConfigFileParser.cpp \
	ConfigInfo.cpp \
	Config.cpp
MgmConfig_t_LDFLAGS = -static

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/ndbapi \
               -I$(top_srcdir)/storage/ndb/src/mgmapi \
               -I$(top_srcdir)/storage/ndb/src/common/mgmcommon \
               -I$(top_srcdir)/storage/ndb/src/mgmclient

LDADD_LOC = $(top_builddir)/storage/ndb/src/mgmclient/CommandInterpreter.lo \
            $(top_builddir)/storage/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbuglt.la \
            $(top_builddir)/mysys/libmysyslt.la \
            $(top_builddir)/strings/libmystringslt.la \
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

