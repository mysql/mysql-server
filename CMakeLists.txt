cmake_minimum_required(VERSION 2.6)
project(MySQL_NodeJS_Adapter)

set(MYSQL_HOME /Users/jdd/bzr-repo/install-tests/7.2)
set(NODE_INCLUDES /usr/local/include/node)

include_directories(${NODE_INCLUDES})

include_directories(Adapter/impl/include)
include_directories(${MYSQL_HOME}/include)
include_directories(${MYSQL_HOME}/include/mysql)
include_directories(${MYSQL_HOME}/include/storage/ndb)
include_directories(${MYSQL_HOME}/include/storage/ndb/ndbapi)
include_directories(${MYSQL_HOME}/include/mysql/storage/ndb)
include_directories(${MYSQL_HOME}/include/mysql/storage/ndb/ndbapi)

link_directories(${MYSQL_HOME}/lib)

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
  Adapter/impl/src/mysqlclient_wrapper.cpp
  Adapter/impl/src/node_module.cpp
  Adapter/impl/src/unified_debug.cpp
)

set(ADAPTER_REQUIRED_LIBS 
  ndbclient
  mysqlclient
)


add_library(ndb_adapter MODULE ${ADAPTER_SOURCE_FILES})

target_link_libraries(ndb_adapter ${ADAPTER_REQUIRED_LIBS})

if(APPLE) 
  set(FINAL_LINK_FLAGS "-undefined dynamic_lookup")
endif()

set_target_properties(ndb_adapter PROPERTIES
                      PREFIX ""
                      SUFFIX ".node"
                      LINK_FLAGS "${FINAL_LINK_FLAGS}")


  