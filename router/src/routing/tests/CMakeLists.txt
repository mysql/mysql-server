# Copyright (c) 2015, 2020, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(
  ../include
  ../src
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
  ${PROJECT_SOURCE_DIR}/src/mysql_protocol/include
  ${PROJECT_SOURCE_DIR}/src/harness/tests
  ../../../tests/helpers
)

ADD_DEFINITIONS(-DSSL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/mysql-test/std_data/")

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

INCLUDE_DIRECTORIES(SYSTEM
  ${MYSQLX_GENERATE_DIR}/protobuf_lite
)

ADD_LIBRARY(routing_tests STATIC ${ROUTING_SOURCE_FILES})
TARGET_LINK_LIBRARIES(routing_tests routertest_helpers router_lib
                      metadata_cache_static mysql_protocol
                      mysqlxclient_lite mysqlxmessages_lite ${PROTOBUF_LITE_LIBRARY})

TARGET_COMPILE_DEFINITIONS(routing_tests PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
TARGET_COMPILE_DEFINITIONS(routing_tests PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)
TARGET_COMPILE_DEFINITIONS(routing_tests PRIVATE -DROUTING_STATIC_DEFINE=1)
TARGET_INCLUDE_DIRECTORIES(routing_tests PUBLIC $<TARGET_PROPERTY:routing,INCLUDE_DIRECTORIES>)


ADD_LIBRARY(routing_plugin_tests STATIC ${ROUTING_PLUGIN_SOURCE_FILES})
TARGET_LINK_LIBRARIES(routing_plugin_tests routing_tests
                      routertest_helpers router_lib)
SET_TARGET_PROPERTIES(routing_plugin_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)
TARGET_INCLUDE_DIRECTORIES(routing_plugin_tests PRIVATE ${include_dirs})

TARGET_COMPILE_DEFINITIONS(routing_plugin_tests PUBLIC -DROUTING_STATIC_DEFINE=1)

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(routing_tests -lnsl -lsocket)
  TARGET_LINK_LIBRARIES(routing_plugin_tests -lnsl -lsocket)
ENDIF()

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

SET(TEST_MODULE "routing")
SET(test_files
  test_block_clients.cc
  test_config.cc
  # test_connection.cc
  test_connection_container.cc
  test_first_available.cc
  test_metadata_cache_group.cc
  test_next_available.cc
  test_round_robin.cc
  test_routing.cc
  test_setup_named_socket_service.cc
  test_setup_tcp_service.cc
  )
# This compiler fails to handle some of the mock functions involved.
IF(MY_COMPILER_IS_SUNPRO)
  LIST(REMOVE_ITEM test_files test_connection.cc)
ENDIF()

FOREACH(test_file ${test_files})
  add_test_file(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
  MODULE ${TEST_MODULE}
  LIB_DEPENDS routing_tests test-helpers
  INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/harness/shared/include)
ENDFOREACH()

add_test_file(${CMAKE_CURRENT_SOURCE_DIR}/plugin/test_plugin.cc
  MODULE "routing"
  LIB_DEPENDS
    routing_tests
    routing_plugin_tests
    test-helpers
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}/tests/helpers
    ${PROJECT_SOURCE_DIR}/src/harness/shared/include
  )

FOREACH(test_file
    test_bug21873666.cc
    test_bug21962350.cc
    test_bug22062859.cc
    test_bug22579989.cc
    )
  add_test_file(${CMAKE_CURRENT_SOURCE_DIR}/issues/${test_file}
    MODULE issues
  LIB_DEPENDS
    routing_tests
    routing_plugin_tests
    test-helpers
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}/tests/helpers
    ${PROJECT_SOURCE_DIR}/src/harness/shared/include
  )
ENDFOREACH()

ROUTERTEST_GET_TARGET(test_target "test_metadata_cache_group.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
  PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)

ROUTERTEST_GET_TARGET(test_target "test_routing.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
  PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)
TARGET_LINK_LIBRARIES(${test_target}
  harness_net_ts)

ADD_ROUTER_TEST_FILE(
  test_classic_protocol_splicer.cc
  MODULE routing
  LIB_DEPENDS
    mysql_protocol
    test-helpers
    mysqlxclient_lite
    mysqlxmessages_lite
    ${PROTOBUF_LITE_LIBRARY}

  EXTRA_SOURCES
    ../src/channel.cc
    ../src/classic_protocol_splicer.cc
    ../src/basic_protocol_splicer.cc
  )
