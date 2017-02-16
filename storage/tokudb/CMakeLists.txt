# ft-index only supports x86-64 and cmake-2.8.9+
IF(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND
    NOT CMAKE_VERSION VERSION_LESS "2.8.9")
CHECK_CXX_SOURCE_COMPILES(
"
struct a {int b; int c; };
struct a d = { .b=1, .c=2 };
int main() { return 0; }
" TOKUDB_OK)
ENDIF()

IF(NOT TOKUDB_OK OR WITHOUT_TOKUDB OR WITHOUT_TOKUDB_STORAGE_ENGINE)
  RETURN()
ENDIF()

IF(NOT LIBJEMALLOC)
  MESSAGE(WARNING "TokuDB is enabled, but jemalloc is not. This configuration is not supported")
ENDIF()

IF (HAVE_WVLA)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-vla")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-vla")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wno-vla")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-vla")
ENDIF()

############################################
SET(TOKUDB_VERSION "tokudb-7.5.7")
SET(TOKUDB_DEB_FILES "usr/lib/mysql/plugin/ha_tokudb.so\netc/mysql/conf.d/tokudb.cnf\nusr/bin/tokuftdump\nusr/share/doc/mariadb-server-5.5/README-TOKUDB\nusr/share/doc/mariadb-server-5.5/README.md" PARENT_SCOPE)
SET(USE_BDB OFF CACHE BOOL "")
MARK_AS_ADVANCED(BUILDNAME)
MARK_AS_ADVANCED(BUILD_TESTING)
MARK_AS_ADVANCED(CMAKE_TOKUDB_REVISION)
MARK_AS_ADVANCED(LIBTOKUDB)
MARK_AS_ADVANCED(LIBTOKUPORTABILITY)
MARK_AS_ADVANCED(TOKUDB_DATA)
MARK_AS_ADVANCED(TOKU_DEBUG_PARANOID)
MARK_AS_ADVANCED(USE_BDB)
MARK_AS_ADVANCED(USE_VALGRIND)
MARK_AS_ADVANCED(XZ_SOURCE_DIR)
############################################

SET(BUILD_TESTING OFF CACHE BOOL "")
SET(USE_VALGRIND OFF CACHE BOOL "")
SET(TOKU_DEBUG_PARANOID OFF CACHE BOOL "")

IF(NOT DEFINED TOKUDB_VERSION)
    IF(DEFINED ENV{TOKUDB_VERSION})
        SET(TOKUDB_VERSION $ENV{TOKUDB_VERSION})
    ENDIF()
ENDIF()
IF(DEFINED TOKUDB_VERSION)
    ADD_DEFINITIONS("-DTOKUDB_VERSION=${TOKUDB_VERSION}")
    IF (${TOKUDB_VERSION} MATCHES "^tokudb-([0-9]+)\\.([0-9]+)\\.([0-9]+.*)")
        ADD_DEFINITIONS("-DTOKUDB_VERSION_MAJOR=${CMAKE_MATCH_1}")
        ADD_DEFINITIONS("-DTOKUDB_VERSION_MINOR=${CMAKE_MATCH_2}")
        ADD_DEFINITIONS("-DTOKUDB_VERSION_PATCH=${CMAKE_MATCH_3}")
    ENDIF()
ENDIF()

IF(DEFINED TOKUDB_NOPATCH_CONFIG)
    ADD_DEFINITIONS("-DTOKUDB_NOPATCH_CONFIG=${TOKUDB_NOPATCH_CONFIG}")
ENDIF()

IF(DEFINED TOKUDB_CHECK_JEMALLOC)
    ADD_DEFINITIONS("-DTOKUDB_CHECK_JEMALLOC=${TOKUDB_CHECK_JEMALLOC}")
ENDIF()

## adds a compiler flag if the compiler supports it
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

macro(set_cflags_if_supported)
  foreach(flag ${ARGN})
    string(REGEX REPLACE "-" "_" temp_flag ${flag})
    check_c_compiler_flag(${flag} HAVE_C_${temp_flag})
    if (HAVE_C_${temp_flag})
      set(CMAKE_C_FLAGS "${flag} ${CMAKE_C_FLAGS}")
    endif ()
    check_cxx_compiler_flag(${flag} HAVE_CXX_${temp_flag})
    if (HAVE_CXX_${temp_flag})
      set(CMAKE_CXX_FLAGS "${flag} ${CMAKE_CXX_FLAGS}")
    endif ()
  endforeach(flag)
endmacro(set_cflags_if_supported)

macro(append_cflags_if_supported)
  foreach(flag ${ARGN})
    string(REGEX REPLACE "-" "_" temp_flag ${flag})
    check_c_compiler_flag(${flag} HAVE_C_${temp_flag})
    if (HAVE_C_${temp_flag})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
    endif ()
    check_cxx_compiler_flag(${flag} HAVE_CXX_${temp_flag})
    if (HAVE_CXX_${temp_flag})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif ()
  endforeach(flag)
endmacro(append_cflags_if_supported)

set_cflags_if_supported(-Wno-missing-field-initializers)
append_cflags_if_supported(-Wno-vla)

ADD_SUBDIRECTORY(ft-index)

INCLUDE_DIRECTORIES(ft-index)
INCLUDE_DIRECTORIES(ft-index/portability)
INCLUDE_DIRECTORIES(ft-index/util)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/ft-index)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/ft-index/buildheader)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/ft-index/portability)

SET(TOKUDB_PLUGIN_DYNAMIC "ha_tokudb")
SET(TOKUDB_SOURCES ha_tokudb.cc)
MYSQL_ADD_PLUGIN(tokudb ${TOKUDB_SOURCES} STORAGE_ENGINE MODULE_ONLY
    LINK_LIBRARIES tokufractaltree_static tokuportability_static ${ZLIB_LIBRARY} stdc++)
SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} -flto -fuse-linker-plugin")
SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} -flto -fuse-linker-plugin")

SET(CPACK_RPM_server_PACKAGE_OBSOLETES
  "${CPACK_RPM_server_PACKAGE_OBSOLETES} MariaDB-tokudb-engine < 5.5.34" PARENT_SCOPE)

IF (INSTALL_SYSCONF2DIR)
  INSTALL(FILES tokudb.cnf DESTINATION ${INSTALL_SYSCONF2DIR} COMPONENT Server)
ENDIF(INSTALL_SYSCONF2DIR)

