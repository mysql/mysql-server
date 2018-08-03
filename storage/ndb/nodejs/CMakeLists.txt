# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

set(WITH_NODEJS_INCLUDE_PATH "" CACHE PATH "location of node.js header files")

# Which version of Node.js?
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8.0")
  set(NODE_VERSION 0.12.15)
else()
  set(NODE_VERSION 6.10.2)
endif()


IF(NDB_NODEJS_EXTRAS_PATH)
  set(NODE_LIBRARIES ${NDB_NODEJS_EXTRAS_PATH}/windows-libs/${NODE_VERSION})
  set(NODE_HEADERS ${NDB_NODEJS_EXTRAS_PATH}/node-headers/${NODE_VERSION})
  set(WITH_NODEJS_INCLUDE_PATH ${NODE_HEADERS}/include/node)
  MESSAGE(STATUS "Using node.js version " ${NODE_VERSION})
ENDIF()

if(WITH_NODEJS_INCLUDE_PATH)
  MESSAGE(STATUS "Building jones-ndb to install in share/nodejs")
  MESSAGE(STATUS "Using node.js from " ${WITH_NODEJS_INCLUDE_PATH})
else()
  RETURN()
endif()


# Build rules.
#
include_directories(
  ${WITH_NODEJS_INCLUDE_PATH}
  jones-ndb/impl/include/common
  jones-ndb/impl/include/ndb
  ${NDB_SOURCE_DIR}/src/ndbapi
)


#link_directories(
# ${NDB_SOURCE_DIR}/src
# ${MYSQL_HOME}/lib)

add_definitions(-DNODE_GYP_MODULE_NAME=ndb_adapter)
add_definitions(-DBUILDING_NODE_EXTENSION)

set(ADAPTER_SOURCE_FILES
  jones-ndb/impl/src/common/async_common.cpp
  jones-ndb/impl/src/common/unified_debug.cpp
  jones-ndb/impl/src/ndb/AsyncNdbContext_wrapper.cpp
  jones-ndb/impl/src/ndb/AsyncNdbContext.cpp
  jones-ndb/impl/src/ndb/BlobHandler.cpp
  jones-ndb/impl/src/ndb/ColumnHandler.cpp
  jones-ndb/impl/src/ndb/ColumnProxy.cpp
  jones-ndb/impl/src/ndb/DBDictionaryImpl.cpp
  jones-ndb/impl/src/ndb/DBOperationHelper.cpp
  jones-ndb/impl/src/ndb/BatchImpl_wrapper.cpp
  jones-ndb/impl/src/ndb/BatchImpl.cpp
  jones-ndb/impl/src/ndb/SessionImpl_wrapper.cpp
  jones-ndb/impl/src/ndb/SessionImpl.cpp
  jones-ndb/impl/src/ndb/TransactionImpl_wrapper.cpp
  jones-ndb/impl/src/ndb/TransactionImpl.cpp
  jones-ndb/impl/src/ndb/EncoderCharset.cpp
  jones-ndb/impl/src/ndb/IndexBoundHelper.cpp
  jones-ndb/impl/src/ndb/KeyOperation.cpp  
  jones-ndb/impl/src/ndb/Ndb_cluster_connection_wrapper.cpp
  jones-ndb/impl/src/ndb/Ndb_init_wrapper.cpp
  jones-ndb/impl/src/ndb/Ndb_util_wrapper.cpp
  jones-ndb/impl/src/ndb/Ndb_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbError_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbInterpretedCode_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbRecordObject.cpp
  jones-ndb/impl/src/ndb/NdbScanFilter_wrapper.cpp
  jones-ndb/impl/src/ndb/NdbTypeEncoders.cpp
  jones-ndb/impl/src/ndb/Record_wrapper.cpp
  jones-ndb/impl/src/ndb/Record.cpp
  jones-ndb/impl/src/ndb/ScanOperation_wrapper.cpp
  jones-ndb/impl/src/ndb/ScanOperation.cpp
  jones-ndb/impl/src/ndb/ValueObject.cpp
  jones-ndb/impl/src/ndb/node_module.cpp
  jones-ndb/impl/src/ndb/QueryOperation.cpp
  jones-ndb/impl/src/ndb/QueryOperation_wrapper.cpp
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

if(WIN32) 
  set(ADAPTER_REQUIRED_LIBS ndbclient_static node)
  add_definitions(-DWIN32)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  if(CMAKE_CL_64)
    link_directories(${NODE_LIBRARIES}/win-x64)
  else()
    link_directories(${NODE_LIBRARIES}/win-x86)
  endif()
else()
  if(NODE_VERSION VERSION_GREATER "4.0.0")
    # Remove any -std=xxx compiler flag present, and add -std=c++11
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BTYPE)
    set(CXX_FLAGS_VAL ${CMAKE_CXX_FLAGS_${BTYPE}})
    STRING(REGEX REPLACE "-std=[cgnu]+..[0-9]+" "" CXX_FLAGS_VAL "${CXX_FLAGS_VAL}")
    set(CMAKE_CXX_FLAGS_${BTYPE} "${CXX_FLAGS_VAL} -std=c++11")
  endif()
  set(ADAPTER_REQUIRED_LIBS ndbclient_static)
 
  # Remove -Wundef from C++ compiler flags since there are several
  # undefined identifiers which are evaluated in for example v8config.h
  STRING(REPLACE "-Wundef " "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

#  IT SHOULD BE POSSIBLE TO LINK DYNAMICALLY WITH PROPER RPATH
#  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()


# Disable certain compiler warnings
FOREACH(warning
        "unused-parameter"
        "cast-function-type"
        "undefined-reinterpret-cast"
       )
  STRING(REPLACE "-" "_" warning_ ${warning})
  STRING(TOUPPER ${warning_} WARNING)
  CHECK_CXX_COMPILER_FLAG("-W${warning}" HAVE_${WARNING})
  IF(HAVE_${WARNING})
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${warning}")
  ENDIF()
ENDFOREACH()

add_library(ndb_adapter MODULE ${ADAPTER_SOURCE_FILES})

target_link_libraries(ndb_adapter ${ADAPTER_REQUIRED_LIBS})


# non-gcc linker flags to deal with undefined & multiply-defined symbols
if(APPLE)
  set(FINAL_LINK_FLAGS "-undefined dynamic_lookup")
elseif(WIN32)
  set(FINAL_LINK_FLAGS "/FORCE:MULTIPLE")
elseif(CMAKE_C_COMPILER_ID MATCHES "SunPro")
  set(FINAL_LINK_FLAGS "-z muldefs")
endif()

set_target_properties(ndb_adapter PROPERTIES
                      PREFIX ""
                      SUFFIX ".node"
                      LINK_FLAGS "${FINAL_LINK_FLAGS}")


# Install rules

set(nodejs_install ${INSTALL_MYSQLSHAREDIR}/nodejs)

INSTALL(
  DIRECTORY perftest samples
  DESTINATION ${nodejs_install}
  COMPONENT nodejs)

INSTALL(
  DIRECTORY jones-ndb 
  DESTINATION ${nodejs_install}/node_modules
  COMPONENT nodejs)

INSTALL(
  TARGETS ndb_adapter
  DESTINATION ${nodejs_install}/node_modules/jones-ndb/build/Static
  COMPONENT nodejs)
   
INSTALL(
  FILES ReadMe jones_deployments.js run-jones-test-suite.js
  DESTINATION ${nodejs_install}
  COMPONENT nodejs)
