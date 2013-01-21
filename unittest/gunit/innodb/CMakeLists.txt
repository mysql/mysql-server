# Copyright (c) 2013 Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE_DIRECTORIES(
  ${GTEST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
  ${CMAKE_SOURCE_DIR}/unittest/gunit
)

SET(TESTS
	#example
	ut0crc32
	ut0mem
)

FOREACH(test ${TESTS})
  ADD_EXECUTABLE(${test}-t ${test}-t.cc)

  # (unfortunately) the order of the libraries matters
  TARGET_LINK_LIBRARIES(${test}-t
    gunit_large
    strings
    dbug
    regex
    mysys
    innobase
    sql
    binlog
    rpl
    master
    slave
  )

  IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
    TARGET_LINK_LIBRARIES(${test}-t perfschema pfs_server_stubs)
  ENDIF()

  ADD_TEST(${test} ${test}-t)
ENDFOREACH()
