# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# This is the CMakeLists for NDB/Memcache

# Skip attempting to build this component on Windows (for now) 
IF(WIN32)
  RETURN()
ENDIF()

include("FindMemcached.cmake")

IF(MEMCACHED_FOUND) 
  MESSAGE(STATUS "Building NDB Memcache using " ${MEMCACHED_ROOT_DIR})
ELSE()
  MESSAGE(STATUS "Skipping NDB Memcache (Memcached not found)")
  RETURN()
ENDIF()  

# Paths to header files
include_directories(BEFORE include)
include_directories(BEFORE cache-src)
include_directories(${NDB_SOURCE_DIR}/src/ndbjtie/mysql)
include_directories(AFTER ${MEMCACHED_INCLUDE_DIR})
include_directories(${NDB_SOURCE_DIR}/src/ndbapi/)

# Paths to libraries
link_directories(${NDB_SOURCE_DIR}/src ${NDB_SOURCE_DIR}/src/common/util)

# Source files for this module 
set(NDB_MEMCACHE_SOURCE_FILES
    src/ClusterConnectionPool.cc
    src/Configuration.cc
    src/Config_v1.cc
    src/DataTypeHandler.cc
    src/KeyPrefix.cc
    src/NdbInstance.cc
    src/Operation.cc
    src/QueryPlan.cc
    src/Record.cc
    src/TabSeparatedValues.cc
    src/TableSpec.cc
    src/atomics.c
    src/debug.c
    src/hash_item_util.c
    src/ndb_configuration.cc
    src/ndb_engine_private.h
    src/ndb_pipeline.cc
    src/ndb_worker.cc
    src/schedulers
    src/thread_identifier.c
    src/timing.c
    src/workitem.c
    src/workqueue.c
    src/schedulers/S_sched.cc
    src/schedulers/Stockholm.cc
    src/ndb_engine.c
    src/embedded_default_engine.c
    cache-src/assoc.c
    cache-src/items.c
    cache-src/slabs.c
 )

# Check for atomic operations
include(atomics.cmake)

# Define DEBUG_OUTPUT by default (enables runtime "debug" option in memcached)
add_definitions(-DDEBUG_OUTPUT)

# Set extra flags for the C compiler
IF(${CMAKE_COMPILER_IS_GNUCC})
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=gnu99 -Wall -Wredundant-decls")
ELSEIF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -xc99=all")
ENDIF()

# Set extra flags for the C++ compiler
IF(${CMAKE_COMPILER_IS_GNUCXX})
  STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
ENDIF()


# Checks for system headers
CHECK_INCLUDE_FILE("mach/mach_time.h" HAVE_MACH_MACH_TIME_H)

# Checks for library functions
CHECK_FUNCTION_EXISTS(srandomdev HAVE_SRANDOMDEV)
CHECK_FUNCTION_EXISTS(gethrtime  HAVE_GETHRTIME)
CHECK_FUNCTION_EXISTS(gethrvtime HAVE_GETHRVTIME)
CHECK_FUNCTION_EXISTS(memset     HAVE_MEMSET)
# Also: log in libm, dlsym in libdl? 

# Build ndbmemcache_config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/ndbmemcache_config.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/include/ndbmemcache_config.h)

# Build sandbox.sh
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sandbox.sh.in
               ${CMAKE_CURRENT_SOURCE_DIR}/sandbox.sh)
               
# Build the perl include file used by mtr
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/memcached_path.pl.in
               ${CMAKE_CURRENT_SOURCE_DIR}/memcached_path.pl)

# memcached_path.pl is also installed, for use by installed mtr
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/memcached_path.pl
        DESTINATION ${INSTALL_MYSQLTESTDIR}/lib)
               
# Build the module
add_library(ndb_engine MODULE ${NDB_MEMCACHE_SOURCE_FILES})
target_link_libraries(ndb_engine ndbclient_static ndbgeneral)

add_dependencies(ndb_engine ndbclient_static ndbgeneral)
# If we are using the bundled memcache, it is a dependency:
if(WITH_MEMCACHE)
  add_dependencies(ndb_engine memcached)
endif()

# Extra linker flags because CMake's "MODULE" support is not quite right.  
if(APPLE) 
  set(FINAL_LINK_FLAGS "-flat_namespace -undefined suppress -bind_at_load")
elseif(${CMAKE_COMPILER_IS_GNUCC}) 
  set(FINAL_LINK_FLAGS "-shared")
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS") 
  set(FINAL_LINK_FLAGS "-G -lCrun")
else()
  set(FINAL_LINK_FLAGS "")
endif()

set_target_properties(ndb_engine PROPERTIES
                      PREFIX ""
                      LINK_FLAGS "${FINAL_LINK_FLAGS}")


# Install the ndb_engine.so module
install(TARGETS ndb_engine DESTINATION ${INSTALL_LIBDIR})

# Install the metadata.sql script
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndb_memcache_metadata.sql 
        DESTINATION ${INSTALL_MYSQLSHAREDIR})

### Install the memcache-api directory  ################
install(DIRECTORY DESTINATION memcache-api)
install(PROGRAMS sandbox.sh DESTINATION memcache-api)
install(FILES README DESTINATION memcache-api)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndb_memcache_metadata.sql 
        DESTINATION memcache-api)
######################################################## 


# Build the library used by the unit tests
add_library(ndbmemcache SHARED ${NDB_MEMCACHE_SOURCE_FILES})
target_link_libraries(ndbmemcache ndbclient_static ndbgeneral ${MEMCACHED_UTILITIES_LIBRARY})
set_target_properties(ndbmemcache PROPERTIES LINK_FLAGS "${FINAL_LINK_FLAGS}")

add_subdirectory(unit)
