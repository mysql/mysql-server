noinst_LIBRARIES = libdbtup.a

libdbtup_a_SOURCES = \
        DbtupExecQuery.cpp \
        DbtupBuffer.cpp \
        DbtupRoutines.cpp \
        DbtupCommit.cpp \
        DbtupFixAlloc.cpp \
        DbtupTrigger.cpp \
        DbtupAbort.cpp \
        DbtupLCP.cpp \
        DbtupUndoLog.cpp \
        DbtupPageMap.cpp \
        DbtupPagMan.cpp  \
        DbtupStoredProcDef.cpp \
        DbtupMeta.cpp \
        DbtupTabDesMan.cpp \
	DbtupGen.cpp \
        DbtupSystemRestart.cpp \
	DbtupIndex.cpp \
        DbtupDebug.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
