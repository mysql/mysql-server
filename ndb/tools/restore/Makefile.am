
ndbtools_PROGRAMS = ndb_restore

ndb_restore_SOURCES = main.cpp consumer.cpp consumer_restore.cpp consumer_printer.cpp Restore.cpp

LDADD_LOC = \
    $(top_builddir)/ndb/src/libndbclient.la \
    $(top_builddir)/dbug/libdbug.a \
    $(top_builddir)/mysys/libmysys.a \
    $(top_builddir)/strings/libmystrings.a @NDB_SCI_LIBS@

include $(top_srcdir)/ndb/config/common.mk.am

INCLUDES += -I.. -I$(top_srcdir)/include  -I$(top_srcdir)/ndb/include  -I$(top_srcdir)/ndb/src/ndbapi  -I$(top_srcdir)/ndb/include/ndbapi -I$(top_srcdir)/ndb/include/util  -I$(top_srcdir)/ndb/include/portlib -I$(top_srcdir)/ndb/include/kernel

ndb_restore_LDFLAGS = @ndb_bin_am_ldflags@
