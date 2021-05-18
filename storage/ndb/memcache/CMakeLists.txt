# Copyright (c) 2011, 2020, Oracle and/or its affiliates.
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

# This is the CMakeLists for NDB/Memcache

# Skip attempting to build this component on Windows (for now)
IF(WIN32)
  RETURN()
ENDIF()

######### BUNDLED SOURCES FOR MEMCACHED AND LIBEVENT

OPTION(WITH_BUNDLED_MEMCACHED "Use bundled memcached" ON)
IF(WITH_BUNDLED_MEMCACHED)
  #
  # Build the memcached bundled with NDB
  #

  # Handle the deprecated option to build libevent from
  # code bundled with NDB. I.e the libevent bundled with NDB
  # is not built anymore and instead the libevent bundled
  # with MySQL or system libs are used.
  OPTION(WITH_BUNDLED_LIBEVENT "Deprecated, use WITH_LIBEVENT")
  MARK_AS_ADVANCED(WITH_BUNDLED_LIBEVENT)
  IF(WITH_BUNDLED_LIBEVENT)
    MESSAGE(WARNING "Ignoring deprecated WITH_BUNDLED_LIBEVENT, use "
                   "WITH_LIBEVENT to configure which library to use "
                   "when building memcached")
  ENDIF()

  # The libevent library is needed to build memcached.
  # Make sure it has been found or configured to build
  # by the code in cmake/libevent.cmake
  IF (NOT LIBEVENT_FOUND)
    MESSAGE(SEND_ERROR "The libevent library is necessary to build "
                       "memcached, use WITH_LIBEVENT to configure which "
                       "library to use")
  ELSE()
    NDB_REQUIRE_VARIABLE(LIBEVENT_LIBRARIES)
    NDB_REQUIRE_VARIABLE(LIBEVENT_INCLUDE_DIRS)
  ENDIF()

  ADD_SUBDIRECTORY(extra/memcached)
ENDIF()



####################################################

# Disable specific types of warnings for current directory
# if the compiler supports the flag
FOREACH(warning
    "unused-but-set-variable"
    "strict-aliasing"
    "missing-field-initializers"
    "unused-parameter"
    "cast-qual"
    "suggest-override"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
    STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

# Don't disable assert
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


INCLUDE("FindMemcached.cmake")
INCLUDE(CheckLibraryExists)

IF(MEMCACHED_FOUND)
  MESSAGE(STATUS "Building NDB Memcache using " ${MEMCACHED_ROOT_DIR})
ELSE()
  MESSAGE(STATUS "Skipping NDB Memcache (Memcached not found)")
  RETURN()
ENDIF()

# Check for atomic operations
INCLUDE(atomics.cmake)
IF(NO_ATOMICS)
  RETURN()
ENDIF()

# Reuse the default engine from the bundled source tree
SET(BUNDLED_MEMCACHED extra/memcached)
SET(CACHE_SRC ${BUNDLED_MEMCACHED}/engines/default_engine)

# Paths to header files
INCLUDE_DIRECTORIES(BEFORE include)
INCLUDE_DIRECTORIES(BEFORE ${CACHE_SRC})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi/)
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}/storage/ndb/memcache/include/)
INCLUDE_DIRECTORIES(AFTER ${MEMCACHED_INCLUDE_DIR})
INCLUDE_DIRECTORIES(AFTER ${BUNDLED_MEMCACHED})

# Source files for this module
SET(NDB_MEMCACHE_SOURCE_FILES
  ${CACHE_SRC}/assoc.c
  ${CACHE_SRC}/items.c
  ${CACHE_SRC}/slabs.c
  src/ClusterConnectionPool.cc
  src/Config_v1.cc
  src/Configuration.cc
  src/ConnQueryPlanSet.cc
  src/DataTypeHandler.cc
  src/ExpireTime.cc
  src/ExternalValue.cc
  src/GlobalConfigManager.cc
  src/KeyPrefix.cc
  src/NdbInstance.cc
  src/Operation.cc
  src/QueryPlan.cc
  src/Record.cc
  src/Scheduler.cc
  src/SchedulerConfigManager.cc
  src/TabSeparatedValues.cc
  src/TableSpec.cc
  src/atomics.cc
  src/debug.cc
  src/embedded_default_engine.c
  src/hash_item_util.cc
  src/ndb_configuration.cc
  src/ndb_engine_errors.cc
  src/ndb_engine_private.h
  src/ndb_error_logger.cc
  src/ndb_flush.cc
  src/ndb_pipeline.cc
  src/ndb_worker.cc
  src/schedulers
  src/schedulers/S_sched.cc
  src/schedulers/Scheduler73.cc
  src/schedulers/Stockholm.cc
  src/schedulers/Trondheim.cc
  src/thread_identifier.cc
  src/timing.cc
  src/workitem.cc
  src/workqueue.cc
)

# The sourced C files from storage/ndb/memcache/extra/memcached can't be built
# with -Werror
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-error")
ENDIF()

# Set extra flags for the C compiler
IF(${CMAKE_COMPILER_IS_GNUCC})
  STRING_APPEND(CMAKE_C_FLAGS " --std=gnu99")
ELSEIF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
  STRING_APPEND(CMAKE_C_FLAGS " -xc99=all")
ENDIF()

# Set extra flags for the C++ compiler
IF(${CMAKE_COMPILER_IS_GNUCXX})
  STRING(REPLACE "-fno-implicit-templates" ""
    CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
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

CHECK_CXX_SOURCE_COMPILES("
    int main()  {
      const char * f = __func__;
      (void)f; // unused
      return 0;
    }"
    HAVE_FUNC_IN_CXX)

# Define DEBUG_OUTPUT by default (enables runtime "debug" option in memcached)
# if(HAVE_FUNC_IN_CXX)
  ADD_DEFINITIONS(-DDEBUG_OUTPUT)
# endif()


########### GENERATED FILES #############

# Build ndbmemcache_config.h
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/include/ndbmemcache_config.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/ndbmemcache_config.h)

# Build sandbox.sh
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/sandbox.sh.in
  ${CMAKE_CURRENT_BINARY_DIR}/sandbox.sh)

# Build the perl include file used by mtr
# - setup special variables since perl hasn't got
#   the same understanding of "boolean variables" as cmake has.
#   i.e the cmake variables can be "1", "ON", "TRUE" etc.
SET(NDB_MEMCACHED_IS_AVAILABLE 0)
IF (MEMCACHED_FOUND)
  SET(NDB_MEMCACHED_IS_AVAILABLE 1)
ENDIF()
SET(NDB_MEMCACHED_IS_BUNDLED 0)
IF(WITH_BUNDLED_MEMCACHED)
  SET(NDB_MEMCACHED_IS_BUNDLED 1)
ENDIF()
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/memcached_path.pl.in
  ${CMAKE_CURRENT_BINARY_DIR}/memcached_path.pl)

# Copy the SQL script into /scripts/ in the build directory.
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndb_memcache_metadata.sql
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

ADD_LIBRARY(ndbmemcache STATIC ${NDB_MEMCACHE_SOURCE_FILES})

TARGET_LINK_LIBRARIES(ndbmemcache
  ndbclient_static
  ndbgeneral
  ${MEMCACHED_UTILITIES_LIBRARY}
  )

### Build the module
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  # Turn off array-bounds warning in ndb_engine.cc wich occurs
  # when it uses the struct engine_info in the external memcached
  # file engine.h to define its engine capabilities.
  ADD_COMPILE_FLAGS(src/ndb_engine.cc COMPILE_FLAGS "-Wno-array-bounds")
ENDIF()

ADD_LIBRARY(ndb_engine MODULE src/ndb_engine.cc src/stub.cc)

IF(APPLE)
  SET_TARGET_PROPERTIES(ndb_engine PROPERTIES
    MACOSX_RPATH ON
  )
ENDIF()

IF(APPLE AND HAVE_CRYPTO_DYLIB AND HAVE_OPENSSL_DYLIB)
  ADD_CUSTOM_COMMAND(TARGET ndb_engine POST_BUILD
    COMMAND install_name_tool -change
    "${CRYPTO_VERSION}" "@loader_path/${CRYPTO_VERSION}"
    $<TARGET_LINKER_FILE:ndb_engine>
    COMMAND install_name_tool -change
    "${OPENSSL_VERSION}" "@loader_path/${OPENSSL_VERSION}"
    $<TARGET_LINKER_FILE:ndb_engine>
    )
ENDIF()

# Collect all dynamic libraries in the same directory
SET_TARGET_PROPERTIES(ndb_engine PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

# ndb_engine is a MODULE (plugin) but has DESTINATION lib ??
# ADD_INSTALL_RPATH_FOR_OPENSSL(ndb_engine)
ADD_INSTALL_RPATH(ndb_engine "\$ORIGIN/private")

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  # Turn off array-bounds warning in ndb_engine.c wich occurs
  # when it uses the struct engine_info in the external memcached
  # file engine.h to define its engine capabilities.
  ADD_COMPILE_FLAGS(src/ndb_engine.cc COMPILE_FLAGS "-Wno-array-bounds")
ENDIF()

TARGET_LINK_LIBRARIES(ndb_engine ndbmemcache ndbclient_static)

### If we are using the bundled memcache, it is a dependency:
IF(WITH_BUNDLED_MEMCACHED)
  ADD_DEPENDENCIES(ndb_engine memcached)
ENDIF()

### Extra linker flags because CMake's "MODULE" support is not quite right.
IF(APPLE)
  SET(FINAL_LINK_FLAGS "-flat_namespace -undefined suppress -bind_at_load")
ELSEIF(${CMAKE_COMPILER_IS_GNUCC})
  SET(FINAL_LINK_FLAGS "-shared")
ELSEIF(SOLARIS)
  SET(FINAL_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
ELSE()
  SET(FINAL_LINK_FLAGS "")
ENDIF()

#  Prepend any link flags required for building shared lib
STRING_PREPEND(FINAL_LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} ")

SET_TARGET_PROPERTIES(ndb_engine PROPERTIES
  PREFIX ""
  LINK_FLAGS "${FINAL_LINK_FLAGS}")

############ INSTALLER RULES #########
### Install the ndb_engine.so module
###
INSTALL(TARGETS ndb_engine DESTINATION ${INSTALL_LIBDIR})

### Install memclient
###
INSTALL(PROGRAMS ${MEMCLIENT} DESTINATION ${INSTALL_BINDIR})

### Install the memcache-api directory
###
SET(MEMCACHE_API_DIR "${INSTALL_MYSQLSHAREDIR}/memcache-api")
INSTALL(DIRECTORY DESTINATION ${MEMCACHE_API_DIR} )
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/sandbox.sh
  DESTINATION  ${MEMCACHE_API_DIR})
INSTALL(FILES README DESTINATION  ${MEMCACHE_API_DIR})
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndb_memcache_metadata.sql
  DESTINATION  ${MEMCACHE_API_DIR})

# Upgrader scripts:
INSTALL(DIRECTORY DESTINATION ${MEMCACHE_API_DIR}/upgrade)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_to_1.2.sql
  DESTINATION ${MEMCACHE_API_DIR}/upgrade)

# memcached_path.pl is also installed, for use by installed mtr
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/memcached_path.pl
  DESTINATION ${INSTALL_MYSQLTESTDIR}/lib)

########################################################


ADD_SUBDIRECTORY(unit)
