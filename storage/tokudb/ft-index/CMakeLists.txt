set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

project(TokuDB)

# suppress -rdynamic
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

## Versions of gcc >= 4.9.0 require special version of 'ar' and 'ranlib' for
## link-time optimizations to work properly.
##
## From https://gcc.gnu.org/gcc-4.9/changes.html:
##
##     When using a linker plugin, compiling with the -flto option now
##     generates slim objects files (.o) which only contain intermediate
##     language representation for LTO. Use -ffat-lto-objects to create
##     files which contain additionally the object code. To generate
##     static libraries suitable for LTO processing, use gcc-ar and
##     gcc-ranlib; to list symbols from a slim object file use
##     gcc-nm. (Requires that ar, ranlib and nm have been compiled with
##     plugin support.)
if ((CMAKE_CXX_COMPILER_ID STREQUAL GNU) AND
    NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.0"))
  find_program(gcc_ar "gcc-ar")
  if (gcc_ar)
    set(CMAKE_AR "${gcc_ar}")
  endif ()
  find_program(gcc_ranlib "gcc-ranlib")
  if (gcc_ranlib)
    set(CMAKE_RANLIB "${gcc_ranlib}")
  endif ()
endif()

include(TokuFeatureDetection)
include(TokuSetupCompiler)
include(TokuSetupCTest)
include(TokuThirdParty)

set(TOKU_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TokuMergeLibs)

## need a way to change the name of libs we build
set(LIBTOKUPORTABILITY "tokuportability" CACHE STRING "Name of libtokuportability.so")
set(LIBTOKUDB "tokufractaltree" CACHE STRING "Name of libtokufractaltree.so")

set(INSTALL_LIBDIR "lib" CACHE STRING "where to install libs")

if (USE_VALGRIND AND NOT VALGRIND_INCLUDE_DIR MATCHES NOTFOUND)
  include_directories(
    ${VALGRIND_INCLUDE_DIR}
    )
endif()
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/portability
  ${CMAKE_CURRENT_SOURCE_DIR}  ## so you can include <ft/ft-ops.h> from inside src/
  ${CMAKE_CURRENT_BINARY_DIR}  ## for logging code
  )
## include where config.h will be generated
include_directories(${CMAKE_CURRENT_BINARY_DIR}/portability)

## build db.h and include where it will be generated
add_subdirectory(buildheader)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/buildheader)

## default includes and libraries
include_directories(SYSTEM
  /usr/local/include
  ${ZLIB_INCLUDE_DIRS}
  )

## add subdirectories
add_subdirectory(util)
add_subdirectory(portability)
add_subdirectory(ft)
add_subdirectory(locktree)
add_subdirectory(src)
add_subdirectory(tools)

INSTALL_DOCUMENTATION(README.md README-TOKUDB COMPONENT Server)

## build tags
#include(TokuBuildTagDatabases)
