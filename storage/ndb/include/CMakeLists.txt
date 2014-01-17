# Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
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

# Check the size of our types and configure ndb_types.h
INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE(char NDB_SIZEOF_CHAR)
CHECK_TYPE_SIZE(short NDB_SIZEOF_SHORT)
CHECK_TYPE_SIZE(int NDB_SIZEOF_INT)
CHECK_TYPE_SIZE(long NDB_SIZEOF_LONG)
CHECK_TYPE_SIZE("char*" NDB_SIZEOF_CHARP)
CHECK_TYPE_SIZE("long long" NDB_SIZEOF_LONG_LONG)
CONFIGURE_FILE(ndb_types.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/ndb_types.h
               @ONLY)
# Exclude ndb_types.h from "make dist"
LIST(APPEND CPACK_SOURCE_IGNORE_FILES include/ndb_types\\\\.h$)

#
# Read ndb_configure.m4 and extract the NDB_VERSION_XX=YY variables
#
INCLUDE("${NDB_SOURCE_DIR}/cmake/ndb_get_config_value.cmake")
NDB_GET_CONFIG_VALUE(NDB_VERSION_MAJOR major)
SET(NDB_VERSION_MAJOR "${major}" CACHE INTERNAL "NDB Major Version" FORCE)

NDB_GET_CONFIG_VALUE(NDB_VERSION_MINOR minor)
SET(NDB_VERSION_MINOR "${minor}" CACHE INTERNAL "NDB Minor Version" FORCE)

NDB_GET_CONFIG_VALUE(NDB_VERSION_BUILD build)
SET(NDB_VERSION_BUILD "${build}" CACHE INTERNAL "NDB Build Version" FORCE)

NDB_GET_CONFIG_VALUE(NDB_VERSION_STATUS status)
# Trim surrounding "'s from status
STRING(REGEX REPLACE "\"" "" status "${status}")
SET(NDB_VERSION_STATUS "${status}" CACHE INTERNAL "NDB Status Version" FORCE)

IF(NOT DEFINED NDB_VERSION_MAJOR OR
   NOT DEFINED NDB_VERSION_MINOR OR
   NOT DEFINED NDB_VERSION_BUILD)
  MESSAGE(STATUS "NDB_VERSION_MAJOR: ${NDB_VERSION_MAJOR}")
  MESSAGE(STATUS "NDB_VERSION_MINOR: ${NDB_VERSION_MINOR}")
  MESSAGE(STATUS "NDB_VERSION_BUILD: ${NDB_VERSION_BUILD}")
  MESSAGE(FATAL_ERROR "Couldn't parse version numbers from ndb_configure.m4")
ENDIF()

IF (DEFINED MYSQL_CLUSTER_VERSION)
  # This is MySQL Cluster, the MySQL Cluster version must match NDB version
  IF(NOT MYSQL_CLUSTER_VERSION_MAJOR EQUAL NDB_VERSION_MAJOR OR
     NOT MYSQL_CLUSTER_VERSION_MINOR EQUAL NDB_VERSION_MINOR OR  
     NOT MYSQL_CLUSTER_VERSION_BUILD EQUAL NDB_VERSION_BUILD)
    MESSAGE(STATUS "MYSQL_CLUSTER_VERSION_MAJOR: ${MYSQL_CLUSTER_VERSION_MAJOR}")
    MESSAGE(STATUS "MYSQL_CLUSTER_VERSION_MINOR: ${MYSQL_CLUSTER_VERSION_MINOR}")
    MESSAGE(STATUS "MYSQL_CLUSTER_VERSION_BUILD: ${MYSQL_CLUSTER_VERSION_BUILD}")
    MESSAGE(FATAL_ERROR "MySQL Cluster version does not match NDB version")
  ENDIF()
ENDIF()

# Create ndb_version.h
CONFIGURE_FILE(ndb_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/ndb_version.h
               @ONLY)
# Exclude ndb_version.h from "make dist"
LIST(APPEND CPACK_SOURCE_IGNORE_FILES include/ndb_version\\\\.h$)


# Install public headers
SET(NDB_GENERAL_HEADERS
    ndb_constants.h
    ndb_init.h
    # The below files are generated and thus found in build dir
    ${CMAKE_CURRENT_BINARY_DIR}/ndb_types.h
    ${CMAKE_CURRENT_BINARY_DIR}/ndb_version.h)


SET(NDB_NDBAPI_HEADERS
    ndbapi/ndbapi_limits.h
    ndbapi/ndb_opt_defaults.h
    ndbapi/Ndb.hpp
    ndbapi/NdbApi.hpp
    ndbapi/NdbTransaction.hpp
    ndbapi/NdbDictionary.hpp
    ndbapi/NdbError.hpp
    ndbapi/NdbEventOperation.hpp
    ndbapi/NdbIndexOperation.hpp
    ndbapi/NdbOperation.hpp
    ndbapi/ndb_cluster_connection.hpp
    ndbapi/NdbBlob.hpp
    ndbapi/NdbPool.hpp
    ndbapi/NdbRecAttr.hpp
    ndbapi/NdbReceiver.hpp
    ndbapi/NdbScanFilter.hpp
    ndbapi/NdbScanOperation.hpp
    ndbapi/NdbIndexScanOperation.hpp
    ndbapi/NdbIndexStat.hpp
    ndbapi/ndberror.h
    ndbapi/NdbInterpretedCode.hpp)

SET(NDB_MGMAPI_HEADERS
    mgmapi/mgmapi.h
    mgmapi/mgmapi_error.h
    mgmapi/mgmapi_debug.h
    mgmapi/mgmapi_config_parameters.h
    mgmapi/mgmapi_config_parameters_debug.h
    mgmapi/ndb_logevent.h
    mgmapi/ndbd_exit_codes.h)

INSTALL(FILES ${NDB_GENERAL_HEADERS}
        DESTINATION ${INSTALL_INCLUDEDIR}/storage/ndb
        COMPONENT Developement)

INSTALL(FILES ${NDB_NDBAPI_HEADERS}
        DESTINATION ${INSTALL_INCLUDEDIR}/storage/ndb/ndbapi
        COMPONENT Developement)

INSTALL(FILES ${NDB_MGMAPI_HEADERS}
        DESTINATION ${INSTALL_INCLUDEDIR}/storage/ndb/mgmapi
        COMPONENT Developement)
