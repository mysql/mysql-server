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

INCLUDE(GenerateExportHeader)

DISABLE_MISSING_PROFILE_WARNING()

IF(WIN32_CLANG)
  # Clang complains that classes to be DLLexported need an explicit
  # copy assignment operator.
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated-copy-with-dtor")
ENDIF()

INCLUDE(source_files.cmake)
IF(HAVE_SETNS)
  LIST(APPEND CLIENT_SRC "${CMAKE_SOURCE_DIR}/sql-common/net_ns.cc")
ENDIF()

ADD_LIBRARY(mysqlxclient STATIC ${CLIENT_SRC})

ADD_DEPENDENCIES(mysqlxclient GenError)

TARGET_LINK_LIBRARIES(mysqlxclient PUBLIC ext::lz4 ext::zstd ext::zlib)
TARGET_LINK_LIBRARIES(mysqlxclient PUBLIC ${MYSQLX_PROTOCOL_FULL_LIB})
TARGET_INCLUDE_DIRECTORIES(mysqlxclient SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${CMAKE_CURRENT_BINARY_DIR})
TARGET_INCLUDE_DIRECTORIES(mysqlxclient PRIVATE
  ${MYSQLX_GENERATE_DIR})
TARGET_COMPILE_DEFINITIONS(mysqlxclient PUBLIC
  USE_MYSQLX_FULL_PROTO
  MYSQLXCLIENT_STATIC_DEFINE
  )

# mysqlxclient - shared

# create shared lib that's not built by default. It is specified
# to allow GENERATE_EXPORT_HEADER create the right dllexport() declarations
# and provided router_mysqlxclient a template.
ADD_LIBRARY(mysqlxclient_shared SHARED
  EXCLUDE_FROM_ALL
  ${CLIENT_SRC})
GENERATE_EXPORT_HEADER(mysqlxclient_shared
  BASE_NAME mysqlxclient)

ADD_DEPENDENCIES(mysqlxclient_shared GenError)

TARGET_LINK_LIBRARIES(mysqlxclient_shared PUBLIC ext::lz4 ext::zstd ext::zlib)
TARGET_LINK_LIBRARIES(mysqlxclient_shared PUBLIC ${MYSQLX_PROTOCOL_FULL_LIB})
TARGET_LINK_LIBRARIES(mysqlxclient_shared PRIVATE mysys vio)
TARGET_INCLUDE_DIRECTORIES(mysqlxclient_shared SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${CMAKE_CURRENT_BINARY_DIR}/  # for the mysqlxclient_export.h
  )
TARGET_INCLUDE_DIRECTORIES(mysqlxclient_shared PRIVATE
  ${MYSQLX_GENERATE_DIR})
TARGET_COMPILE_DEFINITIONS(mysqlxclient_shared PUBLIC
  USE_MYSQLX_FULL_PROTO)

ADD_LIBRARY(mysqlxclient_lite STATIC ${CLIENT_SRC})
ADD_DEPENDENCIES(mysqlxclient_lite GenError)
ADD_DEPENDENCIES(mysqlxclient_lite ${MYSQLX_PROTOCOL_LITE_LIB})
TARGET_LINK_LIBRARIES(mysqlxclient_lite PUBLIC ext::lz4 ext::zstd ext::zlib)
# Do not link in libmysqlxmessages_lite.a here, we may get ODR violations
# in targets linking with both these static libraries.
# TARGET_LINK_LIBRARIES(mysqlxclient_lite PUBLIC ${MYSQLX_PROTOCOL_LITE_LIB})
TARGET_LINK_LIBRARIES(mysqlxclient_lite PUBLIC ext::libprotobuf-lite)
TARGET_INCLUDE_DIRECTORIES(mysqlxclient_lite SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${CMAKE_CURRENT_BINARY_DIR}/  # for the mysqlxclient_export.h
  )
TARGET_INCLUDE_DIRECTORIES(mysqlxclient_lite PRIVATE
  ${MYSQLX_GENERATE_DIR})
TARGET_COMPILE_DEFINITIONS(mysqlxclient_lite PUBLIC
  MYSQLXCLIENT_STATIC_DEFINE
  )


IF(WITH_PROTOBUF STREQUAL "bundled")
  TARGET_INCLUDE_DIRECTORIES(mysqlxclient SYSTEM BEFORE PUBLIC
    "${BUNDLED_ABSEIL_SRCDIR}")
  TARGET_INCLUDE_DIRECTORIES(mysqlxclient_lite SYSTEM BEFORE PUBLIC
    "${BUNDLED_ABSEIL_SRCDIR}")
ENDIF()

# See MYSQL_PROTOBUF_GENERATE_CPP_LIBRARY()
IF(MSVC AND NOT WIN32_CLANG)
  TARGET_COMPILE_OPTIONS(mysqlxclient PRIVATE /wd4251 /wd4091)
  TARGET_COMPILE_OPTIONS(mysqlxclient_lite PRIVATE /wd4251 /wd4091)
ENDIF()


SET(MYSQLX_CLIENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
SET(MYSQLX_CLIENT_FULL_LIB mysqlxclient PARENT_SCOPE)
SET(MYSQLX_CLIENT_LITE_LIB mysqlxclient_lite PARENT_SCOPE)
