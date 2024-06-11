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

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/protobuf.cmake)
INCLUDE(GenerateExportHeader)

SET(PROTOBUF_FULL_LIB_NAME mysqlxmessages)
SET(PROTOBUF_LITE_LIB_NAME mysqlxmessages_lite)

SET(PROTOBUF_FULL_GENERATE_DIR ${MYSQLX_GENERATE_DIR}/protobuf)
SET(PROTOBUF_LITE_GENERATE_DIR ${MYSQLX_GENERATE_DIR}/protobuf_lite)
FILE(MAKE_DIRECTORY ${PROTOBUF_FULL_GENERATE_DIR} ${PROTOBUF_LITE_GENERATE_DIR})
SET(MYSQLX_PROTOBUF_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR})

INCLUDE(source_files.cmake)
FOREACH(FILE ${MYSQLX_PROTOBUF_PROTO_SRC})
  LIST(APPEND MYSQLX_PROTOBUF_PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
ENDFOREACH()

SET(GENERATED_LITE_PROTO_FILES)
# Generate protobuf-lite versions of proto files.
FOREACH(PROTOBUF_FILE ${MYSQLX_PROTOBUF_PROTO_FILES})
  GET_FILENAME_COMPONENT(NAME_WD ${PROTOBUF_FILE} NAME)
  LIST(APPEND GENERATED_LITE_PROTO_FILES
    ${PROTOBUF_LITE_GENERATE_DIR}/${NAME_WD})
  ADD_CUSTOM_COMMAND(
    OUTPUT "${PROTOBUF_LITE_GENERATE_DIR}/${NAME_WD}"
    COMMAND ${CMAKE_COMMAND}
            -D TARGET_DIR="${PROTOBUF_LITE_GENERATE_DIR}"
            -D PROTO_FILE="${PROTOBUF_FILE}"
            -D PROTO_FILE_FLAGS="PROTOBUF_LITE"
            -P ${CMAKE_SOURCE_DIR}/cmake/protobuf_proto_transform_to_lite.cmake
    COMMENT "transform_to_lite ${PROTOBUF_FILE}"
    DEPENDS ${PROTOBUF_FILE})
  SET_SOURCE_FILES_PROPERTIES(
    "${PROTOBUF_LITE_GENERATE_DIR}/${NAME_WD}"
    PROPERTIES GENERATED TRUE)
ENDFOREACH()

# Be sure to convert all proto files to protobuf_lite before running protoc
MY_ADD_CUSTOM_TARGET(GenLiteProtos ALL
  DEPENDS ${GENERATED_LITE_PROTO_FILES})

MYSQL_PROTOBUF_GENERATE_CPP_LIBRARY(
  ${PROTOBUF_FULL_LIB_NAME}
  ${MYSQLX_PROTOBUF_PROTO_FILES}
  OUTPUT_DIRECTORY ${PROTOBUF_FULL_GENERATE_DIR}
  DLLEXPORT_DECL MYSQLXMESSAGES_EXPORT  # EXPORT_HEADER_NAME of GenerateExportHeader
  )
TARGET_LINK_LIBRARIES(${PROTOBUF_FULL_LIB_NAME} PUBLIC ext::libprotobuf)

TARGET_COMPILE_DEFINITIONS(${PROTOBUF_FULL_LIB_NAME}
  PUBLIC MYSQLXMESSAGES_STATIC_DEFINE
  )

# force the include of the export header.
IF(MSVC)
  TARGET_COMPILE_OPTIONS(mysqlxmessages
    PUBLIC /FI${CMAKE_CURRENT_BINARY_DIR}/mysqlxmessages_export.h)
ELSE()
  TARGET_COMPILE_OPTIONS(mysqlxmessages
    PUBLIC -include ${CMAKE_CURRENT_BINARY_DIR}/mysqlxmessages_export.h)
ENDIF()

TARGET_INCLUDE_DIRECTORIES(${PROTOBUF_FULL_LIB_NAME} SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${PROTOBUF_FULL_GENERATE_DIR}
  ${MYSQLX_GENERATE_DIR})
TARGET_COMPILE_DEFINITIONS(${PROTOBUF_FULL_LIB_NAME} PUBLIC
  MYSQLXMESSAGES_STATIC_DEFINE
  USE_MYSQLX_FULL_PROTO)

# create shared lib that's not built by default. It is specified
# to allow GENERATE_EXPORT_HEADER create the right dllexport() declarations
# and provided router_mysqlxmessages a template.
IF(APPLE)
  # For Xcode the ADD_LIBRARY below fails with
  # The custom command generating ..... is attached to multiple targets:
  #    mysqlxmessages  mysqlxmessages_shared
  # Rather than having a special implementation for Xcode only,
  # we do this for all builds (including make/ninja) on MacOS:
  MERGE_LIBRARIES_SHARED(mysqlxmessages_shared mysqlxmessages
    EXCLUDE_FROM_ALL
    SKIP_INSTALL
    )
ELSE()
  ADD_LIBRARY(mysqlxmessages_shared SHARED
    EXCLUDE_FROM_ALL
    $<TARGET_PROPERTY:mysqlxmessages,SOURCES>
    )
ENDIF()
SET_TARGET_PROPERTIES(mysqlxmessages_shared
  PROPERTIES
  EXCLUDE_FROM_ALL TRUE
  EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
GENERATE_EXPORT_HEADER(mysqlxmessages_shared
  BASE_NAME mysqlxmessages
  )

TARGET_COMPILE_DEFINITIONS(mysqlxmessages_shared
  PUBLIC
  USE_MYSQLX_FULL_PROTO)
TARGET_COMPILE_OPTIONS(mysqlxmessages_shared
  PUBLIC $<TARGET_PROPERTY:mysqlxmessages,INTERFACE_COMPILE_OPTIONS>)

# get the INCLUDE_DIRECTORIES manually, as cmake 3.6.x will fail if
#
#   $<TARGET_PROPERTY,mysqlxmessages:INCLUDE_DIRECTORIES>
#
# is used in TARGET_INCLUDE_DIRECTORIES().

GET_TARGET_PROPERTY(XMSG_INC_DIRS mysqlxmessages
  INCLUDE_DIRECTORIES)
GET_TARGET_PROPERTY(XMSG_SYS_INC_DIRS mysqlxmessages
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)

TARGET_INCLUDE_DIRECTORIES(mysqlxmessages_shared
  PRIVATE ${XMSG_INC_DIRS})
TARGET_INCLUDE_DIRECTORIES(mysqlxmessages_shared
  SYSTEM PUBLIC ${XMSG_SYS_INC_DIRS})


MYSQL_PROTOBUF_GENERATE_CPP_LIBRARY(
  ${PROTOBUF_LITE_LIB_NAME}
  ${GENERATED_LITE_PROTO_FILES}
  OUTPUT_DIRECTORY ${PROTOBUF_LITE_GENERATE_DIR}
  DEPENDENCIES GenLiteProtos
  ADDITIONAL_COMMENT "- lite version")
TARGET_LINK_LIBRARIES(${PROTOBUF_LITE_LIB_NAME} PUBLIC ext::libprotobuf-lite)

TARGET_INCLUDE_DIRECTORIES(${PROTOBUF_LITE_LIB_NAME} SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${PROTOBUF_LITE_GENERATE_DIR}
  ${MYSQLX_GENERATE_DIR})

TARGET_COMPILE_DEFINITIONS(${PROTOBUF_LITE_LIB_NAME}
  PUBLIC
  MYSQLXMESSAGES_STATIC_DEFINE
  )

SET(MYSQLX_PROTOCOL_FULL_LIB ${PROTOBUF_FULL_LIB_NAME} PARENT_SCOPE)
SET(MYSQLX_PROTOCOL_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
SET(MYSQLX_PROTOCOL_LITE_LIB ${PROTOBUF_LITE_LIB_NAME} PARENT_SCOPE)
SET(MYSQLX_PROTOBUF_PROTO_DIR ${MYSQLX_PROTOBUF_PROTO_DIR} PARENT_SCOPE)
SET(MYSQLX_PROTOBUF_PROTO_FILES ${MYSQLX_PROTOBUF_PROTO_FILES} PARENT_SCOPE)
