# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(SYSTEM ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(helpers
  ${PROJECT_SOURCE_DIR}/src/harness/include
  ${PROJECT_SOURCE_DIR}/src/router/include/mysqlrouter
  )

SET(helper_sources
  helpers/router_test_helpers.cc
  helpers/mysql_session_replayer.cc
  helpers/router_component_test.cc
  helpers/tcp_port_pool.cc
  helpers/script_generator.cc
)

IF(WIN32)
  LIST(APPEND helper_sources helpers/cmd_exec-windows.cc)
ELSE()
  LIST(APPEND helper_sources helpers/cmd_exec.cc)
ENDIF()

ADD_LIBRARY(routertest_helpers STATIC ${helper_sources})

TARGET_LINK_LIBRARIES(routertest_helpers harness-library router_lib)

FOREACH(test_dir legal tools)
    IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir})
        add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}/${test_dir} MODULE project)
    ENDIF()
ENDFOREACH()

ADD_SUBDIRECTORY(component)
ADD_SUBDIRECTORY(fuzzers)

ADD_DEFINITIONS(-DCOMPONENT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/component/data/")
