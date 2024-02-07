# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})

SET(helper_sources
  helpers/filesystem_utils.cc
  helpers/gtest_testname.cc
  helpers/mock_server_testutils.cc
  helpers/core_dumper.cc
  helpers/core_finder.cc
  helpers/process_manager.cc
  helpers/process_wrapper.cc
  helpers/router_component_clusterset.cc
  helpers/router_component_test.cc
  helpers/router_component_testutils.cc
  helpers/router_component_system_layout.cc
  helpers/router_component_metadata.cc
  helpers/router_test_helpers.cc
  helpers/script_generator.cc
  helpers/shared_server.cc
  helpers/tcp_port_pool.cc
  helpers/rest_api_testutils.cc
  helpers/rest_metadata_client.cc
)

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

ADD_LIBRARY(routertest_helpers STATIC ${helper_sources})

TARGET_LINK_LIBRARIES(routertest_helpers harness-library router_lib
  rest_api_lib mock_server_rest_client)
TARGET_INCLUDE_DIRECTORIES(routertest_helpers
  PUBLIC
  helpers
  ${PROJECT_SOURCE_DIR}/src/harness/shared/include/
  )



ADD_SUBDIRECTORY(component)
ADD_SUBDIRECTORY(fuzzers)
ADD_SUBDIRECTORY(integration)

ADD_DEFINITIONS(
  -DCOMPONENT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/component/data/")
ADD_DEFINITIONS(
  -DSSL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/mysql-test/std_data/")
