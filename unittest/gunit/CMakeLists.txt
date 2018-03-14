# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

# We want release-1.8.0.zip in order to build these unit tests.
# If you have already downloaded it,
# invoke cmake with -DWITH_GMOCK=/path/to/release-1.8.0.zip
#                or -DWITH_GMOCK=/path/to
#
# Alternatively, set an environment variable
# export WITH_GMOCK=/path/to/release-1.8.0.zip
#
# You can also do cmake -DENABLE_DOWNLOADS=1 
# and we will download it from https://github.com/google/googletest/archive/
#
# Either way: we will unpack the zip, compile gmock-all.cc and gtest-all.cc
# and link them into the executables.


# Default location for where to download and build gmock/gtest.
IF(NOT DOWNLOAD_ROOT)
  SET(DOWNLOAD_ROOT ${CMAKE_SOURCE_DIR}/source_downloads)
ENDIF()

# We want googletest version 1.8, which also contains googlemock.
SET(GMOCK_PACKAGE_NAME "release-1.8.0")

IF (DEFINED ENV{WITH_GMOCK} AND NOT DEFINED WITH_GMOCK)
  FILE(TO_CMAKE_PATH "$ENV{WITH_GMOCK}" WITH_GMOCK)
ENDIF()

IF(LOCAL_GMOCK_ZIP
   AND NOT ${LOCAL_GMOCK_ZIP} MATCHES ".*${GMOCK_PACKAGE_NAME}\\.zip")
 SET(LOCAL_GMOCK_ZIP 0)
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
  ELSE()
    ## Did we get a path name to the directory of the .zip file?
    ## Check for both release-x.y.z.zip and googletest-release-x.y.z.zip
    FIND_FILE(LOCAL_GMOCK_ZIP
              NAMES "${GMOCK_PACKAGE_NAME}.zip" "googletest-${GMOCK_PACKAGE_NAME}.zip"
              PATHS ${WITH_GMOCK}
              NO_DEFAULT_PATH
              )
    ## If WITH_GMOCK is a directory, use it for download.
    SET(DOWNLOAD_ROOT ${WITH_GMOCK})
  ENDIF()
  MESSAGE(STATUS "Local gmock zip ${LOCAL_GMOCK_ZIP}")
ENDIF()

IF(NOT EXISTS DOWNLOAD_ROOT)
  MAKE_DIRECTORY(${DOWNLOAD_ROOT})
ENDIF()
SET(GMOCK_SOURCE_DIR ${DOWNLOAD_ROOT}/googletest-${GMOCK_PACKAGE_NAME}/googlemock)
SET(GTEST_SOURCE_DIR ${DOWNLOAD_ROOT}/googletest-${GMOCK_PACKAGE_NAME}/googletest)

# We may have downloaded gmock/gtest already, building in a different directory.
IF(EXISTS ${GMOCK_SOURCE_DIR} OR EXISTS ${LOCAL_GMOCK_ZIP})
  MESSAGE(STATUS "GMOCK_SOURCE_DIR:${GMOCK_SOURCE_DIR}")
  SET(GMOCK_DOWNLOADED 1 CACHE INTERNAL "")
  SET(GMOCK_FOUND 1 CACHE INTERNAL "")
# If source dir does not exist, reset dependent variables (might be set from before).
ELSE()
  SET(LOCAL_GMOCK_ZIP 0 CACHE INTERNAL "")
  SET(GMOCK_DOWNLOADED 0 CACHE INTERNAL "")
  SET(GMOCK_FOUND 0 CACHE INTERNAL "")
  SET(GMOCK_INCLUDE_DIRS 0 CACHE INTERNAL "")
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
  "Download and build 3rd party source code components, e.g. googletest"
  OFF)

# While experimenting, use local URL rather than google.
SET(GMOCK_TARBALL "googletest-${GMOCK_PACKAGE_NAME}.zip")
SET(GMOCK_DOWNLOAD_URL 
  "https://github.com/google/googletest/archive/${GMOCK_PACKAGE_NAME}.zip"
  )
  
MACRO(HTTP_PROXY_HINT)
  MESSAGE(STATUS
    "If you are inside a firewall, you may need to use an https proxy: "
    "export https_proxy=http://example.com:80")
ENDMACRO()


IF(NOT GMOCK_FOUND)
  IF(NOT ENABLE_DOWNLOADS)
    # Give warning
    MESSAGE(STATUS
      "Googletest was not found. gtest-based unit tests will be disabled. "
      "You can run cmake . -DENABLE_DOWNLOADS=1 to automatically download "
      "and build required components from source.")
    HTTP_PROXY_HINT()
    RETURN()
  ENDIF()
  
  # Download googletest source
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
          "To enable googletest, please download ${GMOCK_DOWNLOAD_URL} "
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

SET(GMOCK_INCLUDE_DIRS
  ${GMOCK_SOURCE_DIR}
  ${GMOCK_SOURCE_DIR}/include
  ${GTEST_SOURCE_DIR}
  ${GTEST_SOURCE_DIR}/include
  CACHE INTERNAL "")

# Build gmock/gtest libraries.
INCLUDE_DIRECTORIES(SYSTEM
  ${GMOCK_SOURCE_DIR}
  ${GMOCK_SOURCE_DIR}/include
  ${GTEST_SOURCE_DIR}
  ${GTEST_SOURCE_DIR}/include
)

# Some tests require Boost.
INCLUDE_DIRECTORIES(SYSTEM ${BOOST_PATCHES_DIR} ${BOOST_INCLUDE_DIR})

MY_CHECK_CXX_COMPILER_FLAG("-fno-builtin-memcmp" HAVE_NO_BUILTIN_MEMCMP)

ADD_LIBRARY(gmock STATIC ${GMOCK_SOURCE_DIR}/src/gmock-all.cc)
ADD_LIBRARY(gtest STATIC ${GTEST_SOURCE_DIR}/src/gtest-all.cc)
SET(GTEST_LIBRARIES gmock gtest)

SET(DISABLE_PSI_DEFINITIONS
  "DISABLE_PSI_COND"
  "DISABLE_PSI_FILE"
  "DISABLE_PSI_METADATA"
  "DISABLE_PSI_MUTEX"
  "DISABLE_PSI_RWLOCK"
  "DISABLE_PSI_STAGE"
  )
SET(DISABLE_PSI_OPTIONS
  "-DDISABLE_PSI_COND"
  "-DDISABLE_PSI_FILE"
  "-DDISABLE_PSI_METADATA"
  "-DDISABLE_PSI_MUTEX"
  "-DDISABLE_PSI_RWLOCK"
  "-DDISABLE_PSI_STAGE"
  )

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include
  SYSTEM ${ICU_INCLUDE_DIRS}
)

# main-wrapper libraries (with tap-compatible option).
ADD_LIBRARY(gunit_small STATIC
  benchmark.cc
  fake_costmodel.cc
  gunit_test_main.cc
  skip_trailing.cc
  strnxfrm.cc
  tap_event_listener.cc
  thread_utils.cc
  fake_table.cc
)
SET_TARGET_PROPERTIES(gunit_small
  PROPERTIES COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}"
)

# Several "small" unit tests got 'missing symbol client_errors'
# When building with Sun Studio
ADD_LIBRARY(libmysql_errmsg STATIC
  ${CMAKE_SOURCE_DIR}/libmysql/errmsg.cc
)
SET_TARGET_PROPERTIES(libmysql_errmsg
  PROPERTIES COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}"
)

ADD_LIBRARY(gunit_large STATIC
  benchmark.cc
  gunit_test_main_server.cc
  tap_event_listener.cc
  test_utils.cc
  thread_utils.cc
)
ADD_DEPENDENCIES(gunit_small GenError)
ADD_DEPENDENCIES(gunit_large GenError)
TARGET_LINK_LIBRARIES(gunit_small
  mysys mysys_ssl mytap dbug strings myisam_sys ${GTEST_LIBRARIES} libmysql_errmsg)
TARGET_LINK_LIBRARIES(gunit_large
  mysys mysys_ssl mytap dbug strings ${GTEST_LIBRARIES})
MESSAGE(STATUS "GTEST_LIBRARIES:${GTEST_LIBRARIES}")

IF(WIN32)
  ## Create an object library of nt_servc.cc, so we don't have to
  ## compile it again for each target
  ADD_LIBRARY(my_nt_servc OBJECT ../../sql/nt_servc.cc)
ENDIF()

# Add some defines.
ADD_DEFINITIONS(-DMYSQL_SERVER -DEXTRA_CODE_FOR_UNIT_TESTING)
ADD_DEFINITIONS(-DERRMSG_DIR="${PROJECT_BINARY_DIR}/share")
ADD_DEFINITIONS(-DDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}")

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

# Add tests (link them with gunit/gmock libraries) 
SET(TESTS
  alignment
  bounded_queue
#  bounds_checked_array
  bitmap
  byteorder
  calloc
  cost_estimate
  dbug
  decimal
  dynarray
  filesort_buffer
  filesort_compare
  float_compare
  inplace_vector
  key
  like_range
  mdl
  my_bitmap
  my_error
  my_fileutils
  my_murmur3
  my_thread
  mysys_base64
  mysys_lf
  mysys_my_b_vprintf
  mysys_my_loadpath
  mysys_my_malloc
  mysys_my_pwrite
  mysys_my_rdtsc
  mysys_my_symlink
  mysys_my_write
  mysys_my_read
  mysys_pathfuncs
  nullable
  opt_recperkey
  partitioned_rwlock
  pattern_matcher
  prealloced_array
  priority_queue
  record_buffer
  sql_class_header
  sql_list
  sql_plist
  sql_string
  stl_alloc
  strings_skip_trailing
  strings_strnxfrm
  strings_utf8
  strings_valid_check
  strtoll
  thread_utils
  my_timer
  template_utils
  timespec
  my_alloc
  pump_object_filter
  val_int_compare
  varlen_sort
  collation_loader
  )
 
IF(WIN32)
  LIST(APPEND TESTS win_tests)
ENDIF()

SET(ALL_SMALL_TESTS)
FOREACH(test ${TESTS})
  LIST(APPEND ALL_SMALL_TESTS ${test}-t.cc)
ENDFOREACH()

# Add tests (link them with gunit/gmock libraries and the server libraries) 
SET(SERVER_TESTS
  character_set_deprecation
  copy_info
  create_field
  dd_cache
  dd_column_statistics
  dd_info_schema_native_func
  dd_properties
  dd_schema
  dd_sdi
  dd_string_type
  dd_table
  debug_sync
  explain_filename
  field
  get_diagnostics
  gis_algos
  gis_geometries
  gis_is_simple
  gis_isvalid
  gis_relops
  gis_wkb_parser
  handler
  histograms
  initialize_password
  insert_delayed
  into_syntax
  item
  item_filter
  item_func_case
  item_func_now_local
  item_json_func
  item_func_regexp
  item_like
  item_timefunc
  join_syntax
  join_tab_sort
  json_binary
  json_dom
  json_path
  locking_clause_syntax
  locking_service
  log_throttle
  make_sortkey
  mdl_sync
  my_decimal
  opt_costconstants
  opt_costmodel
  opt_guessrecperkey
  opt_range
  opt_ref
  opt_trace
  regexp_engine
  regexp_facade
  security_context
  segfault
  select_lex_visitor
  sql_table
  subquery_syntax
  table_cache
  table_factor_syntax
  tc_log_mmap
  temptable_allocator
  temptable_storage
  thd_manager
  union_syntax
  unique
  value_map
  wild_case_compare
  sha2_password
)

SET(ALL_LARGE_TESTS)
FOREACH(test ${SERVER_TESTS})
  LIST(APPEND ALL_LARGE_TESTS ${test}-t.cc)
ENDFOREACH()

SET(SQL_GUNIT_LIB_SOURCE 
  ${CMAKE_SOURCE_DIR}/sql/filesort_utils.cc 
  ${CMAKE_SOURCE_DIR}/sql/mdl.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_list.cc
  ${CMAKE_SOURCE_DIR}/sql/stateless_allocator.cc
  ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc
  ${CMAKE_SOURCE_DIR}/sql/thr_malloc.cc
  )

IF(WIN32)
  LIST(APPEND SQL_GUNIT_LIB_SOURCE ${CMAKE_SOURCE_DIR}/sql/named_pipe.cc)
ENDIF()
ADD_CONVENIENCE_LIBRARY(sqlgunitlib ${SQL_GUNIT_LIB_SOURCE})
ADD_DEPENDENCIES(sqlgunitlib GenError)
SET_TARGET_PROPERTIES(sqlgunitlib
  PROPERTIES COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}"
)

## Merging tests into fewer executables saves *a lot* of
## link time and disk space ...
OPTION(MERGE_UNITTESTS "Merge tests into one executable" ON)
IF (MERGE_UNITTESTS)
  MYSQL_ADD_EXECUTABLE(merge_small_tests-t ${ALL_SMALL_TESTS}
    ADD_TEST merge_small_tests)
  TARGET_LINK_LIBRARIES(merge_small_tests-t
    mysys mysys_ssl sqlgunitlib gunit_small)
  
  IF(WIN32)
    LIST(APPEND ALL_LARGE_TESTS ../../sql/nt_servc.cc)
  ENDIF()
  LIST(APPEND ALL_LARGE_TESTS ../../storage/example/ha_example.cc)

  MYSQL_ADD_EXECUTABLE(merge_large_tests-t ${ALL_LARGE_TESTS}
    ADD_TEST merge_large_tests)
  TARGET_LINK_LIBRARIES(merge_large_tests-t sql_main binlog rpl master slave)
  TARGET_LINK_LIBRARIES(merge_large_tests-t sql_main sql_gis sql_dd)
  TARGET_LINK_LIBRARIES(merge_large_tests-t
    gunit_large strings dbug mysys ${ICU_LIBRARIES})
  TARGET_LINK_LIBRARIES(merge_large_tests-t
    sql_main binlog rpl master slave sql_gis sql_main sql_dd)

  IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
    TARGET_LINK_LIBRARIES(merge_large_tests-t perfschema)
    TARGET_LINK_LIBRARIES(merge_small_tests-t perfschema)
  ENDIF()

ENDIF(MERGE_UNITTESTS)

SET(EXCLUDE_FROM_ALL "EXCLUDE_FROM_ALL")
IF (NOT MERGE_UNITTESTS)
  UNSET(EXCLUDE_FROM_ALL)
ENDIF()

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc SKIP_INSTALL ${EXCLUDE_FROM_ALL})
  ADD_COMPILE_FLAGS(${test}-t.cc COMPILE_FLAGS ${DISABLE_PSI_OPTIONS})
  TARGET_LINK_LIBRARIES(${test}-t gunit_small sqlgunitlib strings dbug)
  TARGET_LINK_LIBRARIES(${test}-t ${ICU_LIBRARIES})
  IF(NOT MERGE_UNITTESTS)
    ADD_TEST(${test} ${test}-t)
  ENDIF()
ENDFOREACH()

# See comments about __builtin_memcmp in the source file.
IF(HAVE_NO_BUILTIN_MEMCMP)
  ADD_COMPILE_FLAGS(
    filesort_compare-t.cc
    COMPILE_FLAGS "-fno-builtin-memcmp"
    )
ENDIF()

FOREACH(test ${SERVER_TESTS})
  SET(SRC_FILES ${test}-t.cc)
  IF(WIN32)
    LIST(APPEND SRC_FILES $<TARGET_OBJECTS:my_nt_servc>)
  ENDIF()
  IF(test MATCHES "table_cache")
    LIST(APPEND SRC_FILES ../../storage/example/ha_example.cc)
  ENDIF()
  MYSQL_ADD_EXECUTABLE(${test}-t ${SRC_FILES} SKIP_INSTALL ${EXCLUDE_FROM_ALL})
  TARGET_LINK_LIBRARIES(${test}-t sql_main binlog rpl master slave sql_main)
  TARGET_LINK_LIBRARIES(${test}-t sql_main sql_gis sql_dd)
  TARGET_LINK_LIBRARIES(${test}-t gunit_large strings dbug mysys)
  TARGET_LINK_LIBRARIES(${test}-t
    sql_main binlog rpl master slave sql_gis sql_main sql_dd sql_gis)
  TARGET_LINK_LIBRARIES(${test}-t ${ICU_LIBRARIES})

  IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
    TARGET_LINK_LIBRARIES(${test}-t perfschema)
  ENDIF()

  IF(NOT MERGE_UNITTESTS)
    ADD_TEST(${test} ${test}-t)
  ENDIF()
ENDFOREACH()

ADD_SUBDIRECTORY(innodb)
ADD_SUBDIRECTORY(keyring)
ADD_SUBDIRECTORY(components/mysql_server)
ADD_SUBDIRECTORY(xplugin)
ADD_SUBDIRECTORY(group_replication)
ADD_SUBDIRECTORY(libmysqlgcs)
