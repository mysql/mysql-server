# Copyright (C) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
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

IF(SOURCE_SUBLIBS)
  # Sourced by libmysqld/CMakeLists.txt in 5.1 only to get
  # NDBCLUSTER_SOURCES and NDBCLUSTER_LIBS, don't configure
  # again
ELSE()
  INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/ndb_configure.cmake)

  IF(NOT DEFINED WITH_ZLIB)
    # Hardcode use of the bundled zlib
    MESSAGE(STATUS "using bundled zlib (hardcoded)")
    SET(ZLIB_LIBRARY zlib)
    SET(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/zlib)
  ENDIF()

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
    ${CMAKE_CURRENT_BINARY_DIR}/include

    # Util library includes
    ${ZLIB_INCLUDE_DIR})

  # The root of storage/ndb/
  SET(NDB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

ENDIF()

OPTION(WITH_NDB_TEST
  "Include the NDB Cluster ndbapi test programs" OFF)
IF(WITH_NDB_TEST)
  MESSAGE(STATUS "Building NDB test programs")
ENDIF()

SET(WITH_NDB_PORT "" CACHE INTEGER
  "Default port used by NDB Cluster management server")
IF(WITH_NDB_PORT GREATER 0)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDB_PORT=\"${WITH_NDB_PORT}\"")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDB_PORT=\"${WITH_NDB_PORT}\"")
  MESSAGE ( STATUS "Setting management server port to ${WITH_NDB_PORT}" )
ENDIF()

OPTION(WITH_NDB_BINLOG
  "Disable NDB binlog" ON)

# Use same compiler defines as MySQL Server for debug compile
SET(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

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

IF(MSVC)
  # Disable some warnings for NDB build on Windows
  # 4355: 'this': used in base member initializer list
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4355")
  SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /wd4355")
ENDIF()

IF(NOT SOURCE_SUBLIBS)
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
  ../../sql/ha_ndbinfo.cc)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)

IF(EXISTS ${CMAKE_SOURCE_DIR}/storage/mysql_storage_engine.cmake)
  # Old plugin support on Windows only
  INCLUDE(${CMAKE_SOURCE_DIR}/storage/mysql_storage_engine.cmake)
  SET(NDBCLUSTER_LIBS ndbclient)
  MYSQL_STORAGE_ENGINE(NDBCLUSTER)
ELSE()
  # New plugin support, cross-platform
  MYSQL_ADD_PLUGIN(ndbcluster ${NDBCLUSTER_SOURCES} STORAGE_ENGINE
    DEFAULT MODULE_OUTPUT_NAME ha_ndb)
ENDIF()
