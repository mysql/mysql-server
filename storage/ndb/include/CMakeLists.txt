# Copyright (c) 2008, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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

# Test that public headers can be compiled with different language standards.

SET(NDBAPI_CXX_STANDARD 11)
SET(NDBAPI_C_STANDARD 99)

# Different versions of cmake recognize different language versions.
IF(CMAKE_VERSION VERSION_LESS 3.21)
  SET(RECENT_C_STANDARD 11)
ELSE()
  SET(RECENT_C_STANDARD 23)
ENDIF()

SET(INCLUDE_DIRECTORIES
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

# Check all C and C++ header files compiles with C++11 (NDBAPI_CXX_STANDARD)
# They will also implicitly be checked to build with the current C++ version
# used to compile the whole MySQL.

STRING(REGEX REPLACE
  "(${CMAKE_CURRENT_BINARY_DIR}/)?([^;}]+);*" "#include \"\\2\"\n"
  CXX_HEADERS_CONTENT
  "${NDB_GENERAL_HEADERS};${NDB_MGMAPI_HEADERS};${NDB_NDBAPI_HEADERS}")
STRING_APPEND(CXX_HEADERS_CONTENT "\nint main(){}\n")

CONFIGURE_FILE_CONTENT("${CXX_HEADERS_CONTENT}"
  ${CMAKE_CURRENT_BINARY_DIR}/ndb-api-cxx.cpp
  )

MYSQL_ADD_EXECUTABLE(ndb_installed_cxx
  ${CMAKE_CURRENT_BINARY_DIR}/ndb-api-cxx.cpp
  DEPENDENCIES GenError
  SKIP_INSTALL
  )
SET_PROPERTY(TARGET ndb_installed_cxx
  PROPERTY CXX_STANDARD ${NDBAPI_CXX_STANDARD})
SET_PROPERTY(TARGET ndb_installed_cxx PROPERTY CXX_EXTENSIONS OFF)
SET_PROPERTY(TARGET ndb_installed_cxx PROPERTY CXX_STANDARD_REQUIRED ON)

# Check all C header files compiles with C99 (NDBAPI_C_STANDARD) and recent C
# standard.

STRING(REGEX REPLACE
  "(${CMAKE_CURRENT_BINARY_DIR}/)?([^;}]+);*" "#include \"\\2\"\n"
  C_HEADERS_CONTENT "${NDB_GENERAL_HEADERS};${NDB_MGMAPI_HEADERS}")
STRING_APPEND(C_HEADERS_CONTENT "\nint main(){}\n")

CONFIGURE_FILE_CONTENT("${C_HEADERS_CONTENT}"
  ${CMAKE_CURRENT_BINARY_DIR}/ndb-api-c.c
  )

FOREACH(VER ${NDBAPI_C_STANDARD} ${RECENT_C_STANDARD})
  MYSQL_ADD_EXECUTABLE(ndb_installed_c${VER}
    ${CMAKE_CURRENT_BINARY_DIR}/ndb-api-c.c
    DEPENDENCIES GenError
    SKIP_INSTALL
    )
  SET_PROPERTY(TARGET ndb_installed_c${VER} PROPERTY C_STANDARD ${VER})
  SET_PROPERTY(TARGET ndb_installed_c${VER} PROPERTY C_EXTENSIONS OFF)
ENDFOREACH()

SET_PROPERTY(TARGET ndb_installed_c${NDBAPI_C_STANDARD}
  PROPERTY C_STANDARD_REQUIRED ON)
# All platforms may not have support for recent C standard allow fallback to
# older standard.
SET_PROPERTY(TARGET ndb_installed_c${RECENT_C_STANDARD}
  PROPERTY C_STANDARD_REQUIRED OFF)
