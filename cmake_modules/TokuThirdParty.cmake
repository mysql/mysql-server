include(ExternalProject)

## add jemalloc with an external project
set(JEMALLOC_SOURCE_DIR "${TokuDB_SOURCE_DIR}/third_party/jemalloc" CACHE FILEPATH "Where to find jemalloc sources.")
if (NOT EXISTS "${JEMALLOC_SOURCE_DIR}/configure")
  message(FATAL_ERROR "Can't find jemalloc sources.  Please check them out to ${JEMALLOC_SOURCE_DIR} or modify JEMALLOC_SOURCE_DIR.")
endif ()
set(jemalloc_configure_opts "CC=${CMAKE_C_COMPILER}" "--with-jemalloc-prefix=" "--with-private-namespace=tokudb_jemalloc_internal_" "--enable-cc-silence")
if (NOT CMAKE_BUILD_TYPE MATCHES Release)
  list(APPEND jemalloc_configure_opts --enable-debug)
endif ()
ExternalProject_Add(build_jemalloc
  PREFIX jemalloc
  SOURCE_DIR "${JEMALLOC_SOURCE_DIR}"
  CONFIGURE_COMMAND
      "${JEMALLOC_SOURCE_DIR}/configure" ${jemalloc_configure_opts}
      "--prefix=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/jemalloc"
  )

add_library(jemalloc STATIC IMPORTED GLOBAL)
set_target_properties(jemalloc PROPERTIES IMPORTED_LOCATION
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/jemalloc/lib/libjemalloc_pic.a")
add_dependencies(jemalloc build_jemalloc)
add_library(jemalloc_nopic STATIC IMPORTED GLOBAL)
set_target_properties(jemalloc_nopic PROPERTIES IMPORTED_LOCATION
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/jemalloc/lib/libjemalloc.a")
add_dependencies(jemalloc_nopic build_jemalloc)

install(
  DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/jemalloc/lib"
  DESTINATION .
  )

## add lzma with an external project
set(xz_configure_opts --with-pic --enable-static)
if (APPLE)
  ## lzma has some assembly that doesn't work on darwin
  list(APPEND xz_configure_opts --disable-assembler)
endif ()

list(APPEND xz_configure_opts CC=${CMAKE_C_COMPILER})
if (NOT CMAKE_BUILD_TYPE MATCHES Release)
  list(APPEND xz_configure_opts --enable-debug)
endif ()

set(XZ_SOURCE_DIR "${TokuDB_SOURCE_DIR}/third_party/xz-4.999.9beta" CACHE FILEPATH "Where to find sources for xz (lzma).")
if (NOT EXISTS "${XZ_SOURCE_DIR}/configure")
    message(FATAL_ERROR "Can't find the xz sources.  Please check them out to ${XZ_SOURCE_DIR} or modify XZ_SOURCE_DIR.")
endif ()

if (CMAKE_GENERATOR STREQUAL Ninja)
  ## ninja doesn't understand "$(MAKE)"
  set(SUBMAKE_COMMAND make)
else ()
  ## use "$(MAKE)" for submakes so they can use the jobserver, doesn't
  ## seem to break Xcode...
  set(SUBMAKE_COMMAND $(MAKE))
endif ()

FILE(GLOB XZ_ALL_FILES ${XZ_SOURCE_DIR}/*)
ExternalProject_Add(build_lzma
    PREFIX xz
    DOWNLOAD_COMMAND
        cp -au "${XZ_ALL_FILES}" "<SOURCE_DIR>/"
    CONFIGURE_COMMAND
        "<SOURCE_DIR>/configure" ${xz_configure_opts}
        "--prefix=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz"
    BUILD_COMMAND
        ${SUBMAKE_COMMAND} -C src/liblzma
    INSTALL_COMMAND
        ${SUBMAKE_COMMAND} -C src/liblzma install
)
FILE(GLOB_RECURSE XZ_ALL_FILES_RECURSIVE ${XZ_SOURCE_DIR}/*)
ExternalProject_Add_Step(build_lzma reclone_src # Names of project and custom step
    COMMENT "(re)cloning xz source..."     # Text printed when step executes
    DEPENDERS download configure   # Steps that depend on this step
    DEPENDS   ${XZ_ALL_FILES_RECURSIVE}   # Files on which this step depends
)

set_source_files_properties(
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/base.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/bcj.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/block.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/check.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/container.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/delta.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/filter.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/index.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/index_hash.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/lzma.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/stream_flags.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/subblock.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/version.h"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include/lzma/vli.h"
  PROPERTIES GENERATED TRUE)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/include")

add_library(lzma STATIC IMPORTED)
set_target_properties(lzma PROPERTIES IMPORTED_LOCATION
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz/lib/liblzma.a")
add_dependencies(lzma build_lzma)
