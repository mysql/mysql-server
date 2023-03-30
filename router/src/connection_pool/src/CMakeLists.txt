# Copyright (c) 2021, 2023, Oracle and/or its affiliates.
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

INCLUDE(GenerateExportHeader)

# connection pool library

ADD_LIBRARY(connection_pool SHARED
  connection_pool.cc
  connection_pool_component.cc
  )
TARGET_LINK_LIBRARIES(connection_pool
  PUBLIC
  harness_stdx
  harness_net_ts
  mysql_protocol
  PRIVATE
  ${SSL_LIBRARIES}
  )
TARGET_INCLUDE_DIRECTORIES(connection_pool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
GENERATE_EXPORT_HEADER(connection_pool
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/connection_pool_export.h
  )

SET_TARGET_PROPERTIES(connection_pool PROPERTIES
  OUTPUT_NAME "mysqlrouter_connection_pool"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

ADD_INSTALL_RPATH_FOR_OPENSSL(connection_pool)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(connection_pool)

INSTALL(TARGETS connection_pool
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

# connection pool plugin

ADD_HARNESS_PLUGIN(connection_pool_plugin
  OUTPUT_NAME "connection_pool"
  SOURCES
  connection_pool_plugin.cc
  REQUIRES
  router_lib
  harness_stdx
  connection_pool
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
TARGET_INCLUDE_DIRECTORIES(connection_pool_plugin PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )

GENERATE_EXPORT_HEADER(connection_pool_plugin
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/connection_pool_plugin_export.h
  )
