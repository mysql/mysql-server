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

INCLUDE(GenerateExportHeader)

# connection pool library

ROUTER_ADD_SHARED_LIBRARY(connection_pool
  connection_pool.cc
  connection_pool_component.cc

  OUTPUT_NAME "mysqlrouter_connection_pool"

  LINK_LIBRARIES
  PUBLIC
  harness_stdx
  harness_net_ts
  mysql_protocol
  routing_connections

  PRIVATE
  OpenSSL::SSL OpenSSL::Crypto
  )

# connection pool plugin

ADD_HARNESS_PLUGIN(connection_pool_plugin
  OUTPUT_NAME "connection_pool"
  SOURCES
  connection_pool_plugin.cc
  REQUIRES
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
