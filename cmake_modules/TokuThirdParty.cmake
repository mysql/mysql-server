# TODO(leif): will need to be smarter about this when we do trunk/tags/branches
set(TOKU_SVNROOT "${TokuDB_SOURCE_DIR}/../.." CACHE FILEPATH "The top of the tokudb source tree, usod to find xz sources.")

## add lzma with an external project
include(ExternalProject)
set(xz_configure_opts --with-pic --enable-static)
if (APPLE)
  ## lzma has some assembly that doesn't work on darwin
  list(APPEND xz_configure_opts --disable-assembler)
endif ()

list(APPEND xz_configure_opts CC=${CMAKE_C_COMPILER})
if (NOT CMAKE_BUILD_TYPE MATCHES Release)
  list(APPEND xz_configure_opts --enable-debug)
endif ()

set(XZ_SOURCE_DIR "${TOKU_SVNROOT}/xz-4.999.9beta")
if (NOT EXISTS "${XZ_SOURCE_DIR}/configure")
  message(FATAL_ERROR "Can't find the xz sources.  Please check them out to ${XZ_SOURCE_DIR} or modify TOKU_SVNROOT.")
endif ()

if (CMAKE_GENERATOR STREQUAL Ninja)
  ## ninja doesn't understand "$(MAKE)"
  ExternalProject_Add(build_lzma
    PREFIX xz
    SOURCE_DIR "${XZ_SOURCE_DIR}"
    CONFIGURE_COMMAND
        "${XZ_SOURCE_DIR}/configure" ${xz_configure_opts}
        "--prefix=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz"
    BUILD_COMMAND
        make -C src/liblzma
    INSTALL_COMMAND
        make -C src/liblzma install
    )
else ()
  ## use "$(MAKE)" for submakes so they can use the jobserver, doesn't
  ## seem to break Xcode...
  ExternalProject_Add(build_lzma
    PREFIX xz
    SOURCE_DIR "${XZ_SOURCE_DIR}"
    CONFIGURE_COMMAND
        "${XZ_SOURCE_DIR}/configure" ${xz_configure_opts}
        "--prefix=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xz"
    BUILD_COMMAND
        $(MAKE) -C src/liblzma
    INSTALL_COMMAND
        $(MAKE) -C src/liblzma install
    )
endif ()

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

# TODO(leif): jemalloc?