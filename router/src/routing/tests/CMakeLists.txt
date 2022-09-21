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

ADD_DEFINITIONS(-DSSL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/mysql-test/std_data/")

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

# routing-library as object-lib for testing
STATICLIB_FROM_TARGET(routing_export_all routing)
TARGET_COMPILE_DEFINITIONS(routing_export_all
  PUBLIC ROUTING_STATIC_DEFINE=1)

SET(TEST_MODULE "routing")
SET(test_files
  test_block_clients.cc
  # test_connection.cc
  test_connection_container.cc
  test_first_available.cc
  test_metadata_cache_group.cc
  test_next_available.cc
  test_round_robin.cc
  test_routing.cc
  test_setup_named_socket_service.cc
  test_setup_tcp_service.cc
  test_auth_digest.cc
  )

FOREACH(test_file ${test_files})
  ADD_TEST_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
    MODULE ${TEST_MODULE}
    LIB_DEPENDS
      routing_export_all
      test-helpers
    INCLUDE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}/../src
      ${CMAKE_CURRENT_SOURCE_DIR}/../../harness/tests  # mock_io_service.h
  )
ENDFOREACH()

ADD_TEST_FILE(${CMAKE_CURRENT_SOURCE_DIR}/plugin/test_plugin.cc
  MODULE "routing"
  LIB_DEPENDS
    routing_export_all
    test-helpers
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}/tests/helpers
  EXTRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_config.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/routing_plugin.cc
  )
ROUTERTEST_GET_TARGET(T "plugin/test_plugin.cc" "routing")
TARGET_COMPILE_DEFINITIONS(${T}
  PRIVATE ROUTING_PLUGIN_STATIC_DEFINE=1
  )

# gcc12 with -O2 and -flto for Debug build reports
# error: __builtin_memcpy reading 106 bytes from a region of size 16
# for socket_name = make_string(MAX_SOCKET_NAME_LEN - 1); in test_plugin.cc
IF(MY_COMPILER_IS_GNU AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11)
  MY_TARGET_LINK_OPTIONS(${T} "-Wno-stringop-overread")
ENDIF()

ADD_ROUTER_TEST_FILE(
  test_classic_protocol_splicer.cc
  MODULE routing
  LIB_DEPENDS
    mysql_protocol
    test-helpers
    mysqlxclient_lite
    mysqlxmessages_lite
    connection_pool
    ${PROTOBUF_LITE_LIBRARY}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
  EXTRA_SOURCES
    ../src/channel.cc
    ../src/context.cc
    ../src/destination_ssl_context.cc
  )

ROUTERTEST_GET_TARGET(T "test_classic_protocol_splicer.cc" "routing")
TARGET_INCLUDE_DIRECTORIES(${T}
  PRIVATE $<TARGET_PROPERTY:routing,INCLUDE_DIRECTORIES>)
TARGET_COMPILE_DEFINITIONS(${T}
  PRIVATE ROUTING_STATIC_DEFINE=1)
