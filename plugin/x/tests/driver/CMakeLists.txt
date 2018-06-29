# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

SET(XTEST_EXE_NAME mysqlxtest)

FILE(GLOB XTEST_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/connector/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/formatters/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/parsers/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/processor/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/processor/commands/*.cc"
)

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
  ${MYSQLX_PROJECT_DIR}
  ${MYSQLX_PROJECT_DIR}/ngs/include
  ${MYSQLX_PROTOCOL_INCLUDE_DIR}
  ${MYSQLX_CLIENT_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

INCLUDE_DIRECTORIES(SYSTEM
  ${PROTOBUF_INCLUDE_DIRS}
  ${LIBEVENT_INCLUDE_DIR}
)

MYSQL_ADD_EXECUTABLE(${XTEST_EXE_NAME}
  ${XTEST_SRC} COMPONENT Test
)

ADD_DEPENDENCIES(${XTEST_EXE_NAME}
  GenError GenClientError
  ${MYSQLX_PROTOCOL_FULL_LIB}
  ${MYSQLX_CLIENT_FULL_LIB}
)

MYSQLX_APPEND_COMPILE_FLAGS(${XTEST_EXE_NAME}
  "${MYSQLX_PROTOCOL_FLAGS}"
)

MYSQLX_APPEND_COMPILE_DEFINITIONS(${XTEST_EXE_NAME}
  "NGS_DISABLE_LOGGING;USE_MYSQLX_FULL_PROTO"
)

TARGET_LINK_LIBRARIES(${XTEST_EXE_NAME}
  mysys
  strings
  mysqlclient
  ${MYSQLX_CLIENT_FULL_LIB}
  ${MYSQLX_PROTOCOL_FULL_LIB}
  ${PROTOBUF_LIBRARY}
)

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  MY_CHECK_CXX_COMPILER_FLAG("-Wunused-but-set-parameter" HAVE_UNUSED_BUT_SET)
  IF(HAVE_UNUSED_BUT_SET)
    ADD_COMPILE_FLAGS(${XTEST_SRC} COMPILE_FLAGS
      "-Wno-unused-but-set-parameter")
  ENDIF()
ENDIF()
