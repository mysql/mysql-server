# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

SET(PLUGIN_INFO_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/library_file.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_info_app.cc
)

SET(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  ${CMAKE_SOURCE_DIR}/src/harness/include
)

IF(NOT WIN32)
  ADD_LIBRARY(plugin_info_tests SHARED ${PLUGIN_INFO_SOURCES})
ELSE()
  ADD_LIBRARY(plugin_info_tests STATIC ${PLUGIN_INFO_SOURCES})
ENDIF()

TARGET_LINK_LIBRARIES(plugin_info_tests harness-library)

SET_TARGET_PROPERTIES(plugin_info_tests PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
                      )

SET_PATH_TO_SSL(plugin_info_tests ${CMAKE_BINARY_DIR}/library_output_directory)

TARGET_INCLUDE_DIRECTORIES(plugin_info_tests PRIVATE ${include_dirs})

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
             MODULE "mysqlrouter_plugin_info"
             LIB_DEPENDS plugin_info_tests
             INCLUDE_DIRS ${include_dirs}
             SYSTEM_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS}
)
