cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
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
set(LIBTOKUDB "tokudb" CACHE STRING "Name of libtokudb.so")

include_directories(
  ${VALGRIND_INCLUDE_DIR}
  )
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/toku_include
  ${CMAKE_CURRENT_SOURCE_DIR}/portability
  ${CMAKE_CURRENT_SOURCE_DIR}  ## so you can include <ft/ft-ops.h> from inside src/
  ${CMAKE_CURRENT_BINARY_DIR}  ## for logging code
  )
## include where config.h will be generated
include_directories(${CMAKE_CURRENT_BINARY_DIR}/toku_include)

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
add_subdirectory(utils)
add_subdirectory(db-benchmark-test)

## subdirectories that just install things
add_subdirectory(include)
add_subdirectory(toku_include)
add_subdirectory(examples)

install(
  FILES README.md README-TOKUDB
  DESTINATION .
  )

## build tags
include(TokuBuildTagDatabases)
