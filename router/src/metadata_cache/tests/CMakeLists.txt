# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

SET(METADATA_CACHE_TESTS_HELPER
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/cluster_metadata.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/metadata_cache.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/cache_api.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/plugin_config.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/group_replication_metadata.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper/mock_metadata.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper/mock_metadata_factory.cc
)


SET(include_dirs
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper
  ${PROJECT_SOURCE_DIR}/tests/helpers
  )

# We do not link to the metadata cache libraries since the sources are
# already built as part of the test libraries.
IF(NOT WIN32)
  ADD_LIBRARY(metadata_cache_tests SHARED ${METADATA_CACHE_TESTS_HELPER})
  TARGET_LINK_LIBRARIES(metadata_cache_tests router_lib ${MySQL_CLIENT_LIB})
ELSE()
  ADD_LIBRARY(metadata_cache_tests STATIC ${METADATA_CACHE_TESTS_HELPER})
  TARGET_LINK_LIBRARIES(metadata_cache_tests router_lib metadata_cache
                        ${MySQL_CLIENT_LIB})
  TARGET_COMPILE_DEFINITIONS(metadata_cache_tests
                             PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
  TARGET_COMPILE_DEFINITIONS(metadata_cache_tests
                             PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
ENDIF()

SET_PATH_TO_SSL(metadata_cache_tests
                ${CMAKE_BINARY_DIR}/library_output_directory)

SET_TARGET_PROPERTIES(metadata_cache_tests PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${CMAKE_BINARY_DIR}/library_output_directory)


TARGET_INCLUDE_DIRECTORIES(metadata_cache_tests PRIVATE ${include_dirs})

SET(TEST_MODULE metadata_cache)
add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
             MODULE ${TEST_MODULE}
             LIB_DEPENDS
               metadata_cache_tests
               test-helpers
             INCLUDE_DIRS
               ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
               ${PROJECT_SOURCE_DIR}/src/metadata_cache/src
               ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper
               ${PROJECT_SOURCE_DIR}/tests/helpers
               ${PROJECT_SOURCE_DIR}/src/harness/shared/include
)


ROUTERTEST_GET_TARGET(test_target "test_cache_plugin.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)

ROUTERTEST_GET_TARGET(test_target "test_metadata_cache.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)

ROUTERTEST_GET_TARGET(test_target "test_cache_plugin.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_metadata_cache.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_metadata.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_metadata.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_failover.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_failover.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_plugin_config.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
ROUTERTEST_GET_TARGET(test_target "test_plugin_config.cc" ${TEST_MODULE})
TARGET_COMPILE_DEFINITIONS(${test_target}
                           PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
