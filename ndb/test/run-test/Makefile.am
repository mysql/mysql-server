
testdir=$(prefix)/mysql-test/ndb

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am
include $(top_srcdir)/ndb/config/type_mgmapiclient.mk.am

test_PROGRAMS = atrt
test_DATA=daily-basic-tests.txt daily-devel-tests.txt
test_SCRIPTS=atrt-analyze-result.sh atrt-gather-result.sh atrt-setup.sh \
          atrt-clear-result.sh make-config.sh make-index.sh make-html-reports.sh

atrt_SOURCES = main.cpp
INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmclient
LDADD_LOC = $(top_builddir)/ndb/src/mgmclient/CpcClient.o $(top_builddir)/ndb/src/libndbclient.la

# Don't update the files from bitkeeper
%::SCCS/s.%
