
noinst_LTLIBRARIES = liblogger.la

liblogger_la_SOURCES = Logger.cpp LogHandlerList.cpp LogHandler.cpp \
           ConsoleLogHandler.cpp FileLogHandler.cpp SysLogHandler.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
