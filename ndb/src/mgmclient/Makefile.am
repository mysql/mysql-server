
ndbtools_PROGRAMS = ndb_mgm

ndb_mgm_SOURCES = \
	main.cpp \
	CommandInterpreter.cpp \
	CpcClient.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

INCLUDES += -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD_LOC = $(top_builddir)/ndb/src/libndbclient.la \
            $(top_builddir)/ndb/src/common/editline/libeditline.a \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a \
            @TERMCAP_LIB@ @NDB_SCI_LIBS@

ndb_mgm_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%
