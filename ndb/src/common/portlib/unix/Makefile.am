
noinst_LTLIBRARIES = libportlib.la

libportlib_la_SOURCES = \
	NdbCondition.c NdbMutex.c NdbSleep.c NdbTick.c \
	NdbEnv.c NdbThread.c NdbHost.c NdbTCP.c	    \
	NdbDaemon.c NdbMem.c

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
