# Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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

# The sources are re-built to test the metadata cache implementation with
# a store layer that can supply the metadata without an actual connection
# to the metadata node. In this case, mock_metadata.cc implements the metadata
# interface without implementing a connection to the underlying metadata node.
# The MockMetadata class does the functionality of the Metadata class.

STATICLIB_FROM_TARGET(metadata_cache_export_all metadata_cache)
TARGET_COMPILE_DEFINITIONS(metadata_cache_export_all
  PUBLIC METADATA_CACHE_STATIC_DEFINE=1)

SET(TEST_MODULE metadata_cache)
FOREACH(test_file
    test_cache_plugin.cc
    test_instance_attributes.cc
    )
  ADD_TEST_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
    MODULE ${TEST_MODULE}
    LIB_DEPENDS
      metadata_cache_export_all
      test-helpers
    INCLUDE_DIRS
      ../src
      ${CMAKE_CURRENT_SOURCE_DIR}/helper
    EXTRA_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/helper/mock_metadata.cc
      ${CMAKE_CURRENT_SOURCE_DIR}/helper/mock_metadata_factory.cc
)
ENDFOREACH()

ADD_TEST_FILE(test_plugin_config.cc
  EXTRA_SOURCES
    ../src/plugin_config.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS
    metadata_cache
    test-helpers
    extra::rapidjson
  )
ROUTERTEST_GET_TARGET(T "test_plugin_config.cc" "${TEST_MODULE}")
TARGET_COMPILE_DEFINITIONS(${T}
  PRIVATE
  METADATA_CACHE_PLUGIN_STATIC_DEFINE=1
  )

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
