
noinst_LTLIBRARIES = liblogger.la

SOURCE_WIN = Logger.cpp LogHandlerList.cpp LogHandler.cpp \
           ConsoleLogHandler.cpp FileLogHandler.cpp
liblogger_la_SOURCES = $(SOURCE_WIN) SysLogHandler.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
windoze-dsp: liblogger.dsp

liblogger.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(SOURCE_WIN)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
