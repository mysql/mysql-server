
ndbbin_PROGRAMS = ndb_mgmd

ndb_mgmd_SOURCES = \
	MgmtSrvr.cpp \
	MgmtSrvrGeneralSignalHandling.cpp \
	main.cpp \
	Services.cpp \
	convertStrToInt.cpp \
        NodeLogLevel.cpp \
	NodeLogLevelList.cpp \
	SignalQueue.cpp \
	MgmtSrvrConfig.cpp \
	CommandInterpreter.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/ndbapi \
               -I$(top_srcdir)/ndb/src/mgmapi \
               -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD_LOC = $(top_builddir)/ndb/src/libndbclient.la \
	    $(top_builddir)/ndb/src/common/editline/libeditline.a \
            $(top_builddir)/ndb/src/libndbclient.la \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a
	    @TERMCAP_LIB@

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

ndb_mgmd_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%
