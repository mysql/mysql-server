noinst_HEADERS = gcc.cpp

noinst_LTLIBRARIES = libportlib.la

libportlib_la_SOURCES = \
	NdbCondition.c NdbMutex.c NdbSleep.c NdbTick.c \
	NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp	    \
	NdbDaemon.c NdbMem.c \
	NdbConfig.c

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

EXTRA_PROGRAMS = memtest PortLibTest munmaptest

PortLibTest_SOURCES = NdbPortLibTest.cpp
munmaptest_SOURCES = munmaptest.cpp

# Don't update the files from bitkeeper
WIN_src = win32/NdbCondition.c \
              win32/NdbDaemon.c \
              win32/NdbEnv.c \
              win32/NdbHost.c \
              win32/NdbMem.c \
              win32/NdbMutex.c \
              win32/NdbSleep.c \
              win32/NdbTCP.c \
              win32/NdbThread.c \
              win32/NdbTick.c

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
	@$(top_srcdir)/ndb/config/win-sources $@ $(WIN_src)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
