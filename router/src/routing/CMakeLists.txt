# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

IF (NOT PROTOBUF_LITE_LIBRARY)
  MESSAGE(FATAL_ERROR "Routing plugin requires protobuf-lite library")
ENDIF()

SET(ROUTING_SOURCE_FILES_X_PROTOCOL
  ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/x_protocol.cc
)

SET(ROUTING_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mysql_routing.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/destination.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_metadata_cache.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_first_available.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_next_available.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_round_robin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/classic_protocol.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/connection.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/context.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mysql_routing_common.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/connection_container.cc
  ${ROUTING_SOURCE_FILES_X_PROTOCOL}
)

SET(ROUTING_PLUGIN_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing_plugin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin_config.cc
)

SET(include_dirs
  ${PROJECT_SOURCE_DIR}/src/router/include
  ${PROJECT_SOURCE_DIR}/src/routing/include
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
  ${PROJECT_SOURCE_DIR}/src/mysql_protocol/include
)

SET(system_include_dirs
  ${MYSQLX_GENERATE_DIR}/protobuf_lite
)

# link_directories(${PROJECT_BINARY_DIR}/ext/protobuf/protobuf-3.0.0/cmake/)
# The Plugin
add_harness_plugin(routing
 SOURCES ${ROUTING_PLUGIN_SOURCE_FILES} ${ROUTING_SOURCE_FILES}
 REQUIRES mysql_protocol metadata_cache
 DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
 )
TARGET_INCLUDE_DIRECTORIES(routing PRIVATE ${include_dirs})
TARGET_INCLUDE_DIRECTORIES(routing SYSTEM PRIVATE ${system_include_dirs})
MY_TARGET_INCLUDE_SYSTEM_DIRECTORIES(routing PROTOBUF)
ADD_DEPENDENCIES(routing GenLiteProtos)

TARGET_LINK_LIBRARIES(routing PRIVATE mysqlxmessages_lite ${PROTOBUF_LITE_LIBRARY})

IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  TARGET_LINK_LIBRARIES(routing PRIVATE -lnsl PRIVATE -lsocket)
ENDIF()

# don't install headers until a) a final destination is found and b) API is stable
# FILE(GLOB routing_headers include/mysqlrouter/*.h)
# INSTALL(FILES ${routing_headers}
#   DESTINATION "include/mysql/${HARNESS_NAME}")

IF(WIN32)
  INSTALL(TARGETS routing 
    RUNTIME DESTINATION "lib"
    COMPONENT Router)
ENDIF()

IF(WITH_UNIT_TESTS AND GMOCK_FOUND)
  ADD_SUBDIRECTORY(tests/)
ENDIF()
