
noinst_LIBRARIES = libNDBT.a

libNDBT_a_SOURCES = \
	NDBT_ReturnCodes.cpp \
	NDBT_Error.cpp NDBT_Tables.cpp NDBT_ResultRow.cpp \
	NDBT_Test.cpp HugoCalculator.cpp \
	HugoOperations.cpp HugoTransactions.cpp \
	HugoAsynchTransactions.cpp UtilTransactions.cpp \
	NdbRestarter.cpp NdbRestarts.cpp NDBT_Output.cpp \
	NdbBackup.cpp  NdbConfig.cpp NdbGrep.cpp NDBT_Table.cpp \
	NdbSchemaCon.cpp NdbSchemaOp.cpp getarg.c \
	CpcClient.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/common/mgmcommon -I$(top_srcdir)/ndb/include/mgmcommon -I$(top_srcdir)/ndb/include/kernel -I$(top_srcdir)/ndb/src/mgmapi

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitest.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libNDBT.dsp

libNDBT.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libNDBT_a_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
