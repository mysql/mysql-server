# Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/protobuf.cmake)

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
  OUTPUT_DIRECTORY ${PROTOBUF_FULL_GENERATE_DIR})
TARGET_INCLUDE_DIRECTORIES(${PROTOBUF_FULL_LIB_NAME} SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${PROTOBUF_FULL_GENERATE_DIR}
  ${MYSQLX_GENERATE_DIR})
MYSQLX_APPEND_INTERFACE_COMPILE_DEFINITIONS(${PROTOBUF_FULL_LIB_NAME}
  "USE_MYSQLX_FULL_PROTO")

MYSQL_PROTOBUF_GENERATE_CPP_LIBRARY(
  ${PROTOBUF_LITE_LIB_NAME}
  ${GENERATED_LITE_PROTO_FILES}
  OUTPUT_DIRECTORY ${PROTOBUF_LITE_GENERATE_DIR}
  DEPENDENCIES GenLiteProtos
  ADDITIONAL_COMMENT "- lite version")
TARGET_INCLUDE_DIRECTORIES(${PROTOBUF_LITE_LIB_NAME} SYSTEM PUBLIC
  ${MYSQLX_PROJECT_DIR}/client/
  ${PROTOBUF_LITE_GENERATE_DIR}
  ${MYSQLX_GENERATE_DIR})


SET(MYSQLX_PROTOCOL_FLAGS "${MY_PUBLIC_PROTOBUF_FLAGS}" PARENT_SCOPE)
SET(MYSQLX_PROTOCOL_FULL_LIB ${PROTOBUF_FULL_LIB_NAME} PARENT_SCOPE)
SET(MYSQLX_PROTOCOL_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
SET(MYSQLX_PROTOCOL_LITE_LIB ${PROTOBUF_LITE_LIB_NAME} PARENT_SCOPE)
SET(MYSQLX_PROTOBUF_PROTO_DIR ${MYSQLX_PROTOBUF_PROTO_DIR} PARENT_SCOPE)
SET(MYSQLX_PROTOBUF_PROTO_FILES ${MYSQLX_PROTOBUF_PROTO_FILES} PARENT_SCOPE)
