# Copyright (c) 2009, 2024, Oracle and/or its affiliates.
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
 ${CMAKE_SOURCE_DIR}/storage/ndb/include
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/logger
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/util
 ${CMAKE_SOURCE_DIR}/storage/ndb/test/include
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmclient
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmcommon
)
IF(NOT WIN32)
  ADD_DEFINITIONS(-DDEFAULT_PREFIX=\"${CMAKE_INSTALL_PREFIX}\")
ENDIF()

MYSQL_ADD_EXECUTABLE(atrt
  main.cpp
  setup.cpp
  files.cpp
  db.cpp
  command.cpp
  process_management.cpp
  test_execution_resources.cpp
  DESTINATION mysql-test/ndb
  ENABLE_EXPORTS
  LINK_LIBRARIES ndbNDBT ndbclient_static mysqlclient
  )

IF(MY_COMPILER_IS_CLANG)
  ADD_COMPILE_FLAGS(command.cpp COMPILE_FLAGS
    " -Wno-error=dynamic-class-memaccess")
ENDIF()

FILE(GLOB testcase_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*-tests.txt")
ADD_TEST(NAME check_testcase_files
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND atrt --check-testcase-files ${testcase_files})

ADD_CUSTOM_COMMAND(OUTPUT check_testcase_files.passed
  COMMAND atrt
          --check-testcase-files ${testcase_files}
          > ${CMAKE_CURRENT_BINARY_DIR}/check_testcase_files.output
  COMMAND echo See output in ${CMAKE_CURRENT_BINARY_DIR}/check_testcase_files.output
          > ${CMAKE_CURRENT_BINARY_DIR}/check_testcase_files.passed
          DEPENDS atrt ${testcase_files}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          )

ADD_CUSTOM_TARGET(check_testcase_files ALL
  SOURCES check_testcase_files.passed)

INSTALL(PROGRAMS atrt-mysql-test-run
  DESTINATION bin)

INSTALL(PROGRAMS
  atrt-analyze-result.sh
  atrt-backtrace.sh
  atrt-gather-result.sh
  atrt-setup.sh
  atrt-analyze-coverage.sh
  atrt-compute-coverage.sh
  atrt-mysqltest
  autotest-boot.sh
  autotest-run.sh
  check-tests.sh
  make-config.sh
  make-html-reports.sh
  make-index.sh
  ndb-autotest.sh
  upgrade-boot.sh
  DESTINATION mysql-test/ndb
  )

INSTALL(DIRECTORY .
  DESTINATION mysql-test/ndb
  FILES_MATCHING
  PATTERN "*-tests.txt"
  PATTERN "conf-*.cnf"
  )
