# Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  MY_CHECK_CXX_COMPILER_FLAG("-Wno-null-dereference" HAVE_NO_NULL)
  IF(HAVE_NO_NULL)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-null-dereference")
  ENDIF()
  MY_CHECK_CXX_COMPILER_FLAG("-Wno-unused-local-typedefs" HAVE_NO_UNUSED_TYPEDEFS)
  IF(HAVE_NO_UNUSED_TYPEDEFS)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
  ENDIF()

  SET(LIBCPG_PATH /usr/lib/corosync /usr/lib64/corosync /usr/local/lib /usr/local/lib64 CACHE STRING "Default locations for libcpg (cluster library)" )

  CHECK_INCLUDE_FILES(corosync/cpg.h HAVE_COROSYNC_CPG_H)

  IF (NOT HAVE_COROSYNC_CPG_H)
    MESSAGE(STATUS
            "Skipping GCS replication plugin unit tests due to Corosync headers not found.")
    RETURN()
  ENDIF (NOT HAVE_COROSYNC_CPG_H)

  INCLUDE_DIRECTORIES(${GMOCK_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/group_replication/include)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/group_replication/gcs/interface)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/group_replication/gcs/bindings/corosync)

  # Add tests
  SET(TESTS
      gcs_message
      gcs_corosync_statistics_interface
      gcs_corosync_communication_interface
      gcs_corosync_control_interface
      gcs_corosync_interface
      gcs_state_exchange
      group_replication_member_info
     )

  FOREACH(test ${TESTS})
    ADD_EXECUTABLE(${test}-t ${test}-t.cc)
    TARGET_LINK_LIBRARIES(${test}-t gcs_interface gcs_corosync gcs_unit_test_resource gunit_small)
    ADD_TEST(${test} ${test}-t)
  ENDFOREACH()
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")
