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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(DISABLE_SHARED)
  RETURN()
ENDIF()

INCLUDE_DIRECTORIES(
# ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/components/example
  ${CMAKE_SOURCE_DIR}/components/mysql_server
)

REMOVE_DEFINITIONS(-DMYSQL_SERVER)

# Add tests
SET(TESTS registry dynamic_loader)

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t
    ${test}-t.cc
    ${CMAKE_SOURCE_DIR}/sql/rwlock_scoped_lock.cc
    ${CMAKE_SOURCE_DIR}/sql/server_component/mysql_string_service.cc
    ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc
    builtins_stubs.cc
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
    ADD_TEST ${test}
  )
  TARGET_LINK_LIBRARIES(${test}-t
    ${CMAKE_THREAD_LIBS_INIT} gunit_small component_mysql_server)
ENDFOREACH()

MYSQL_ADD_COMPONENT(self_required_test_component
  self_required_test_component.cc
  NO_INSTALL MODULE)
MYSQL_ADD_COMPONENT(cyclic_dependency_test_component_1
  cyclic_dependency_test_component_1.cc
  NO_INSTALL MODULE)
MYSQL_ADD_COMPONENT(cyclic_dependency_test_component_2
  cyclic_dependency_test_component_2.cc
  NO_INSTALL MODULE)

SET(EXAMPLE_COMPONENTS
  component_example_component1
  component_example_component2
  component_example_component3
  component_self_required_test_component
  component_cyclic_dependency_test_component_1
  component_cyclic_dependency_test_component_2
  )

FOREACH(EXAMPLE_COMPONENT ${EXAMPLE_COMPONENTS})
  ADD_DEPENDENCIES(dynamic_loader-t ${EXAMPLE_COMPONENT})
ENDFOREACH()
