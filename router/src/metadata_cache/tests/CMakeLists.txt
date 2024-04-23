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

# The sources are re-built to test the metadata cache implementation with
# a store layer that can supply the metadata without an actual connection
# to the metadata node. In this case, mock_metadata.cc implements the metadata
# interface without implementing a connection to the underlying metadata node.
# The MockMetadata class does the functionality of the Metadata class.

SET(TEST_MODULE metadata_cache)
ADD_TEST_FILE(test_cache_plugin.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers
    router_mysqlxclient
    router_mysqlxmessages
    router_cluster    # ClusterMetadataDynamicState
  INCLUDE_DIRS
    ../src
    ${CMAKE_CURRENT_SOURCE_DIR}/helper
  EXTRA_SOURCES
    ../src/router_options.cc
    ../src/log_suppressor.cc
    ../src/cluster_metadata_ar.cc
    ../src/cluster_metadata_gr.cc
    ../src/cluster_metadata.cc
    ../src/metadata_cache.cc
    ../src/metadata_cache_ar.cc
    ../src/metadata_cache_gr.cc
    ../src/cache_api.cc
    ../src/gr_notifications_listener.cc
    ../src/group_replication_metadata.cc
    ../src/metadata_factory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/mock_metadata.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/mock_metadata_factory.cc
  )

ROUTERTEST_GET_TARGET(T "test_cache_plugin.cc" "${TEST_MODULE}")
TARGET_INCLUDE_DIRECTORIES(${T}
  PRIVATE
    $<TARGET_PROPERTY:metadata_cache,INCLUDE_DIRECTORIES>
  )
TARGET_COMPILE_DEFINITIONS(${T}
  PRIVATE
  METADATA_CACHE_PLUGIN_STATIC_DEFINE=1
  METADATA_CACHE_STATIC_DEFINE=1
  )


ADD_TEST_FILE(test_plugin_config.cc
  EXTRA_SOURCES
    ../src/plugin_config.cc
    ../src/router_options.cc
    ../src/log_suppressor.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    test-helpers    # init_test_logger
    router_cluster  # ClusterMetadataDynamicState
  )
ROUTERTEST_GET_TARGET(T "test_plugin_config.cc" "${TEST_MODULE}")
TARGET_INCLUDE_DIRECTORIES(${T}
  PRIVATE
    $<TARGET_PROPERTY:metadata_cache,INCLUDE_DIRECTORIES>
  )
TARGET_COMPILE_DEFINITIONS(${T}
  PRIVATE
  METADATA_CACHE_PLUGIN_STATIC_DEFINE=1
  METADATA_CACHE_STATIC_DEFINE=1
  )
