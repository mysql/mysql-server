include .defs.mk

TYPE :=		odbcclient
#TYPE :=	odbcdriver

BIN_TARGET :=	testOdbcClient
#BIN_TARGET :=	testodbc2


# Source files of non-templated classes (.C files)
SOURCES =	main.cpp \
		SQLFetchTest.cpp \
		SQLDisconnectTest.cpp \
		SQLTablesTest.cpp \
		SQLGetInfoTest.cpp \
		SQLGetTypeInfoTest.cpp \
		SQLGetFunctionsTest.cpp \
		SQLGetDataTest.cpp \
		SQLCancelTest.cpp \
		SQLTransactTest.cpp \
		SQLGetCursorNameTest.cpp \
		SQLSetCursorNameTest.cpp \
		SQLRowCountTest.cpp \
		SQLNumResultColsTest.cpp \
		SQLDescribeColTest.cpp \
		SQLExecDirectTest.cpp \
		SQLColAttributeTest.cpp \
		SQLColAttributeTest1.cpp \
		SQLColAttributeTest2.cpp \
		SQLColAttributeTest3.cpp \
		SQLBindColTest.cpp \
		SQLDriverConnectTest.cpp \
		SQLPrepareTest.cpp \
		SQLGetDiagRecSimpleTest.cpp \
		SQLConnectTest.cpp

XSOURCES =	testodbc2.cpp
XSOURCES = \
		main.cpp \
		SQLDriverConnectTest.cpp \
		SQLPrepareTest.cpp \
		SQLMoreResultsTest.cpp \
		SQLGetStmtAttrTest.cpp \
		SQLGetEnvAttrTest.cpp \
		SQLGetConnectAttrTest.cpp \
		SQLExecuteTest.cpp \
		SQLExecDirectTest.cpp \
		SQLDisconnectTest.cpp \
		SQLCloseCursorTest.cpp \
		SQLCancelTest.cpp \
		SQLBindColTest.cpp \
		SQLDescribeColTest.cpp \
		SQLGetTypeInfoTest.cpp \
		SQLGetFunctionsTest.cpp \
		SQLNumResultColsTest.cpp \
		SQLSetDescFieldTest.cpp \
		SQLGetDescRecTest.cpp \
		SQLEndTranTest.cpp \
		SQLGetInfoTest.cpp \
		SQLConnectTest.cpp \
		SQLAllocHandleTest.cpp \
		SQLAllocEnvTest.cpp \
		SQLRowCountTest.cpp \
		SQLFetchScrollTest.cpp \
		SQLFetchTest.cpp \
		SQLGetDescFieldTest.cpp \
		SQLSetDescRecTest.cpp \
		SQLFreeHandleTest.cpp

ifeq ($(TYPE),odbcdriver)
LIBS_SPEC += \
		-lodbcdriver_pic \
		-lodbchandles_pic \
		-lodbccodegen_pic \
		-lodbccompiler_pic \
		-lodbcexecutor_pic \
		-lodbccommon_pic \
		-lodbcdictionary_pic \
		-lNDBT \
		-lportlib
endif

ifeq ($(TYPE),odbcclient)
LIBS_SPEC += \
		-lportlib \
		-lNDBT 
endif

CCFLAGS_LOC += 	-I/usr/local/include \
	        -I$(NDB_TOP)/include/ndbapi \
		-I$(NDB_TOP)/test/include

include $(NDB_TOP)/Epilogue.mk
#LIBS_LOC +=	-L/usr/local/opt/iODBC/lib
#LIBS_SPEC =	-liodbc -lNDBT -lportlib
