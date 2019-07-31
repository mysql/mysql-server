# Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
INCLUDE_DIRECTORIES(../include ../src ../../../tests/helpers)

SET(TESTS
  test_cluster_metadata.cc
  test_config_files.cc
  test_config_generator.cc
  test_datatypes.cc
  test_log_filter.cc
  test_metadata_check.cc
  test_mysql_session.cc
  test_uri.cc
  test_utils.cc
  test_windows_service.cc
  mysqlrouter_app.cc
  mysqlrouter_utils.cc
  )

FOREACH(TEST_FILE ${TESTS})
  add_test_file(${TEST_FILE}
    MODULE "router"
    #ENVIRONMENT "MYSQL_ROUTER_HOME=${MySQLRouter_BINARY_STAGE_DIR}/etc/"
    LIB_DEPENDS test-helpers
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/harness/shared/include
    SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
ENDFOREACH()

add_test_file(test_keyring_frontend.cc
  MODULE "router"
  #ENVIRONMENT "MYSQL_ROUTER_HOME=${MySQLRouter_BINARY_STAGE_DIR}/etc/"
  LIB_DEPENDS test-helpers
  INCLUDE_DIRS
    ${MySQLRouter_SOURCE_DIR}/src/harness/shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/keyring_frontend.cc
  )

FOREACH(test_file
    issues/test_bug22084430.cc
    issues/test_bug22572346.cc
    issues/test_bug24909259.cc
    )
  add_test_file(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
    MODULE issues
  LIB_DEPENDS test-helpers
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
ENDFOREACH()

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

# Use configuration file templates to generate configuration files
CONFIGURE_TEST_FILE_TEMPLATES(${CMAKE_CURRENT_SOURCE_DIR})
COPY_TEST_FILES(${CMAKE_CURRENT_SOURCE_DIR})
