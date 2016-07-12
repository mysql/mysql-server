# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

IF(MSVC)
  SET(MYSQLX_PROTOBUF_MSVC_DISABLED_WARNINGS "/wd4267 /wd4244")
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

    LIST(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.cc")
    LIST(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.h")
    LIST(APPEND ${SRCS_LITE}
      "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${FIL_WE}.pb.cc")
    LIST(APPEND ${HDRS_LITE}
      "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${FIL_WE}.pb.h")

    ADD_CUSTOM_COMMAND(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.h"
      COMMAND ${CMAKE_COMMAND}
            -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf"
           -I "${CMAKE_CURRENT_SOURCE_DIR}/protocol" ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE}
      COMMENT "Running C++ protocol buffer compiler on ${FIL}"
      VERBATIM)
    
    ADD_CUSTOM_COMMAND(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${FIL_WE}.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${FIL_WE}.pb.h"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite"
           -I "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite"
           "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${FIL_WE}.proto"
      DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${FIL_WE}.proto"
              ${PROTOBUF_PROTOC_EXECUTABLE} GenLiteProtos
      COMMENT "Running C++ protocol buffer compiler (lite) on ${FIL}"
      VERBATIM)
  ENDFOREACH()

  SET_SOURCE_FILES_PROPERTIES(
    ${${SRCS}} ${${HDRS}} ${${SRCS_LITE}} ${${HDRS_LITE}}
    PROPERTIES GENERATED TRUE)
  
  IF(MSVC)
    ADD_COMPILE_FLAGS(${${SRCS}} ${${SRCS_LITE}}
      COMPILE_FLAGS ${MYSQLX_PROTOBUF_MSVC_DISABLED_WARNINGS})
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
    LIST(APPEND ${SRC_NAMES}
      "${CMAKE_BINARY_DIR}/rapid/plugin/x/generated/protobuf/${FIL_WE}.pb.cc")
  ENDFOREACH()
  
  SET_SOURCE_FILES_PROPERTIES(${${SRC_NAMES}} PROPERTIES GENERATED TRUE)
  
  IF(MSVC)
    ADD_COMPILE_FLAGS(${${SRC_NAMES}}
      COMPILE_FLAGS ${MYSQLX_PROTOBUF_MSVC_DISABLED_WARNINGS})
  ENDIF()
  
  SET(${SRC_NAMES} ${${SRC_NAMES}} PARENT_SCOPE)
ENDFUNCTION()
