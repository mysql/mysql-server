# Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${NDB_SOURCE_DIR}/src/mgmapi)

ADD_CONVENIENCE_LIBRARY(ndbapi
            NdbEventOperation.cpp
            NdbEventOperationImpl.cpp
            NdbIndexStat.cpp
            NdbIndexStatImpl.cpp
            NdbIndexStatFrmData.cpp
            NdbInterpretedCode.cpp
            TransporterFacade.cpp
            ClusterMgr.cpp
            Ndb.cpp
            NdbPoolImpl.cpp
            NdbPool.cpp
            Ndblist.cpp
            Ndbif.cpp
            Ndbinit.cpp
            Ndberr.cpp
            ndberror.c
            NdbErrorOut.cpp
            NdbTransaction.cpp
            NdbTransactionScan.cpp
            NdbOperation.cpp
            NdbOperationSearch.cpp
            NdbOperationScan.cpp
            NdbOperationInt.cpp
            NdbOperationDefine.cpp
            NdbOperationExec.cpp
            NdbScanOperation.cpp
            NdbScanFilter.cpp
            NdbIndexOperation.cpp
            NdbQueryBuilder.cpp
            NdbQueryOperation.cpp
            NdbApiSignal.cpp
            NdbRecAttr.cpp
            NdbUtil.cpp
            NdbReceiver.cpp
            NdbDictionary.cpp
            NdbDictionaryImpl.cpp
            DictCache.cpp
            ndb_cluster_connection.cpp
            NdbBlob.cpp
            SignalSender.cpp
            ObjectMap.cpp
            NdbInfo.cpp
            NdbInfoScanNodes.cpp
            NdbInfoScanVirtual.cpp
            ${NDB_SOURCE_DIR}/src/kernel/blocks/dbtc/DbtcStateDesc.cpp
            ${NDB_SOURCE_DIR}/src/kernel/blocks/dblqh/DblqhStateDesc.cpp
            NdbWaitGroup.cpp
            WakeupHandler.cpp
            ndb_internal.cpp
	    trp_client.cpp
            trp_node.cpp
	    trp_buffer.cpp)
