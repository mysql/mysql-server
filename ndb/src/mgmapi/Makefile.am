
noinst_LTLIBRARIES = libmgmapi.la

libmgmapi_la_SOURCES = mgmapi.cpp mgmapi_configuration.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon
DEFS_LOC = -DNO_DEBUG_MESSAGES

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

#ndbtest_PROGRAMS = ndb_test_mgmapi
#ndb_test_mgmapi_SOURCES = test_mgmapi.cpp
#ndb_test_mgmapi_LDFLAGS = @ndb_bin_am_ldflags@ 
#  $(top_builddir)/ndb/src/libndbclient.la

# Don't update the files from bitkeeper
%::SCCS/s.%
