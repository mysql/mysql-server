noinst_LTLIBRARIES = libmgmsrvcommon.la

libmgmsrvcommon_la_SOURCES = \
	ConfigRetriever.cpp \
	IPCConfig.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmapi -I$(top_srcdir)/ndb/src/mgmsrv

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am
include $(top_srcdir)/ndb/config/type_mgmapiclient.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
