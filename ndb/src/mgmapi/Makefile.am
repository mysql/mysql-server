
noinst_LTLIBRARIES = libmgmapi.la

libmgmapi_la_SOURCES = mgmapi.cpp mgmapi_configuration.cpp LocalConfig.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/mgmapi

DEFS_LOC = -DNO_DEBUG_MESSAGES -DNDB_PORT="\"@ndb_port@\""

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

#ndbtest_PROGRAMS = ndb_test_mgmapi

# Don't update the files from bitkeeper
%::SCCS/s.%
