
bin_PROGRAMS = mgmtsrvr

mgmtsrvr_SOURCES = \
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

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am
INCLUDES += -I$(top_srcdir)/ndb/src/ndbapi -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon

LDADD += \
	$(top_srcdir)/ndb/src/mgmapi/libmgmapi.la \
	$(top_srcdir)/ndb/src/ndbapi/libNDB_API.la \
	$(top_srcdir)/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
	$(top_srcdir)/ndb/src/common/util/libgeneral.la \
	$(top_srcdir)/ndb/src/common/editline/libeditline.a \
	@TERMCAP_LIB@

# Don't update the files from bitkeeper
%::SCCS/s.%
