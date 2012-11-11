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

# In case we need to override the choice of compiler, see below.
INCLUDE(CMakeForceCompiler)

# Builtin GTest support is only available with CMake 2.8. For 2.6, we use 
# own copy of the FindGTest.cmake module, located in current source directory.
IF("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS "2.8")
  SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")  
ELSE()
  SET(HAVE_CMAKE_2_8 1)
ENDIF() 

IF(MSVC)
  # Restore exception handling flags to avoid tons of warnings when using STL.
  ADD_DEFINITIONS(/EHcs)
ENDIF()

# Where to download and build gtest if not found.
IF(NOT DOWNLOAD_ROOT)
  SET(DOWNLOAD_ROOT ${CMAKE_SOURCE_DIR}/source_downloads)
ENDIF()
IF(NOT EXISTS DOWNLOAD_ROOT)
  MAKE_DIRECTORY(${DOWNLOAD_ROOT})
ENDIF()
SET(GTEST_PACKAGE_NAME "gtest-1.5.0")
SET(GTEST_SOURCE_DIR ${DOWNLOAD_ROOT}/${GTEST_PACKAGE_NAME})


# Hint for FindGTest: With MSVC, we compile with static CRT (compile flags
# /MT or /MTd) and need compatible gtest.lib
SET(GTEST_MSVC_SEARCH MT) 

# Hint for where to look: pushbuild uses GTEST_ROOT env.var.
IF (DEFINED ENV{GTEST_PREFIX} AND NOT DEFINED ENV{GTEST_ROOT})
  SET(ENV{GTEST_ROOT} $ENV{GTEST_PREFIX})
ENDIF()

# We may have downloaded gtest already, building in a different directory.
IF(NOT DEFINED ENV{GTEST_ROOT} AND EXISTS ${GTEST_SOURCE_DIR})
  MESSAGE(STATUS "GTEST_SOURCE_DIR:${GTEST_SOURCE_DIR}")
  SET(GTEST_DOWNLOADED 1 CACHE INTERNAL "")
  SET(GTEST_FOUND 1 CACHE INTERNAL "")
ENDIF()


IF(NOT GTEST_DOWNLOADED)
  # Hint for find_library(): reverse list ".so;.a" to prefer static linking.
  # LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)

  # Now look for gtest.
  FIND_PACKAGE(GTest QUIET)

  # Revert suffix list back to what it was before.
  # LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
ENDIF()


OPTION(ENABLE_DOWNLOADS
  "Download and build 3rd party source code components, e.g google test"
  OFF)

# While experimenting, use local URL rather than google.
SET(GTEST_TARBALL "${GTEST_PACKAGE_NAME}.tar.gz")
SET(GTEST_DOWNLOAD_URL 
  "http://googletest.googlecode.com/files/${GTEST_TARBALL}"
# "https://clustra.norway.sun.com/~td136807/${GTEST_TARBALL}"
  )
  
MACRO(HTTP_PROXY_HINT)
  MESSAGE(STATUS
    "If you are inside a firewall, you may need to use an http proxy: "
    "export http_proxy=http://foo.bar.com:80")
ENDMACRO()


IF(NOT GTEST_FOUND)
  IF(NOT ENABLE_DOWNLOADS)
    # Give one-time warning
    IF(NOT ONETIME_GTEST_WARNING)
      MESSAGE(STATUS 
        "Googletest was not found. gtest-based unit tests will be disabled. "
        "You can run cmake . -DENABLE_DOWNLOADS=1 to automatically download "
        "and build required components from source.")
      HTTP_PROXY_HINT()
      SET(ONETIME_GTEST_WARNING 1 CACHE INTERNAL "")
    ENDIF()
    RETURN()
  ENDIF()
  
  # Download gtest source
  IF(NOT EXISTS ${GTEST_SOURCE_DIR})
    IF(NOT EXISTS ${DOWNLOAD_ROOT}/${GTEST_TARBALL})
      # Download the tarball
      IF(NOT HAVE_CMAKE_2_8)
        # In versions earlier than 2.8, try wget for downloading
        FIND_PROGRAM(WGET_EXECUTABLE wget)
        MARK_AS_ADVANCED(WGET_EXECUTABLE)
        IF(WGET_EXECUTABLE)
          EXECUTE_PROCESS(COMMAND ${WGET_EXECUTABLE} -T 30 ${GTEST_DOWNLOAD_URL}
            WORKING_DIRECTORY ${DOWNLOAD_ROOT}
            ERROR_VARIABLE ERR
            OUTPUT_VARIABLE OUT
            RESULT_VARIABLE RES
          )
          IF(RES EQUAL 0)
            SET(DOWNLOAD_SUCCEEDED 1)
          ELSE()
            MESSAGE(STATUS "Download failed, "
              "command: ${WGET_EXECUTABLE} -T 30 ${GTEST_DOWNLOAD_URL}")
            MESSAGE(STATUS "stdout: ${OUT}")
            MESSAGE(STATUS "stderr: ${ERR}")
          ENDIF()
        ENDIF()
      ELSE()
        # Use CMake builtin download capabilities
        FILE(DOWNLOAD ${GTEST_DOWNLOAD_URL}
          ${DOWNLOAD_ROOT}/${GTEST_TARBALL} TIMEOUT 30 STATUS ERR)
        IF(ERR EQUAL 0)
          SET(DOWNLOAD_SUCCEEDED 1)
        ELSE()
          MESSAGE(STATUS "Download failed, error: ${ERR}")
          # A failed DOWNLOAD leaves an empty file, remove it
          FILE(REMOVE ${DOWNLOAD_ROOT}/${GTEST_TARBALL})
        ENDIF()
      ENDIF()
      
      IF (DOWNLOAD_SUCCEEDED)
        MESSAGE(STATUS 
          "Successfully downloaded ${GTEST_DOWNLOAD_URL} to ${DOWNLOAD_ROOT}")
      ELSE()
        MESSAGE(STATUS 
          "To enable google test, please download ${GTEST_DOWNLOAD_URL} "
          "to the directory ${DOWNLOAD_ROOT}")
        HTTP_PROXY_HINT()
        RETURN()
      ENDIF()
    ENDIF()
    
    # Unpack tarball
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_COMMAND} -E tar xfz  "${DOWNLOAD_ROOT}/${GTEST_TARBALL}"
        WORKING_DIRECTORY "${DOWNLOAD_ROOT}"
        OUTPUT_QUIET
        ERROR_QUIET
      )
  ENDIF()
  IF(EXISTS ${GTEST_SOURCE_DIR})
    SET(GTEST_DOWNLOADED 1 CACHE INTERNAL "")
    SET(GTEST_FOUND 1 CACHE INTERNAL "")
  ENDIF()
ENDIF()

IF(NOT GTEST_FOUND)
  RETURN()
ENDIF()

IF(GTEST_DOWNLOADED)
  # Build gtest library
  INCLUDE_DIRECTORIES(
    ${GTEST_SOURCE_DIR} 
    ${GTEST_SOURCE_DIR}/include
  )
  ADD_LIBRARY(gtest STATIC ${GTEST_SOURCE_DIR}/src/gtest-all.cc)

  # Set CMake variables to make FindPackage(GTest) happy next time.
  SET(GTEST_FOUND 1 CACHE INTERNAL "")
  SET(GTEST_LIBRARY gtest CACHE INTERNAL "")
  SET(GTEST_LIBRARIES gtest CACHE INTERNAL "")
  SET(GTEST_MAIN_LIBRARY no_gtest_main_library CACHE INTERNAL "")
  SET(GTEST_INCLUDE_DIRS ${GTEST_SOURCE_DIR}/include CACHE INTERNAL "")
  SET(GTEST_INCLUDE_DIR "${GTEST_SOURCE_DIR}/include" CACHE INTERNAL "")
ENDIF()

IF (WITH_SSL STREQUAL "bundled")
  ADD_SUBDIRECTORY(yassl)
ENDIF()

INCLUDE_DIRECTORIES(
  ${GTEST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/unittest/mytap
  ${CMAKE_SOURCE_DIR}/storage/example
)

# main-wrapper library (with tap-compatible option).
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

# libgtest.dylib may have been moved after it was built.
# We can use otool(1) and install_name_tool(1) to patch the executables.
# If it moves after linking, use DYLD_LIBRARY_PATH env.var.
IF(APPLE)
  SET(GTEST_DYLIB_LOCATION)
  STRING(REGEX MATCH ".*dylib$" GTEST_LIB_IS_SHARED ${GTEST_LIBRARIES})
  IF(GTEST_LIB_IS_SHARED)
    EXECUTE_PROCESS(
      COMMAND otool -D ${GTEST_LIBRARIES}
      RESULT_VARIABLE result
      OUTPUT_VARIABLE output
    )
    IF(result EQUAL 0)
      STRING(REPLACE "${GTEST_LIBRARIES}:" "" install_name ${output})
      STRING(REGEX REPLACE "\n" "" install_name ${install_name})
      SET(GTEST_DYLIB_LOCATION "${install_name}")
    ENDIF()
  ENDIF()
ENDIF()

# Add tests (link them with gunit library) 
SET(TESTS
  alignment
  bounded_queue
  bounds_checked_array
  byteorder
  client_string
  cost_estimate
  dbug
  decimal
  dynarray
  filesort_buffer
  filesort_compare
  mdl
  mdl_mytap
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
  
IF(WIN32)
  LIST(APPEND TESTS win_tests)
ENDIF()

# Add tests (link them with gunit library and the server libraries) 
SET(SERVER_TESTS
  copy_info
  create_field
  field
  get_diagnostics
  handler
  insert_delayed
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

FOREACH(test ${TESTS})
  ADD_EXECUTABLE(${test}-t ${test}-t.cc)
  TARGET_LINK_LIBRARIES(${test}-t gunit_small sqlgunitlib strings dbug regex)
  IF(APPLE AND GTEST_DYLIB_LOCATION)
    ADD_CUSTOM_COMMAND(TARGET ${test}-t POST_BUILD
      COMMAND install_name_tool -change
        ${GTEST_DYLIB_LOCATION} ${GTEST_LIBRARIES} ${test}-t
      VERBATIM)
  ENDIF()
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
  IF(APPLE AND GTEST_DYLIB_LOCATION)
    ADD_CUSTOM_COMMAND(TARGET ${test}-t POST_BUILD
      COMMAND install_name_tool -change
        ${GTEST_DYLIB_LOCATION} ${GTEST_LIBRARIES} ${test}-t
      VERBATIM)
  ENDIF()
  ADD_TEST(${test} ${test}-t)
ENDFOREACH()

## Most executables depend on libeay32.dll (through mysys_ssl).
COPY_OPENSSL_DLLS(copy_openssl_gunit)
