cmake_minimum_required(VERSION 2.8.8)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

## this needs to happen before calling project(), when cmake detects some
## basic things about the compiler
include(TokuSetupIntelCompiler)

project(TokuDB)

include(TokuFeatureDetection)
include(TokuSetupCompiler)
include(TokuSetupCTest)

## add lzma with an external project
include(ExternalProject)
set(xz_configure_opts --with-pic)
if (CMAKE_SYSTEM_NAME MATCHES Darwin)
  ## lzma has some assembly that doesn't work on osx
  list(APPEND xz_configure_opts --disable-assembler)
endif ()

if (CMAKE_BUILD_TYPE MATCHES Release)
  if (CMAKE_C_COMPILER_ID MATCHES Intel)
    list(APPEND xz_configure_opts CC=icc "CFLAGS=-O2 -g -ip -ipo1" AR=xiar)
  endif ()
else ()
  list(APPEND xz_configure_opts --enable-debug)
endif ()

if (CMAKE_GENERATOR STREQUAL Ninja)
  ## ninja doesn't understand "$(MAKE)"
  ExternalProject_Add(ep_lzma
    PREFIX xz
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/xz-4.999.9beta
    CONFIGURE_COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/xz-4.999.9beta/configure ${xz_configure_opts}
        "--prefix=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz"
    BUILD_COMMAND
        make -C src/liblzma
    INSTALL_COMMAND
        make -C src/liblzma install
    )
else ()
  ## use "$(MAKE)" for submakes so they can use the jobserver, doesn't
  ## seem to break Xcode...
  ExternalProject_Add(ep_lzma
    PREFIX xz
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/xz-4.999.9beta
    CONFIGURE_COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/xz-4.999.9beta/configure ${xz_configure_opts}
        "--prefix=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz"
    BUILD_COMMAND
        $(MAKE) -C src/liblzma
    INSTALL_COMMAND
        $(MAKE) -C src/liblzma install
    )
endif ()
add_custom_target(build_lzma DEPENDS ep_lzma)

add_library(lzma STATIC IMPORTED)
set_target_properties(lzma PROPERTIES IMPORTED_LOCATION
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/lib/liblzma.a")

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
  ${CMAKE_CURRENT_SOURCE_DIR}  ## so you can include <newbrt/brt.h> from inside src/
  )
## include where config.h will be generated
include_directories(${CMAKE_CURRENT_BINARY_DIR}/toku_include)

## build db.h and include where it will be generated
add_subdirectory(buildheader)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/buildheader)

## add subdirectories
add_subdirectory(portability)
add_subdirectory(newbrt)
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

## set up lists of sources and headers for tags
file(GLOB_RECURSE all_srcs
  include/*.c
  toku_include/*.c
  buildheader/*.c
  portability/*.c
  newbrt/*.c
  src/*.c
  utils/*.c
  db-benchmark-test/*.c
  )
file(GLOB_RECURSE all_hdrs
  include/*.h
  toku_include/*.h
  buildheader/*.h
  portability/*.h
  newbrt/*.h
  src/*.h
  utils/*.h
  db-benchmark-test/*.h
  )

## build tags
include(TokuBuildTagDatabases)
