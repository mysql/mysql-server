# Copyright (c) 2016, 2026, Oracle and/or its affiliates.
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

ADD_LIBRARY(metadata_cache_x_full STATIC gr_notifications_listener.cc)

TARGET_LINK_LIBRARIES(metadata_cache_x_full
  PRIVATE
  harness-library_interface
  harness_stdx_interface
  router_utils_interface
  routing_guidelines_interface
  router_mysqlxclient
  router_mysqlxmessages
  extra::rapidjson
)

TARGET_INCLUDE_DIRECTORIES(metadata_cache_x_full
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../include/)

TARGET_COMPILE_DEFINITIONS(metadata_cache_x_full
  PRIVATE METADATA_CACHE_STATIC_DEFINE=1)

ADD_LIBRARY(metadata_cache_x_lite STATIC gr_notifications_listener.cc)

TARGET_LINK_LIBRARIES(metadata_cache_x_lite
  PRIVATE
  harness-library_interface
  harness_stdx_interface
  router_utils_interface
  routing_guidelines_interface
  mysqlxclient_lite
  mysqlxmessages_lite
  extra::rapidjson
)

TARGET_INCLUDE_DIRECTORIES(metadata_cache_x_lite
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../include/)

TARGET_COMPILE_DEFINITIONS(metadata_cache_x_lite
  PRIVATE METADATA_CACHE_STATIC_DEFINE=1)

ROUTER_ADD_SHARED_LIBRARY(metadata_cache
  SOURCES
  metadata_factory.cc
  cluster_metadata.cc
  cluster_metadata_ar.cc
  cluster_metadata_gr.cc
  metadata_cache.cc
  metadata_cache_ar.cc
  metadata_cache_gr.cc
  cache_api.cc
  group_replication_metadata.cc
  log_suppressor.cc
  router_options.cc

  LINK_LIBRARIES_IF
  harness-library_interface
  harness_stdx_interface
  router_cluster_interface
  router_utils_interface
  routing_guidelines_interface
  extra::rapidjson

  LINK_LIBRARIES
  PUBLIC
  harness_stdx
  router_cluster
  metadata_cache_x_full
  router_utils
  routing_guidelines
  PRIVATE
  harness_tls
  extra::rapidjson

  OUTPUT_NAME "mysqlrouter_metadata_cache"

  COMPILE_DEFINITIONS
  MYSQL_ROUTER_LOG_DOMAIN="metadata_cache"
  )


add_harness_plugin(metadata_cache_plugin
  OUTPUT_NAME "metadata_cache"

  SOURCES
  metadata_cache_plugin.cc
  plugin_config.cc

  REQUIRES_IF
  metadata_cache_interface

  REQUIRES
  metadata_cache
  PRIVATE extra::rapidjson

  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
