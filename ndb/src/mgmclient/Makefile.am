
ndbbin_PROGRAMS = mgmtclient

mgmtclient_SOURCES = \
	main.cpp \
	CommandInterpreter.cpp \
	CpcClient.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

INCLUDES += -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD_LOC = $(top_srcdir)/ndb/src/.libs/libndbclient.a \
            $(top_srcdir)/ndb/src/common/editline/libeditline.a \
            @TERMCAP_LIB@

# Don't update the files from bitkeeper
%::SCCS/s.%
