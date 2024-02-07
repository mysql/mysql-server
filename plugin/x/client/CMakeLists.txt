# Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

DISABLE_MISSING_PROFILE_WARNING()

INCLUDE(source_files.cmake)
IF(HAVE_SETNS)
  LIST(APPEND CLIENT_SRC "${CMAKE_SOURCE_DIR}/sql-common/net_ns.cc")
ENDIF()

SET(XCLIENT_FLAGS "")

ADD_COMPILE_FLAGS(${CLIENT_SRC}
  COMPILE_FLAGS
  "${XCLIENT_FLAGS}"
  "${MYSQLX_PROTOCOL_FLAGS}"
)

ADD_LIBRARY(mysqlxclient STATIC ${CLIENT_SRC})
ADD_DEPENDENCIES(mysqlxclient GenError)

TARGET_LINK_LIBRARIES(mysqlxclient PUBLIC ext::lz4 ext::zstd ext::zlib)
TARGET_LINK_LIBRARIES(mysqlxclient PUBLIC ${MYSQLX_PROTOCOL_FULL_LIB})
TARGET_INCLUDE_DIRECTORIES(mysqlxclient SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/)
TARGET_INCLUDE_DIRECTORIES(mysqlxclient PRIVATE
  ${MYSQLX_GENERATE_DIR})
MYSQLX_APPEND_COMPILE_DEFINITIONS(mysqlxclient
  "USE_MYSQLX_FULL_PROTO")

ADD_LIBRARY(mysqlxclient_lite STATIC ${CLIENT_SRC})
ADD_DEPENDENCIES(mysqlxclient_lite GenError)
ADD_DEPENDENCIES(mysqlxclient_lite ${MYSQLX_PROTOCOL_LITE_LIB})
TARGET_LINK_LIBRARIES(mysqlxclient_lite PUBLIC ext::lz4 ext::zstd ext::zlib)
# Do not link in libmysqlxmessages_lite.a here, we may get ODR violations
# in targets linking with both these static libraries.
# TARGET_LINK_LIBRARIES(mysqlxclient_lite PUBLIC ${MYSQLX_PROTOCOL_LITE_LIB})
TARGET_LINK_LIBRARIES(mysqlxclient_lite PUBLIC ext::libprotobuf-lite)
TARGET_INCLUDE_DIRECTORIES(mysqlxclient_lite SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/)
TARGET_INCLUDE_DIRECTORIES(mysqlxclient_lite PRIVATE
  ${MYSQLX_GENERATE_DIR})

IF(WITH_PROTOBUF STREQUAL "bundled")
  TARGET_INCLUDE_DIRECTORIES(mysqlxclient SYSTEM BEFORE PUBLIC
    "${BUNDLED_ABSEIL_SRCDIR}")
  TARGET_INCLUDE_DIRECTORIES(mysqlxclient_lite SYSTEM BEFORE PUBLIC
    "${BUNDLED_ABSEIL_SRCDIR}")
ENDIF()

SET(MYSQLX_CLIENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
SET(MYSQLX_CLIENT_FULL_LIB mysqlxclient PARENT_SCOPE)
SET(MYSQLX_CLIENT_LITE_LIB mysqlxclient_lite PARENT_SCOPE)
