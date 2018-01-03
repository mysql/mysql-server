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

ADD_SUBDIRECTORY(bank)

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitest.cmake)
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
)

ADD_EXECUTABLE(create_all_tabs create_all_tabs.cpp)
ADD_EXECUTABLE(create_tab create_tab.cpp)
ADD_EXECUTABLE(drop_all_tabs drop_all_tabs.cpp)
ADD_EXECUTABLE(flexAsynch flexAsynch.cpp)
ADD_EXECUTABLE(flexBench flexBench.cpp)
ADD_EXECUTABLE(flexHammer flexHammer.cpp)
ADD_EXECUTABLE(flexTT flexTT.cpp)
ADD_EXECUTABLE(testBackup testBackup.cpp)
ADD_EXECUTABLE(testBasic testBasic.cpp)
ADD_EXECUTABLE(testBasicAsynch testBasicAsynch.cpp)
ADD_EXECUTABLE(testBlobs testBlobs.cpp)
ADD_EXECUTABLE(testDataBuffers testDataBuffers.cpp)
ADD_EXECUTABLE(testDict testDict.cpp)
ADD_EXECUTABLE(testIndex testIndex.cpp)
ADD_EXECUTABLE(testMgm testMgm.cpp)
ADD_EXECUTABLE(testNdbApi testNdbApi.cpp)
ADD_EXECUTABLE(testNodeRestart testNodeRestart.cpp)
ADD_EXECUTABLE(testOIBasic testOIBasic.cpp)
ADD_EXECUTABLE(testOperations testOperations.cpp)
ADD_EXECUTABLE(testRestartGci testRestartGci.cpp)
ADD_EXECUTABLE(testScan testScan.cpp ScanFunctions.hpp)
ADD_EXECUTABLE(testInterpreter testInterpreter.cpp)
ADD_EXECUTABLE(testScanFilter testScanFilter.cpp)
ADD_EXECUTABLE(testScanInterpreter testScanInterpreter.cpp ScanFilter.hpp ScanInterpretTest.hpp )
ADD_EXECUTABLE(testScanPerf testScanPerf.cpp)
ADD_EXECUTABLE(testSystemRestart testSystemRestart.cpp)
ADD_EXECUTABLE(testTimeout testTimeout.cpp)
ADD_EXECUTABLE(testTransactions testTransactions.cpp)
ADD_EXECUTABLE(testDeadlock testDeadlock.cpp)
ADD_EXECUTABLE(test_event test_event.cpp)
ADD_EXECUTABLE(ndbapi_slow_select slow_select.cpp)
ADD_EXECUTABLE(testReadPerf testReadPerf.cpp)
ADD_EXECUTABLE(testLcp testLcp.cpp)
ADD_EXECUTABLE(testPartitioning testPartitioning.cpp)
ADD_EXECUTABLE(testBitfield testBitfield.cpp)
ADD_EXECUTABLE(DbCreate bench/mainPopulate.cpp bench/dbPopulate.cpp bench/userInterface.cpp bench/dbPopulate.h bench/userInterface.h bench/testData.h bench/testDefinitions.h bench/ndb_schema.hpp bench/ndb_error.hpp)
ADD_EXECUTABLE(DbAsyncGenerator bench/mainAsyncGenerator.cpp bench/asyncGenerator.cpp bench/ndb_async2.cpp bench/dbGenerator.h bench/macros.h bench/userInterface.h bench/testData.h bench/testDefinitions.h bench/ndb_schema.hpp bench/ndb_error.hpp)
ADD_EXECUTABLE(testSRBank testSRBank.cpp)
ADD_EXECUTABLE(test_event_merge test_event_merge.cpp)
Add_EXECUTABLE(testNdbinfo testNdbinfo.cpp)
ADD_EXECUTABLE(testNativeDefault testNativeDefault.cpp)
ADD_EXECUTABLE(testSpj testSpj.cpp)
TARGET_LINK_LIBRARIES(testBackup ndbbank)
TARGET_LINK_LIBRARIES(testSRBank ndbbank)
ADD_EXECUTABLE(testLimits testLimits.cpp)
ADD_EXECUTABLE(testSingleUserMode testSingleUserMode.cpp)
ADD_EXECUTABLE(testIndexStat testIndexStat.cpp)
ADD_EXECUTABLE(testRedo testRedo.cpp)
ADD_EXECUTABLE(testUpgrade testUpgrade.cpp)
ADD_EXECUTABLE(testAsynchMultiwait testAsynchMultiwait.cpp)
ADD_EXECUTABLE(testDebugger testDebugger.cpp)
ADD_EXECUTABLE(testFK testFK.cpp)

SET(BINS create_all_tabs create_tab
  drop_all_tabs flexAsynch flexBench
  flexHammer flexTT testBackup testBasic testBasicAsynch
  testBlobs testDataBuffers testDict testIndex testMgm
  testNdbApi testNodeRestart testOIBasic testOperations
  testRestartGci testScan testSRBank testInterpreter
  testScanFilter testScanInterpreter testScanPerf
  testSystemRestart testTimeout testTransactions
  testDeadlock test_event ndbapi_slow_select testReadPerf
  testLcp testPartitioning DbCreate
  DbAsyncGenerator test_event_merge testNdbinfo
  testNativeDefault testLimits testSpj
  testSingleUserMode
  testIndexStat testRedo
  testUpgrade
  testAsynchMultiwait
  testDebugger
  testFK)

IF(MSVC)
  
ELSE()
  ADD_EXECUTABLE(testMgmd testMgmd.cpp)
  SET(BINS ${BINS} testMgmd)
ENDIF()

FOREACH(B ${BINS})
  TARGET_LINK_LIBRARIES(${B} ndbNDBT)
  TARGET_LINK_LIBRARIES(${B} ndbclient_so)
ENDFOREACH()

TARGET_LINK_LIBRARIES(testBitfield ndbNDBT ndbclient_static)
SET(BINS ${BINS} testBitfield)

TARGET_LINK_LIBRARIES(testUpgrade mysqlclient)
TARGET_LINK_LIBRARIES(testSystemRestart mysqlclient)

INSTALL(TARGETS ${BINS} DESTINATION bin)
