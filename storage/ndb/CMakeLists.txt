# Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.
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

# Add both MySQL and NDB cmake repositories to search path
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    ${CMAKE_SOURCE_DIR}/storage/ndb/cmake)
INCLUDE(ndb_add_target_property)
INCLUDE(ndb_add_test)

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

# Temporarily remove -Werror from compiler flags until
# storage/ndb/ can be built with it
IF(CMAKE_C_FLAGS)
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDIF()
IF(CMAKE_CXX_FLAGS)
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
ENDIF()

# Disable specific types of warnings for storage/ndb
# if the compiler supports the flag
IF(CMAKE_COMPILER_IS_GNUCXX)
  INCLUDE(CheckCXXCompilerFlag)
  FOREACH(warning "unused-but-set-variable" "strict-aliasing")
    STRING(REPLACE "-" "_" warning_ ${warning})
    STRING(TOUPPER ${warning_} WARNING)
    CHECK_CXX_COMPILER_FLAG("-Wno-${warning}" HAVE_${WARNING})
    IF(HAVE_${WARNING})
      MESSAGE(STATUS "Disabling -W${warning} warning")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${warning}")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${warning}")
    ENDIF()
  ENDFOREACH()
ENDIF()

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
)

# Include directories used when building ha_ndbcluster
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)

IF(NOT MYSQL_CLUSTER_VERSION)
 # The function 'make_db_list' is not available in non
 # MySQL Cluster version yet, compile ndbcluster without it
 ADD_DEFINITIONS(-DNDB_WITHOUT_MAKE_DB_LIST)
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

IF(NOT WITHOUT_SERVER)
  MYSQL_ADD_PLUGIN(ndbcluster ${NDBCLUSTER_SOURCES} STORAGE_ENGINE
    ${is_default_plugin} STATIC_ONLY RECOMPILE_FOR_EMBEDDED
    LINK_LIBRARIES ndbclient_static)
ENDIF()

#
# Build NDB only if MYSQL_ADD_PLUGIN has decided(by looking at various
# cmake args same as all other plugins have) to build ndbcluster(our handler)
#
IF (NOT WITH_NDBCLUSTER)
  MESSAGE(STATUS "Not building NDB")
  RETURN()
ENDIF()

# Add HAVE_NDB_BINLOG to the list of compile definitions used when compiling
# the ndbcluster plugin library(NOTE! there is also ndbcluster_embedded which
# is compiled without this define)
NDB_ADD_TARGET_PROPERTY(ndbcluster COMPILE_DEFINITIONS "HAVE_NDB_BINLOG")

IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
  MESSAGE(STATUS "Building NDB 32-bit")
ELSE()
  MESSAGE(STATUS "Building NDB 64-bit")
ENDIF()

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/ndb_configure.cmake)

INCLUDE_DIRECTORIES(
  # MySQL Server includes
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include

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

  # Util library includes
  ${EDITLINE_INCLUDE_DIR})

# The root of storage/ndb/
SET(NDB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

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
  ENDIF()
ENDIF()

OPTION(WITH_NDB_BINLOG
  "Disable NDB binlog" ON)

OPTION(WITH_ERROR_INSERT
  "Enable error injection in NDB" OFF)

OPTION(WITH_NDB_DEBUG
  "Enable special ndb NDB features(including ERROR_INSERT)" OFF)

IF(WITH_NDB_DEBUG OR CMAKE_BUILD_TYPE MATCHES "Debug")
  MESSAGE(STATUS "Turning on special NDB debug features")
  ADD_DEFINITIONS(-DVM_TRACE)
  ADD_DEFINITIONS(-DNDB_DEBUG)
  ADD_DEFINITIONS(-DERROR_INSERT)
  ADD_DEFINITIONS(-DARRAY_GUARD)
  ADD_DEFINITIONS(-DACC_SAFE_QUEUE)
  ADD_DEFINITIONS(-DAPI_TRACE)
ELSE()
  IF(WITH_ERROR_INSERT)
    ADD_DEFINITIONS(-DERROR_INSERT)
  ENDIF()
  ADD_DEFINITIONS(-DNDEBUG)
ENDIF()

# Set extra compiler switches from WITH_NDB_CCFLAGS
SET(WITH_NDB_CCFLAGS "" CACHE STRING "Extra compiler flags for NDB")
IF(WITH_NDB_CCFLAGS)
  MESSAGE(STATUS "Appending extra compiler flags for NDB: \"${WITH_NDB_CCFLAGS}\"")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WITH_NDB_CCFLAGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WITH_NDB_CCFLAGS}")
ENDIF()

# Set extra compiler switches from environment variable NDB_EXTRA_FLAGS
SET(ndb_flags_env $ENV{NDB_EXTRA_FLAGS})
IF(ndb_flags_env)
  MESSAGE(STATUS "Appending extra compiler flags for NDB from "
                 "environment: \"${ndb_flags_env}\"")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ndb_flags_env}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ndb_flags_env}")
ENDIF()

SET(HAVE_JAVA FALSE)
SET(HAVE_JDK FALSE)
SET(WITH_NDB_JAVA_DEFAULT ON)
# Check if the default for WITH_NDB_JAVA should be overriden
# by environment variable
IF (DEFINED ENV{WITH_NDB_JAVA_DEFAULT})
  SET(WITH_NDB_JAVA_DEFAULT $ENV{WITH_NDB_JAVA_DEFAULT})
  MESSAGE(STATUS "Default value for WITH_NDB_JAVA set to ${WITH_NDB_JAVA_DEFAULT}")
ELSE()
  # Check if the default for WITH_NDB_JAVA should be
  # magically overriden
  SET(magic_value $ENV{PRODUCT_ID})
  IF (magic_value STREQUAL "bootstrap-cmake-tar-gz" OR
      magic_value STREQUAL "source-dist-tar-gz")
    SET(WITH_NDB_JAVA_DEFAULT OFF)
    MESSAGE(STATUS "Default value for WITH_NDB_JAVA set to 0 since "
                   "bootstrapping was detected")
  ENDIF()
ENDIF()
OPTION(WITH_NDB_JAVA
  "Include NDB Cluster Java components" ${WITH_NDB_JAVA_DEFAULT})
IF(WITH_NDB_JAVA)
  #
  # Check for Java and JDK needed by ndbjtie and clusterj
  #

  # Print value of JAVA_HOME if set
  IF(DEFINED ENV{JAVA_HOME})
    MESSAGE(STATUS "Looking for Java in JAVA_HOME=" $ENV{JAVA_HOME} " "
                   "and standard locations")
  ELSE()
    MESSAGE(STATUS "Looking for Java in standard locations")
  ENDIF()

  FIND_PACKAGE(Java 1.6 COMPONENTS Development)
  IF(NOT JAVA_FOUND)
    IF(DEFINED ENV{JAVA_HOME})
      # Could not find Java in the specific location set by JAVA_HOME
      # or in standard paths, don't search further
      MESSAGE(FATAL_ERROR "Could NOT find Java: neither in specified "
                          "JAVA_HOME=" $ENV{JAVA_HOME} " or standard location")
    ENDIF()

    #
    # Continue looking for Java in some additional
    # well known locations
    #

    # Prefer Java with same bit size as current build
    SET(_bit_suffix)
    IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
      SET(_bit_suffix "-64")
    ENDIF()

    # Use well known standard base
    SET(_base_path /usr/local/java/)
    IF(WINDOWS)
      SET(_base_path C:\\java\\)
    ENDIF()

    # Search for version in specified order
    SET(_preferred_versions
       1.8
       1.7
       1.6)

    FOREACH(_version ${_preferred_versions})
      SET(_path ${_base_path}jdk${_version}${_bit_suffix})
      MESSAGE(STATUS "Looking for Java in ${_path}...")
      SET(ENV{JAVA_HOME} ${_path})
      FIND_PACKAGE(Java ${_version} COMPONENTS Development)
      IF(JAVA_FOUND)
        # Found java, no need to search further
        MESSAGE(STATUS "Found Java in ${_path}")
        BREAK()
      ENDIF()
    ENDFOREACH()

    IF(NOT JAVA_FOUND)
      # Could not find Java in well known locations either
      MESSAGE(FATAL_ERROR "Could NOT find suitable version of Java")
    ENDIF()

  ENDIF()

  MESSAGE(STATUS "Java_VERSION: ${Java_VERSION}")
  MESSAGE(STATUS "Java_VERSION_STRING: ${Java_VERSION_STRING}")
  MESSAGE(STATUS "JAVA_RUNTIME: ${JAVA_RUNTIME}")
  MESSAGE(STATUS "JAVA_COMPILE: ${JAVA_COMPILE}")
  MESSAGE(STATUS "JAVA_ARCHIVE: ${JAVA_ARCHIVE}")
  NDB_REQUIRE_VARIABLE(JAVA_RUNTIME)
  NDB_REQUIRE_VARIABLE(JAVA_COMPILE)
  NDB_REQUIRE_VARIABLE(JAVA_ARCHIVE)

  # Help FindJNI by setting JAVA_HOME (if not already set)
  # to point at the java found above
  IF(NOT DEFINED ENV{JAVA_HOME})
    # Convert to realpath
    GET_FILENAME_COMPONENT(java_home ${JAVA_COMPILE} REALPATH)
    # Remove filename
    GET_FILENAME_COMPONENT(java_home ${java_home} PATH)
    # Remove dir
    GET_FILENAME_COMPONENT(java_home ${java_home} PATH)
    MESSAGE(STATUS "Setting JAVA_HOME=${java_home}")
    SET(ENV{JAVA_HOME} ${java_home})
  ENDIF()

  FIND_PACKAGE(JNI REQUIRED)
  MESSAGE(STATUS "JNI_FOUND: ${JNI_FOUND}")
  MESSAGE(STATUS "JNI_INCLUDE_DIRS: ${JNI_INCLUDE_DIRS}")
  MESSAGE(STATUS "JNI_LIBRARIES: ${JNI_LIBRARIES}")
  NDB_REQUIRE_VARIABLE(JNI_INCLUDE_DIRS)

  INCLUDE("${NDB_SOURCE_DIR}/config/type_JAVA.cmake")
  SET(HAVE_JAVA TRUE)
  SET(HAVE_JDK TRUE)
  SET(WITH_CLASSPATH ${WITH_CLASSPATH} CACHE STRING
    "Enable the classpath for MySQL Cluster Java Connector")
ELSE(WITH_NDB_JAVA)
  MESSAGE(STATUS "Excluding Cluster Java components")
ENDIF(WITH_NDB_JAVA)

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(test)
IF(WITH_NDB_TEST)
  ADD_SUBDIRECTORY(src/cw/cpcd)
ENDIF()
IF (HAVE_JAVA AND HAVE_JDK)
  ADD_SUBDIRECTORY(clusterj)
ENDIF()
ADD_SUBDIRECTORY(memcache)
ADD_SUBDIRECTORY(mcc)

SET(WITH_NDB_NODEJS_DEFAULT ON)

# Check if the default for WITH_NDB_NODEJS should be overriden
# by environment variable
IF(DEFINED ENV{WITH_NDB_NODEJS_DEFAULT})
  SET(WITH_NDB_NODEJS_DEFAULT $ENV{WITH_NDB_NODEJS_DEFAULT})
  MESSAGE(STATUS "Default value for WITH_NDB_NODEJS set to ${WITH_NDB_NODEJS_DEFAULT}")
ENDIF(DEFINED ENV{WITH_NDB_NODEJS_DEFAULT})
OPTION(WITH_NDB_NODEJS
  "Include NDB Cluster node.js components" ${WITH_NDB_NODEJS_DEFAULT})
IF(WITH_NDB_NODEJS)
  ADD_SUBDIRECTORY(nodejs)
ENDIF(WITH_NDB_NODEJS)

OPTION(WITH_NDBAPI_EXAMPLES "Build the NdbApi example programs" OFF)
IF(WITH_NDBAPI_EXAMPLES)
  MESSAGE(STATUS "Building the NdbApi example programs")
  ADD_SUBDIRECTORY(ndbapi-examples)
ENDIF()

IF(WITHOUT_PARTITION_STORAGE_ENGINE)
  MESSAGE(FATAL_ERROR "NDBCLUSTER can't be compiled without PARTITION")
ENDIF(WITHOUT_PARTITION_STORAGE_ENGINE)

ADD_SUBDIRECTORY(wix)

IF(WITH_UNIT_TESTS)
  FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/unittest/ndb)
ENDIF()
