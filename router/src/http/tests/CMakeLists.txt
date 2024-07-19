# Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/tests/)

SET(TEST_FILES
  server-key.pem
  server-cert.pem
  )
COPY_TEST_FILES(${CMAKE_SOURCE_DIR}/mysql-test/std_data/ "${TEST_FILES}")


add_googletest_file(test_time.cc
  MODULE http
  LIB_DEPENDS http_common
  )

add_googletest_file(test_uri.cc
  MODULE http
  LIB_DEPENDS http_common
  )

add_googletest_file(test_base64.cc
  MODULE http
  LIB_DEPENDS http_common
  )

add_googletest_file(test_auth_backend.cc
  MODULE http
  LIB_DEPENDS http_auth_backend_lib
  )
TARGET_INCLUDE_DIRECTORIES(routertest_http_auth_backend
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/http/src/
  )

add_googletest_file(test_kdf_sha_crypt.cc
  MODULE http
  LIB_DEPENDS http_auth_backend_lib
  )

add_googletest_file(test_kdf_pbkdf2.cc
  MODULE http
  LIB_DEPENDS http_auth_backend_lib
  )

add_googletest_file(test_passwd.cc
  MODULE http
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/passwd.cc
  LIB_DEPENDS
    http_auth_backend_lib
  )
TARGET_INCLUDE_DIRECTORIES(routertest_http_passwd
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/http/src/
  )


add_googletest_file(test_auth.cc
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/http_auth.cc
  MODULE http
  LIB_DEPENDS
    http_auth_realm_lib
  )
## http_auth.cc and friends are from the http_server_lib and
## their symbols are marked a "dllimport" by default.
## As we use them directly, mark them as "static" instead.
SET_TARGET_PROPERTIES(routertest_http_auth PROPERTIES
  COMPILE_DEFINITIONS "HTTP_SERVER_LIB_STATIC_DEFINE")

TARGET_INCLUDE_DIRECTORIES(routertest_http_auth
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/http/src/
  ${PROJECT_SOURCE_DIR}/src/http/include/
  )

add_googletest_file(test_auth_method.cc
  MODULE http
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/http_auth_method_basic.cc
  )
## http_auth_method_basic.cc is from the http_common library and
## its symbols are marked a "dllimport" by default.
## As we use them directly, mark them as "static" instead.
SET_TARGET_PROPERTIES(routertest_http_auth_method PROPERTIES
  COMPILE_DEFINITIONS "http_common_DEFINE_STATIC")

# the unit-test needs the http_common's include-directories.
#
# it would be better to use $<TARGET_PROPERTY:http_common,INCLUDE_DIRECTORIES>
# here, but it triggers a CMake Error with cmake-3.14.6 on el7-arm:
#
#   Found relative path while evaluating include directories of
#     "routertest_http_auth_method":
#
#         "p�y"

TARGET_INCLUDE_DIRECTORIES(routertest_http_auth_method
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/http/src/
  ${PROJECT_SOURCE_DIR}/src/http/include/
  )

add_googletest_file(test_content_type.cc
  MODULE http
  )
TARGET_INCLUDE_DIRECTORIES(routertest_http_content_type
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/http/src/
  ${PROJECT_SOURCE_DIR}/src/http/include/
  )

add_googletest_file(test_matcher.cc
  MODULE http
  )
TARGET_INCLUDE_DIRECTORIES(routertest_http_matcher
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/http/src/
  ${PROJECT_SOURCE_DIR}/src/http/include/
  )

add_googletest_file(test_http_request_router.cc
  MODULE http
  EXTRA_SOURCES
    ${CMAKE_SOURCE_DIR}/unittest/gunit/benchmark.cc
  LIB_DEPENDS
    router_utils        # uri
    http_server_lib     # request-router
    test-helpers        # init_test_logger
    ext::icu
  )

IF(LIBFUZZER_COMPILE_FLAGS)
  ## http_auth_backend
  MYSQL_ADD_EXECUTABLE(routertest_fuzz_http_auth_backend
    fuzz_auth_backend.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/http_auth_backend.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/http_auth_error.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kdf_sha_crypt.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kdf_pbkdf2.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mcf_error.cc
    COMPONENT Router
    LINK_LIBRARIES
      OpenSSL::SSL
      OpenSSL::Crypto
      extra::rapidjson
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routertest_fuzz_http_auth_backend
    PRIVATE $<TARGET_PROPERTY:http_auth_backend,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routertest_fuzz_http_auth_backend
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_auth_backend)

  ## http_auth_method_basic
  MYSQL_ADD_EXECUTABLE(routertest_fuzz_http_auth_method_basic
    fuzz_auth_method_basic.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/http_auth_method_basic.cc
    COMPONENT Router
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routertest_fuzz_http_auth_method_basic
    PRIVATE $<TARGET_PROPERTY:http_server_lib,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routertest_fuzz_http_auth_method_basic
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_auth_method_basic)

  ## base64
  MYSQL_ADD_EXECUTABLE(routertest_fuzz_http_base64
    fuzz_base64.cc
    COMPONENT Router
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routertest_fuzz_http_base64
    PRIVATE $<TARGET_PROPERTY:http_common,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routertest_fuzz_http_base64
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_base64)

ENDIF()
