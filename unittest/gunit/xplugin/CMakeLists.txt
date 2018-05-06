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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(WITHOUT_MYSQLX OR NOT GMOCK_FOUND OR DISABLE_SHARED OR NOT PROTOBUF_FOUND)
  RETURN()
ENDIF()

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

INCLUDE(CTest)

SET(MYSQLX_PROJECT_DIR "${PROJECT_SOURCE_DIR}/plugin/x")

GET_DIRECTORY_PROPERTY(MYSQLX_PROTOCOL_INCLUDE_DIR
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_PROTOCOL_INCLUDE_DIR
)

GET_DIRECTORY_PROPERTY(MYSQLX_PROTOCOL_LIB
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_PROTOCOL_FULL_LIB
)

GET_DIRECTORY_PROPERTY(MYSQLX_PROTOCOL_FLAGS
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_PROTOCOL_FLAGS
)

GET_DIRECTORY_PROPERTY(MYSQLX_CLIENT_INCLUDE_DIR
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_CLIENT_INCLUDE_DIR
)

GET_DIRECTORY_PROPERTY(MYSQLX_CLIENT_LIB
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_CLIENT_FULL_LIB
)

GET_DIRECTORY_PROPERTY(MYSQLX_PLUGIN_SRC
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION xplugin_SRC
)

GET_DIRECTORY_PROPERTY(MYSQLX_PLUGIN_INCLUDE_DIRS
  DIRECTORY ${MYSQLX_PROJECT_DIR} INCLUDE_DIRECTORIES
)

# Turn off some warning flags when compiling GUnit and proto files.
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
ENDIF()

ADD_DEFINITIONS(
  ${GMOCK_CFLAGS}
  -DUSE_MYSQLX_FULL_PROTO
  -DXPLUGIN_UNIT_TESTS
  -DDISABLE_PSI_THREAD
  -DDISABLE_PSI_MUTEX
  -DDISABLE_PSI_RWLOCK
  -DDISABLE_PSI_SOCKET
  -DDISABLE_PSI_COND
)

ADD_DEFINITIONS(${GMOCK_CFLAGS} -DUSE_MYSQLX_FULL_PROTO -DXPLUGIN_UNIT_TESTS
                -DDISABLE_ERROR_LOGGING -DEXTRA_CODE_FOR_UNIT_TESTING)

INCLUDE_DIRECTORIES(SYSTEM
  ${PROTOBUF_INCLUDE_DIRS}
  ${LIBEVENT_INCLUDE_DIR}
  ${GMOCK_INCLUDE_DIRS}
  ${GTEST_INCLUDE_DIR}
)

INCLUDE_DIRECTORIES(
  ${MYSQLX_PLUGIN_INCLUDE_DIRS}
  ${MYSQLX_PROTOCOL_INCLUDE_DIR}
  ${MYSQLX_CLIENT_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/libbinlogevents/include
  ${PROJECT_SOURCE_DIR}/sql
  ${PROJECT_SOURCE_DIR}/include/mysql
)

FILE(GLOB unit_tests_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/xpl/*_t.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xpl/mysqlx_pb_wrapper.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xcl/*_t.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xpl/stubs/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cc"
  "${MYSQLX_PROJECT_DIR}/src/mysql_function_names.cc"
  "${MYSQLX_PROJECT_DIR}/mysqlxtest_src/mysqlx_row.cc"
)

SET(unit_tests_EXE xplugin_unit_tests)

MYSQL_ADD_EXECUTABLE(${unit_tests_EXE}
  ${unit_tests_SRC}
  ${MYSQLX_PLUGIN_SRC}
  ADD_TEST xplugin
)

ADD_COMPILE_FLAGS(${unit_tests_SRC} ${MYSQLX_PLUGIN_SRC}
  COMPILE_FLAGS "${MYSQLX_PROTOCOL_FLAGS}"
)

ADD_DEPENDENCIES(${unit_tests_EXE}
  ${MYSQLX_CLIENT_LIB}
  ${MYSQLX_PROTOCOL_LIB}
  xprotocol_tags
)

# New warning in Visual Studio 2008
# '%$S': virtual function overrides '%$pS', previous versions of the compiler
# did not override when parameters only differed by const/volatile qualifiers
IF(WIN32)
  SET_TARGET_PROPERTIES(${unit_tests_EXE} PROPERTIES COMPILE_FLAGS "/wd4373")
ENDIF(WIN32)

# assertion failed in function assert_with_dumps() @ ssa.c:621 when using -xO3
# Segfault in the unit test, so lower optimization level there as well
IF(CMAKE_CXX_COMPILER_ID MATCHES "SunPro")
  ADD_COMPILE_FLAGS(
    ${CMAKE_CURRENT_SOURCE_DIR}/xpl/callback_command_delegate_t.cc
    ${MYSQLX_PROJECT_DIR}/ngs/src/client.cc
    COMPILE_FLAGS "-xO2")
ENDIF()

TARGET_LINK_LIBRARIES(${unit_tests_EXE}
  gtest
  regex
  gmock
  mysqlservices
  mysqlclient
  ${MYSQLX_CLIENT_LIB}
  ${MYSQLX_PROTOCOL_LIB}
  ${LIBEVENT_LIBRARY}
  ${LIBEVENT_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${PROTOBUF_LIBRARY}
  ${GCOV_LDFLAGS}
)

