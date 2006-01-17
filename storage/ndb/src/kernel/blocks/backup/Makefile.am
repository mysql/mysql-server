
ndbtools_PROGRAMS = ndb_print_backup_file
ndb_print_backup_file_SOURCES = read.cpp
ndb_print_backup_file_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/strings/libmystrings.a

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

