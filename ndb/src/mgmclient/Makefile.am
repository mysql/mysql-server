
noinst_LTLIBRARIES = libndbmgmclient.la
ndbtools_PROGRAMS = ndb_mgm

libndbmgmclient_la_SOURCES = CommandInterpreter.cpp

ndb_mgm_SOURCES = main.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

INCLUDES += -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD_LOC = $(top_builddir)/ndb/src/mgmclient/libndbmgmclient.la \
            @readline_link@ \
            $(top_builddir)/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a \
            @TERMCAP_LIB@ @NDB_SCI_LIBS@

ndb_mgm_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%
