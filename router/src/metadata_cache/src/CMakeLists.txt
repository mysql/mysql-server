# Copyright (c) 2016, 2024, Oracle and/or its affiliates.
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

ADD_LIBRARY(metadata_cache
  SHARED
  metadata_factory.cc
  cluster_metadata.cc
  cluster_metadata_ar.cc
  cluster_metadata_gr.cc
  metadata_cache.cc
  metadata_cache_ar.cc
  metadata_cache_gr.cc
  cache_api.cc
  group_replication_metadata.cc
  gr_notifications_listener.cc
  log_suppressor.cc
  router_options.cc
)
TARGET_COMPILE_FEATURES(metadata_cache PUBLIC cxx_std_20)
TARGET_LINK_LIBRARIES(metadata_cache
  PUBLIC
  harness_stdx
  router_lib
  mysqlxclient_lite
  router_mysqlxmessages
  PRIVATE
    extra::rapidjson
  )
TARGET_INCLUDE_DIRECTORIES(metadata_cache
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
GENERATE_EXPORT_HEADER(metadata_cache
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/metadata_cache_export.h
  )
TARGET_COMPILE_DEFINITIONS(metadata_cache PRIVATE
  MYSQL_ROUTER_LOG_DOMAIN="metadata_cache")

SET_TARGET_PROPERTIES(metadata_cache PROPERTIES
  OUTPUT_NAME "mysqlrouter_metadata_cache"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
INSTALL(TARGETS metadata_cache
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )
ADD_INSTALL_RPATH_FOR_OPENSSL(metadata_cache)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(metadata_cache)


add_harness_plugin(metadata_cache_plugin
  OUTPUT_NAME "metadata_cache"
  SOURCES
  metadata_cache_plugin.cc
  plugin_config.cc
  REQUIRES
  router_lib
  metadata_cache
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
TARGET_LINK_LIBRARIES(metadata_cache_plugin PRIVATE extra::rapidjson)
GENERATE_EXPORT_HEADER(metadata_cache_plugin
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/metadata_cache_plugin_export.h
  )
