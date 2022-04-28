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

INCLUDE(source_files.cmake)

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
)

ADD_COMPILE_FLAGS(${PROTOC_PLUGIN_SRC}
  COMPILE_FLAGS "${MYSQLX_PROTOCOL_FLAGS}"
)

MYSQL_ADD_EXECUTABLE(
  xprotocol_plugin
  ${PROTOC_PLUGIN_SRC}
  SKIP_INSTALL
)

TARGET_LINK_LIBRARIES(
  xprotocol_plugin
  ${MYSQLX_PROTOCOL_FULL_LIB}
  ${PROTOBUF_PROTOC_LIBRARY}
  ${PROTOBUF_LIBRARY}
)

# Workaround for an issue present on Debian 5.3.7-1
# and gcc9.
#
# The problem is that the linker eliminates the dependency
# to libpthread.so, even presents of -pthread and -lpthread
# flags.
# This leaves some unresolved symbols which trigger
# exception while starting the ELF file.
#
# The issue is not visible while using gcc standard linker.
# It can be observed with gcc+gold and gcc+lld.
#
IF(MY_COMPILER_IS_GNU)
  MY_TARGET_LINK_OPTIONS(xprotocol_plugin "-Wl,--no-as-needed -lpthread")
ENDIF()

SET(PROTOC_PLUGIN_GENERATED_HDR
  "xprotocol_tags.h"
)

ADD_CUSTOM_COMMAND(
  OUTPUT "${MYSQLX_GENERATE_DIR}/${PROTOC_PLUGIN_GENERATED_HDR}"
  COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
  ARGS --plugin=protoc-gen-yplg=$<TARGET_FILE:xprotocol_plugin>
       --yplg_out "${MYSQLX_GENERATE_DIR}"
       -I "${PROTOBUF_INCLUDE_DIR}"
       -I "${MYSQLX_PROTOBUF_PROTO_DIR}"
       ${MYSQLX_PROTOBUF_PROTO_FILES}
  DEPENDS
  xprotocol_plugin
  COMMENT "Running protocol buffer plugin on all 'proto' files"
  VERBATIM
)

MY_ADD_CUSTOM_TARGET(xprotocol_tags
  DEPENDS
  xprotocol_plugin
  "${MYSQLX_GENERATE_DIR}/${PROTOC_PLUGIN_GENERATED_HDR}"
  SOURCES
  "${MYSQLX_GENERATE_DIR}/${PROTOC_PLUGIN_GENERATED_HDR}"
)
