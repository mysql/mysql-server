SUBDIRS = signaldata

noinst_LTLIBRARIES = libtrace.la

libtrace_la_SOURCES = SignalLoggerManager.cpp DebuggerNames.cpp BlockNames.cpp EventLogger.cpp GrepError.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
