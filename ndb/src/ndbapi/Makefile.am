#SUBDIRS = signal-sender

noinst_LTLIBRARIES = libndbapi.la

libndbapi_la_SOURCES = \
        TransporterFacade.cpp   \
        ClusterMgr.cpp  \
        Ndb.cpp \
        NdbPoolImpl.cpp \
        NdbPool.cpp \
        Ndblist.cpp     \
        Ndbif.cpp       \
        Ndbinit.cpp     \
        Ndberr.cpp    \
        ndberror.c    \
        NdbErrorOut.cpp    \
        NdbConnection.cpp       \
        NdbConnectionScan.cpp \
        NdbOperation.cpp        \
        NdbOperationSearch.cpp  \
        NdbOperationScan.cpp    \
        NdbOperationInt.cpp     \
        NdbOperationDefine.cpp  \
        NdbOperationExec.cpp    \
        NdbResultSet.cpp        \
        NdbCursorOperation.cpp  \
        NdbScanReceiver.cpp NdbScanOperation.cpp \
        NdbScanFilter.cpp \
        NdbIndexOperation.cpp   \
        NdbEventOperation.cpp   \
        NdbEventOperationImpl.cpp   \
        NdbApiSignal.cpp        \
        NdbRecAttr.cpp  \
        NdbSchemaCon.cpp        \
        NdbSchemaOp.cpp \
        NdbUtil.cpp \
        NdbReceiver.cpp \
        NdbDictionary.cpp \
        NdbDictionaryImpl.cpp \
        DictCache.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmapi

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
