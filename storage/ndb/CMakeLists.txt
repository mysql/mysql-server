<<<<<<< HEAD
# Copyright (c) 2008, 2022, Oracle and/or its affiliates.
=======
# Copyright (c) 2008, 2021, Oracle and/or its affiliates.
>>>>>>> pr/231
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

# Add both MySQL and NDB cmake repositories to search path
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    ${CMAKE_SOURCE_DIR}/storage/ndb/cmake)
INCLUDE(ndb_add_target_property)
INCLUDE(ndb_add_test)
INCLUDE(ndb_add_executable)

<<<<<<< HEAD
DISABLE_MISSING_PROFILE_WARNING()
=======
MESSAGE(STATUS "Using cmake version ${CMAKE_VERSION}")

# Check if this is MySQL Cluster build i.e the MySQL Server
# version string ends in -ndb-Y.Y.Y[-status]
MACRO(NDB_CHECK_MYSQL_CLUSTER version_string)

  IF(${version_string} MATCHES "(.*)-ndb-(.*)")
    SET(mysql_version ${CMAKE_MATCH_1})
    SET(cluster_version ${CMAKE_MATCH_2})

    # Sanity check that the mysql_version matches precalcuated
    # values from higher level scripts
    IF(NOT ${mysql_version} EQUAL "${MYSQL_NO_DASH_VERSION}")
      MESSAGE(FATAL_ERROR "Sanity check of version_string failed!")
    ENDIF()

    # Split the cluster_version further into Y.Y.Y subcomponents
    IF(${cluster_version} MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
      SET(MYSQL_CLUSTER_VERSION_MAJOR ${CMAKE_MATCH_1})
      SET(MYSQL_CLUSTER_VERSION_MINOR ${CMAKE_MATCH_2})
      SET(MYSQL_CLUSTER_VERSION_BUILD ${CMAKE_MATCH_3})
    ENDIF()

    MESSAGE(STATUS  "This is MySQL Cluster ${cluster_version}")
    # Finally set MYSQL_CLUSTER_VERSION to be used as an indicator
    # that this is a MySQL Cluster build, yay!
    SET(MYSQL_CLUSTER_VERSION ${cluster_version})
  ENDIF()
ENDMACRO()

NDB_CHECK_MYSQL_CLUSTER(${VERSION})

<<<<<<< HEAD
=======
# Disable specific types of warnings for storage/ndb
# if the compiler supports the flag
IF(CMAKE_COMPILER_IS_GNUCXX)
  INCLUDE(CheckCXXCompilerFlag)

  FOREACH(warning
      "class-memaccess"
      )
    STRING(REPLACE "-" "_" warning_ ${warning})
    STRING(TOUPPER ${warning_} WARNING)
    CHECK_CXX_COMPILER_FLAG("-W${warning}" HAVE_${WARNING})
    IF(HAVE_${WARNING})
      MESSAGE(STATUS "Disabling -W${warning} warning for building NDB")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${warning}")
    ENDIF()
  ENDFOREACH()

  FOREACH(warning
      "strict-aliasing"
      "unused-but-set-variable"
      # Downgrade -Werror to warning for "may be used uninitialized"
      # and "is used uninitialized in this function"
      "error=maybe-uninitialized"
      "error=uninitialized"
      # Downgrade placement-new as part of Bug #24822203
      "error=placement-new"
      )
    STRING(REPLACE "-" "_" warning_ ${warning})
    STRING(TOUPPER ${warning_} WARNING)
    CHECK_CXX_COMPILER_FLAG("-W${warning}" HAVE_${WARNING})
    IF(HAVE_${WARNING})
      MESSAGE(STATUS "Disabling -W${warning} warning for building NDB")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${warning}")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${warning}")
    ENDIF()
  ENDFOREACH()
ENDIF()

>>>>>>> upstream/cluster-7.6
#
# Add the ndbcluster plugin
#
SET(NDBCLUSTER_SOURCES
  ../../sql/ha_ndbcluster.cc
  ../../sql/ha_ndbcluster_cond.cc
  ../../sql/ha_ndbcluster_push.cc
  ../../sql/ha_ndbcluster_connection.cc
  ../../sql/ha_ndbcluster_binlog.cc
  ../../sql/ha_ndb_index_stat.cc
  ../../sql/ha_ndb_ddl_fk.cc
  ../../sql/ha_ndbinfo.cc
  ../../sql/ndb_local_connection.cc
  ../../sql/ndb_share.cc
  ../../sql/ndb_thd.cc
  ../../sql/ndb_thd_ndb.cc
  ../../sql/ndb_global_schema_lock.cc
  ../../sql/ndb_mi.cc
  ../../sql/ndb_conflict_trans.cc
  ../../sql/ndb_anyvalue.cc
  ../../sql/ndb_ndbapi_util.cc
  ../../sql/ndb_binlog_extra_row_info.cc
  ../../sql/ndb_event_data.cc
  ../../sql/ndb_schema_object.cc
  ../../sql/ndb_schema_dist.cc
  ../../sql/ndb_component.cc
  ../../sql/ndb_local_schema.cc
  ../../sql/ndb_repl_tab.cc
  ../../sql/ndb_conflict.cc
  ../../sql/ndb_tdc.cc
  ../../sql/ndb_log.cc
  ../../sql/ndb_binlog_thread.cc
  ../../sql/ndb_name_util.cc
  ../../sql/ndb_bitmap.cc
  ../../sql/ndb_table_map.cc
  ../../sql/ndb_dd.cc
  ../../sql/ndb_modifiers.cc
  ../../sql/ndb_dd_client.cc
  ../../sql/ndb_dd_table.cc
  ../../sql/ndb_dd_sdi.cc
  ../../sql/ndb_plugin_reference.cc
  ../../sql/ndb_server_hooks.cc
  ../../sql/ndb_binlog_client.cc
)

# Include directories used when building ha_ndbcluster
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)

IF(NOT MYSQL_CLUSTER_VERSION)
ENDIF()

# NDB is DEFAULT plugin in MySQL Cluster
SET(is_default_plugin "")
OPTION(WITH_NDB_DEFAULT_PLUGIN_DETECT
  "Autodetect MySQL Cluster version and set ndbcluster as DEFAULT plugin" ON)
MARK_AS_ADVANCED(WITH_NDB_DEFAULT_PLUGIN_DETECT)
IF(WITH_NDB_DEFAULT_PLUGIN_DETECT)
  IF(MYSQL_CLUSTER_VERSION)
    # ndbcluster is DEFAULT plugin in MySQL Cluster
    SET(is_default_plugin "DEFAULT")
  ENDIF()
ENDIF()
>>>>>>> pr/231

IF(NOT WITHOUT_SERVER)
  IF(NOT WITH_NDB AND NOT DEFINED WITH_NDBCLUSTER_STORAGE_ENGINE)
    IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
      # Do not build ndbcluster plugin by default for 32-bit MySQL Server
      SET(WITH_NDBCLUSTER_STORAGE_ENGINE 0)
    ENDIF()
  ENDIF()
  #
  # Add the ndbcluster plugin
  #
  SET(NDBCLUSTER_SOURCES
    plugin/ha_ndbcluster.cc
    plugin/ha_ndbcluster_cond.cc
    plugin/ha_ndbcluster_push.cc
    plugin/ha_ndbcluster_connection.cc
    plugin/ha_ndbcluster_binlog.cc
    plugin/ha_ndb_index_stat.cc
    plugin/ha_ndb_ddl_fk.cc
    plugin/ha_ndbinfo.cc
    plugin/ha_ndbinfo_sql.cc
    plugin/ha_query_plan.cc
    plugin/ndb_local_connection.cc
    plugin/ndb_share.cc
    plugin/ndb_thd.cc
    plugin/ndb_thd_ndb.cc
    plugin/ndb_global_schema_lock.cc
    plugin/ndb_mi.cc
    plugin/ndb_conflict_trans.cc
    plugin/ndb_anyvalue.cc
    plugin/ndb_ndbapi_util.cc
    plugin/ndb_binlog_extra_row_info.cc
    plugin/ndb_event_data.cc
    plugin/ndb_schema_object.cc
    plugin/ndb_schema_dist.cc
    plugin/ndb_component.cc
    plugin/ndb_repl_tab.cc
    plugin/ndb_conflict.cc
    plugin/ndb_tdc.cc
    plugin/ndb_log.cc
    plugin/ndb_binlog_thread.cc
    plugin/ndb_name_util.cc
    plugin/ndb_bitmap.cc
    plugin/ndb_table_map.cc
    plugin/ndb_dd.cc
    plugin/ndb_modifiers.cc
    plugin/ndb_dd_client.cc
    plugin/ndb_dd_schema.cc
    plugin/ndb_dd_table.cc
    plugin/ndb_dd_sdi.cc
    plugin/ndb_plugin_reference.cc
    plugin/ndb_server_hooks.cc
    plugin/ndb_binlog_client.cc
    plugin/ndb_metadata.cc
    plugin/ndb_dd_disk_data.cc
    plugin/ndb_dd_upgrade_table.cc
    plugin/ndb_binlog_hooks.cc
    plugin/ndb_util_table.cc
    plugin/ndb_schema_dist_table.cc
    plugin/ndb_fk_util.cpp
    plugin/ndb_schema_trans_guard.cc
    plugin/ndb_create_helper.cc
    plugin/ndb_apply_status_table.cc
    plugin/ndb_metadata_change_monitor.cc
    plugin/ndb_schema_result_table.cc
    plugin/ndb_metadata_sync.cc
    plugin/ndb_ddl_transaction_ctx.cc
    plugin/ndb_record_layout.cc
    plugin/ndb_sql_metadata_table.cc
    plugin/ndb_stored_grants.cc
    plugin/ndb_dd_fk.cc
    plugin/ndb_pfs_init.cc
    plugin/ndb_pfs_table.cc
    plugin/ndb_sync_pending_objects_table.cc
    plugin/ndb_sync_excluded_objects_table.cc
    plugin/ndb_dd_sync.cc
    plugin/ndb_upgrade_util.cc
    plugin/ndb_mysql_services.cc
    plugin/ndb_table_stats.cc
    plugin/ndb_index_stat_head_table.cc
    plugin/ndb_index_stat_sample_table.cc
  )

  # Include directories used when building ha_ndbcluster
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)

  # This will add WITH_NDBCLUSTER_STORAGE_ENGINE to the CACHE,
  # unless it is explicitly disabled on the command line.
  MYSQL_ADD_PLUGIN(ndbcluster ${NDBCLUSTER_SOURCES} STORAGE_ENGINE
    DEFAULT STATIC_ONLY
    LINK_LIBRARIES ndbclient_static)

  # Sanity check that MYSQL_ADD_PLUGIN didn't decide to skip build
  IF (NOT WITH_NDBCLUSTER_STORAGE_ENGINE)
    IF(WITH_NDB)
      MESSAGE(FATAL_ERROR
        "Ndbcluster plugin required when building MySQL Cluster")
    ELSE()
      RETURN()
    ENDIF()
  ELSE()
    MESSAGE(STATUS "Building ndbcluster plugin")
  ENDIF()

  NDB_ADD_TEST("ndb_bitmap-t" "plugin/ndb_bitmap.cc" LIBS ndbgeneral)
  NDB_ADD_TEST("ndb_blobs_buffer-t" "plugin/ndb_blobs_buffer.cc" LIBS ndbgeneral)
ELSEIF(NOT WITH_NDB)
  IF(WITH_NDBCLUSTER_STORAGE_ENGINE)
    MESSAGE(WARNING "Skips building ndbcluster plugin, WITHOUT_SERVER=ON "
      "overrides WITH_NDBCLUSTER_STORAGE_ENGINE=ON")
  ENDIF()
  RETURN()
ENDIF()

# Disable strict-aliasing for storage/ndb
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING_APPEND(CMAKE_C_FLAGS   " -fno-strict-aliasing")
  STRING_APPEND(CMAKE_CXX_FLAGS " -fno-strict-aliasing")
ENDIF()

# Disable specific types of warnings for current directory, and subdirectories,
# if the compiler supports the flag
FOREACH(warning
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
    STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

IF(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  MESSAGE(WARNING "Only 64-bit build of NDB is supported")
ENDIF()

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/ndb_configure.cmake)

INCLUDE_DIRECTORIES(
  # NDB includes
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/util
  ${CMAKE_CURRENT_SOURCE_DIR}/include/portlib
  ${CMAKE_CURRENT_SOURCE_DIR}/include/debugger
  ${CMAKE_CURRENT_SOURCE_DIR}/include/transporter
  ${CMAKE_CURRENT_SOURCE_DIR}/include/kernel
  ${CMAKE_CURRENT_SOURCE_DIR}/include/mgmapi
  ${CMAKE_CURRENT_SOURCE_DIR}/include/mgmcommon
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ndbapi
  ${CMAKE_CURRENT_SOURCE_DIR}/include/logger
  ${CMAKE_CURRENT_BINARY_DIR}/include
<<<<<<< HEAD
)
=======

  # Util library includes
  ${EDITLINE_INCLUDE_DIR})
>>>>>>> upstream/cluster-7.6

SET(DEBUG_FLAGS
  -DACC_SAFE_QUEUE
  -DAPI_TRACE
  -DARRAY_GUARD
  -DERROR_INSERT
  -DNDB_DEBUG
  -DVM_TRACE
  )
SET(DEBUG_FLAGS_STRING "")
FOREACH(flag ${DEBUG_FLAGS})
  STRING_APPEND(DEBUG_FLAGS_STRING " ${flag}")
ENDFOREACH()

<<<<<<< HEAD
STRING_APPEND(CMAKE_C_FLAGS_DEBUG "${DEBUG_FLAGS_STRING}")
STRING_APPEND(CMAKE_CXX_FLAGS_DEBUG "${DEBUG_FLAGS_STRING}")
=======
SET(WITH_NDBAPI_EXAMPLES_DEFAULT OFF)
OPTION(WITH_NDB_TEST
  "Include the NDB Cluster ndbapi test programs" OFF)
IF(WITH_NDB_TEST)
  MESSAGE(STATUS "Building NDB test programs")
ELSE()
  # Turn on NDB tests automatically for gcov builds
  IF(ENABLE_GCOV AND NOT WIN32 AND NOT APPLE)
    MESSAGE(STATUS "Building NDB test programs for improved gcov coverage")
    SET(WITH_NDB_TEST ON)
  ENDIF()
  # Turn on NDB tests automatically for valgrind builds
  IF(WITH_VALGRIND)
    MESSAGE(STATUS "Building NDB test programs for improved valgrind coverage")
    SET(WITH_NDB_TEST ON)
    MESSAGE(STATUS "Building NdbApi examples for improved valgrind coverage")
    SET(WITH_NDBAPI_EXAMPLES_DEFAULT ON)
  ENDIF()
  # Turn on NDB tests automatically for "-devbld" debug builds
  # to increase test coverage
  IF(PACKAGE_SUFFIX STREQUAL "-devbld" AND
     CMAKE_BUILD_TYPE MATCHES "Debug")
    MESSAGE(STATUS "Building NDB test programs for \"-devbld\" debug build")
    SET(WITH_NDB_TEST ON)
    MESSAGE(STATUS "Building NdbApi examples for \"-devbld\" debug build")
    SET(WITH_NDBAPI_EXAMPLES_DEFAULT ON)
  ENDIF()
ENDIF()
>>>>>>> pr/231

OPTION(WITH_ERROR_INSERT
  "Enable error injection in NDB" OFF)
IF(WITH_ERROR_INSERT)
  ADD_DEFINITIONS(-DERROR_INSERT)
ENDIF()

OPTION(WITH_NDB_DEBUG
  "Enable special ndb NDB features(including ERROR_INSERT)" OFF)
IF(WITH_NDB_DEBUG)
  ADD_DEFINITIONS(${DEBUG_FLAGS})
ENDIF()

# Set extra compiler switches from WITH_NDB_CCFLAGS
SET(WITH_NDB_CCFLAGS "" CACHE STRING "Extra compiler flags for NDB")
IF(WITH_NDB_CCFLAGS)
  MESSAGE(STATUS
    "Appending extra compiler flags for NDB: \"${WITH_NDB_CCFLAGS}\"")
  STRING_APPEND(CMAKE_C_FLAGS   " ${WITH_NDB_CCFLAGS}")
  STRING_APPEND(CMAKE_CXX_FLAGS " ${WITH_NDB_CCFLAGS}")
ENDIF()

# Set extra compiler switches from environment variable NDB_EXTRA_FLAGS
SET(ndb_flags_env $ENV{NDB_EXTRA_FLAGS})
IF(ndb_flags_env)
  MESSAGE(STATUS "Appending extra compiler flags for NDB from "
                 "environment: \"${ndb_flags_env}\"")
  STRING_APPEND(CMAKE_C_FLAGS   " ${ndb_flags_env}")
  STRING_APPEND(CMAKE_CXX_FLAGS " ${ndb_flags_env}")
ENDIF()

# Moved here from configure.cmake
CHECK_FUNCTION_EXISTS (getrlimit HAVE_GETRLIMIT)

IF(WITH_ASAN)
  MESSAGE(STATUS "Skip building ClusterJ by default for ASAN builds")
  SET(WITH_NDB_JAVA_DEFAULT OFF)
ELSE()
  # Check if the default for WITH_NDB_JAVA should be
  # magically overriden
  SET(magic_value $ENV{PRODUCT_ID})
  IF (magic_value STREQUAL "bootstrap-cmake-tar-gz" OR
      magic_value STREQUAL "source-dist-tar-gz")
    SET(WITH_NDB_JAVA_DEFAULT OFF)
    MESSAGE(STATUS "Default value for WITH_NDB_JAVA set to 0 since "
                   "bootstrapping was detected")
  ELSE()
  SET(WITH_NDB_JAVA_DEFAULT ${WITH_NDB})
  ENDIF()
ENDIF()
OPTION(WITH_NDB_JAVA
  "Include NDB Cluster Java components" ${WITH_NDB_JAVA_DEFAULT})

IF(WITH_NDB_JAVA)
  INCLUDE(ndb_find_java)
ELSE(WITH_NDB_JAVA)
  MESSAGE(STATUS "Excluding Cluster Java components")
ENDIF(WITH_NDB_JAVA)

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(src)

IF(NOT WITH_NDB)
  RETURN()
ENDIF()

IF (OPENSSL_MAJOR_MINOR_FIX_VERSION VERSION_LESS "1.0.2")
  MESSAGE(WARNING
    "MySQL Cluster Transparent Data Encryption (TDE) not supported."
    "Needs OpenSSL 1.0.2 or newer.")
ENDIF()

SET(WITH_NDBAPI_EXAMPLES_DEFAULT OFF)
OPTION(WITH_NDB_TEST
  "Include the NDB Cluster ndbapi test programs" OFF)
IF(WITH_NDB_TEST)
  MESSAGE(STATUS "Building NDB test programs")
ELSE()
  # Turn on NDB tests automatically for gcov builds
  IF(ENABLE_GCOV AND NOT WIN32 AND NOT APPLE)
    MESSAGE(STATUS "Building NDB test programs for improved gcov coverage")
    SET(WITH_NDB_TEST ON)
    MESSAGE(STATUS "Building NdbApi examples for improved gcov coverage")
    SET(WITH_NDBAPI_EXAMPLES_DEFAULT ON)
  ENDIF()
  # Turn on NDB tests automatically for valgrind builds
  IF(WITH_VALGRIND)
    MESSAGE(STATUS "Building NDB test programs for improved valgrind coverage")
    SET(WITH_NDB_TEST ON)
    MESSAGE(STATUS "Building NdbApi examples for improved valgrind coverage")
    SET(WITH_NDBAPI_EXAMPLES_DEFAULT ON)
  ENDIF()
  # Turn on NDB tests automatically for "-devbld" debug builds
  # to increase test coverage
  IF(PACKAGE_SUFFIX STREQUAL "-devbld" AND
     CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
    MESSAGE(STATUS "Building NDB test programs for \"-devbld\" debug build")
    SET(WITH_NDB_TEST ON)
    MESSAGE(STATUS "Building NdbApi examples for \"-devbld\" debug build")
    SET(WITH_NDBAPI_EXAMPLES_DEFAULT ON)
  ENDIF()
  # Turn on build of NDB tests automatically for "-werror" builds
  IF(PACKAGE_SUFFIX STREQUAL "-werror")
    MESSAGE(STATUS "Building NDB test programs for \"-werror\" build")
    SET(WITH_NDB_TEST ON)
    MESSAGE(STATUS "Building NdbApi examples for \"-werror\" build")
    SET(WITH_NDBAPI_EXAMPLES_DEFAULT ON)
  ENDIF()
ENDIF()

ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(test)
IF(WITH_NDB_TEST)
  ADD_SUBDIRECTORY(src/cw/cpcd)
ENDIF()
IF(WITH_NDB_JAVA)
  ADD_SUBDIRECTORY(clusterj)
ENDIF()

<<<<<<< HEAD
SET(WITH_NDB_NODEJS_DEFAULT ON)

# Check if the default for WITH_NDB_NODEJS should be overriden
# by environment variable
IF(DEFINED ENV{WITH_NDB_NODEJS_DEFAULT})
  SET(WITH_NDB_NODEJS_DEFAULT $ENV{WITH_NDB_NODEJS_DEFAULT})
  MESSAGE(STATUS
    "Default value for WITH_NDB_NODEJS set to ${WITH_NDB_NODEJS_DEFAULT}")
ENDIF(DEFINED ENV{WITH_NDB_NODEJS_DEFAULT})

OPTION(WITH_NDB_NODEJS
  "Include NDB Cluster node.js components" ${WITH_NDB_NODEJS_DEFAULT})
IF(WITH_NDB_NODEJS)
  ADD_SUBDIRECTORY(nodejs)
ENDIF(WITH_NDB_NODEJS)

=======
>>>>>>> pr/231
OPTION(WITH_NDBAPI_EXAMPLES
  "Build the NdbApi example programs" ${WITH_NDBAPI_EXAMPLES_DEFAULT})
IF(WITH_NDBAPI_EXAMPLES)
  MESSAGE(STATUS "Building the NdbApi example programs")
  ADD_SUBDIRECTORY(ndbapi-examples)
ENDIF()

ADD_SUBDIRECTORY(wix)
