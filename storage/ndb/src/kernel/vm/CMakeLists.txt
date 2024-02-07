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
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel/blocks
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel/error
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi)

ADD_LIBRARY(ndbkernel STATIC
  ArenaPool.cpp
  Configuration.cpp
  DynArr256.cpp
  Emulator.cpp
  GlobalData.cpp
  LockQueue.cpp
  Mutex.cpp
  Ndbinfo.cpp
  Pool.cpp
  Rope.cpp
  SafeCounter.cpp
  SafeMutex.cpp
  SectionReader.cpp
  SegmentList.cpp
  StaticSlotPool.cpp
  TransientPagePool.cpp
  TransientSlotPool.cpp
  VMSignal.cpp
  WatchDog.cpp
  mt_thr_config.cpp
  ndbd_malloc.cpp
  ndbd_malloc_impl.cpp
)

# 'sbrk' is deprecated [-Wdeprecated-declarations]
IF(APPLE)
  ADD_COMPILE_FLAGS(ndbd_malloc_impl.cpp COMPILE_FLAGS
    " -Wno-error=deprecated-declarations")
ENDIF()

ADD_LIBRARY(ndbsched STATIC
  FastScheduler.cpp
  LongSignal_nonmt.cpp
  SimplePropertiesSection_nonmt.cpp
  SimulatedBlock_nonmt.cpp
  ThreadConfig.cpp
  TimeQueue.cpp
  TransporterCallback_nonmt.cpp
  dummy_nonmt.cpp
)

IF(WITH_NDBMTD)
  ADD_LIBRARY(ndbsched_mt STATIC
    LongSignal_mt.cpp
    SimplePropertiesSection_mt.cpp
    SimulatedBlock_mt.cpp
    TransporterCallback_mt.cpp
    dummy_mt.cpp
    mt.cpp
    )
ENDIF()

ADD_LIBRARY(ndbtest STATIC
  test_context.cpp
  ../blocks/dbtup/tuppage.cpp
)

NDB_ADD_TEST(RefConvert-t RefConvert.cpp LIBS ndbgeneral ndbportlib)

# ${ALL_UNIT_TEST_LIBS} may be listed twice in the linker command due
# to inter-library dependencies
SET(ALL_UNIT_TEST_LIBS
  ndberror
  ndbgeneral
  ndbkernel
  ndbmgmapi
  ndbmgmcommon
  ndbportlib
  ndbsched
  ndbtest
  ndbtransport
  )

FOREACH(testprog
    CountingPool
    DynArr256
    IntrusiveList
    LHLevel
    Rope
    SegmentList
    TransientPagePool
    TransientSlotPool
    testDataBuffer
    testSimplePropertiesSection
    )
  NDB_ADD_TEST("${testprog}-t" "${testprog}.cpp"
    LIBS ${ALL_UNIT_TEST_LIBS} ${ALL_UNIT_TEST_LIBS})
ENDFOREACH(testprog)

NDB_ADD_EXECUTABLE(testCopy testCopy.cpp NDBCLIENT SKIP_INSTALL)

NDB_ADD_TEST(ndbd_malloc-t ndbd_malloc_impl.cpp LIBS ${ALL_UNIT_TEST_LIBS} ${ALL_UNIT_TEST_LIBS})

IF(WITH_NDBMTD)
  NDB_ADD_TEST(mt-send-t mt-send-t.cpp LIBS ${ALL_UNIT_TEST_LIBS})
ENDIF()
