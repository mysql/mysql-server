include .defs.mk

TYPE := ndbapi

PIC_ARCHIVE    := Y
NONPIC_ARCHIVE := Y
ARCHIVE_TARGET := ndbapi

A_LIB       := Y
SO_LIB      := Y
PIC_LIB     := Y
LIB_TARGET  := NDB_API

LIB_TARGET_ARCHIVES := $(ARCHIVE_TARGET) \
                       transporter       \
                       general           \
                       signaldataprint   \
                       mgmsrvcommon      \
                       portlib           \
                       logger            \
                       trace

DIRS := signal-sender

# Source files of non-templated classes (.cpp files)
SOURCES = \
        TransporterFacade.cpp   \
        ClusterMgr.cpp  \
        Ndb.cpp \
        NdbPoolImpl.cpp NdbPool.cpp \
        Ndblist.cpp     \
        Ndbif.cpp       \
        Ndbinit.cpp     \
        Ndberror.cpp    \
        NdbConnection.cpp       \
        NdbConnectionScan.cpp \
        NdbOperation.cpp        \
        NdbOperationSearch.cpp  \
        NdbOperationScan.cpp    \
        NdbOperationInt.cpp     \
        NdbOperationDefine.cpp  \
        NdbOperationExec.cpp    \
        NdbScanReceiver.cpp     \
        NdbResultSet.cpp        \
        NdbCursorOperation.cpp  \
        NdbScanOperation.cpp    NdbScanFilter.cpp \
        NdbIndexOperation.cpp   \
        NdbEventOperation.cpp   \
        NdbEventOperationImpl.cpp   \
        NdbApiSignal.cpp        \
        NdbRecAttr.cpp  \
        NdbSchemaCon.cpp        \
        NdbSchemaOp.cpp \
        NdbUtil.cpp \
        NdbReceiver.cpp \
        NdbDictionary.cpp NdbDictionaryImpl.cpp DictCache.cpp

include $(NDB_TOP)/Epilogue.mk

###
# Backward compatible
