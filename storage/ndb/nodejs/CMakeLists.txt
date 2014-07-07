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

# Install rule for source distribution

INSTALL(
  DIRECTORY . 
  DESTINATION ${INSTALL_MYSQLSHAREDIR}/nodejs
  COMPONENT nodejs)


# Build rules.
#
# The build environment depends on NDB_NODEJS_PATH and NDB_NODEJS_EXTRAS_PATH

IF(NOT NDB_NODEJS_PATH)
  MESSAGE(STATUS "Skipping mysql-js (No NDB_NODEJS_PATH)")
  RETURN()
ENDIF()

IF(NDB_NODEJS_EXTRAS_PATH)
  MESSAGE(STATUS "Building mysql-js using " ${NDB_NODEJS_EXTRAS_PATH})
ELSE()
  MESSAGE(STATUS "Skipping mysql-js (NO NDB_NODEJS_EXTRAS_PATH)")
  RETURN()
ENDIF()

set(NODE_VERSION 0.10.4)
set(NODE_GYP ${NDB_NODEJS_EXTRAS_PATH}/node-gyp/${NODE_VERSION})
set(NODE_INCLUDES ${NODE_GYP}/src ${NODE_GYP}/deps/uv/include ${NODE_GYP}/deps/v8/include)

include_directories(${NODE_INCLUDES})
include_directories(Adapter/impl/common/include)
include_directories(Adapter/impl/ndb/include) 

link_directories(
  ${NDB_SOURCE_DIR}/src
  ${MYSQL_HOME}/lib)

add_definitions(-D_BUILDING_V8_SHARED=1 -DBUILDING_UV_SHARED=1)
add_definitions(-D_BUILDING_NODE_EXTENSION)

set(ADAPTER_SOURCE_FILES
  Adapter/impl/common/src/async_common.cpp
  Adapter/impl/common/src/unified_debug.cpp
  Adapter/impl/common/src/common_v8_values.cpp
  Adapter/impl/ndb/src/AsyncNdbContext_wrapper.cpp
  Adapter/impl/ndb/src/AsyncNdbContext.cpp
  Adapter/impl/ndb/src/BlobHandler.cpp
  Adapter/impl/ndb/src/ColumnHandler.cpp
  Adapter/impl/ndb/src/ColumnProxy.cpp
  Adapter/impl/ndb/src/DBDictionaryImpl.cpp
  Adapter/impl/ndb/src/DBOperationHelper.cpp
  Adapter/impl/ndb/src/DBOperationSet_wrapper.cpp
  Adapter/impl/ndb/src/DBOperationSet.cpp
  Adapter/impl/ndb/src/DBSessionImpl_wrapper.cpp
  Adapter/impl/ndb/src/DBSessionImpl.cpp
  Adapter/impl/ndb/src/DBTransactionContext_wrapper.cpp
  Adapter/impl/ndb/src/DBTransactionContext.cpp
  Adapter/impl/ndb/src/EncoderCharset.cpp
  Adapter/impl/ndb/src/IndexBoundHelper.cpp
  Adapter/impl/ndb/src/KeyOperation.cpp  
  Adapter/impl/ndb/src/Ndb_cluster_connection_wrapper.cpp
  Adapter/impl/ndb/src/Ndb_init_wrapper.cpp
  Adapter/impl/ndb/src/Ndb_util_wrapper.cpp
  Adapter/impl/ndb/src/Ndb_wrapper.cpp
  Adapter/impl/ndb/src/NdbError_wrapper.cpp
  Adapter/impl/ndb/src/NdbInterpretedCode_wrapper.cpp
  Adapter/impl/ndb/src/NdbRecordObject.cpp
  Adapter/impl/ndb/src/NdbScanFilter_wrapper.cpp
  Adapter/impl/ndb/src/NdbTypeEncoders.cpp
  Adapter/impl/ndb/src/Record_wrapper.cpp
  Adapter/impl/ndb/src/Record.cpp
  Adapter/impl/ndb/src/ScanOperation_wrapper.cpp
  Adapter/impl/ndb/src/ScanOperation.cpp
  Adapter/impl/ndb/src/ValueObject.cpp
  Adapter/impl/ndb/src/node_module.cpp
)


if(WIN32) 
  set(ADAPTER_REQUIRED_LIBS ndbclient_static mysqlclient node)
  add_definitions(-DWIN32)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  if(CMAKE_CL_64)
    link_directories(${NODE_GYP}/x64)
  else()
    link_directories(${NODE_GYP}/ia32) 
  endif()
else()
  set(ADAPTER_REQUIRED_LIBS ndbclient_so mysqlclient)
  add_definitions(-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)
endif()
 

add_library(ndb_adapter MODULE ${ADAPTER_SOURCE_FILES})

target_link_libraries(ndb_adapter ${ADAPTER_REQUIRED_LIBS})

if(APPLE) 
  set(FINAL_LINK_FLAGS "-undefined dynamic_lookup")
elseif(WIN32)
  set(FINAL_LINK_FLAGS "/FORCE:MULTIPLE")
endif()

set_target_properties(ndb_adapter PROPERTIES
                      PREFIX ""
                      SUFFIX ".node"
                      LINK_FLAGS "${FINAL_LINK_FLAGS}")

