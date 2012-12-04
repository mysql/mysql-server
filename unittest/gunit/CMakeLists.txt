# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

# We want gmock-1.6.0.zip in order to build these unit tests.
# If you have already downloaded it,
# invoke cmake with -DWITH_GMOCK=/path/to/gmock-1.6.0.zip
#                or -DWITH_GMOCK=/path/to
#
# Alternatively, set an environment variable
# export WITH_GMOCK=/path/to/gmock-1.6.0.zip
#
# You can also do cmake -DENABLE_DOWNLOADS=1 
# and we will download it from http://googlemock.googlecode.com
#
# Either way: we will unpack the zip, compile gmock-all.cc and gtest-all.cc
# and link them into the executables.

# Where to download and build gmock/gtest.
IF(NOT DOWNLOAD_ROOT)
  SET(DOWNLOAD_ROOT ${CMAKE_SOURCE_DIR}/source_downloads)
ENDIF()
IF(NOT EXISTS DOWNLOAD_ROOT)
  MAKE_DIRECTORY(${DOWNLOAD_ROOT})
ENDIF()

# We want googlemock version 1.6, which also contains googletest.
SET(GMOCK_PACKAGE_NAME "gmock-1.6.0")
SET(GMOCK_SOURCE_DIR ${DOWNLOAD_ROOT}/${GMOCK_PACKAGE_NAME})
SET(GTEST_SOURCE_DIR ${DOWNLOAD_ROOT}/${GMOCK_PACKAGE_NAME}/gtest)

IF (DEFINED ENV{WITH_GMOCK} AND NOT DEFINED WITH_GMOCK)
  SET(WITH_GMOCK "$ENV{WITH_GMOCK}")
ENDIF()

IF (WITH_GMOCK)
  ## Did we get a full path name, including file name?
  IF (${WITH_GMOCK} MATCHES ".*\\.zip")
    GET_FILENAME_COMPONENT(GMOCK_DIR ${WITH_GMOCK} PATH)
    GET_FILENAME_COMPONENT(GMOCK_ZIP ${WITH_GMOCK} NAME)
    FIND_FILE(LOCAL_GMOCK_ZIP
              NAMES ${GMOCK_ZIP}
              PATHS ${GMOCK_DIR}
              NO_DEFAULT_PATH
             )
  ENDIF()
  ## Did we get a path name to the directory of the .zip file?
  FIND_FILE(LOCAL_GMOCK_ZIP
            NAMES "${GMOCK_PACKAGE_NAME}.zip"
            PATHS ${WITH_GMOCK}
            NO_DEFAULT_PATH
           )
  MESSAGE(STATUS "Local gmock zip ${LOCAL_GMOCK_ZIP}")
ENDIF()

# We may have downloaded gmock/gtest already, building in a different directory.
IF(EXISTS ${GMOCK_SOURCE_DIR})
  MESSAGE(STATUS "GMOCK_SOURCE_DIR:${GMOCK_SOURCE_DIR}")
  SET(GMOCK_DOWNLOADED 1 CACHE INTERNAL "")
  SET(GMOCK_FOUND 1 CACHE INTERNAL "")
ENDIF()


IF(LOCAL_GMOCK_ZIP AND NOT EXISTS ${GMOCK_SOURCE_DIR})
  # Unpack tarball
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E tar xfz  "${LOCAL_GMOCK_ZIP}"
    WORKING_DIRECTORY "${DOWNLOAD_ROOT}"
    RESULT_VARIABLE tar_result
  )
  IF (tar_result MATCHES 0)
    SET(GMOCK_FOUND 1 CACHE INTERNAL "")
  ENDIF()
ENDIF()


OPTION(ENABLE_DOWNLOADS
  "Download and build 3rd party source code components, e.g. google mock"
  OFF)

# While experimenting, use local URL rather than google.
SET(GMOCK_TARBALL "${GMOCK_PACKAGE_NAME}.zip")
SET(GMOCK_DOWNLOAD_URL 
  "http://googlemock.googlecode.com/files/${GMOCK_TARBALL}"
  )
  
MACRO(HTTP_PROXY_HINT)
  MESSAGE(STATUS
    "If you are inside a firewall, you may need to use an http proxy: "
    "export http_proxy=http://foo.bar.com:80")
ENDMACRO()


IF(NOT GMOCK_FOUND)
  IF(NOT ENABLE_DOWNLOADS)
    # Give one-time warning
    IF(NOT ONETIME_GTEST_WARNING)
      MESSAGE(STATUS 
        "Googlemock was not found. gtest-based unit tests will be disabled. "
        "You can run cmake . -DENABLE_DOWNLOADS=1 to automatically download "
        "and build required components from source.")
      HTTP_PROXY_HINT()
      SET(ONETIME_GTEST_WARNING 1 CACHE INTERNAL "")
    ENDIF()
    RETURN()
  ENDIF()
  
  # Download gmock source
  IF(NOT EXISTS ${GMOCK_SOURCE_DIR})
    IF(NOT EXISTS ${DOWNLOAD_ROOT}/${GMOCK_TARBALL})
      # Download the tarball
      # Use CMake builtin download capabilities
      FILE(DOWNLOAD ${GMOCK_DOWNLOAD_URL}
        ${DOWNLOAD_ROOT}/${GMOCK_TARBALL} TIMEOUT 30 STATUS ERR)
      IF(ERR EQUAL 0)
        SET(DOWNLOAD_SUCCEEDED 1)
      ELSE()
        MESSAGE(STATUS "Download failed, error: ${ERR}")
        # A failed DOWNLOAD leaves an empty file, remove it
        FILE(REMOVE ${DOWNLOAD_ROOT}/${GMOCK_TARBALL})
      ENDIF()

      IF (DOWNLOAD_SUCCEEDED)
        MESSAGE(STATUS 
          "Successfully downloaded ${GMOCK_DOWNLOAD_URL} to ${DOWNLOAD_ROOT}")
      ELSE()
        MESSAGE(STATUS 
          "To enable google test, please download ${GMOCK_DOWNLOAD_URL} "
          "to the directory ${DOWNLOAD_ROOT}")
        HTTP_PROXY_HINT()
        RETURN()
      ENDIF()
    ENDIF()
    
    # Unpack tarball
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_COMMAND} -E tar xfz  "${DOWNLOAD_ROOT}/${GMOCK_TARBALL}"
        WORKING_DIRECTORY "${DOWNLOAD_ROOT}"
      )
  ENDIF()
  IF(EXISTS ${GMOCK_SOURCE_DIR})
    SET(GMOCK_DOWNLOADED 1 CACHE INTERNAL "")
    SET(GMOCK_FOUND 1 CACHE INTERNAL "")
  ENDIF()
ENDIF()


IF(NOT GMOCK_FOUND)
  RETURN()
ENDIF()

# Build gmock/gtest libraries.
INCLUDE_DIRECTORIES(
  ${GMOCK_SOURCE_DIR}
  ${GMOCK_SOURCE_DIR}/include
  ${GTEST_SOURCE_DIR}
  ${GTEST_SOURCE_DIR}/include
)
ADD_LIBRARY(gmock STATIC ${GMOCK_SOURCE_DIR}/src/gmock-all.cc)
ADD_LIBRARY(gtest STATIC ${GTEST_SOURCE_DIR}/src/gtest-all.cc)
SET(GTEST_LIBRARIES gmock gtest)

IF (WITH_SSL STREQUAL "bundled")
  ADD_SUBDIRECTORY(yassl)
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/unittest/mytap
  ${CMAKE_SOURCE_DIR}/storage/example
)

# main-wrapper libraries (with tap-compatible option).
ADD_LIBRARY(gunit_small STATIC
  gunit_test_main.cc tap_event_listener.cc thread_utils.cc)
ADD_LIBRARY(gunit_large STATIC
  gunit_test_main_server.cc test_utils.cc tap_event_listener.cc thread_utils.cc)
ADD_DEPENDENCIES(gunit_small GenError)
ADD_DEPENDENCIES(gunit_large GenError)
TARGET_LINK_LIBRARIES(gunit_small
  mysys mysys_ssl mytap dbug strings ${GTEST_LIBRARIES})
TARGET_LINK_LIBRARIES(gunit_large
  mysys mysys_ssl mytap dbug strings ${GTEST_LIBRARIES})
MESSAGE(STATUS "GTEST_LIBRARIES:${GTEST_LIBRARIES}")

# Add some defines.
ADD_DEFINITIONS(-DMYSQL_SERVER)

# Add tests (link them with gunit/gmock libraries) 
SET(TESTS
  alignment
  bounded_queue
  bounds_checked_array
  byteorder
##  client_string TODO: move to a client test directory
  cost_estimate
  dbug
  decimal
  dynarray
  filesort_buffer
  filesort_compare
  mdl
  my_bitmap
  my_error
  my_fileutils
  my_regex
  sql_list
  sql_plist
  sql_string
  stdcxx
  thread_utils
  )

# Add tests (link them with gunit/gmock libraries and the server libraries) 
SET(SERVER_TESTS
  copy_info
  create_field
  delayable_insert_operation
  field
  get_diagnostics
  handler
  item
  item_func_now_local
  join_tab_sort
  make_sortkey
  my_decimal
  opt_range
  opt_trace
  segfault
  sql_table
  table_cache
)

## Merging tests into fewer executables saves *a lot* of
## link time and disk space ...
OPTION(MERGE_UNITTESTS "Merge tests into one executable" ON)
IF (MERGE_UNITTESTS)
  SET(MERGE_LARGE_TESTS ${CMAKE_CURRENT_BINARY_DIR}/merge_large_tests.cc)
  SET(MERGE_SMALL_TESTS ${CMAKE_CURRENT_BINARY_DIR}/merge_small_tests.cc)
  SET_SOURCE_FILES_PROPERTIES(MERGE_SMALL_TESTS MERGE_LARGE_TESTS
    PROPERTIES GENERATED 1)

  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

  FILE(WRITE ${MERGE_LARGE_TESTS} "// Merging large unit tests\n")
  FILE(WRITE ${MERGE_SMALL_TESTS} "// Merging small unit tests\n")
  FOREACH(test ${TESTS})
    FILE(APPEND ${MERGE_SMALL_TESTS} "#include \"${test}-t.cc\"\n")
  ENDFOREACH()
  FOREACH(test ${SERVER_TESTS})
    FILE(APPEND ${MERGE_LARGE_TESTS} "#include \"${test}-t.cc\"\n")
  ENDFOREACH()

  ADD_EXECUTABLE(merge_small_tests-t ${MERGE_SMALL_TESTS})
  SET(SRC_FILES ${MERGE_LARGE_TESTS})
  IF(WIN32)
    LIST(APPEND SRC_FILES ../../sql/nt_servc.cc)
  ENDIF()
  LIST(APPEND SRC_FILES ../../storage/example/ha_example.cc)
  SET_SOURCE_FILES_PROPERTIES(
    ../../storage/example/ha_example.cc
    PROPERTIES COMPILE_FLAGS "-DDISABLE_DTRACE"
  )
  ADD_EXECUTABLE(merge_large_tests-t ${SRC_FILES})

  TARGET_LINK_LIBRARIES(merge_small_tests-t
    gunit_small sqlgunitlib strings dbug regex)
  TARGET_LINK_LIBRARIES(merge_large_tests-t sql binlog rpl master slave sql)
  TARGET_LINK_LIBRARIES(merge_large_tests-t
    gunit_large strings dbug regex mysys)
  TARGET_LINK_LIBRARIES(merge_large_tests-t sql binlog rpl master slave sql)

  ADD_TEST(merge_large_tests merge_large_tests-t)
  ADD_TEST(merge_small_tests merge_small_tests-t)

ELSE()

  FOREACH(test ${TESTS})
    ADD_EXECUTABLE(${test}-t ${test}-t.cc)
    TARGET_LINK_LIBRARIES(${test}-t gunit_small sqlgunitlib strings dbug regex)
    ADD_TEST(${test} ${test}-t)
  ENDFOREACH()
  
  FOREACH(test ${SERVER_TESTS})
    SET(SRC_FILES ${test}-t.cc)
    IF(WIN32)
      LIST(APPEND SRC_FILES ../../sql/nt_servc.cc)
    ENDIF()
    IF(test MATCHES "table_cache")
      LIST(APPEND SRC_FILES ../../storage/example/ha_example.cc)
      SET_SOURCE_FILES_PROPERTIES(
        ../../storage/example/ha_example.cc
        PROPERTIES COMPILE_FLAGS "-DDISABLE_DTRACE"
      )
    ENDIF()
    ADD_EXECUTABLE(${test}-t ${SRC_FILES})
    TARGET_LINK_LIBRARIES(${test}-t sql binlog rpl master slave sql)
    TARGET_LINK_LIBRARIES(${test}-t gunit_large strings dbug regex mysys)
    TARGET_LINK_LIBRARIES(${test}-t sql binlog rpl master slave sql)
    ADD_TEST(${test} ${test}-t)
  ENDFOREACH()

ENDIF()

## Most executables depend on libeay32.dll (through mysys_ssl).
COPY_OPENSSL_DLLS(copy_openssl_gunit)
