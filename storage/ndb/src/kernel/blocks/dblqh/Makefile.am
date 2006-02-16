EXTRA_PROGRAMS = ndbd_redo_log_reader

ndbd_redo_log_reader_SOURCES = redoLogReader/records.cpp \
  redoLogReader/redoLogFileReader.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

ndbd_redo_log_reader_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

# Don't update the files from bitkeeper
%::SCCS/s.%
