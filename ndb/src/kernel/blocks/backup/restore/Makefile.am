
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

windoze-dsp: ndb_restore.dsp

ndb_restore.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(ndbtools_PROGRAMS)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_restore_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)
