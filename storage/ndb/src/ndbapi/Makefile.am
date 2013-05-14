# Copyright (c) 2004-2007 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

EXTRA_DIST = CMakeLists.txt

noinst_PROGRAMS = ndberror_check-t

ndberror_check_t_SOURCES = ndberror_check.cpp

noinst_LTLIBRARIES = libndbapi.la

libndbapi_la_SOURCES = \
        NdbQueryBuilder.cpp     \
        NdbQueryOperation.cpp   \
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
        NdbWaitGroup.cpp \
        NdbScanOperation.cpp    \
        NdbScanFilter.cpp       \
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
	NdbIndexStatImpl.cpp \
	NdbIndexStatFrmData.cpp \
        SignalSender.cpp \
        ObjectMap.cpp \
	NdbInterpretedCode.cpp \
	NdbInfo.cpp \
	NdbInfoScanOperation.cpp \
        ndb_internal.cpp \
        trp_client.cpp \
        trp_node.cpp \
        trp_buffer.cpp \
        WakeupHandler.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/mgmapi

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am

EXTRA_PROGRAMS = testSectionIterators

testSectionIterators_CXXFLAGS = -DUNIT_TEST
testSectionIterators_SOURCES = TransporterFacade.cpp
testSectionIterators_LDFLAGS = @ndb_bin_am_ldflags@	 \
  $(top_builddir)/storage/ndb/src/libndbclient.la
