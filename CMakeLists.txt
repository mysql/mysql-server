cmake_minimum_required(VERSION 2.6)
project(MySQL_NodeJS_Adapter)

set(MYSQL_HOME c:\\Users\\jdd.win7\\mysql\\mysql-cluster-gpl-7.2.10-winx64)
set(NODE_GYP c:\\Users\\jdd.win7\\.node-gyp\\0.8.16)
set(NODE_INCLUDES ${NODE_GYP}/src ${NODE_GYP}/deps/uv/include ${NODE_GYP}/deps/v8/include)

include_directories(${NODE_INCLUDES})

include_directories(${MYSQL_HOME}/include)
include_directories(${MYSQL_HOME}/include/mysql)
include_directories(${MYSQL_HOME}/include/storage/ndb)
include_directories(${MYSQL_HOME}/include/storage/ndb/ndbapi)
include_directories(${MYSQL_HOME}/include/mysql/storage/ndb)
include_directories(${MYSQL_HOME}/include/mysql/storage/ndb/ndbapi)

include_directories(Adapter/impl/include)

link_directories(${MYSQL_HOME}/lib)

add_definitions(-D_BUILDING_V8_SHARED=1 -DBUILDING_UV_SHARED=1)
add_definitions(-D_BUILDING_NODE_EXTENSION)

set(ADAPTER_SOURCE_FILES
  Adapter/impl/src/DBDictionaryImpl.cpp
  Adapter/impl/src/DBOperationHelper.cpp
  Adapter/impl/src/DBSessionImpl.cpp
  Adapter/impl/src/NdbError_wrapper.cpp
  Adapter/impl/src/NdbOperation_wrapper.cpp
  Adapter/impl/src/NdbTransaction_wrapper.cpp
  Adapter/impl/src/Ndb_cluster_connection_wrapper.cpp
  Adapter/impl/src/Ndb_init_wrapper.cpp
  Adapter/impl/src/Ndb_util_wrapper.cpp
  Adapter/impl/src/Ndb_wrapper.cpp
  Adapter/impl/src/Operation.cpp
  Adapter/impl/src/Operation_wrapper.cpp
  Adapter/impl/src/Recode.cpp
  Adapter/impl/src/Record.cpp
  Adapter/impl/src/Record_wrapper.cpp
  Adapter/impl/src/async_common.cpp
  Adapter/impl/src/node_module.cpp
  Adapter/impl/src/unified_debug.cpp
)

set(ADAPTER_REQUIRED_LIBS 
  ndbclient_static
  mysqlclient
)

if(WIN32) 
  add_definitions(-DWIN32)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  set(ADAPTER_REQUIRED_LIBS ${ADAPTER_REQUIRED_LIBS} node)
  link_directories(${NODE_GYP}/x64/)
else()
  add_definitions(-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)
endif()
 

add_library(ndb_adapter MODULE ${ADAPTER_SOURCE_FILES})

target_link_libraries(ndb_adapter ${ADAPTER_REQUIRED_LIBS})

if(APPLE) 
  set(FINAL_LINK_FLAGS "-undefined dynamic_lookup")
endif()

set_target_properties(ndb_adapter PROPERTIES
                      PREFIX ""
                      SUFFIX ".node"
                      LINK_FLAGS "${FINAL_LINK_FLAGS}")


  