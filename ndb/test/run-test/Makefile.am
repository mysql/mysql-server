
ndbtest_PROGRAMS = atrt

atrt_SOURCES = main.cpp

ndbtest_SCRIPTS = atrt-analyze-result.sh atrt-gather-result.sh atrt-setup.sh \
          atrt-clear-result.sh make-config.sh make-index.sh make-html-reports.sh
EXTRA_DIST = $(ndbtest_SCRIPTS)

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmclient
LDADD_LOC = $(top_builddir)/ndb/src/mgmclient/CpcClient.o $(top_builddir)/ndb/src/libndbclient.la

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am
include $(top_srcdir)/ndb/config/type_mgmapiclient.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
