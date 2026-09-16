# Copyright (c) 2026, Oracle and/or its affiliates.
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

MY_CHECK_CXX_COMPILER_WARNING("-Wno-unused-local-typedefs" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})

ADD_STATIC_LIBRARY(mysql_scheduler_server_test_support
  ../../../../libs/mysql/scheduler/dependency_tracker_single_predecessor_example.cpp
  LINK_LIBRARIES mysql_concurrency_server extra::unordered_dense
)

ADD_STATIC_LIBRARY(mysql_scheduler_test_support
  ../../../../libs/mysql/scheduler/dependency_tracker_single_predecessor_example.cpp
  COMPILE_DEFINITIONS PRIVATE STANDALONE_LIBS_MYSQL
  LINK_LIBRARIES mysql_concurrency extra::unordered_dense
)

# Add tests
SET(TESTS
  sharded_counter
  scheduler
  scheduler_perf
  scheduler_perf_dependencies
  scheduler_sync
  scheduler_task_phases
  scheduler_throughput
  task_registry
  stat_monitor
)

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc
    ADD_TEST ${test}
    LINK_LIBRARIES mysql_scheduler mysql_scheduler_test_support gunit_small
    COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}" "STANDALONE_LIBS_MYSQL")
ENDFOREACH()

LIST(TRANSFORM TESTS APPEND "-t.cc" OUTPUT_VARIABLE ALL_SCHEDULER_SERVER_TESTS)

MYSQL_ADD_EXECUTABLE(merge_scheduler_srv_tests-t
  ${ALL_SCHEDULER_SERVER_TESTS}
  ADD_TEST merge_scheduler_srv_tests-t
  EXCLUDE_FROM_GCOV
  LINK_LIBRARIES mysql_concurrency_server
    mysql_scheduler_server_test_support gunit_large
    server_unittest_library)

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-srv-t ${test}-t.cc
    EXCLUDE_FROM_ALL
    SKIP_INSTALL
    ADD_GCOV_TEST ${test}-srv
    LINK_LIBRARIES mysql_concurrency_server
      mysql_scheduler_server_test_support gunit_large
      server_unittest_library)
ENDFOREACH()
