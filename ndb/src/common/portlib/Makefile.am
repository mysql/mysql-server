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
