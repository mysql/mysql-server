# Copyright (C) 2007 MySQL ABVE
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

INCLUDE("${CMAKE_SOURCE_DIR}/storage/ndb/cmake/libutils.cmake")
INCLUDE("${CMAKE_SOURCE_DIR}/storage/ndb/config/type_JAVA.cmake")

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DVM_TRACE -DNDB_DEBUG -DERROR_INSERT -DARRAY_GUARD")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DVM_TRACE -DNDB_DEBUG -DERROR_INSERT -DARRAY_GUARD")

# Disable some warnings for NDB build:
# 4355: 'this': used in base member initializer list

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4355")
SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /wd4355")

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

IF(${WITH_CLASSPATH} MATCHES "junit-(.+).jar")
  MESSAGE ( STATUS "FOUND JUNIT" )
  SET(HAVE_JUNIT TRUE)
ENDIF()

IF(${WITH_CLASSPATH} MATCHES "openjpa-(.+).jar")
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

INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include)
SET(NDBCLUSTER_SOURCES
	../../sql/ha_ndbcluster.cc
	../../sql/ha_ndbcluster_cond.cc
	../../sql/ha_ndbcluster_connection.cc
	../../sql/ha_ndbcluster_binlog.cc
	../../sql/ha_ndbinfo.cc)
SET(NDBCLUSTER_LIBS ndbclient)
MYSQL_STORAGE_ENGINE(NDBCLUSTER)
