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

SET(ZZZ "zstd-1.5.0")
SET(ZSTD_SRCS
  ${ZZZ}/lib/common/debug.c
  ${ZZZ}/lib/common/entropy_common.c
  ${ZZZ}/lib/common/error_private.c
  ${ZZZ}/lib/common/fse_decompress.c
  ${ZZZ}/lib/common/pool.c
  ${ZZZ}/lib/common/threading.c
  ${ZZZ}/lib/common/xxhash.c
  ${ZZZ}/lib/common/zstd_common.c

  ${ZZZ}/lib/compress/fse_compress.c
  ${ZZZ}/lib/compress/hist.c
  ${ZZZ}/lib/compress/huf_compress.c
  ${ZZZ}/lib/compress/zstd_compress.c
  ${ZZZ}/lib/compress/zstd_compress_literals.c
  ${ZZZ}/lib/compress/zstd_compress_sequences.c
  ${ZZZ}/lib/compress/zstd_compress_superblock.c
  ${ZZZ}/lib/compress/zstd_double_fast.c
  ${ZZZ}/lib/compress/zstd_fast.c
  ${ZZZ}/lib/compress/zstd_lazy.c
  ${ZZZ}/lib/compress/zstd_ldm.c
  ${ZZZ}/lib/compress/zstdmt_compress.c
  ${ZZZ}/lib/compress/zstd_opt.c

  ${ZZZ}/lib/decompress/huf_decompress.c
  ${ZZZ}/lib/decompress/zstd_ddict.c
  ${ZZZ}/lib/decompress/zstd_decompress_block.c
  ${ZZZ}/lib/decompress/zstd_decompress.c

  ${ZZZ}/lib/dictBuilder/cover.c
  ${ZZZ}/lib/dictBuilder/divsufsort.c
  ${ZZZ}/lib/dictBuilder/fastcover.c
  ${ZZZ}/lib/dictBuilder/zdict.c
)

ADD_CONVENIENCE_LIBRARY(zstd
  ${ZSTD_SRCS}
  INCLUDE_DIRECTORIES ${ZZZ}/lib/common
  )

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  TARGET_COMPILE_OPTIONS(zstd_objlib PRIVATE ${HAS_MISSING_PROFILE})
ENDIF()


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
