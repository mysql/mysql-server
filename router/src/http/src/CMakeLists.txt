# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

ADD_LIBRARY(http_common
  SHARED
  http_common.cc
  http_time.cc
  )
TARGET_LINK_LIBRARIES(http_common
  ${LIBEVENT_LIBRARIES})

SET_TARGET_PROPERTIES(http_common PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

ADD_HARNESS_PLUGIN(http_server
  NO_INSTALL
  SOURCES http_server_plugin.cc
  static_files.cc
  http_server_component.cc
  REQUIRES router_lib;http_common;${LIBRT})

ADD_LIBRARY(http_client
  SHARED
  http_client.cc
  rest_client.cc
  )
TARGET_LINK_LIBRARIES(http_client
  ${LIBEVENT_LIBRARIES}
  ${LIBRT}
  http_common)

SET_TARGET_PROPERTIES(http_client PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

IF(WIN32)
  ADD_CUSTOM_COMMAND(TARGET http_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/http_client.dll"
    "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/http_client.dll"
   )

   ADD_CUSTOM_COMMAND(TARGET http_common POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/http_common.dll"
    "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/http_common.dll"
   )
ENDIF()

ADD_EXECUTABLE(rest_cli
  rest_cli.cc)
TARGET_LINK_LIBRARIES(rest_cli
  harness-library
  http_client)

INCLUDE_DIRECTORIES(${LIBEVENT_INCLUDE_DIR}/;${LIBEVENT_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/harness/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/ext/rapidjson/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/router/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/metadata_cache/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/http/include/)
