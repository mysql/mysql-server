# Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.
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


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi)
INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

ADD_CONVENIENCE_LIBRARY(ndbkernel
    VMSignal.cpp
    Emulator.cpp
    Configuration.cpp
    WatchDog.cpp
    SectionReader.cpp
    SegmentList.cpp
    Mutex.cpp
    SafeCounter.cpp
    Rope.cpp
    ndbd_malloc_impl.cpp
    ndbd_malloc.cpp
    Pool.cpp
    DynArr256.cpp
    LockQueue.cpp
    GlobalData.cpp
    SafeMutex.cpp
    Ndbinfo.cpp
    NdbinfoTables.cpp
    ArenaPool.cpp
    mt_thr_config.cpp
)
#    WOPool.cpp
#    RWPool.cpp

ADD_CONVENIENCE_LIBRARY(ndbsched
    TimeQueue.cpp
    ThreadConfig.cpp
    FastScheduler.cpp
    TransporterCallback_nonmt.cpp
    SimulatedBlock_nonmt.cpp
    LongSignal_nonmt.cpp
    SimplePropertiesSection_nonmt.cpp
    dummy_nonmt.cpp
)

IF(WITH_NDBMTD)
  ADD_CONVENIENCE_LIBRARY(ndbsched_mt
    SimulatedBlock_mt.cpp
    TransporterCallback_mt.cpp
    LongSignal_mt.cpp
    SimplePropertiesSection_mt.cpp
    mt.cpp
    dummy_mt.cpp)
ENDIF()

ADD_CONVENIENCE_LIBRARY(ndbtest
    test_context.cpp
    ../blocks/dbtup/tuppage.cpp
)

NDB_ADD_TEST(mt_thr_config-t mt_thr_config.cpp LIBS ndbgeneral)

# ${ALL_UNIT_TEST_LIBS} may be listed twice in the linker command due
# to inter-library dependencies
SET(ALL_UNIT_TEST_LIBS ndbtest ndbkernel ndbsched ndberror ndbtransport
                       ndbmgmcommon ndbmgmapi ndbportlib ndbgeneral)

FOREACH(testprog CountingPool DynArr256 IntrusiveList LHLevel SegmentList Rope)
  NDB_ADD_TEST("${testprog}-t" "${testprog}.cpp"
               LIBS ${ALL_UNIT_TEST_LIBS} ${ALL_UNIT_TEST_LIBS})
ENDFOREACH(testprog)

IF(WITH_NDBMTD)
  NDB_ADD_TEST(mt-send-t mt-send-t.cpp LIBS ${ALL_UNIT_TEST_LIBS})
ENDIF()
