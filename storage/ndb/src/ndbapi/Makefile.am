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
        NdbTransaction.cpp       \
        NdbTransactionScan.cpp \
        NdbOperation.cpp        \
        NdbOperationSearch.cpp  \
        NdbOperationScan.cpp    \
        NdbOperationInt.cpp     \
        NdbOperationDefine.cpp  \
        NdbOperationExec.cpp    \
        NdbScanOperation.cpp    NdbScanFilter.cpp \
        NdbIndexOperation.cpp   \
        NdbEventOperation.cpp   \
        NdbEventOperationImpl.cpp   \
        NdbApiSignal.cpp        \
        NdbRecAttr.cpp  \
        NdbUtil.cpp \
        NdbReceiver.cpp \
        NdbDictionary.cpp \
        NdbDictionaryImpl.cpp \
        DictCache.cpp \
        ndb_cluster_connection.cpp \
	NdbBlob.cpp \
	NdbIndexStat.cpp \
        SignalSender.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/mgmapi

# Ndbapi cannot handle -O3
NDB_CXXFLAGS_RELEASE_LOC = -O2

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libndbapi.dsp

libndbapi.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libndbapi_la_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
