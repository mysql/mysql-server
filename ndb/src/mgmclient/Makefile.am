
bin_PROGRAMS = mgmtclient

mgmtclient_SOURCES = \
	main.cpp \
	CommandInterpreter.cpp \
	CpcClient.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am
INCLUDES += -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD += \
	$(top_srcdir)/ndb/src/common/debugger/libtrace.la \
	$(top_srcdir)/ndb/src/common/logger/liblogger.la \
	$(top_srcdir)/ndb/src/common/util/libgeneral.la \
	$(top_srcdir)/ndb/src/mgmapi/libmgmapi.la \
	$(top_srcdir)/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
	$(top_srcdir)/ndb/src/common/portlib/unix/libportlib.la \
	$(top_srcdir)/ndb/src/common/editline/libeditline.a

# Don't update the files from bitkeeper
%::SCCS/s.%
