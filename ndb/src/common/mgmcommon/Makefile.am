noinst_LTLIBRARIES = libmgmsrvcommon.la

libmgmsrvcommon_la_SOURCES = \
	LocalConfig.cpp \
	ConfigRetriever.cpp \
	IPCConfig.cpp NdbConfig.c

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmapi -I$(top_srcdir)/ndb/src/mgmsrv

DEFS_LOC = -DNDB_PORT="\"@ndb_port@\""

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am
include $(top_srcdir)/ndb/config/type_mgmapiclient.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libmgmsrvcommon.dsp

libmgmsrvcommon.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libmgmsrvcommon_la_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ $(LDADD)
