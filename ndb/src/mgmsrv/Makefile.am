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
	Config.cpp \
	CommandInterpreter.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/ndbapi \
               -I$(top_srcdir)/ndb/src/mgmapi \
               -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD_LOC = $(top_builddir)/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a @NDB_SCI_LIBS@
	    @TERMCAP_LIB@

DEFS_LOC =		-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DMYSQLCLUSTERDIR="\"$(MYSQLCLUSTERdir)\"" \
			-DNDB_BASE_PORT="\"@ndb_port_base@\""

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

ndb_mgmd_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%
