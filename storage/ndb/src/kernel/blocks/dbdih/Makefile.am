noinst_LIBRARIES = libdbdih.a
EXTRA_PROGRAMS = ndbd_sysfile_reader

libdbdih_a_SOURCES = DbdihInit.cpp DbdihMain.cpp
ndbd_sysfile_reader_SOURCES = printSysfile/printSysfile.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

LDADD +=  \
              $(top_builddir)/ndb/src/common/util/libgeneral.la \
              $(top_builddir)/ndb/src/common/portlib/libportlib.la

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdbdih.dsp

libdbdih.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libdbdih_a_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
