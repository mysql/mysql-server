
bin_PROGRAMS = flexBench

flexBench_SOURCES = flexBench.cpp

LDADD_LOC = $(top_srcdir)/ndb/test/src/libNDBT.a \
            $(top_srcdir)/ndb/src/ndbapi/libNDB_API.la \
            $(top_srcdir)/ndb/src/mgmapi/libMGM_API.la

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitest.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
