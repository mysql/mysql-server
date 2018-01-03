# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

IF(MSVC)
  SET(MYSQLX_PROTOBUF_DISABLED_WARNINGS "/wd4267 /wd4244")
ENDIF()

# Protoc version 2.6.1 uses atomicops_internals_macosx.h on mac
# which calls OSAtomicAdd64Barrier etc.
IF(APPLE)
  SET(MYSQLX_PROTOBUF_DISABLED_WARNINGS "-Wno-deprecated-declarations")
ENDIF()

# Standard PROTOBUF_GENERATE_CPP modified to generate both
# protobuf and protobuf-lite C++ files.
FUNCTION(MYSQLX_PROTOBUF_GENERATE_CPP SRCS HDRS SRCS_LITE HDRS_LITE)
  IF(NOT ARGN)
    MESSAGE(SEND_ERROR
      "Error: MYSQLX_PROTOBUF_GENERATE_CPP() called without any proto files")
    RETURN()
  ENDIF()

  SET(${SRCS})
  SET(${HDRS})
  SET(${SRCS_LITE})
  SET(${HDRS_LITE})
  FOREACH(FIL ${ARGN})
    GET_FILENAME_COMPONENT(ABS_FIL ${FIL} ABSOLUTE)
    GET_FILENAME_COMPONENT(FIL_WE ${FIL} NAME_WE)

    LIST(APPEND ${SRCS} "${PROTOCOL_FULL_DIR}/${FIL_WE}.pb.cc")
    LIST(APPEND ${HDRS} "${PROTOCOL_FULL_DIR}/${FIL_WE}.pb.h")
    LIST(APPEND ${SRCS_LITE} "${PROTOCOL_LITE_DIR}/${FIL_WE}.pb.cc")
    LIST(APPEND ${HDRS_LITE} "${PROTOCOL_LITE_DIR}/${FIL_WE}.pb.h")

    ADD_CUSTOM_COMMAND(
      OUTPUT "${PROTOCOL_FULL_DIR}/${FIL_WE}.pb.cc"
             "${PROTOCOL_FULL_DIR}/${FIL_WE}.pb.h"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out "${PROTOCOL_FULL_DIR}"
           -I "${PROTOBUF_INCLUDE_DIR}"
           -I "${CMAKE_CURRENT_SOURCE_DIR}" ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE}
      COMMENT "Running C++ protocol buffer compiler on ${FIL}"
      VERBATIM)

    ADD_CUSTOM_COMMAND(
      OUTPUT "${PROTOCOL_LITE_DIR}/${FIL_WE}.pb.cc"
             "${PROTOCOL_LITE_DIR}/${FIL_WE}.pb.h"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out "${PROTOCOL_LITE_DIR}"
           -I "${PROTOCOL_LITE_DIR}"
           "${PROTOCOL_LITE_DIR}/${FIL_WE}.proto"
      DEPENDS "${PROTOCOL_LITE_DIR}/${FIL_WE}.proto"
              ${PROTOBUF_PROTOC_EXECUTABLE} GenLiteProtos
      COMMENT "Running C++ protocol buffer compiler (lite) on ${FIL}"
      VERBATIM)
  ENDFOREACH()

  SET_SOURCE_FILES_PROPERTIES(
    ${${SRCS}} ${${HDRS}} ${${SRCS_LITE}} ${${HDRS_LITE}}
    PROPERTIES GENERATED TRUE)

  IF(MSVC OR APPLE)
    ADD_COMPILE_FLAGS(${${SRCS}} ${${SRCS_LITE}}
      COMPILE_FLAGS ${MYSQLX_PROTOBUF_DISABLED_WARNINGS})
  ENDIF()

  SET(${SRCS} ${${SRCS}} PARENT_SCOPE)
  SET(${HDRS} ${${HDRS}} PARENT_SCOPE)
  SET(${SRCS_LITE} ${${SRCS_LITE}} PARENT_SCOPE)
  SET(${HDRS_LITE} ${${HDRS_LITE}} PARENT_SCOPE)
ENDFUNCTION()


# Generates protobuf .cc file names for a set of .proto files.
FUNCTION(MYSQLX_PROTOBUF_GENERATE_CPP_NAMES SRC_NAMES)
  IF(NOT ARGN)
    MESSAGE(SEND_ERROR
  "Error: MYSQLX_PROTOBUF_GENERATE_CPP_NAMES() called without any proto files")
    RETURN()
  ENDIF()

  SET(${SRC_NAMES})

  FOREACH(FIL ${ARGN})
    GET_FILENAME_COMPONENT(FIL_WE ${FIL} NAME_WE)
    LIST(APPEND ${SRC_NAMES} "${PROTOCOL_FULL_DIR}/${FIL_WE}.pb.cc")
  ENDFOREACH()

  SET_SOURCE_FILES_PROPERTIES(${${SRC_NAMES}} PROPERTIES GENERATED TRUE)

  IF(MSVC OR APPLE)
    ADD_COMPILE_FLAGS(${${SRC_NAMES}}
      COMPILE_FLAGS ${MYSQLX_PROTOBUF_DISABLED_WARNINGS})
  ENDIF()

  SET(${SRC_NAMES} ${${SRC_NAMES}} PARENT_SCOPE)
ENDFUNCTION()
