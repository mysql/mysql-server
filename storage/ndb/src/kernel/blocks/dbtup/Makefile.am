noinst_LIBRARIES = libdbtup.a

libdbtup_a_SOURCES = \
        DbtupExecQuery.cpp \
        DbtupBuffer.cpp \
        DbtupRoutines.cpp \
        DbtupCommit.cpp \
        DbtupFixAlloc.cpp \
        DbtupTrigger.cpp \
        DbtupAbort.cpp \
        DbtupPageMap.cpp \
        DbtupPagMan.cpp  \
        DbtupStoredProcDef.cpp \
        DbtupMeta.cpp \
        DbtupTabDesMan.cpp \
	DbtupGen.cpp \
	DbtupIndex.cpp \
        DbtupDebug.cpp \
	DbtupScan.cpp \
        DbtupDiskAlloc.cpp DbtupVarAlloc.cpp \
        tuppage.cpp Undo_buffer.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/kernel/blocks/dblqh

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdbtup.dsp

libdbtup.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libdbtup_a_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)

ndbtest_PROGRAMS = test_varpage
test_varpage_SOURCES = test_varpage.cpp tuppage.cpp
test_varpage_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysys.a \
  $(top_builddir)/dbug/libdbug.a \
  $(top_builddir)/strings/libmystrings.a
			     
