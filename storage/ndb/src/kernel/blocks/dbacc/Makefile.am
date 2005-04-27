
noinst_LIBRARIES = libdbacc.a

libdbacc_a_SOURCES = DbaccInit.cpp DbaccMain.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/kernel/blocks/dbtup

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdbacc.dsp

libdbacc.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libdbacc_a_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
