
bin_PROGRAMS = atrt

atrt_SOURCES = main.cpp

bin_SCRIPTS = atrt-analyze-result.sh atrt-gather-result.sh atrt-setup.sh \
          atrt-clear-result.sh make-config.sh

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/mgmclient
LDADD_LOC = $(top_srcdir)/ndb/src/mgmapi/libmgmapi.la $(top_srcdir)/ndb/src/mgmclient/CpcClient.o

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
