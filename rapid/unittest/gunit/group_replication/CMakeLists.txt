# Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

IF(NOT GMOCK_FOUND)
  RETURN()
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wno-null-dereference" HAVE_NO_NULL)
IF(HAVE_NO_NULL)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-null-dereference")
ENDIF()
MY_CHECK_CXX_COMPILER_FLAG("-Wno-unused-local-typedefs" HAVE_NO_UNUSED_TYPEDEFS)
IF(HAVE_NO_UNUSED_TYPEDEFS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
ENDIF()

INCLUDE_DIRECTORIES(${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/rapid/plugin/group_replication/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/rapid/plugin/group_replication/libmysqlgcs/include)

# Add tests
SET(TESTS
    group_replication_member_info
    group_replication_compatibility_module
    group_replication_member_version
   )

FOREACH(test ${TESTS})
  ADD_EXECUTABLE(${test}-t ${test}-t.cc)
  TARGET_LINK_LIBRARIES(${test}-t gr_unit_test_resource)
  TARGET_LINK_LIBRARIES(${test}-t gunit_small)
  ADD_TEST(${test} ${test}-t)
ENDFOREACH()
