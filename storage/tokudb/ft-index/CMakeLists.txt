set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

project(TokuDB)

# suppress -rdynamic
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

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

## subdirectories that just install things
#add_subdirectory(examples)

INSTALL_DOCUMENTATION(README.md README-TOKUDB COMPONENT Server)

## build tags
#include(TokuBuildTagDatabases)
