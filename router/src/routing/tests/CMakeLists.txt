# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
  ../../../tests/helpers
)

INCLUDE_DIRECTORIES(SYSTEM
  ${PROTOBUF_INCLUDE_DIR}
  ${MYSQLX_GENERATE_DIR}/protobuf_lite
)

# link_directories(${PROJECT_BINARY_DIR}/ext/protobuf/protobuf-3.0.0/cmake/)
ADD_LIBRARY(routing_tests STATIC ${ROUTING_SOURCE_FILES})
TARGET_LINK_LIBRARIES(routing_tests routertest_helpers router_lib
                      metadata_cache_static mysql_protocol_static
                      mysqlxmessages_lite ${PROTOBUF_LITE_LIBRARY})
TARGET_COMPILE_DEFINITIONS(routing_tests PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
TARGET_COMPILE_DEFINITIONS(routing_tests PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)
TARGET_INCLUDE_DIRECTORIES(routing PRIVATE ${include_dirs})


ADD_LIBRARY(routing_plugin_tests STATIC ${ROUTING_PLUGIN_SOURCE_FILES})
TARGET_LINK_LIBRARIES(routing_plugin_tests routing_tests
                      routertest_helpers router_lib)
SET_TARGET_PROPERTIES(routing_plugin_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)
TARGET_INCLUDE_DIRECTORIES(routing_plugin_tests PRIVATE ${include_dirs})

TARGET_COMPILE_DEFINITIONS(routing_plugin_tests PRIVATE -Drouting_DEFINE_STATIC=1)

IF(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  TARGET_LINK_LIBRARIES(routing_tests -lnsl -lsocket)
  TARGET_LINK_LIBRARIES(routing_plugin_tests -lnsl -lsocket)
ENDIF()

SET(TEST_MODULE "routing")
add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
  MODULE ${TEST_MODULE}
  LIB_DEPENDS routing_tests test-helpers
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/harness/shared/include)

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}/plugin
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

add_test_dir(issues MODULE issues
  LIB_DEPENDS
    routing_tests
    routing_plugin_tests
    test-helpers
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}/tests/helpers
    ${PROJECT_SOURCE_DIR}/src/harness/shared/include
  )

ROUTERTEST_GET_TARGET(test_target "test_classic_protocol.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
  PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)

ROUTERTEST_GET_TARGET(test_target "test_metadata_cache_group.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
  PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)

ROUTERTEST_GET_TARGET(test_target "test_routing.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
  PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)

ROUTERTEST_GET_TARGET(test_target "test_x_protocol.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
  PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)
