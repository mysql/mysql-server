# Copyright (c) 2021, 2024, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Include(GenerateExportHeader)

# get the INCLUDE_DIRECTORIES manually, as cmake 3.6.x will fail if
#
#   $<TARGET_PROPERTY,mysqlxmessages:INCLUDE_DIRECTORIES>
#
# is used in TARGET_INCLUDE_DIRECTORIES().

GET_TARGET_PROPERTY(XMSG_INC_DIRS mysqlxmessages_shared
  INCLUDE_DIRECTORIES)
GET_TARGET_PROPERTY(XMSG_SYS_INC_DIRS mysqlxmessages_shared
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
# get the sources from 'mysqlxmessages' instead of
# the shared version to get the dependencies lined up properly.
GET_TARGET_PROPERTY(XMSG_SOURCES mysqlxmessages
  SOURCES)
GET_TARGET_PROPERTY(XMSG_COMPILE_DEFS mysqlxmessages_shared
  INTERFACE_COMPILE_DEFINITIONS)
GET_TARGET_PROPERTY(XMSG_COMPILE_OPTS mysqlxmessages_shared
  INTERFACE_COMPILE_OPTIONS)

SET_SOURCE_FILES_PROPERTIES(${XMSG_SOURCES}
  PROPERTIES GENERATED TRUE)

ROUTER_ADD_SHARED_LIBRARY(router_mysqlxmessages
  ${XMSG_SOURCES}

  NO_EXPORT_HEADER
  OUTPUT_NAME "mysqlrouter_mysqlxmessages"
  # depend on mysqlxmessages to generate all the .pb.cc files.
  DEPENDENCIES mysqlxmessages

  LINK_LIBRARIES
  PUBLIC
  ext::libprotobuf
  )

TARGET_INCLUDE_DIRECTORIES(router_mysqlxmessages
  PUBLIC ${XMSG_INC_DIRS}
  )
TARGET_INCLUDE_DIRECTORIES(router_mysqlxmessages
  SYSTEM PUBLIC ${XMSG_SYS_INC_DIRS}
  )
TARGET_COMPILE_DEFINITIONS(router_mysqlxmessages
  PUBLIC ${XMSG_COMPILE_DEFS}
  PRIVATE mysqlxmessages_shared_EXPORTS=1
  )
TARGET_COMPILE_OPTIONS(router_mysqlxmessages
  PUBLIC ${XMSG_COMPILE_OPTS}
  )
# See MY_PROTOBUF_FLAGS in MYSQL_PROTOBUF_GENERATE_CPP_LIBRARY
IF(MY_COMPILER_IS_GNU_OR_CLANG AND NOT WIN32_CLANG)
  MY_CHECK_CXX_COMPILER_WARNING("-Wshadow-field" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    TARGET_COMPILE_OPTIONS(router_mysqlxmessages PRIVATE ${HAS_WARN_FLAG})
  ENDIF()
ENDIF()

# with LTO:
#
# plugin/x/generated/protobuf/mysqlx_prepare.pb.cc:
# In member function 'InternalSwap':
# lto1: error: writing 16 bytes into a region of size 0 [-Werror=stringop-overflow=]
# plugin/x/generated/protobuf/mysqlx_prepare.pb.h:377:27:
# note: at offset 16 into destination object 'find_' of size 8
#
IF((WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO) AND MY_COMPILER_IS_GNU)
  TARGET_LINK_OPTIONS(router_mysqlxmessages PRIVATE
    -Wno-error=stringop-overflow)
ENDIF()

## rebuild mysqlxclient as router_mysqlxclient

GET_TARGET_PROPERTY(XCLI_INC_DIRS mysqlxclient_shared
  INCLUDE_DIRECTORIES)
GET_TARGET_PROPERTY(XCLI_SYS_INC_DIRS mysqlxclient_shared
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
GET_TARGET_PROPERTY(XCLI_SOURCES mysqlxclient_shared
  SOURCES)
GET_TARGET_PROPERTY(XCLI_SOURCE_DIR mysqlxclient_shared
  SOURCE_DIR)

SET(XCLI_SOURCE_ABSOLUTE)
FOREACH(F ${XCLI_SOURCES})
  IF(IS_ABSOLUTE ${F})
    LIST(APPEND XCLI_SOURCES_ABSOLUTE ${F})
  ELSE()
    LIST(APPEND XCLI_SOURCES_ABSOLUTE ${XCLI_SOURCE_DIR}/${F})
  ENDIF()
ENDFOREACH()

ROUTER_ADD_SHARED_LIBRARY(router_mysqlxclient
  ${XCLI_SOURCES_ABSOLUTE}

  NO_EXPORT_HEADER
  OUTPUT_NAME "mysqlrouter_mysqlxclient"
  DEPENDENCIES GenError

  LINK_LIBRARIES
  PUBLIC
  router_mysqlxmessages
  ext::lz4
  ext::zstd
  ext::zlib
  OpenSSL::SSL
  OpenSSL::Crypto
  router_mysqlclient
  )

TARGET_INCLUDE_DIRECTORIES(router_mysqlxclient
  PUBLIC ${XCLI_INC_DIRS}
  )
TARGET_INCLUDE_DIRECTORIES(router_mysqlxclient
  SYSTEM PUBLIC ${XCLI_SYS_INC_DIRS}
  )
TARGET_COMPILE_DEFINITIONS(router_mysqlxclient
  PUBLIC USE_MYSQLX_FULL_PROTO=1
  PRIVATE mysqlxclient_shared_EXPORTS=1
  )
