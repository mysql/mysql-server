#SUBDIRS = signal-sender

ndblib_LTLIBRARIES = libndbapi.la libNDB_API.la

libndbapi_la_SOURCES_loc = \
        TransporterFacade.cpp   \
        ClusterMgr.cpp  \
        Ndb.cpp \
        NdbPoolImpl.cpp NdbPool.cpp \
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

libndbapi_la_SOURCES = $(libndbapi_la_SOURCES_loc)
libNDB_API_la_SOURCES = $(libndbapi_la_SOURCES_loc)

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

libNDB_API_la_LIBADD =  \
              $(top_srcdir)/ndb/src/common/transporter/libtransporter.la   \
              $(top_srcdir)/ndb/src/common/debugger/libtrace.la     \
              $(top_srcdir)/ndb/src/common/debugger/signaldata/libsignaldataprint.la \
              $(top_srcdir)/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
              $(top_srcdir)/ndb/src/common/portlib/unix/libportlib.la      \
              $(top_srcdir)/ndb/src/common/logger/liblogger.la       \
              $(top_srcdir)/ndb/src/common/util/libgeneral.la 

# Don't update the files from bitkeeper
%::SCCS/s.%
