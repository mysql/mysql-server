
bin_PROGRAMS = ndb_cpcd

ndb_cpcd_SOURCES = main.cpp CPCD.cpp Process.cpp APIService.cpp Monitor.cpp common.cpp

LDADD_LOC = $(top_srcdir)/ndb/src/common/logger/liblogger.la

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
