# Copyright (c) 2008, 2022, Oracle and/or its affiliates.
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

INCLUDE(ndb_require_variable)
# Require the MySQL Cluster version variable to be set by
# higher level script.
NDB_REQUIRE_VARIABLE(MYSQL_CLUSTER_VERSION)

# Split the cluster version and set NDB version components
IF(${MYSQL_CLUSTER_VERSION} MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)")
  SET(NDB_VERSION_MAJOR
    "${CMAKE_MATCH_1}" CACHE INTERNAL "NDB Major Version" FORCE)
  SET(NDB_VERSION_MINOR
    "${CMAKE_MATCH_2}" CACHE INTERNAL "NDB Minor Version" FORCE)
  SET(NDB_VERSION_BUILD
    "${CMAKE_MATCH_3}" CACHE INTERNAL "NDB Build Version" FORCE)
  SET(NDB_VERSION_STATUS
    "${CMAKE_MATCH_4}" CACHE INTERNAL "NDB Status Version" FORCE)
ELSE()
  MESSAGE(FATAL_ERROR
    "Failed to split MYSQL_CLUSTER_VERSION: ${MYSQL_CLUSTER_VERSION}")
ENDIF()

# Create ndb_version.h
CONFIGURE_FILE(ndb_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/ndb_version.h
  @ONLY)
# Exclude ndb_version.h from "make dist"
LIST(APPEND CPACK_SOURCE_IGNORE_FILES include/ndb_version\\\\.h$)

IF(NOT WITH_NDB)
  RETURN()
ENDIF()

# Install public headers
SET(NDB_GENERAL_HEADERS
  ndb_constants.h
  ndb_init.h
  # The below files are generated and thus found in build dir
  ${CMAKE_CURRENT_BINARY_DIR}/ndb_types.h
  ${CMAKE_CURRENT_BINARY_DIR}/ndb_version.h
  )

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
  ndbapi/NdbInterpretedCode.hpp
  )

SET(NDB_MGMAPI_HEADERS
  mgmapi/mgmapi.h
  mgmapi/mgmapi_error.h
  mgmapi/mgmapi_debug.h
  mgmapi/mgmapi_config_parameters.h
  mgmapi/mgmapi_config_parameters_debug.h
  mgmapi/ndb_logevent.h
  mgmapi/ndbd_exit_codes.h
  )

INSTALL(FILES ${NDB_GENERAL_HEADERS}
  DESTINATION ${INSTALL_INCLUDEDIR}/storage/ndb
  COMPONENT Development)

INSTALL(FILES ${NDB_NDBAPI_HEADERS}
  DESTINATION ${INSTALL_INCLUDEDIR}/storage/ndb/ndbapi
  COMPONENT Development)

INSTALL(FILES ${NDB_MGMAPI_HEADERS}
  DESTINATION ${INSTALL_INCLUDEDIR}/storage/ndb/mgmapi
  COMPONENT Development)
