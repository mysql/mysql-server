# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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

ADD_SUBDIRECTORY(bank)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/test/include
)

NDB_ADD_EXECUTABLE(create_all_tabs create_all_tabs.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(create_tab create_tab.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(drop_all_tabs drop_all_tabs.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(flexAsynch flexAsynch.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(flexBench flexBench.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(flexHammer flexHammer.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(flexTT flexTT.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testBackup testBackup.cpp)
NDB_ADD_EXECUTABLE(testBasic testBasic.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testBasicAsynch testBasicAsynch.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testBlobs testBlobs.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testDataBuffers testDataBuffers.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testDict testDict.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testIndex testIndex.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testMgm testMgm.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testNdbApi testNdbApi.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testNodeRestart testNodeRestart.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testOIBasic testOIBasic.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testOperations testOperations.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testRestartGci testRestartGci.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testScan testScan.cpp ScanFunctions.hpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testInterpreter testInterpreter.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testScanFilter testScanFilter.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testScanInterpreter
  testScanInterpreter.cpp
  ScanFilter.hpp
  ScanInterpretTest.hpp
  NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testScanPerf testScanPerf.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testSystemRestart testSystemRestart.cpp
  NDBTEST NDBCLIENT MYSQLCLIENT)
NDB_ADD_EXECUTABLE(testTimeout testTimeout.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testTransactions testTransactions.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testDeadlock testDeadlock.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(test_event test_event.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(ndbapi_slow_select slow_select.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testReadPerf testReadPerf.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testLcp testLcp.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testPartitioning testPartitioning.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testBitfield testBitfield.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(DbCreate
  bench/dbPopulate.cpp
  bench/dbPopulate.h
  bench/mainPopulate.cpp
  bench/ndb_error.hpp
  bench/ndb_schema.hpp
  bench/testData.h
  bench/testDefinitions.h
  bench/userInterface.cpp
  bench/userInterface.h
  NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(DbAsyncGenerator
  bench/asyncGenerator.cpp
  bench/dbGenerator.h
  bench/macros.h
  bench/mainAsyncGenerator.cpp
  bench/ndb_async2.cpp
  bench/ndb_error.hpp
  bench/ndb_schema.hpp
  bench/testData.h
  bench/testDefinitions.h
  bench/userInterface.h
  NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testSRBank testSRBank.cpp)
NDB_ADD_EXECUTABLE(test_event_merge test_event_merge.cpp NDBTEST NDBCLIENT)
TARGET_LINK_LIBRARIES(test_event_merge mysys) # uses handle_options
NDB_ADD_EXECUTABLE(testNdbinfo testNdbinfo.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testNativeDefault testNativeDefault.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testSpj testSpj.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testLimits testLimits.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testSingleUserMode testSingleUserMode.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testIndexStat testIndexStat.cpp NDBTEST NDBCLIENT)
TARGET_LINK_LIBRARIES(testIndexStat mysys) # uses handle_options
NDB_ADD_EXECUTABLE(testRedo testRedo.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testDowngrade testUpgrade.cpp NDBTEST NDBCLIENT MYSQLCLIENT)
NDB_ADD_EXECUTABLE(testUpgrade testUpgrade.cpp NDBTEST NDBCLIENT MYSQLCLIENT)
NDB_ADD_EXECUTABLE(testAsynchMultiwait testAsynchMultiwait.cpp
  NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testDebugger testDebugger.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testFK testFK.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(testLoad testLoad.cpp NDBTEST NDBCLIENT)

TARGET_LINK_LIBRARIES(testBackup ndbbank ndbNDBT ndbclient_so)
TARGET_LINK_LIBRARIES(testSRBank ndbbank ndbNDBT ndbclient_so)

IF(WIN32)
  NDB_ADD_EXECUTABLE(testMgmd testMgmd.cpp NDBTEST NDBCLIENT)
ELSE()
  NDB_ADD_EXECUTABLE(testMgmd testMgmd.cpp NDBTEST NDBCLIENT)
ENDIF()
