# Copyright (C) 2004-2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

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
        NdbApiSignal.cpp        \
        NdbRecAttr.cpp  \
        NdbUtil.cpp \
        NdbReceiver.cpp \
        NdbDictionary.cpp \
        NdbDictionaryImpl.cpp \
        DictCache.cpp \
        ndb_cluster_connection.cpp \
	NdbBlob.cpp \
        SignalSender.cpp \
        ObjectMap.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmapi

# Ndbapi cannot handle -O3
NDB_CXXFLAGS_RELEASE_LOC = -O2

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapi.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libndbapi.dsp

libndbapi.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libndbapi_la_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
