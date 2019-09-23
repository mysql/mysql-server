# Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/tests/)

add_test_file(test_time.cc
  MODULE http
  LIB_DEPENDS http_client
  )

add_test_file(test_posix_re.cc
  MODULE http
  )
TARGET_INCLUDE_DIRECTORIES(routertest_http_posix_re
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  )

add_test_file(test_http_uri.cc
  MODULE http
  LIB_DEPENDS http_common
  )

add_test_file(test_base64.cc
  MODULE http
  LIB_DEPENDS http_common
  )

add_test_file(test_auth_backend.cc
  MODULE http
  )

add_test_file(test_kdf_sha_crypt.cc
  MODULE http
  )

add_test_file(test_kdf_pbkdf2.cc
  MODULE http
  )

add_test_file(test_passwd.cc
  MODULE http
  EXTRA_SOURCES ../src/passwd.cc
  )

add_test_file(test_auth.cc
  MODULE http
  LIB_DEPENDS http_server
  )

add_test_file(test_auth_method.cc
  MODULE http
  LIB_DEPENDS http_server
  )

add_test_file(test_content_type.cc
  MODULE http
  LIB_DEPENDS http_server
  )


add_test_file(test_matcher.cc
  MODULE http
  )
TARGET_INCLUDE_DIRECTORIES(routertest_http_matcher
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  )


IF(LIBFUZZER_COMPILE_FLAGS)
  ## http_auth_backend
  MYSQL_ADD_EXECUTABLE(routerfuzz_auth_backend
    fuzz_auth_backend.cc
    ../src/http_auth_backend.cc
    ../src/kdf_sha_crypt.cc
    ../src/kdf_pbkdf2.cc
    ../src/mcf_error.cc
    ../src/base64.cc
    COMPONENT Router
    LINK_LIBRARIES ${SSL_LIBRARIES}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routerfuzz_auth_backend
    PRIVATE $<TARGET_PROPERTY:http_auth_backend,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routerfuzz_auth_backend
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_auth_backend)

  ## http_auth_method_basic
  MYSQL_ADD_EXECUTABLE(routerfuzz_auth_method_basic
    fuzz_auth_method_basic.cc
    ../src/http_auth_method_basic.cc
    ../src/base64.cc
    COMPONENT Router
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routerfuzz_auth_method_basic
    PRIVATE $<TARGET_PROPERTY:http_server,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routerfuzz_auth_method_basic
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_auth_method_basic)

  ## base64
  MYSQL_ADD_EXECUTABLE(routerfuzz_base64
    fuzz_base64.cc
    ../src/base64.cc
    COMPONENT Router
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routerfuzz_base64
    PRIVATE $<TARGET_PROPERTY:http_common,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routerfuzz_base64
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_base64)

ENDIF()
