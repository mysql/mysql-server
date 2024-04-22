# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_uri.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_utils
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_utils.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_utils
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_mysqlrouter_utils.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_utils
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_log_filter.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_mysql
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

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
    test-helpers
    router_mysql
  INCLUDE_DIRS
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

FOREACH(test_file
    test_instance_attributes.cc
    )
  ADD_HARNESS_TEST_FILE(${test_file}
    MODULE ${TEST_MODULE}
    LIB_DEPENDS
      test-helpers router_cluster
    INCLUDE_DIRS
      $<TARGET_PROPERTY:router_cluster,INCLUDE_DIRECTORIES>
      ../src/
      ../../../tests/helpers/
    SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
ENDFOREACH()

ADD_HARNESS_TEST_FILE(test_windows_service.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
  EXTRA_SOURCES
    ../src/windows/main-windows.cc
    ../src/windows/nt_servc.cc
    ../src/windows/service_operations.cc
    ../src/utils.cc  # g_windows_service
    ../src/default_paths.cc
    ../../../tests/helpers/filesystem_utils.cc
  INCLUDE_DIRS
    $<TARGET_PROPERTY:router_utils,INCLUDE_DIRECTORIES>
    ../src/
    ../../../tests/helpers/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(
  routertest_${TEST_MODULE}_windows_service
  PROPERTIES
  COMPILE_DEFINITIONS ROUTER_UTILS_STATIC_DEFINE=1)


ADD_HARNESS_TEST_FILE(test_mysqlrouter_app.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_frontend_lib
    routing_frontend_lib
  EXTRA_SOURCES
    ../../../tests/helpers/filesystem_utils.cc
  INCLUDE_DIRS
    ../../../tests/helpers/
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

ADD_HARNESS_TEST_FILE(test_keyring_frontend.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_utils
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/keyring_frontend.cc
  INCLUDE_DIRS
    ../src/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  )
DOWNGRADE_STRINGOP_WARNINGS(routertest_router_keyring_frontend)

FOREACH(test_file
    issues/test_bug22084430.cc
    issues/test_bug24909259.cc
    )
  ADD_HARNESS_TEST_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
    MODULE issues
    LIB_DEPENDS
      test-helpers
      router_utils
      router_frontend_lib  # init_keyring
    INCLUDE_DIRS
      $<TARGET_PROPERTY:router_utils,INCLUDE_DIRECTORIES>
      ../src/
    SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  )
ENDFOREACH()
DOWNGRADE_STRINGOP_WARNINGS(routertest_issues_bug24909259)

ADD_HARNESS_TEST_FILE(test_default_paths.cc
  MODULE ${TEST_MODULE}
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
  LIB_DEPENDS
    router_utils
  )

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

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
