EXTRA_PROGRAMS = ndbd_redo_log_reader

ndbd_redo_log_reader_SOURCES = redoLogReader/records.cpp \
  redoLogReader/redoLogFileReader.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

LDADD +=  \
              $(top_builddir)/storage/ndb/src/common/util/libgeneral.la \
              $(top_builddir)/storage/ndb/src/common/portlib/libportlib.la

# Don't update the files from bitkeeper
%::SCCS/s.%
