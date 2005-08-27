noinst_LIBRARIES = libdblqh.a
EXTRA_PROGRAMS = ndbd_redo_log_reader

libdblqh_a_SOURCES = DblqhInit.cpp DblqhMain.cpp
ndbd_redo_log_reader_SOURCES = redoLogReader/records.cpp redoLogReader/redoLogFileReader.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

LDADD +=  \
              $(top_builddir)/storage/ndb/src/common/util/libgeneral.la \
              $(top_builddir)/storage/ndb/src/common/portlib/libportlib.la

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdblqh.dsp

libdblqh.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libdblqh_a_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
