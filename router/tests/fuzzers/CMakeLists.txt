# Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

IF(LIBFUZZER_COMPILE_FLAGS)
  # FIXME: uri.cc depends on split_string() from utils.cc which pulls in
  # the whole harness. utils.cc should be split into a smaller chunks
  # (string, file, ...) to allow to build directly against it.
  #
  MYSQL_ADD_EXECUTABLE(routerfuzz_router_uri
    fuzz_router_uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/utils.cc

    COMPONENT Router

    LINK_LIBRARIES
    harness-library
    mysqlclient
    ${SSL_LIBRARIES}

    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routerfuzz_router_uri
    PRIVATE
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    )
  LIBFUZZER_ADD_TEST(routerfuzz_router_uri
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus/fuzz_router_uri
    )


  # FIXME: uri.cc depends on split_string() from utils.cc which pulls in
  # the whole harness. utils.cc should be split into a smaller chunks
  # (string, file, ...) to allow to build directly against it.
  #
  MYSQL_ADD_EXECUTABLE(routerfuzz_router_uri_tostring
    fuzz_router_uri_tostring.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/utils.cc

    COMPONENT Router

    LINK_LIBRARIES
    harness-library
    mysqlclient
    ${SSL_LIBRARIES}

    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routerfuzz_router_uri_tostring
    PRIVATE
    $<TARGET_PROPERTY:router_lib,INCLUDE_DIRECTORIES>
    )
  LIBFUZZER_ADD_TEST(routerfuzz_router_uri_tostring
    INITIAL_CORPUS_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/corpus/fuzz_router_uri_tostring
    )
ENDIF()
