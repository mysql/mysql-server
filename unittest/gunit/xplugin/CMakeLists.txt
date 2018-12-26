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

IF(WITHOUT_MYSQLX)
  RETURN()
ENDIF()

INCLUDE(CTest)

SET(MYSQLX_PROJECT_DIR "${PROJECT_SOURCE_DIR}/plugin/x")

GET_DIRECTORY_PROPERTY(MYSQLX_CLIENT_LIB
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_CLIENT_LIB
)

GET_DIRECTORY_PROPERTY(MYSQLX_PLUGIN_INCLUDE_DIRS
  DIRECTORY ${MYSQLX_PROJECT_DIR} INCLUDE_DIRECTORIES
)

# Protobuf 3 will generate #if PROTOBUF_INLINE_NOT_IN_HEADERS
# which may fail with -Werror=undef
ADD_DEFINITIONS(-DPROTOBUF_INLINE_NOT_IN_HEADERS=0)

INCLUDE_DIRECTORIES(SYSTEM
  ${PROTOBUF_INCLUDE_DIRS}
  ${GMOCK_INCLUDE_DIRS}
  ${GTEST_INCLUDE_DIR}
)

INCLUDE_DIRECTORIES(
  ${MYSQLX_PLUGIN_INCLUDE_DIRS}
)

SET(XPL_UNIT_TESTS xplugin_unit_tests)

FILE(GLOB XPL_TEST_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xpl/stubs/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xpl/mysqlx_pb_wrapper.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xpl/*_t.cc"
)

MYSQL_ADD_EXECUTABLE(${XPL_UNIT_TESTS}
  ${XPL_TEST_SRC}
  ADD_TEST xplugin
)

ADD_COMPILE_FLAGS(${XPL_TEST_SRC}
  COMPILE_FLAGS "${MYSQLX_PROTOCOL_FLAGS}"
)

IF(DISABLE_MYSQLX_PROTOBUF_LITE)
  ADD_COMPILE_FLAGS(${XPL_TEST_SRC}
    COMPILE_FLAGS "-DUSE_MYSQLX_FULL_PROTO")
ENDIF()

ADD_DEPENDENCIES(${XPL_UNIT_TESTS}
  mysqlx
)

# New warning in Visual Studio 2008
# '%$S': virtual function overrides '%$pS', previous versions of the compiler
# did not override when parameters only differed by const/volatile qualifiers
IF(WIN32)
  SET_TARGET_PROPERTIES(${XPL_UNIT_TESTS} PROPERTIES COMPILE_FLAGS "/wd4373")
ENDIF(WIN32)

# assertion failed in function assert_with_dumps() @ ssa.c:621 when using -xO3
# Segfault in the unit test, so lower optimization level there as well
IF(CMAKE_CXX_COMPILER_ID MATCHES "SunPro")
  ADD_COMPILE_FLAGS(
    ${CMAKE_CURRENT_SOURCE_DIR}/xpl/callback_command_delegate_t.cc
    COMPILE_FLAGS "-xO2")
ENDIF()

TARGET_LINK_LIBRARIES(${XPL_UNIT_TESTS}
  ${GCOV_LDFLAGS}
  ${MYSQLX_CLIENT_LIB}
  mysqlx
  mysqlclient
  gtest
  gmock
)


# --== XClient unit tests (always full version) ==--

GET_DIRECTORY_PROPERTY(MYSQLX_CLIENT_FULL_LIB
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_CLIENT_FULL_LIB
)

GET_DIRECTORY_PROPERTY(MYSQLX_PROTOCOL_FULL_LIB
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_PROTOCOL_FULL_LIB
)

SET(XCL_UNIT_TESTS xclient_unit_tests)

FILE(GLOB XCL_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xcl/*_t.cc"
)

ADD_COMPILE_FLAGS(${XCL_SRC}
  COMPILE_FLAGS "-DUSE_MYSQLX_FULL_PROTO"
)

MYSQL_ADD_EXECUTABLE(${XCL_UNIT_TESTS}
  ${XCL_SRC}
  ADD_TEST xclient
)

ADD_DEPENDENCIES(${XCL_UNIT_TESTS}
  ${MYSQLX_CLIENT_FULL_LIB}
  ${MYSQLX_PROTOCOL_FULL_LIB}
  xprotocol_tags
)

TARGET_LINK_LIBRARIES(${XCL_UNIT_TESTS}
  ${GCOV_LDFLAGS}
  ${MYSQLX_CLIENT_FULL_LIB}
  ${MYSQLX_PROTOCOL_FULL_LIB}
  ${PROTOBUF_LIBRARY}
  mysqlclient
  gtest
  gmock
)
