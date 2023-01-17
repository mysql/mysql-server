# Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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
ADD_HARNESS_PLUGIN(rest_metadata_cache
  SOURCES
  rest_metadata_cache_config.cc
  rest_metadata_cache_list.cc
  rest_metadata_cache_plugin.cc
  rest_metadata_cache_status.cc
  rest_clusters_list.cc
  rest_clusters_nodes.cc
  REQUIRES router_lib;metadata_cache;http_server;rest_api
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}")

TARGET_INCLUDE_DIRECTORIES(rest_metadata_cache
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${PROJECT_SOURCE_DIR}/src/router/include/
    ${PROJECT_SOURCE_DIR}/src/metadata_cache/include/
    ${PROJECT_SOURCE_DIR}/src/http/include/
    ${CMAKE_CURRENT_BINARY_DIR}
    )
