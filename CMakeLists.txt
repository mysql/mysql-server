cmake_minimum_required(VERSION 2.8.8)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

## this needs to happen before calling project(), when cmake detects some
## basic things about the compiler
include(TokuSetupIntelCompiler)

project(TokuDB)

include(TokuFeatureDetection)
include(TokuSetupCompiler)
include(TokuSetupCTest)
include(TokuThirdParty)

## everything needs these libraries
link_libraries(dl pthread z)

## need a way to change the name of libs we build
set(LIBTOKUPORTABILITY "tokuportability" CACHE STRING "Name of libtokuportability.so")
set(LIBTOKUDB "tokudb" CACHE STRING "Name of libtokudb.so")

## add an option for cilk
option(USE_CILK "Use cilk in tokudb." OFF)
## can't use cilk without icc
if (USE_CILK AND (NOT CMAKE_C_COMPILER_ID MATCHES Intel))
  message(FATAL_ERROR "You specified USE_CILK=ON so you need INTEL_CC=ON.")
endif ()

## default includes and libraries
include_directories(SYSTEM
  /usr/local/include
  ${ZLIB_INCLUDE_DIRS}
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

## add subdirectories
add_subdirectory(portability)
add_subdirectory(ft)
add_subdirectory(src)
add_subdirectory(utils)
add_subdirectory(db-benchmark-test)

## subdirectories that just install things
add_subdirectory(include)
add_subdirectory(toku_include)
add_subdirectory(examples)

install(
  FILES README
  DESTINATION .
  )

## build tags
include(TokuBuildTagDatabases)
