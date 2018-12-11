# Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.
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

######### BUNDLED SOURCES FOR MEMCACHED AND LIBEVENT

option(WITH_BUNDLED_MEMCACHED "Use bundled memcached" ON)
if(WITH_BUNDLED_MEMCACHED)
  #
  # Build the memcached bundled with NDB
  #

  # Handle the deprecated option to build libevent from
  # code bundled with NDB. I.e the libevent bundled with NDB
  # is not built anymore and instead the libevent bundled
  # with MySQL or system libs are used.
  option(WITH_BUNDLED_LIBEVENT "Deprecated, use WITH_LIBEVENT")
  mark_as_advanced(WITH_BUNDLED_LIBEVENT)
  if(WITH_BUNDLED_LIBEVENT)
    MESSAGE(WARNING "Ignoring deprecated WITH_BUNDLED_LIBEVENT, use "
                   "WITH_LIBEVENT to configure which library to use "
                   "when building memcached")
  endif()


  # The libevent library is needed to build memcached.
  # Make sure it has been found or configured to build
  # by the code in cmake/libevent.cmake
  if (NOT LIBEVENT_FOUND)
    MESSAGE(SEND_ERROR "The libevent library is necessary to build "
                       "memcached, use WITH_LIBEVENT to configure which "
                       "library to use")
  else()
    # NOTE! Seems to be a slight confusion which variables
    # are set by cmake/libevent.cmake. The comment says
    # LIBEVENT_LIBRARIES and LIBEVENT_INCLUDE_DIRS, but actually
    # slightly different varables are set depending on bundled or not.
    # Thus using the same variable names as plugin/innodb_memcached/ assuming
    # that things does _not_ build unless using bundled libevent.
    ndb_require_variable(LIBEVENT_LIBRARIES)
    ndb_require_variable(LIBEVENT_INCLUDE_DIRS)
  endif()

  add_subdirectory(extra/memcached)
endif()



####################################################

# Don't disable assert
remove_definitions(-DNDEBUG)

include("FindMemcached.cmake")
include(CheckLibraryExists)

IF(MEMCACHED_FOUND) 
  MESSAGE(STATUS "Building NDB Memcache using " ${MEMCACHED_ROOT_DIR})
ELSE()
  MESSAGE(STATUS "Skipping NDB Memcache (Memcached not found)")
  RETURN()
ENDIF()  

# Check for atomic operations
include(atomics.cmake)
if(NO_ATOMICS)
  RETURN()
ENDIF()

# Reuse the default engine from the bundled source tree
set(BUNDLED_MEMCACHED extra/memcached)
set(CACHE_SRC ${BUNDLED_MEMCACHED}/engines/default_engine)

# Paths to header files
include_directories(BEFORE include)
include_directories(BEFORE ${CACHE_SRC})
include_directories(${NDB_SOURCE_DIR}/src/ndbjtie/mysql)
include_directories(${NDB_SOURCE_DIR}/src/ndbapi/)
include_directories(${CMAKE_BINARY_DIR}/storage/ndb/memcache/include/)
include_directories(AFTER ${MEMCACHED_INCLUDE_DIR})
include_directories(AFTER ${BUNDLED_MEMCACHED})

# Paths to libraries
link_directories(${NDB_SOURCE_DIR}/src 
                 ${NDB_SOURCE_DIR}/src/common/util
                 ${CMAKE_CURRENT_SOURCE_DIR}/extra/memcached
                )

# Source files for this module 
set(NDB_MEMCACHE_SOURCE_FILES
    src/ClusterConnectionPool.cc
    src/Configuration.cc
    src/Config_v1.cc
    src/ConnQueryPlanSet.cc
    src/DataTypeHandler.cc
    src/ExpireTime.cc
    src/ExternalValue.cc
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
    src/ndb_engine_errors.cc
    src/ndb_engine_private.h
    src/ndb_error_logger.cc
    src/ndb_flush.cc
    src/ndb_pipeline.cc
    src/ndb_worker.cc
    src/schedulers
    src/thread_identifier.c
    src/timing.c
    src/workitem.c
    src/workqueue.c
    src/Scheduler.cc
    src/GlobalConfigManager.cc
    src/SchedulerConfigManager.cc
    src/schedulers/S_sched.cc
    src/schedulers/Stockholm.cc
    src/schedulers/Scheduler73.cc
    src/schedulers/Trondheim.cc
    ${CACHE_SRC}/items.c
    ${CACHE_SRC}/assoc.c
    ${CACHE_SRC}/slabs.c
    src/embedded_default_engine.c
)


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

IF(${CMAKE_CXX_COMPILER_ID} STREQUAL "SunPro")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -features=extensions") # __func__
ENDIF()


########## ENVIRONMENT TESTS #########

# Checks for system headers
CHECK_INCLUDE_FILE("mach/mach_time.h" HAVE_MACH_MACH_TIME_H)

# Checks for library functions
CHECK_FUNCTION_EXISTS(srandomdev HAVE_SRANDOMDEV)
CHECK_FUNCTION_EXISTS(gethrtime  HAVE_GETHRTIME)
CHECK_FUNCTION_EXISTS(gethrvtime HAVE_GETHRVTIME)
CHECK_FUNCTION_EXISTS(memset     HAVE_MEMSET)
# Also: log in libm, dlsym in libdl? 
CHECK_LIBRARY_EXISTS(dl dlsym "" HAVE_LIBDL)
SET(LIBDL "")
if(${HAVE_LIBDL}) 
    set(LIBDL "dl")
endif()

CHECK_CXX_SOURCE_RUNS("
    main()  {
      const char * f = __func__;
      return 0;
    }"
    HAVE_FUNC_IN_CXX)

# Define DEBUG_OUTPUT by default (enables runtime "debug" option in memcached)
# if(HAVE_FUNC_IN_CXX)
  add_definitions(-DDEBUG_OUTPUT)
# endif()


########### GENERATED FILES #############

# Build ndbmemcache_config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/ndbmemcache_config.in 
               ${CMAKE_CURRENT_BINARY_DIR}/include/ndbmemcache_config.h)

# Build sandbox.sh
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sandbox.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/sandbox.sh)
               
# Build the perl include file used by mtr
# - setup special variables since perl hasn't got
#   the same understanding of "boolean variables" as cmake has.
#   i.e the cmake variables can be "1", "ON", "TRUE" etc.
set(NDB_MEMCACHED_IS_AVAILABLE 0)
if (MEMCACHED_FOUND)
  set(NDB_MEMCACHED_IS_AVAILABLE 1)
endif()
set(NDB_MEMCACHED_IS_BUNDLED 0)
if (WITH_BUNDLED_MEMCACHED)
  set(NDB_MEMCACHED_IS_BUNDLED 1)
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/memcached_path.pl.in
               ${CMAKE_CURRENT_BINARY_DIR}/memcached_path.pl)

# Copy the SQL script into /scripts/ in the build directory.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndb_memcache_metadata.sql
               ${CMAKE_BINARY_DIR}/scripts/ndb_memcache_metadata.sql COPYONLY)

# Build the "memclient" utility
SET(MEMCACHE_PM ${CMAKE_SOURCE_DIR}/mysql-test/lib/My/Memcache.pm)
SET(MEMCLIENT_PL ${CMAKE_CURRENT_SOURCE_DIR}/memclient.pl)
SET(MEMCLIENT ${CMAKE_CURRENT_BINARY_DIR}/memclient)
FILE(READ   ${MEMCLIENT_PL} CONTENTS)
FILE(WRITE  ${MEMCLIENT} "${CONTENTS}")
FILE(READ   ${MEMCACHE_PM} CONTENTS)
FILE(APPEND ${MEMCLIENT} "${CONTENTS}")

               
######### TARGETS ############

### Build the convenience library
ADD_CONVENIENCE_LIBRARY(ndbmemcache ${NDB_MEMCACHE_SOURCE_FILES})

target_link_libraries(ndbmemcache
                      ndbclient_static
                      ndbgeneral
                      ${MEMCACHED_UTILITIES_LIBRARY})

### Build the module
add_library(ndb_engine MODULE src/ndb_engine.c src/stub.cc)

target_link_libraries(ndb_engine ndbmemcache ndbclient_static)

### If we are using the bundled memcache, it is a dependency:
if(WITH_BUNDLED_MEMCACHED)
  add_dependencies(ndb_engine memcached)
endif()

### Extra linker flags because CMake's "MODULE" support is not quite right.  
if(APPLE) 
  set(FINAL_LINK_FLAGS "-flat_namespace -undefined suppress -bind_at_load")
elseif(${CMAKE_COMPILER_IS_GNUCC}) 
  set(FINAL_LINK_FLAGS "-shared")
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS") 
  set(FINAL_LINK_FLAGS "-G -lCrun")
else()
  set(FINAL_LINK_FLAGS "")
endif()
#  Prepend any link flags required for building shared lib
set(FINAL_LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} ${FINAL_LINK_FLAGS}")

set_target_properties(ndb_engine PROPERTIES
                      PREFIX ""
                      LINK_FLAGS "${FINAL_LINK_FLAGS}")

############ INSTALLER RULES #########
### Install the ndb_engine.so module
###
install(TARGETS ndb_engine DESTINATION ${INSTALL_LIBDIR})

### Install memclient 
### 
install(PROGRAMS ${MEMCLIENT} DESTINATION ${INSTALL_BINDIR})

### Install the memcache-api directory
###
SET(MEMCACHE_API_DIR "${INSTALL_MYSQLSHAREDIR}/memcache-api")
install(DIRECTORY DESTINATION ${MEMCACHE_API_DIR} )
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/sandbox.sh 
        DESTINATION  ${MEMCACHE_API_DIR})
install(FILES README DESTINATION  ${MEMCACHE_API_DIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndb_memcache_metadata.sql 
        DESTINATION  ${MEMCACHE_API_DIR})
# Upgrader scripts:
install(DIRECTORY DESTINATION ${MEMCACHE_API_DIR}/upgrade)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_to_1.2.sql
        DESTINATION ${MEMCACHE_API_DIR}/upgrade)
        
# memcached_path.pl is also installed, for use by installed mtr
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/memcached_path.pl
        DESTINATION ${INSTALL_MYSQLTESTDIR}/lib)
        
######################################################## 


add_subdirectory(unit)


