include .defs.mk

TYPE := ndbapitest

ARCHIVE_TARGET := NDBT

SOURCES = NDBT_ReturnCodes.cpp \
	NDBT_Error.cpp NDBT_Tables.cpp NDBT_ResultRow.cpp \
	NDBT_Test.cpp HugoCalculator.cpp \
	HugoOperations.cpp HugoTransactions.cpp \
	HugoAsynchTransactions.cpp UtilTransactions.cpp \
	NdbRestarter.cpp NdbRestarts.cpp NDBT_Output.cpp \
	NdbBackup.cpp  NdbConfig.cpp NdbGrep.cpp NDBT_Table.cpp

SOURCES.c =  

CFLAGS_NdbRestarter.cpp := -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon)
CFLAGS_NdbConfig.cpp := -I$(call fixpath,$(NDB_TOP)/include/mgmcommon) \
                        -I$(call fixpath,$(NDB_TOP)/src/mgmapi)
CFLAGS_NdbRestarts.cpp := -I$(call fixpath,$(NDB_TOP)/include/kernel)
CFLAGS_NdbBackup.cpp := -I$(call fixpath,$(NDB_TOP)/include/mgmcommon) \
                        -I$(call fixpath,$(NDB_TOP)/src/mgmapi) \
                        -I$(call fixpath,$(NDB_TOP)/include/kernel) 
CFLAGS_NdbGrep.cpp   += -I$(call fixpath,$(NDB_TOP)/include/kernel)  -I$(call fixpath,$(NDB_TOP)/include/mgmcommon)

include $(NDB_TOP)/Epilogue.mk







