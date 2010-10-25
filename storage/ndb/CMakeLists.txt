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

INCLUDE(${CMAKE_ROOT}/Modules/FindJava.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/FindJNI.cmake)
IF(NOT EXISTS ${CMAKE_SOURCE_DIR}/cmake/)
  # No MySQL Server cmake support, use the version in storage/ndb/cmake
  INCLUDE("${CMAKE_SOURCE_DIR}/storage/ndb/cmake/libutils.cmake")
ENDIF()
INCLUDE("${CMAKE_SOURCE_DIR}/storage/ndb/config/type_JAVA.cmake")

SET(WITH_CLASSPATH ${WITH_CLASSPATH} CACHE STRING
  "Enable the classpath for MySQL Cluster Java Connector")

OPTION(WITH_NDB_TEST
  "Include the NDB Cluster ndbapi test programs" OFF)

SET(WITH_NDB_PORT "" CACHE INTEGER
  "Default port used by NDB Cluster management server")
IF(WITH_NDB_PORT GREATER 0)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDB_PORT=\"${WITH_NDB_PORT}\"")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDB_PORT=\"${WITH_NDB_PORT}\"")
  MESSAGE ( STATUS "Setting management server port to ${WITH_NDB_PORT}" )
ENDIF()

OPTION(WITH_NDB_BINLOG
  "Disable NDB binlog" ON)

OPTION(WITH_NDBMTD
  "Dont build the MySQL Cluster multithreadded data node" ON)

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

IF(JAVA_INCLUDE_PATH AND JAVA_INCLUDE_PATH2)
  MESSAGE(STATUS "Found JDK - enabling clusterj")
  SET(HAVE_JDK TRUE)
ELSE()
  MESSAGE(STATUS "Found no JDK - disabling clusterj")
  SET(HAVE_JDK FALSE)
ENDIF()

IF(JAVA_COMPILE AND JAVA_ARCHIVE)
  MESSAGE(STATUS "Found Java")
  SET(HAVE_JAVA TRUE)
ELSE()
  MESSAGE(STATUS "Found no Java")
  SET(HAVE_JAVA FALSE)
ENDIF()

IF("${WITH_CLASSPATH}" MATCHES "junit-(.+).jar")
  MESSAGE ( STATUS "FOUND JUNIT" )
  SET(HAVE_JUNIT TRUE)
ENDIF()

IF("${WITH_CLASSPATH}" MATCHES "openjpa-(.+).jar")
  MESSAGE ( STATUS "FOUND OpenJPA" )
  SET(HAVE_OPENJPA TRUE)
ENDIF()

IF(NOT SOURCE_SUBLIBS)
  ADD_SUBDIRECTORY(include)
  ADD_SUBDIRECTORY(src)
  ADD_SUBDIRECTORY(tools)
  ADD_SUBDIRECTORY(test)
  IF(HAVE_JDK)
    MESSAGE(STATUS "including ClusterJ")
    ADD_SUBDIRECTORY(clusterj)
  ENDIF(HAVE_JDK)
  IF(WITH_NDB_TEST)
    ADD_SUBDIRECTORY(src/cw/cpcd)
  ENDIF()
ENDIF()

IF(WITHOUT_PARTITION_STORAGE_ENGINE)
  MESSAGE(FATAL_ERROR "NDBCLUSTER can't be compiled without PARTITION")
ENDIF(WITHOUT_PARTITION_STORAGE_ENGINE)

SET(NDBCLUSTER_SOURCES
  ${CMAKE_SOURCE_DIR}/sql/ha_ndbcluster.cc
  ${CMAKE_SOURCE_DIR}/sql/ha_ndbcluster_cond.cc
  ${CMAKE_SOURCE_DIR}/sql/ha_ndbcluster_connection.cc
  ${CMAKE_SOURCE_DIR}/sql/ha_ndbcluster_binlog.cc
  ${CMAKE_SOURCE_DIR}/sql/ha_ndbinfo.cc)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)


IF(EXISTS ${CMAKE_SOURCE_DIR}/storage/mysql_storage_engine.cmake)
  # Old plugin support on Windows only
  INCLUDE("${CMAKE_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
  SET(NDBCLUSTER_LIBS ndbclient)
  MYSQL_STORAGE_ENGINE(NDBCLUSTER)
ELSE()
  # New plugin support, cross-platform
  MYSQL_ADD_PLUGIN(ndbcluster ${NDBCLUSTER_SOURCES} STORAGE_ENGINE
    DEFAULT MODULE_OUTPUT_NAME ha_ndb)
ENDIF()
