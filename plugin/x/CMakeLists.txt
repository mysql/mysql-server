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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA


OPTION(WITH_MYSQLX "Build MySQL X plugin" ON)

SET(MYSQLX_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SET(MYSQLX_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
SET(MYSQLX_PLUGIN_NAME "mysqlx")

IF(WIN32)
  # Activate necessary dllexport/dllimport declarations.
  ADD_DEFINITIONS(-DPROTOBUF_USE_DLLS)
ENDIF()

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="mysqlx")

INCLUDE(src/source_files.cmake)
FOREACH(FILE ${XPL_SRC})
  LIST(APPEND XPLUGIN_SRC "src/${FILE}")
ENDFOREACH()

SET(XPLUGIN_LOG_PROTOBUF 1 CACHE STRING "Enable logging of protobuf messages")

INCLUDE(utils.cmake)
INCLUDE(variables.cmake)
INCLUDE(configure.cmake)

ADD_SUBDIRECTORY(protocol)
ADD_SUBDIRECTORY(client)

# MYSQL_ADD_PLUGIN may have decided *not* to build mysqlx
IF(WITH_MYSQLX)
  ADD_SUBDIRECTORY(tests)
ENDIF()

OPTION(WITH_MYSQLX_USE_PROTOBUF_FULL
  "Link mysqlx plugin with agains full version of \
protobuf/mysqlxmessages/mysqlxclient for better debugging"
  OFF
)

IF(NOT WITH_MYSQLX_USE_PROTOBUF_FULL AND NOT PROTOBUF_LITE_LIBRARY)
  SET(WITH_MYSQLX_USE_PROTOBUF_FULL ON)
  MESSAGE(WARNING "No protobuf-lite found, using full protobuf \
library for mysqlx plugin.")
ENDIF()

IF(WITH_MYSQLX_USE_PROTOBUF_FULL)
  SET(MYSQLX_PROTOBUF_LIB ${PROTOBUF_LIBRARY})
  SET(MYSQLX_PROTOCOL_LIB ${MYSQLX_PROTOCOL_FULL_LIB})
  SET(MYSQLX_CLIENT_LIB ${MYSQLX_CLIENT_FULL_LIB})
ELSE()
  SET(MYSQLX_PROTOBUF_LIB ${PROTOBUF_LITE_LIBRARY})
  SET(MYSQLX_PROTOCOL_LIB ${MYSQLX_PROTOCOL_LITE_LIB})
  SET(MYSQLX_CLIENT_LIB ${MYSQLX_CLIENT_LITE_LIB})
ENDIF()

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
  ${MYSQLX_PROTOCOL_INCLUDE_DIR}
  ${MYSQLX_CLIENT_INCLUDE_DIR}
)

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)
MY_INCLUDE_SYSTEM_DIRECTORIES(ICU)

ADD_COMPILE_FLAGS(${XPLUGIN_SRC}
  COMPILE_FLAGS "${MYSQLX_PROTOCOL_FLAGS}"
)

# MYSQL_ADD_PLUGIN may have disabled the plugin.
IF(WITH_MYSQLX)
  MYSQL_ADD_PLUGIN(${MYSQLX_PLUGIN_NAME}
    ${XPLUGIN_SRC}
    DEFAULT
    DEPENDENCIES xprotocol_tags
    LINK_LIBRARIES
    ${MYSQLX_PROTBUF_AND_PROTOCOL_LIB}
    ${LIBEVENT_LIBRARIES}
    ${ICU_LIBRARIES}
    ${LZ4_LIBRARY}
    ${MYSQLX_PROTOCOL_LIB}
    ${MYSQLX_PROTOBUF_LIB}
    )
ENDIF()

IF(XPLUGIN_LOG_PROTOBUF)
  MESSAGE(STATUS "MYSQLX - Text log of protobuf messages enabled")
  ADD_DEFINITIONS(-DXPLUGIN_LOG_PROTOBUF)
ENDIF()

IF(XPLUGIN_LOG_DEBUG OR WITH_DEBUG)
  MESSAGE(STATUS "MYSQLX - Enabled debug traces")
  ADD_DEFINITIONS(-DXPLUGIN_LOG_DEBUG)
ENDIF()
