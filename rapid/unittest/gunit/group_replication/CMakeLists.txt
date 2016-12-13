# Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
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
  # mysys_ssl/libmysys_ssl.a already includes SSL objects on Solaris
  IF(NOT CMAKE_SYSTEM_NAME MATCHES "SunOS")
    TARGET_LINK_LIBRARIES(${test}-t mysqlclient)
  ENDIF()
  ADD_TEST(${test} ${test}-t)
ENDFOREACH()

