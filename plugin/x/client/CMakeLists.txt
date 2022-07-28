# Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

DISABLE_MISSING_PROFILE_WARNING()

SET(CLIENT_FULL_LIB_NAME mysqlxclient)
SET(CLIENT_LITE_LIB_NAME mysqlxclient_lite)

INCLUDE(source_files.cmake)
IF(HAVE_SETNS)
  LIST(APPEND CLIENT_SRC "${CMAKE_SOURCE_DIR}/sql-common/net_ns.cc")
ENDIF()

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)

SET(XCLIENT_FLAGS "")

ADD_COMPILE_FLAGS(${CLIENT_SRC}
  COMPILE_FLAGS
  "${XCLIENT_FLAGS}"
  "${MYSQLX_PROTOCOL_FLAGS}"
)

ADD_LIBRARY(${CLIENT_FULL_LIB_NAME} STATIC ${CLIENT_SRC})
ADD_DEPENDENCIES(${CLIENT_FULL_LIB_NAME} ${MYSQLX_PROTOCOL_FULL_LIB} GenError)

TARGET_LINK_LIBRARIES(${CLIENT_FULL_LIB_NAME} ${LZ4_LIBRARY})
TARGET_INCLUDE_DIRECTORIES(${CLIENT_FULL_LIB_NAME} SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/)
TARGET_INCLUDE_DIRECTORIES(${CLIENT_FULL_LIB_NAME} PRIVATE
  ${MYSQLX_GENERATE_DIR})
MYSQLX_APPEND_COMPILE_DEFINITIONS(${CLIENT_FULL_LIB_NAME}
  "USE_MYSQLX_FULL_PROTO")

ADD_LIBRARY(${CLIENT_LITE_LIB_NAME} STATIC ${CLIENT_SRC})
ADD_DEPENDENCIES(${CLIENT_LITE_LIB_NAME} ${MYSQLX_PROTOCOL_LITE_LIB} GenError)
TARGET_LINK_LIBRARIES(${CLIENT_LITE_LIB_NAME} ${LZ4_LIBRARY})
TARGET_INCLUDE_DIRECTORIES(${CLIENT_LITE_LIB_NAME} SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/)
TARGET_INCLUDE_DIRECTORIES(${CLIENT_LITE_LIB_NAME} PRIVATE
  ${MYSQLX_GENERATE_DIR})


SET(MYSQLX_CLIENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
SET(MYSQLX_CLIENT_FULL_LIB ${CLIENT_FULL_LIB_NAME} PARENT_SCOPE)
SET(MYSQLX_CLIENT_LITE_LIB ${CLIENT_LITE_LIB_NAME} PARENT_SCOPE)
