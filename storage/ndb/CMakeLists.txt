# Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.
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
      SET(MYSQL_CLUSTER_VERSION_MAJOR ${CMAKE_MATCH_1} CACHE INTERNAL
        "MySQL Cluster Major version calculated from MySQL version" FORCE)
      SET(MYSQL_CLUSTER_VERSION_MINOR ${CMAKE_MATCH_2} CACHE INTERNAL
        "MySQL Cluster Minor version calculated from MySQL version" FORCE)
      SET(MYSQL_CLUSTER_VERSION_BUILD ${CMAKE_MATCH_3} CACHE INTERNAL
        "MySQL Cluster Build version calculated from MySQL version" FORCE)
    ENDIF()

    # Finally set MYSQL_CLUSTER_VERSION to be used as an indicator
    # that this is a MySQL Cluster build, yay!
    SET(MYSQL_CLUSTER_VERSION ${cluster_version} CACHE INTERNAL
      "This is MySQL Cluster" FORCE)

  ENDIF()
ENDMACRO()


# Temporarily remove -Werror from compiler flags until
# storage/ndb/ can be built with it
STRING(REPLACE "-Werror" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
STRING(REPLACE "-Werror" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

IF (MYSQL_VERSION_ID LESS 50500)
  # Use same compiler defines as MySQL Server for debug compile
  MESSAGE(STATUS "Setting same debug compile defines")
  SET(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
  SET(CMAKE_C_FLAGS_DEBUG
      "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
ENDIF()

IF(SOURCE_SUBLIBS)
  # Sourced by libmysqld/CMakeLists.txt in 5.1 only to get
  # NDBCLUSTER_SOURCES and NDBCLUSTER_LIBS, don't configure
  # again
ELSE()

  NDB_CHECK_MYSQL_CLUSTER(${VERSION})

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
    ${ZLIB_INCLUDE_DIR}
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
  IF(WITH_NDB_DEBUG)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -DVM_TRACE -DNDB_DEBUG -DERROR_INSERT -DARRAY_GUARD -DACC_SAFE_QUEUE")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG} -DVM_TRACE -DNDB_DEBUG -DERROR_INSERT -DARRAY_GUARD -DACC_SAFE_QUEUE")
  ELSE()
    IF(WITH_ERROR_INSERT)
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DERROR_INSERT")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DERROR_INSERT")
    ENDIF()
  ENDIF()

  ADD_SUBDIRECTORY(include)
  ADD_SUBDIRECTORY(src)
  ADD_SUBDIRECTORY(tools)
  ADD_SUBDIRECTORY(test)
  IF(WITH_NDB_TEST)
    ADD_SUBDIRECTORY(src/cw/cpcd)
  ENDIF()
ENDIF()

IF(WITHOUT_PARTITION_STORAGE_ENGINE)
  MESSAGE(FATAL_ERROR "NDBCLUSTER can't be compiled without PARTITION")
ENDIF(WITHOUT_PARTITION_STORAGE_ENGINE)

SET(NDBCLUSTER_SOURCES
  ../../sql/ha_ndbcluster.cc
  ../../sql/ha_ndbcluster_cond.cc
  ../../sql/ha_ndbcluster_connection.cc
  ../../sql/ha_ndbcluster_binlog.cc
  ../../sql/ha_ndb_index_stat.cc
  ../../sql/ha_ndbinfo.cc
  ../../sql/ndb_mi.cc
  ../../sql/ndb_conflict_trans.cc
  ../../sql/ndb_component.cc
  ../../sql/ndb_repl_tab.cc)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)

IF(EXISTS ${CMAKE_SOURCE_DIR}/storage/mysql_storage_engine.cmake)
  # Old plugin support on Windows only
  INCLUDE(${CMAKE_SOURCE_DIR}/storage/mysql_storage_engine.cmake)
  SET(NDBCLUSTER_LIBS ndbclient)
  MYSQL_STORAGE_ENGINE(NDBCLUSTER)
ELSE()
  # New plugin support, cross-platform
  
  # NDB is DEFAULT plugin in MySQL Cluster
  SET(is_default_plugin "")
  IF(MYSQL_CLUSTER_VERSION)
    SET(is_default_plugin "DEFAULT")
  ENDIF()

  MYSQL_ADD_PLUGIN(ndbcluster ${NDBCLUSTER_SOURCES} STORAGE_ENGINE
    ${is_default_plugin} STATIC_ONLY RECOMPILE_FOR_EMBEDDED
    LINK_LIBRARIES ndbclient)

  IF (NOT MCP_BUG58158)
    IF(WITH_EMBEDDED_SERVER)
      # Kludge libndbclient into variable used when creating libmysqld
      SET (MYSQLD_STATIC_PLUGIN_LIBS ${MYSQLD_STATIC_PLUGIN_LIBS}
	ndbclient CACHE INTERNAL "" FORCE)
    ENDIF()
  ENDIF()
ENDIF()
