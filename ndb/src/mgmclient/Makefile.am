
noinst_LTLIBRARIES = libndbmgmclient.la
ndbtools_PROGRAMS = ndb_mgm

libndbmgmclient_la_SOURCES = CommandInterpreter.cpp
libndbmgmclient_la_LIBADD = ../mgmapi/libmgmapi.la \
			    ../common/logger/liblogger.la \
			    ../common/portlib/libportlib.la \
			    ../common/util/libgeneral.la \
			    ../common/portlib/libportlib.la


ndb_mgm_SOURCES = main.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

INCLUDES += -I$(top_srcdir)/ndb/include/mgmapi \
            -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD_LOC = $(noinst_LTLIBRARIES) \
	    ../common/portlib/libportlib.la \
            @readline_link@ \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a \
            @TERMCAP_LIB@ @NDB_SCI_LIBS@

ndb_mgm_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: ndb_mgm.dsp libndbmgmclient.dsp

ndb_mgm.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(ndbtools_PROGRAMS)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_mgm_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

libndbmgmclient.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libndbmgmclient_la_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB
