# Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
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


IF(NOT GMOCK_FOUND)
  RETURN()
ENDIF()

INCLUDE_DIRECTORIES(${BOOST_PATCHES_DIR} ${BOOST_INCLUDE_DIR})

INCLUDE_DIRECTORIES(
  ${GMOCK_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/plugin/keyring
  ${CMAKE_SOURCE_DIR}/plugin/keyring/common
  ${CMAKE_SOURCE_DIR}/unittest/gunit
)

SET(TESTS
  keyring-api
  keys_container
  buffered_file_io
  file_io
)

IF (MERGE_UNITTESTS)
  SET(merge_keyring_file_tests ${CMAKE_CURRENT_BINARY_DIR}/merge_keyring_file_tests-t.cc)
  SET_SOURCE_FILES_PROPERTIES(merge_keyring_file_tests PROPERTIES GENERATED 1)

  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

  FILE(WRITE ${merge_keyring_file_tests} "// Merging Keyring_file unit tests\n")
  FILE(APPEND ${merge_keyring_file_tests} "#include \"my_config.h\"\n")
  FILE(APPEND ${merge_keyring_file_tests} "#define MERGE_UNITTESTS\n")

  FOREACH(test ${TESTS})
    FILE(APPEND ${merge_keyring_file_tests} "#include \"${test}-t.cc\"\n")
  ENDFOREACH()

  SET(SRC_FILES ${merge_keyring_file_tests})
  LIST(APPEND SRC_FILES
       ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keyring_key.cc
       ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_container.cc
       ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_iterator.cc
       ${CMAKE_SOURCE_DIR}/plugin/keyring/buffered_file_io.cc
       ${CMAKE_SOURCE_DIR}/plugin/keyring/file_io.cc
       ${CMAKE_SOURCE_DIR}/plugin/keyring/buffer.cc
       ${CMAKE_SOURCE_DIR}/plugin/keyring/hash_to_buffer_serializer.cc
       )

  IF(WIN32)
    LIST(APPEND SRC_FILES ../../../sql/nt_servc.cc)
  ENDIF()

  ADD_EXECUTABLE(merge_keyring_file_tests-t ${SRC_FILES})

  TARGET_LINK_LIBRARIES(merge_keyring_file_tests-t gunit_large strings dbug regex mysys sql)
  TARGET_LINK_LIBRARIES(merge_keyring_file_tests-t sql binlog rpl master slave sql)

  IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
    TARGET_LINK_LIBRARIES(merge_keyring_file_tests-t perfschema)
  ENDIF()

  ADD_DEPENDENCIES(merge_keyring_file_tests-t GenError)

  ADD_TEST(merge_keyring_file_tests merge_keyring_file_tests-t)

ENDIF(MERGE_UNITTESTS)

  FOREACH(test ${TESTS})
    SET(SRC_FILES ${test}-t.cc
        ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keyring_key.cc
        ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_container.cc
        ${CMAKE_SOURCE_DIR}/plugin/keyring/buffered_file_io.cc
        ${CMAKE_SOURCE_DIR}/plugin/keyring/file_io.cc
        ${CMAKE_SOURCE_DIR}/plugin/keyring/buffer.cc
        ${CMAKE_SOURCE_DIR}/plugin/keyring/hash_to_buffer_serializer.cc
        )
    IF(WIN32)
      LIST(APPEND SRC_FILES ../../../sql/nt_servc.cc)
    ENDIF()
    ADD_EXECUTABLE(${test}-t ${SRC_FILES})

    TARGET_LINK_LIBRARIES(${test}-t gunit_large strings dbug regex mysys sql)
    TARGET_LINK_LIBRARIES(${test}-t sql binlog rpl master slave sql)

    IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
      TARGET_LINK_LIBRARIES(${test}-t perfschema)
      TARGET_LINK_LIBRARIES(${test}-t perfschema pfs_server_stubs)
    ENDIF()

    IF(MERGE_UNITTESTS)
      SET_PROPERTY(TARGET ${test}-t PROPERTY EXCLUDE_FROM_ALL TRUE)
      IF(WIN32)
        SET_PROPERTY(TARGET ${test}-t PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
      ENDIF()
    ELSE()
      ADD_TEST(${test} ${test}-t)
    ENDIF()
  ENDFOREACH()

