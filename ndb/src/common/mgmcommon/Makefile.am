MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)

noinst_LTLIBRARIES = libmgmsrvcommon.la

libmgmsrvcommon_la_SOURCES = \
	LocalConfig.cpp \
	Config.cpp \
	ConfigInfo.cpp \
	ConfigRetriever.cpp \
	InitConfigFileParser.cpp \
	IPCConfig.cpp NdbConfig.c

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmapi

DEFS_LOC =		-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DNDB_BASE_PORT="\"@ndb_port_base@\""

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am
include $(top_srcdir)/ndb/config/type_mgmapiclient.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
