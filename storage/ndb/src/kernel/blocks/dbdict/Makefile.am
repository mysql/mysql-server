
include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

LDADD +=  \
         $(top_builddir)/storage/ndb/src/common/util/libgeneral.la \
         $(top_builddir)/storage/ndb/src/common/portlib/libportlib.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

ndbtools_PROGRAMS = ndb_print_schema_file
ndb_print_schema_file_SOURCES = printSchemaFile.cpp
ndb_print_schema_file_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

# Don't update the files from bitkeeper
%::SCCS/s.%

