# Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

SET(TEST_MODULE router)

# build unit-tests with only the files they actually need.
#
# Ensures that:
#
# - their dependencies are properly defined.
# - fast edit-compile-test cycles for single tests

ADD_HARNESS_TEST_FILE(test_certificate_generator.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers  # init_test_logger
    harness_tls
  EXTRA_SOURCES
    ../src/certificate_generator.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})


ADD_HARNESS_TEST_FILE(test_certificate_handler.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    harness_tls
    harness_stdx
    router_lib
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_uri.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
  EXTRA_SOURCES
    ../src/uri.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_uri
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_utils.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
  EXTRA_SOURCES
    ../src/utils.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_utils
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_mysqlrouter_utils.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
  EXTRA_SOURCES
    ../src/utils.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_mysqlrouter_utils
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_log_filter.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
  EXTRA_SOURCES
    ../src/common/log_filter.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_log_filter
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_config_files.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
  EXTRA_SOURCES
    ../src/config_files.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_config_files
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_mysql_session.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers mysqlclient
  EXTRA_SOURCES
    ../src/common/mysql_session.cc
    ../src/common/log_filter.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_mysql_session
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_metadata_check.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers mysqlclient
  EXTRA_SOURCES
    ../src/common/mysql_session.cc
    ../src/common/log_filter.cc
    ../src/common/utils_sqlstring.cc
    ../src/cluster_metadata.cc
    ../src/uri.cc
    ../src/utils.cc
    ../../../tests/helpers/mysql_session_replayer.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
    ../../../tests/helpers/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_metadata_check
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_cluster_metadata.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers mysqlclient
  EXTRA_SOURCES
    ../src/common/mysql_session.cc
    ../src/common/log_filter.cc
    ../src/common/utils_sqlstring.cc
    ../src/cluster_metadata.cc
    ../src/uri.cc
    ../src/utils.cc
    ../../../tests/helpers/mysql_session_replayer.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
    ../../../tests/helpers/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_cluster_metadata
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_windows_service.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers mysqlclient
  EXTRA_SOURCES
    ../src/windows/main-windows.cc
    ../src/windows/nt_servc.cc
    ../src/windows/service_operations.cc
    ../src/utils.cc  # g_windows_service
    ../src/default_paths.cc
    ../../../tests/helpers/filesystem_utils.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
    ../../../tests/helpers/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_windows_service
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

ADD_HARNESS_TEST_FILE(test_config_generator.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_lib
    router_frontend_lib
  EXTRA_SOURCES
    ../../../tests/helpers/filesystem_utils.cc
    ../../../tests/helpers/mysql_session_replayer.cc
  INCLUDE_DIRS
    ../../../tests/helpers/
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_mysqlrouter_app.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_lib
    router_frontend_lib
  EXTRA_SOURCES
    ../../../tests/helpers/filesystem_utils.cc
    ../../../tests/helpers/mysql_session_replayer.cc
  INCLUDE_DIRS
    ../../../tests/helpers/
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_keyring_frontend.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_lib
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/keyring_frontend.cc
  INCLUDE_DIRS
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  )

FOREACH(test_file
    issues/test_bug22084430.cc
    issues/test_bug24909259.cc
    )
  ADD_HARNESS_TEST_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
    MODULE issues
    LIB_DEPENDS
      test-helpers
      router_lib
      router_frontend_lib
    EXTRA_SOURCES
    INCLUDE_DIRS
      ../src/
    SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  )
ENDFOREACH()

ADD_HARNESS_TEST_FILE(test_default_paths.cc
  MODULE ${TEST_MODULE}
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    ../src/
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/default_paths.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils.cc
  )
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_default_paths
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_LIB_STATIC_DEFINE=1)

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Use configuration file templates to generate configuration files
SET(FILE_TEMPLATES
  mysqlrouter.conf.in
  mysqlrouter_consolelogger.conf.in
  mysqlrouter_extra.conf.in
  mysqlrouter_nologger.conf.in
)
CONFIGURE_TEST_FILE_TEMPLATES(${CMAKE_CURRENT_SOURCE_DIR} "${FILE_TEMPLATES}")
COPY_TEST_FILES(${CMAKE_CURRENT_SOURCE_DIR}
  "config_a.conf;config_b.conf;config_c.ini;parse_error.conf"
  )
