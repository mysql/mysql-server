# Copyright (c) 2008, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi)

ADD_CONVENIENCE_LIBRARY(ndbapi
  AssembleFragments.cpp
  ClusterMgr.cpp
  DictCache.cpp
  Ndb.cpp
  NdbApiSignal.cpp
  NdbBlob.cpp
  NdbDictionary.cpp
  NdbDictionaryImpl.cpp
  NdbErrorOut.cpp
  NdbEventOperation.cpp
  NdbEventOperationImpl.cpp
  NdbIndexOperation.cpp
  NdbIndexStat.cpp
  NdbIndexStatImpl.cpp
  NdbInfo.cpp
  NdbInfoScanNodes.cpp
  NdbInfoScanVirtual.cpp
  NdbInterpretedCode.cpp
  NdbOperation.cpp
  NdbOperationDefine.cpp
  NdbOperationExec.cpp
  NdbOperationInt.cpp
  NdbOperationSearch.cpp
  NdbPool.cpp
  NdbPoolImpl.cpp
  NdbQueryBuilder.cpp
  NdbQueryOperation.cpp
  NdbRecAttr.cpp
  NdbReceiver.cpp
  NdbScanFilter.cpp
  NdbScanOperation.cpp
  NdbTransaction.cpp
  NdbTransactionScan.cpp
  NdbUtil.cpp
  NdbWaitGroup.cpp
  Ndberr.cpp
  Ndbif.cpp
  Ndbinit.cpp
  Ndblist.cpp
  ObjectMap.cpp
  SignalSender.cpp
  SectionIterators.cpp
  TransporterFacade.cpp
  WakeupHandler.cpp
  ndb_cluster_connection.cpp
  ndb_internal.cpp
  ndberror.cpp
  trp_buffer.cpp
  trp_client.cpp
  trp_node.cpp
  LINK_LIBRARIES ext::zlib
  )

IF(MY_COMPILER_IS_GNU AND CMAKE_BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO")
  ADD_COMPILE_FLAGS(Ndb.cpp
    COMPILE_FLAGS
    " -Wno-error=maybe-uninitialized"
    )
ENDIF()

SET_PROPERTY(SOURCE TransporterFacade.cpp
             PROPERTY COMPILE_DEFINITIONS
             NDB_TLS_SEARCH_PATH="${WITH_NDB_TLS_SEARCH_PATH}")

NDB_ADD_TEST(SectionIterators-t SectionIterators.cpp NdbApiSignal.cpp)
