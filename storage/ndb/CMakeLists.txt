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

MESSAGE(STATUS "Using cmake version ${CMAKE_VERSION}")
    
# Check if this is MySQL Cluster build i.e the MySQL Server
# version string ends in -ndb-Y.Y.Y[-status]    
MACRO(NDB_CHECK_MYSQL_CLUSTER version_string)

  IF(${version_string} MATCHES "(.*)-ndb-(.*)")
    SET(mysql_version ${CMAKE_MATCH_1})
    SET(cluster_version ${CMAKE_MATCH_2})
    
    MESSAGE(STATUS  "This is MySQL Cluster ${cluster_version}")
    
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

    # Finally set MYSQL_CLUSTER_VERSION to be used as an indicator
    # that this is a MySQL Cluster build, yay!
    SET(MYSQL_CLUSTER_VERSION ${cluster_version})
  ENDIF()
ENDMACRO()


# Temporarily remove -Werror from compiler flags until
# storage/ndb/ can be built with it
IF(CMAKE_C_FLAGS)
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDIF()
IF(CMAKE_CXX_FLAGS)
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
ENDIF()

NDB_CHECK_MYSQL_CLUSTER(${VERSION})

#
# Add the ndbcluster plugin
#
SET(NDBCLUSTER_SOURCES
  ../../sql/ha_ndbcluster.cc
  ../../sql/ha_ndbcluster_cond.cc
  ../../sql/ha_ndbcluster_connection.cc
  ../../sql/ha_ndbcluster_binlog.cc
  ../../sql/ha_ndb_index_stat.cc
  ../../sql/ha_ndbinfo.cc
  ../../sql/ndb_local_connection.cc
  ../../sql/ndb_share.cc
  ../../sql/ndb_thd.cc
  ../../sql/ndb_thd_ndb.cc
  ../../sql/ndb_global_schema_lock.cc
  ../../sql/ndb_mi.cc
)

# Inlude directories used when building ha_ndbcluster
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)

IF(NOT MYSQL_CLUSTER_VERSION)
 # Online alter table not supported in non MySQL Cluster
 # versions yet, compile ndbcluster without online alter support
 ADD_DEFINITIONS(-DNDB_WITHOUT_ONLINE_ALTER)
 # Distributed privilege tables not supported in non
 # MySQL Cluster version yet, compile ndbcluster without dist priv
 ADD_DEFINITIONS(-DNDB_WITHOUT_DIST_PRIV)
 # Pushdown of join queries not supported in non
 # MySQL Cluster version yet, compile ndbcluster without it
 ADD_DEFINITIONS(-DNDB_WITHOUT_JOIN_PUSHDOWN)
 # COLUMN_FORMAT not supported in non MySQL Cluster version yet 
 ADD_DEFINITIONS(-DNDB_WITHOUT_COLUMN_FORMAT)
 # Global schema lock hook not available, compile without
 ADD_DEFINITIONS(-DNDB_WITHOUT_GLOBAL_SCHEMA_LOCK)
ENDIF()

# NDB is DEFAULT plugin in MySQL Cluster
SET(is_default_plugin "")
IF(MYSQL_CLUSTER_VERSION)
  SET(is_default_plugin "DEFAULT")
ENDIF()

IF(NOT WITHOUT_SERVER)
  MYSQL_ADD_PLUGIN(ndbcluster ${NDBCLUSTER_SOURCES} STORAGE_ENGINE
    ${is_default_plugin} STATIC_ONLY RECOMPILE_FOR_EMBEDDED
    LINK_LIBRARIES ndbclient)
ENDIF()

#
# Add NDB binaries if ndbcluster is built
#
IF (NOT WITH_NDBCLUSTER)
  # Not building NDB 
  MESSAGE(STATUS "Not building NDB")
  RETURN()
ENDIF()

MESSAGE(STATUS "Building NDB")

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
  ${READLINE_INCLUDE_DIR})

# The root of storage/ndb/
SET(NDB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

OPTION(WITH_NDB_TEST
  "Include the NDB Cluster ndbapi test programs" OFF)
IF(WITH_NDB_TEST)
  MESSAGE(STATUS "Building NDB test programs")
ENDIF()

OPTION(WITH_NDB_BINLOG
  "Disable NDB binlog" ON)

OPTION(WITH_ERROR_INSERT
  "Enable error injection in MySQL Cluster" OFF)

OPTION(WITH_NDB_DEBUG
  "Disable special ndb debug features" OFF)

SET(NDB_CCFLAGS "")
IF(DEFINED WITH_NDB_CCFLAGS)
  SET(NDB_CCFLAGS "${WITH_NDB_CCFLAGS}")
ENDIF() 

IF(WITH_NDB_DEBUG OR CMAKE_BUILD_TYPE MATCHES "Debug")
  SET(NDB_CCFLAGS "${NDB_CCFLAGS} -DVM_TRACE -DNDB_DEBUG -DERROR_INSERT -DARRAY_GUARD -DACC_SAFE_QUEUE -DAPI_TRACE")
ELSE()
  IF(WITH_ERROR_INSERT)
    SET(NDB_CCFLAGS "${NDB_CCFLAGS} -DERROR_INSERT")
  ENDIF()
  SET(NDB_CCFLAGS "${NDB_CCFLAGS} -DNDEBUG")
ENDIF()

SET(NDB_CCFLAGS "${NDB_CCFLAGS} $ENV{NDB_EXTRA_FLAGS}") 
MESSAGE(STATUS "Using extra FLAGS for ndb: \"${NDB_CCFLAGS}\"") 

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NDB_CCFLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NDB_CCFLAGS}")

# Check for Java and JDK needed by ndbjtie and clusterj
INCLUDE(FindJava)
INCLUDE(FindJNI)
INCLUDE("${CMAKE_SOURCE_DIR}/storage/ndb/config/type_JAVA.cmake")

IF(JAVA_COMPILE AND JAVA_ARCHIVE)
  MESSAGE(STATUS "Found Java")
  SET(HAVE_JAVA TRUE)
ELSE()
  MESSAGE(STATUS "Could not find Java")
  SET(HAVE_JAVA FALSE)
ENDIF()

IF(JAVA_INCLUDE_PATH AND JAVA_INCLUDE_PATH2)
  MESSAGE(STATUS "Found JDK")
  SET(HAVE_JDK TRUE)
ELSE()
  MESSAGE(STATUS "Could not find JDK")
  SET(HAVE_JDK FALSE)
ENDIF()

SET(WITH_CLASSPATH ${WITH_CLASSPATH} CACHE STRING
  "Enable the classpath for MySQL Cluster Java Connector")

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(test)
IF(WITH_NDB_TEST)
  ADD_SUBDIRECTORY(src/cw/cpcd)
ENDIF()
IF (HAVE_JDK)
  ADD_SUBDIRECTORY(clusterj)
ENDIF()

IF(WITHOUT_PARTITION_STORAGE_ENGINE)
  MESSAGE(FATAL_ERROR "NDBCLUSTER can't be compiled without PARTITION")
ENDIF(WITHOUT_PARTITION_STORAGE_ENGINE)
