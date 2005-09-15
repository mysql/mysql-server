noinst_LIBRARIES = libdbtux.a

libdbtux_a_SOURCES = \
                        DbtuxGen.cpp \
			DbtuxMeta.cpp \
			DbtuxMaint.cpp \
			DbtuxNode.cpp \
			DbtuxTree.cpp \
			DbtuxScan.cpp \
			DbtuxSearch.cpp \
			DbtuxCmp.cpp \
			DbtuxStat.cpp \
			DbtuxDebug.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/kernel/blocks/dbtup

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdbtux.dsp

libdbtux.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libdbtux_a_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
