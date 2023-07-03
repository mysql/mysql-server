<<<<<<< HEAD
# Copyright (c) 2010, 2022, Oracle and/or its affiliates.
#
=======
<<<<<<< HEAD
# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
=======
# Copyright (c) 2010, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
# 
>>>>>>> pr/231
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

<<<<<<< HEAD
IF(NOT WITH_UNIT_TESTS)
=======
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

<<<<<<< HEAD

=======
>>>>>>> upstream/cluster-7.6
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
  FILE(TO_CMAKE_PATH "${WITH_GMOCK}" WITH_GMOCK)
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
>>>>>>> pr/231
  RETURN()
ENDIF()

IF(WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO)
  # We get ODR violations because of multiple bison parsers.
  MY_CHECK_CXX_COMPILER_WARNING("error=odr" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_EXE_LINKER_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDIF()

# Some tests require Boost.
INCLUDE_DIRECTORIES(SYSTEM ${BOOST_PATCHES_DIR} ${BOOST_INCLUDE_DIR})

<<<<<<< HEAD
INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
=======
MY_CHECK_CXX_COMPILER_FLAG("-fno-builtin-memcmp" HAVE_NO_BUILTIN_MEMCMP)
<<<<<<< HEAD
=======
MY_CHECK_CXX_COMPILER_FLAG("-Wformat-overflow" HAVE_FORMAT_OVERFLOW)
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

MY_CHECK_CXX_COMPILER_FLAG("-fno-builtin-memcmp" HAVE_NO_BUILTIN_MEMCMP)

<<<<<<< HEAD
SET(DISABLE_PSI_DEFINITIONS
  "DISABLE_PSI_COND"
  "DISABLE_PSI_FILE"
  "DISABLE_PSI_MEMORY"
  "DISABLE_PSI_METADATA"
  "DISABLE_PSI_MUTEX"
  "DISABLE_PSI_RWLOCK"
  "DISABLE_PSI_STAGE"
  )

# Set these in the cache to make them available globally
SET(DISABLE_PSI_DEFINITIONS ${DISABLE_PSI_DEFINITIONS} CACHE INTERNAL "")

# Set up valgrind options to use with ctest.
# Use `ctest (...) -D ExperimentalMemCheck` to run.
IF(LINUX OR FREEBSD)
  FIND_PROGRAM(MEMORYCHECK_COMMAND valgrind)
  SET(MEMORYCHECK_COMMAND_OPTIONS
    "--leak-check=full --errors-for-leak-kinds=definite,indirect --error-exitcode=42"
    CACHE INTERNAL "")
  SET(MEMORYCHECK_SUPPRESSIONS_FILE "${PROJECT_SOURCE_DIR}/mysql-test/valgrind.supp"
    CACHE INTERNAL "")
ENDIF()

<<<<<<< HEAD
MY_INCLUDE_SYSTEM_DIRECTORIES(ICU)
=======
=======
>>>>>>> upstream/cluster-7.6
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include
<<<<<<< HEAD
  SYSTEM ${ICU_INCLUDE_DIRS}
=======
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/sql/auth
  ${CMAKE_SOURCE_DIR}/storage/example
>>>>>>> upstream/cluster-7.6
)
>>>>>>> pr/231

# main-wrapper libraries (with tap-compatible option).
ADD_LIBRARY(gunit_small STATIC
  benchmark.cc
  fake_costmodel.cc
  gunit_test_main.cc
  skip_trailing.cc
  strnxfrm.cc
  thread_utils.cc
  fake_table.cc
)
SET_TARGET_PROPERTIES(gunit_small
  PROPERTIES COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}"
)

ADD_LIBRARY(gunit_large STATIC
  benchmark.cc
  gunit_test_main_server.cc
  test_utils.cc
  thread_utils.cc
)

# Add a library for enable_win_jemalloc.cc
# and link it with both gunit_small and gunit_large below.
UNSET(UNITTEST_JEMALLOC_LIB)
IF(WIN32)
  ADD_LIBRARY(unittest_jemalloc STATIC ../../sql/enable_win_jemalloc.cc)
  SET(UNITTEST_JEMALLOC_LIB unittest_jemalloc)
ENDIF()

ADD_DEPENDENCIES(gunit_small GenError)
ADD_DEPENDENCIES(gunit_large GenError)
TARGET_LINK_LIBRARIES(gunit_small
  mysys strings ${GTEST_LIBRARIES} ${UNITTEST_JEMALLOC_LIB})
TARGET_LINK_LIBRARIES(gunit_large
  mysys strings ${GTEST_LIBRARIES} ${UNITTEST_JEMALLOC_LIB})

IF(WIN32)
  ## Create an object library of nt_servc.cc, so we don't have to
  ## compile it again for each target
  ADD_LIBRARY(my_nt_servc OBJECT ../../sql/nt_servc.cc)
ENDIF()

# Add some defines.
ADD_DEFINITIONS(-DMYSQL_SERVER -DEXTRA_CODE_FOR_UNIT_TESTING)
ADD_DEFINITIONS(-DERRMSG_DIR="${PROJECT_BINARY_DIR}/share")
ADD_DEFINITIONS(-DDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}")

# Add tests (link them with gunit/gmock libraries)
SET(TESTS
  alignment
  bitmap
  bounds_checked_array
  byteorder
  calloc
  charset_bug28956360
  charset_bug32788301
  collation_loader
  cost_estimate
  dbug
  decimal
  dns_srv_data
  dphyp
  dynarray
  filesort_buffer
  filesort_compare
  filesort_mergechunk
  inplace_vector
  integer_digits
  intrusive_list_iterator
  key
  like_range
  m_string
  mdl
  mem_root_deque
  mutex_lock
  my_alloc
  my_bitmap
  my_error
  my_fileutils
  my_gcvt
  my_murmur3
<<<<<<< HEAD
  my_rcu_lock
=======
<<<<<<< HEAD
=======
  my_qsort_vs_stdsort
  my_regex
  my_snprintf
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  my_thread
  my_timer
  mysys_base64
  mysys_lf
  mysys_my_b_vprintf
  mysys_my_checksum
  mysys_my_getopt
  mysys_my_getpw
  mysys_my_loadpath
  mysys_my_malloc
  mysys_my_pwrite
  mysys_my_rdtsc
  mysys_my_read
  mysys_my_symlink
  mysys_my_time
  mysys_my_write
  mysys_pathfuncs
  opt_recperkey
  overflow_bitset
  partitioned_rwlock
  pattern_matcher
  prealloced_array
  priority_queue
  pump_object_filter
  record_buffer
  sql_class_header
  sql_list
  sql_plist
  sql_string
  stl_alloc
  stream_cipher
  strings_skip_trailing
  strings_strnxfrm
  strings_utf8
  strings_valid_check
  strtod
  strtoll
  template_utils
  thread_utils
  timespec
  unhex
  val_int_compare
  varlen_sort
  )
<<<<<<< HEAD
=======
 
<<<<<<< HEAD
IF(WIN32)
  LIST(APPEND TESTS win_tests)
ENDIF()
>>>>>>> pr/231

SET(ALL_SMALL_TESTS)
FOREACH(test ${TESTS})
  LIST(APPEND ALL_SMALL_TESTS ${test}-t.cc)
ENDFOREACH()

<<<<<<< HEAD
# Add tests (link them with gunit/gmock libraries and the server libraries)
=======
=======
IF (UNIX)
  LIST(APPEND TESTS path auth_utils)
ENDIF()

>>>>>>> upstream/cluster-7.6
# Add tests (link them with gunit/gmock libraries and the server libraries) 
>>>>>>> pr/231
SET(SERVER_TESTS
  bgc_ticket_manager
  character_set_deprecation
  compare_access_paths
  connect_joins
  copy_info
  create_field
  dd_cache
  dd_column_statistics
  dd_info_schema_native_func
  dd_pfs
  dd_properties
  dd_schema
  dd_sdi
  dd_string_type
  dd_table
  debug_sync
  decoy_user
  explain_filename
  field
  get_diagnostics
  gis_algos
  gis_area
  gis_buffer
  gis_difference
  gis_distance
  gis_frechet_distance
  gis_geometries
  gis_hausdorff_distance
  gis_intersection
  gis_is_simple
  gis_isvalid
  gis_line_interpolate_point
  gis_relops
  gis_rtree_support
  gis_setops
  gis_srs
  gis_symdifference
  gis_transform
  gis_union
  gis_wkb_parser
  gis_wkb_writer
  graph_simplification
  handler
  hash_join
  histograms
  histogram_selectivity
  hypergraph_optimizer
  initialize_password
  insert_delayed
  interesting_orders
  into_syntax
  item
  item_filter
  item_func_case
  item_func_now_local
<<<<<<< HEAD
  item_func_regexp
  item_json_func
=======
<<<<<<< HEAD
  item_json_func
  item_func_regexp
=======
  item_func_rlike
  item_timefunc
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
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
  log_timestamp
  make_sortkey
  mdl_sync
  my_decimal
  mysqld_funcs
  opt_costconstants
  opt_costmodel
  opt_guessrecperkey
  opt_range
  opt_ref
  opt_trace
  persisted_variables
  protocol_classic
  regexp_engine
  regexp_facade
  security_context
  segfault
  select_lex_visitor
  sha2_password
  sql_table
  subquery_syntax
  table_cache
  table_factor_syntax
  table_list
  tc_log_mmap
  temptable_allocator
  temptable_storage
  thd_manager
  union_syntax
  unique
  value_map
  walk_access_paths
  wild_case_compare
  ha_info_iterator
  xid_extract
  log_event_status_size
)

IF(MY_COMPILER_IS_GNU AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
  ADD_COMPILE_FLAGS(regexp_engine-t.cc COMPILE_FLAGS "-Wno-restrict")
ENDIF()

IF(WIN32)
  LIST(APPEND SERVER_TESTS win_tests)
ENDIF()

# Suppress -Wformat-overflow= for my_safe_snprintf(" %s ", nullptr)
MY_CHECK_CXX_COMPILER_WARNING("format-overflow" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(segfault-t.cc COMPILE_FLAGS ${HAS_WARN_FLAG})
ENDIF()

# Warnings about missing PGO profile data are not useful for unit tests.
DISABLE_MISSING_PROFILE_WARNING()

SET(ALL_LARGE_TESTS)
FOREACH(test ${SERVER_TESTS})
  LIST(APPEND ALL_LARGE_TESTS ${test}-t.cc)
ENDFOREACH()

SET(SQL_GUNIT_LIB_SOURCE
  ${CMAKE_SOURCE_DIR}/sql/filesort_utils.cc
  ${CMAKE_SOURCE_DIR}/sql/mdl.cc
  ${CMAKE_SOURCE_DIR}/sql/stream_cipher.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_list.cc
  ${CMAKE_SOURCE_DIR}/sql/stateless_allocator.cc
  ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc
  ${CMAKE_SOURCE_DIR}/sql/thr_malloc.cc
  ${CMAKE_SOURCE_DIR}/sql/join_optimizer/hypergraph.cc
  ${CMAKE_SOURCE_DIR}/sql/join_optimizer/online_cycle_finder.cc
  ${CMAKE_SOURCE_DIR}/sql/join_optimizer/overflow_bitset.cc
  ${CMAKE_SOURCE_DIR}/sql/locks/shared_spin_lock.cc
  )

ADD_LIBRARY(sqlgunitlib STATIC ${SQL_GUNIT_LIB_SOURCE})
ADD_DEPENDENCIES(sqlgunitlib GenError)
SET_TARGET_PROPERTIES(sqlgunitlib
  PROPERTIES COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}"
)

<<<<<<< HEAD
MYSQL_ADD_EXECUTABLE(merge_small_tests-t ${ALL_SMALL_TESTS}
  ADD_TEST merge_small_tests
  COMPILE_DEFINITIONS ${DISABLE_PSI_DEFINITIONS}
  ENABLE_EXPORTS
  LINK_LIBRARIES sqlgunitlib gunit_small
  )
=======
IF(WIN32)
  LIST(APPEND SERVER_TESTS win_tests)
ENDIF()

## Merging tests into fewer executables saves *a lot* of
## link time and disk space ...
OPTION(MERGE_UNITTESTS "Merge tests into one executable" ON)
IF (MERGE_UNITTESTS)
<<<<<<< HEAD
  MYSQL_ADD_EXECUTABLE(merge_small_tests-t ${ALL_SMALL_TESTS}
    ADD_TEST merge_small_tests)
=======
  SET(MERGE_LARGE_TESTS ${CMAKE_CURRENT_BINARY_DIR}/merge_large_tests.cc)
  SET(MERGE_SMALL_TESTS ${CMAKE_CURRENT_BINARY_DIR}/merge_small_tests.cc)
  SET_SOURCE_FILES_PROPERTIES(MERGE_SMALL_TESTS MERGE_LARGE_TESTS
    PROPERTIES GENERATED 1)

  ## BOOST_HEAP_ASSERT generates if () ; else ;
  IF(HAVE_EMPTY_BODY)
    ADD_COMPILE_FLAGS(
      ${MERGE_SMALL_TESTS}
      COMPILE_FLAGS "-Wno-empty-body"
    )
  ENDIF()
  # Fixes "C1128: number of sections exceeded object file format limit" in MSVC
  IF(WIN32)
    ADD_COMPILE_FLAGS(
      ${MERGE_LARGE_TESTS}
      COMPILE_FLAGS "/bigobj")
  ENDIF()

  # Suppress -Wformat-overflow= for my_safe_snprintf(" %s ", nullptr)
  IF(HAVE_FORMAT_OVERFLOW)
    ADD_COMPILE_FLAGS(
      ${MERGE_LARGE_TESTS}
      COMPILE_FLAGS "-Wno-format-overflow")
  ENDIF()

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
  ADD_COMPILE_FLAGS(
    ../../storage/example/ha_example.cc
    COMPILE_FLAGS "-DDISABLE_DTRACE"
  )
  ADD_EXECUTABLE(merge_large_tests-t ${SRC_FILES})

>>>>>>> upstream/cluster-7.6
  TARGET_LINK_LIBRARIES(merge_small_tests-t
    mysys mysys_ssl sqlgunitlib gunit_small)
  
  IF(WIN32)
    LIST(APPEND ALL_LARGE_TESTS ../../sql/nt_servc.cc)
  ENDIF()
  LIST(APPEND ALL_LARGE_TESTS ../../storage/example/ha_example.cc)
>>>>>>> pr/231

IF(WIN32)
  LIST(APPEND ALL_LARGE_TESTS ../../sql/nt_servc.cc)
ENDIF()
LIST(APPEND ALL_LARGE_TESTS ../../storage/example/ha_example.cc)

MYSQL_ADD_EXECUTABLE(merge_large_tests-t ${ALL_LARGE_TESTS}
  ADD_TEST merge_large_tests
  ENABLE_EXPORTS
  LINK_LIBRARIES gunit_large server_unittest_library
  )

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc
    COMPILE_DEFINITIONS ${DISABLE_PSI_DEFINITIONS}
    ENABLE_EXPORTS
    EXCLUDE_FROM_ALL
    LINK_LIBRARIES sqlgunitlib gunit_small
    SKIP_INSTALL
    )
ENDFOREACH()

# See comments about __builtin_memcmp in the source file.
IF(HAVE_NO_BUILTIN_MEMCMP)
  ADD_COMPILE_FLAGS(
    filesort_compare-t.cc
    COMPILE_FLAGS "-fno-builtin-memcmp"
    )
ENDIF()

ADD_COMPILE_FLAGS(
  hash_join-t.cc
  COMPILE_FLAGS -I${CMAKE_SOURCE_DIR}/extra/lz4 -I${BUNDLED_LZ4_PATH}
  )

FOREACH(test ${SERVER_TESTS})
  SET(SRC_FILES ${test}-t.cc)
  IF(WIN32)
    LIST(APPEND SRC_FILES $<TARGET_OBJECTS:my_nt_servc>)
  ENDIF()
  IF(test MATCHES "table_cache")
    LIST(APPEND SRC_FILES ../../storage/example/ha_example.cc)
  ENDIF()
<<<<<<< HEAD
  MYSQL_ADD_EXECUTABLE(${test}-t ${SRC_FILES}
    ENABLE_EXPORTS
    EXCLUDE_FROM_ALL
    LINK_LIBRARIES gunit_large server_unittest_library
    SKIP_INSTALL
    )
=======
  MYSQL_ADD_EXECUTABLE(${test}-t ${SRC_FILES} SKIP_INSTALL ${EXCLUDE_FROM_ALL})
  TARGET_LINK_LIBRARIES(${test}-t sql_main binlog rpl master slave sql_main)
  TARGET_LINK_LIBRARIES(${test}-t sql_main sql_gis sql_dd)
  TARGET_LINK_LIBRARIES(${test}-t gunit_large strings dbug mysys)
  TARGET_LINK_LIBRARIES(${test}-t
    sql_main binlog rpl master slave sql_gis sql_main sql_dd)
  TARGET_LINK_LIBRARIES(${test}-t ${ICU_LIBRARIES})

  IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
    TARGET_LINK_LIBRARIES(${test}-t perfschema)
  ENDIF()

  IF(NOT MERGE_UNITTESTS)
    ADD_TEST(${test} ${test}-t)
  ENDIF()
<<<<<<< HEAD
>>>>>>> pr/231
ENDFOREACH()

ADD_LIBRARY(rpl_channel_credentials_lib STATIC
  ${CMAKE_SOURCE_DIR}/sql/rpl_channel_credentials.cc
)

MYSQL_ADD_EXECUTABLE(rpl_channel_credentials-t rpl_channel_credentials-t.cc
  ENABLE_EXPORTS
  ADD_TEST rpl_channel_credentials
  LINK_LIBRARIES rpl_channel_credentials_lib gunit_small
)

ADD_LIBRARY(rpl_commit_order_queue_lib STATIC
  ${CMAKE_SOURCE_DIR}/sql/changestreams/apply/commit_order_queue.cc
)

MYSQL_ADD_EXECUTABLE(rpl_commit_order_queue-t rpl_commit_order_queue-t.cc
  ENABLE_EXPORTS
  ADD_TEST rpl_commit_order_queue
  LINK_LIBRARIES rpl_commit_order_queue_lib gunit_small sqlgunitlib
)

ADD_SUBDIRECTORY(ddl_rewriter)
ADD_SUBDIRECTORY(innodb)
ADD_SUBDIRECTORY(keyring)
ADD_SUBDIRECTORY(components/mysql_server)
ADD_SUBDIRECTORY(components/keyring_common)
ADD_SUBDIRECTORY(xplugin)
ADD_SUBDIRECTORY(group_replication)
ADD_SUBDIRECTORY(libmysqlgcs)
<<<<<<< HEAD
ADD_SUBDIRECTORY(temptable)
ADD_SUBDIRECTORY(binlogevents)
ADD_SUBDIRECTORY(memory)
ADD_SUBDIRECTORY(containers)
ADD_SUBDIRECTORY(locks)
ADD_SUBDIRECTORY(changestreams)
=======
=======

  # Suppress -Wformat-overflow= for my_safe_snprintf(" %s ", nullptr)
  IF(HAVE_FORMAT_OVERFLOW)
    ADD_COMPILE_FLAGS(
      segfault-t.cc
      COMPILE_FLAGS "-Wno-format-overflow")
  ENDIF()

  FOREACH(test ${SERVER_TESTS})
    SET(SRC_FILES ${test}-t.cc)
    IF(WIN32)
      LIST(APPEND SRC_FILES ../../sql/nt_servc.cc)
    ENDIF()
    IF(test MATCHES "table_cache")
      LIST(APPEND SRC_FILES ../../storage/example/ha_example.cc)
      ADD_COMPILE_FLAGS(
        ../../storage/example/ha_example.cc
        COMPILE_FLAGS "-DDISABLE_DTRACE"
      )
    ENDIF()
    ADD_EXECUTABLE(${test}-t ${SRC_FILES})
    TARGET_LINK_LIBRARIES(${test}-t sql binlog rpl master slave sql)
    TARGET_LINK_LIBRARIES(${test}-t gunit_large strings dbug regex mysys)
    TARGET_LINK_LIBRARIES(${test}-t sql binlog rpl master slave sql)

    IF(WITH_PERFSCHEMA_STORAGE_ENGINE)
      TARGET_LINK_LIBRARIES(${test}-t perfschema)
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

ADD_SUBDIRECTORY(innodb)
ADD_SUBDIRECTORY(keyring)
ADD_SUBDIRECTORY(locks)
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
