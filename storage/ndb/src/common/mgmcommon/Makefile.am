noinst_LTLIBRARIES = libmgmsrvcommon.la

libmgmsrvcommon_la_SOURCES = \
	ConfigRetriever.cpp \
	IPCConfig.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/mgmapi -I$(top_srcdir)/storage/ndb/src/mgmsrv

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am
include $(top_srcdir)/storage/ndb/config/type_mgmapiclient.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libmgmsrvcommon.dsp

libmgmsrvcommon.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libmgmsrvcommon_la_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
