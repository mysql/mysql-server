# Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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


# The build environment depends on NDB_NODEJS_EXTRAS_PATH
# or WITH_NODEJS_INCLUDE_PATH

SET(WITH_NODEJS_INCLUDE_PATH "" CACHE PATH "location of node.js header files")

SET(NODE_VERSION 18.12.1)

IF(NDB_NODEJS_EXTRAS_PATH)
  SET(NODE_LIBRARIES ${NDB_NODEJS_EXTRAS_PATH}/windows-libs/${NODE_VERSION})
  SET(NODE_HEADERS ${NDB_NODEJS_EXTRAS_PATH}/node-headers/${NODE_VERSION})
  SET(WITH_NODEJS_INCLUDE_PATH ${NODE_HEADERS}/include/node)
  MESSAGE(STATUS "Using node.js version " ${NODE_VERSION})
ENDIF()

IF(WITH_NODEJS_INCLUDE_PATH)
  MESSAGE(STATUS "Building jones-ndb to install in share/nodejs")
  MESSAGE(STATUS "Using node.js from " ${WITH_NODEJS_INCLUDE_PATH})
ELSE()
  RETURN()
ENDIF()


# Build rules.
#
INCLUDE_DIRECTORIES(
  ${WITH_NODEJS_INCLUDE_PATH}
  jones-ndb/impl/include/common
  jones-ndb/impl/include/ndb
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
)


ADD_DEFINITIONS(-DNODE_GYP_MODULE_NAME=ndb_adapter)
ADD_DEFINITIONS(-DBUILDING_NODE_EXTENSION)

set(ADAPTER_SOURCE_FILES
  jones-ndb/impl/src/common/async_common.cpp
  jones-ndb/impl/src/common/unified_debug.cpp
  jones-ndb/impl/src/ndb/AsyncNdbContext.cpp
  jones-ndb/impl/src/ndb/AsyncNdbContext_wrapper.cpp
  jones-ndb/impl/src/ndb/BatchImpl.cpp
  jones-ndb/impl/src/ndb/BatchImpl_wrapper.cpp
  jones-ndb/impl/src/ndb/BlobHandler.cpp
  jones-ndb/impl/src/ndb/ColumnHandler.cpp
  jones-ndb/impl/src/ndb/ColumnProxy.cpp
  jones-ndb/impl/src/ndb/DBDictionaryImpl.cpp
  jones-ndb/impl/src/ndb/DBOperationHelper.cpp
  jones-ndb/impl/src/ndb/EncoderCharset.cpp
  jones-ndb/impl/src/ndb/IndexBoundHelper.cpp
  jones-ndb/impl/src/ndb/KeyOperation.cpp
  jones-ndb/impl/src/ndb/NdbError_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbInterpretedCode_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbRecordObject.cpp
  jones-ndb/impl/src/ndb/NdbScanFilter_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbTypeEncoders.cpp
  jones-ndb/impl/src/ndb/Ndb_cluster_connection_wrapper.cpp
  jones-ndb/impl/src/ndb/Ndb_init_wrapper.cpp
  jones-ndb/impl/src/ndb/Ndb_util_wrapper.cpp
  jones-ndb/impl/src/ndb/Ndb_wrapper.cpp
  jones-ndb/impl/src/ndb/QueryOperation.cpp
  jones-ndb/impl/src/ndb/QueryOperation_wrapper.cpp
  jones-ndb/impl/src/ndb/Record.cpp
  jones-ndb/impl/src/ndb/Record_wrapper.cpp
  jones-ndb/impl/src/ndb/ScanOperation.cpp
  jones-ndb/impl/src/ndb/ScanOperation_wrapper.cpp
  jones-ndb/impl/src/ndb/SessionImpl.cpp
  jones-ndb/impl/src/ndb/SessionImpl_wrapper.cpp
  jones-ndb/impl/src/ndb/TransactionImpl.cpp
  jones-ndb/impl/src/ndb/TransactionImpl_wrapper.cpp
  jones-ndb/impl/src/ndb/ValueObject.cpp
  jones-ndb/impl/src/ndb/node_module.cpp
)


###### COMPILER AND LINKER OPTIONS

# Enable asserts:
REMOVE_DEFINITIONS(-DNDEBUG)
FOREACH(flag
    CMAKE_C_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_MINSIZEREL
    )
  STRING(REPLACE "-DNDEBUG"  "" "${flag}" "${${flag}}")
ENDFOREACH()

IF(WIN32)
  SET(ADAPTER_REQUIRED_LIBS ndbclient_static node)
  ADD_DEFINITIONS(-DWIN32)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  IF(CMAKE_CL_64)
    LINK_DIRECTORIES(${NODE_LIBRARIES}/win-x64)
  ELSE()
    LINK_DIRECTORIES(${NODE_LIBRARIES}/win-x86)
  ENDIF()
ELSE()
  SET(ADAPTER_REQUIRED_LIBS ndbclient_static)

#  IT SHOULD BE POSSIBLE TO LINK DYNAMICALLY WITH PROPER RPATH
#  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF()


# Disable certain compiler warnings
FOREACH(warning
    "unused-parameter"
    "undef"
    "undefined-reinterpret-cast"
    "odr"
    "lto-type-mismatch"
    "non-virtual-dtor"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

IF(MY_COMPILER_IS_GNU AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
  # It's tricky to check alloc-size-larger-than warning in
  # MY_CHECK_CXX_COMPILER_WARNING(). Handle it differently
  # as done in sql/CMakeLists.txt
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-alloc-size-larger-than")
ENDIF()


ADD_LIBRARY(ndb_adapter MODULE ${ADAPTER_SOURCE_FILES})

TARGET_LINK_LIBRARIES(ndb_adapter ${ADAPTER_REQUIRED_LIBS})


# non-gcc linker flags to deal with undefined & multiply-defined symbols
IF(APPLE)
  SET(FINAL_LINK_FLAGS "-undefined dynamic_lookup")
ELSEIF(WIN32)
  SET(FINAL_LINK_FLAGS "/FORCE:MULTIPLE")
ENDIF()

SET_TARGET_PROPERTIES(ndb_adapter PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  LINK_FLAGS "${FINAL_LINK_FLAGS}"
  )

# Install rules

SET(nodejs_install ${INSTALL_MYSQLSHAREDIR}/nodejs)

INSTALL(
  DIRECTORY perftest samples
  DESTINATION ${nodejs_install}
  COMPONENT nodejs)

INSTALL(
  DIRECTORY jones-ndb
  DESTINATION ${nodejs_install}
  COMPONENT nodejs)

INSTALL(
  TARGETS ndb_adapter
  DESTINATION ${nodejs_install}/jones-ndb/build/Static
  COMPONENT nodejs)

INSTALL(
  FILES ReadMe jones_deployments.js run-jones-test-suite.js create-test-user.sql
  DESTINATION ${nodejs_install}
  COMPONENT nodejs)
