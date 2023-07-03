# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PROJECT(zstd)
# CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)

SET(ZSTD_SRCS
  ${ZSTD_VERSION_DIR}/lib/common/debug.c
  ${ZSTD_VERSION_DIR}/lib/common/entropy_common.c
  ${ZSTD_VERSION_DIR}/lib/common/error_private.c
  ${ZSTD_VERSION_DIR}/lib/common/fse_decompress.c
  ${ZSTD_VERSION_DIR}/lib/common/pool.c
  ${ZSTD_VERSION_DIR}/lib/common/threading.c
  ${ZSTD_VERSION_DIR}/lib/common/xxhash.c
  ${ZSTD_VERSION_DIR}/lib/common/zstd_common.c

  ${ZSTD_VERSION_DIR}/lib/compress/fse_compress.c
  ${ZSTD_VERSION_DIR}/lib/compress/hist.c
  ${ZSTD_VERSION_DIR}/lib/compress/huf_compress.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_compress.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_compress_literals.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_compress_sequences.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_compress_superblock.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_double_fast.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_fast.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_lazy.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_ldm.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstdmt_compress.c
  ${ZSTD_VERSION_DIR}/lib/compress/zstd_opt.c

  ${ZSTD_VERSION_DIR}/lib/decompress/huf_decompress.c
  ${ZSTD_VERSION_DIR}/lib/decompress/zstd_ddict.c
  ${ZSTD_VERSION_DIR}/lib/decompress/zstd_decompress_block.c
  ${ZSTD_VERSION_DIR}/lib/decompress/zstd_decompress.c

  ${ZSTD_VERSION_DIR}/lib/dictBuilder/cover.c
  ${ZSTD_VERSION_DIR}/lib/dictBuilder/divsufsort.c
  ${ZSTD_VERSION_DIR}/lib/dictBuilder/fastcover.c
  ${ZSTD_VERSION_DIR}/lib/dictBuilder/zdict.c
)

ADD_CONVENIENCE_LIBRARY(zstd
  ${ZSTD_SRCS}
  INCLUDE_DIRECTORIES ${ZSTD_VERSION_DIR}/lib/common
  )

# In case we have OPTIMIZE_DEBUG_BUILDS:BOOL=ON
# This seems to be a gcc12 bug, the combination
# FORCE_INLINE_TEMPLATE and -Og breaks the build.
STRING(REPLACE " -Og " "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  TARGET_COMPILE_OPTIONS(zstd_objlib PRIVATE ${HAS_MISSING_PROFILE})
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wdocumentation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

IF(WIN32_CLANG)
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-unused-function")
ENDIF()

MSVC_CPPCHECK_DISABLE()

#-----------------------------------------------------------------------------
# Add extra compilation flags
#-----------------------------------------------------------------------------
#INCLUDE(AddZstdCompilationFlags)
#ADD_ZSTD_COMPILATION_FLAGS()

# Always hide XXHash symbols
ADD_DEFINITIONS(-DXXH_NAMESPACE=ZSTD_)

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------
OPTION(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)
OPTION(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" OFF)
OPTION(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" ON)
OPTION(ZSTD_BUILD_CONTRIB "BUILD CONTRIB" OFF)
OPTION(ZSTD_BUILD_TESTS "BUILD TESTS" OFF)
if (MSVC)
    OPTION(ZSTD_USE_STATIC_RUNTIME "LINK TO STATIC RUN-TIME LIBRARIES" OFF)
endif ()

IF (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=4)
ELSE (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=0)
ENDIF (ZSTD_LEGACY_SUPPORT)
