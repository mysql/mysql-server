
noinst_LTLIBRARIES = libmgmapi.la

libmgmapi_la_SOURCES = mgmapi.cpp ndb_logevent.cpp mgmapi_configuration.cpp LocalConfig.cpp ../kernel/error/ndbd_exit_codes.c

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/mgmapi

DEFS_LOC = -DNO_DEBUG_MESSAGES -DNDB_PORT="\"@ndb_port@\""

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

#ndbtest_PROGRAMS = ndb_test_mgmapi

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libmgmapi.dsp

libmgmapi.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libmgmapi_la_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
