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

windoze-dsp: libdbtup.dsp

libdbtup.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libdbtup_a_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ $(LDADD)
