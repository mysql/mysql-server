SUBDIRS = \
	cmvmi           \
	dbacc		\
	dbdict		\
	dbdih		\
	dblqh		\
	dbtc		\
	dbtup		\
	ndbfs           \
	ndbcntr		\
	qmgr		\
	trix            \
        backup          \
        dbutil          \
        suma            \
	dbtux

noinst_LIBRARIES = libblocks.a

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/kernel/blocks/dblqh
libblocks_a_SOURCES = tsman.cpp lgman.cpp pgman.cpp diskpage.cpp restore.cpp

ndbtools_PROGRAMS = ndb_print_file
ndb_print_file_SOURCES = print_file.cpp diskpage.cpp dbtup/tuppage.cpp
ndb_print_file_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp:
