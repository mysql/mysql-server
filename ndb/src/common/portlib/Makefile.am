noinst_HEADERS = gcc.cpp

noinst_LTLIBRARIES = libportlib.la

libportlib_la_SOURCES = \
	NdbCondition.c NdbMutex.c NdbSleep.c NdbTick.c \
	NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp	    \
	NdbDaemon.c NdbMem.c

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

EXTRA_PROGRAMS = memtest PortLibTest munmaptest

PortLibTest_SOURCES = NdbPortLibTest.cpp
munmaptest_SOURCES = munmaptest.cpp

# Don't update the files from bitkeeper

windoze-dsp: libportlib.dsp

libportlib.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libportlib_la_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ $(LDADD)
