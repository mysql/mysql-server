# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

FILE(GLOB protoc_plugin_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/xprotocol_plugin.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/message_field_chain.cc"
)

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

# Protoc version 2.6.1 generates calls to
# OSAtomicCompareAndSwap64 and others which are deprecated.
IF(APPLE)
  ADD_COMPILE_FLAGS(
    "${CMAKE_CURRENT_SOURCE_DIR}/message_field_chain.cc"
    COMPILE_FLAGS "-Wno-deprecated-declarations")
ENDIF()

ADD_COMPILE_FLAGS(${protoc_plugin_SRC}
  COMPILE_FLAGS "${PUBLIC_XPROTOCOL_FLAGS}"
)

MYSQL_ADD_EXECUTABLE(
  xprotocol_plugin
  ${protoc_plugin_SRC}
  SKIP_INSTALL
)

TARGET_LINK_LIBRARIES(
  xprotocol_plugin
  ${PROTOCOL_FULL_LIB_NAME}
  ${PROTOBUF_PROTOC_LIBRARY}
  ${PROTOBUF_LIBRARY}
)

SET(PROTOC_PLUGIN_GENERATED_HDR
  "xprotocol_tags.h"
)

ADD_CUSTOM_COMMAND(
  OUTPUT "${MYSQLX_GENERATE_DIR}/${PROTOC_PLUGIN_GENERATED_HDR}"
  COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
  ARGS --plugin=protoc-gen-yplg=$<TARGET_FILE:xprotocol_plugin>
       --yplg_out "${MYSQLX_GENERATE_DIR}"
       -I "${PROTOBUF_INCLUDE_DIR}"
       -I "${MYSQLX_PROTOBUF_INCLUDE_DIR}"
       ${MYSQLX_PROTOBUF_FILES}
  DEPENDS
  xprotocol_plugin
  COMMENT "Running protocol buffer plugin on all 'proto' files"
  VERBATIM
)

ADD_CUSTOM_TARGET(xprotocol_tags
  DEPENDS
  xprotocol_plugin
  "${MYSQLX_GENERATE_DIR}/${PROTOC_PLUGIN_GENERATED_HDR}"
  SOURCES
  "${MYSQLX_GENERATE_DIR}/${PROTOC_PLUGIN_GENERATED_HDR}"
)
