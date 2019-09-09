# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PROJECT(zstd)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)
SET(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")


SET(ZSTD_LIB_DIR
  "${CMAKE_SOURCE_DIR}/extra/zstd/lib")
INCLUDE_DIRECTORIES(
  ${ZSTD_LIB_DIR}/common
)
#SET(ZSTD_PUBLIC_HDRS ${ZSTD_LIB_DIR}/zstd.h)
SET(ZSTD_PUBLIC_HDRS
  ${ZSTD_LIB_DIR}/zstd.h
  ${ZSTD_LIB_DIR}/common/bitstream.h
  ${ZSTD_LIB_DIR}/common/compiler.h
  ${ZSTD_LIB_DIR}/common/error_private.h
  ${ZSTD_LIB_DIR}/common/fse.h
  ${ZSTD_LIB_DIR}/common/huf.h
  ${ZSTD_LIB_DIR}/common/mem.h
  ${ZSTD_LIB_DIR}/common/pool.h
  ${ZSTD_LIB_DIR}/common/threading.h
  ${ZSTD_LIB_DIR}/common/xxhash.h
  ${ZSTD_LIB_DIR}/common/zstd_errors.h
  ${ZSTD_LIB_DIR}/common/zstd_internal.h
  ${ZSTD_LIB_DIR}/compress/zstd_compress_internal.h
  ${ZSTD_LIB_DIR}/compress/zstd_double_fast.h
  ${ZSTD_LIB_DIR}/compress/zstd_fast.h
  ${ZSTD_LIB_DIR}/compress/zstd_lazy.h
  ${ZSTD_LIB_DIR}/compress/zstd_ldm.h
  ${ZSTD_LIB_DIR}/compress/zstdmt_compress.h
  ${ZSTD_LIB_DIR}/compress/zstd_opt.h
  ${ZSTD_LIB_DIR}/deprecated/zbuff.h
  ${ZSTD_LIB_DIR}/dictBuilder/divsufsort.h
  ${ZSTD_LIB_DIR}/dictBuilder/zdict.h
)

SET(ZSTD_SRCS
  ${ZSTD_LIB_DIR}/common/entropy_common.c
  ${ZSTD_LIB_DIR}/common/error_private.c
  ${ZSTD_LIB_DIR}/common/fse_decompress.c
  ${ZSTD_LIB_DIR}/common/pool.c
  ${ZSTD_LIB_DIR}/common/threading.c
  ${ZSTD_LIB_DIR}/common/xxhash.c
  ${ZSTD_LIB_DIR}/common/zstd_common.c
  ${ZSTD_LIB_DIR}/compress/fse_compress.c
  ${ZSTD_LIB_DIR}/compress/hist.c
  ${ZSTD_LIB_DIR}/compress/huf_compress.c
  ${ZSTD_LIB_DIR}/compress/zstd_compress.c
  ${ZSTD_LIB_DIR}/compress/zstd_compress_literals.c
  ${ZSTD_LIB_DIR}/compress/zstd_compress_sequences.c
  ${ZSTD_LIB_DIR}/compress/zstd_double_fast.c
  ${ZSTD_LIB_DIR}/compress/zstd_fast.c
  ${ZSTD_LIB_DIR}/compress/zstd_lazy.c
  ${ZSTD_LIB_DIR}/compress/zstd_ldm.c
  ${ZSTD_LIB_DIR}/compress/zstdmt_compress.c
  ${ZSTD_LIB_DIR}/compress/zstd_opt.c
  ${ZSTD_LIB_DIR}/decompress/huf_decompress.c
  ${ZSTD_LIB_DIR}/decompress/zstd_decompress.c
  ${ZSTD_LIB_DIR}/decompress/zstd_decompress_block.c
  ${ZSTD_LIB_DIR}/decompress/zstd_ddict.c
  ${ZSTD_LIB_DIR}/deprecated/zbuff_common.c
  ${ZSTD_LIB_DIR}/deprecated/zbuff_compress.c
  ${ZSTD_LIB_DIR}/deprecated/zbuff_decompress.c
  ${ZSTD_LIB_DIR}/dictBuilder/cover.c
  ${ZSTD_LIB_DIR}/dictBuilder/divsufsort.c
  ${ZSTD_LIB_DIR}/dictBuilder/zdict.c
  ${ZSTD_LIB_DIR}/zstd.h
  ${ZSTD_LIB_DIR}/common/bitstream.h
  ${ZSTD_LIB_DIR}/common/compiler.h
  ${ZSTD_LIB_DIR}/common/error_private.h
  ${ZSTD_LIB_DIR}/common/fse.h
  ${ZSTD_LIB_DIR}/common/huf.h
  ${ZSTD_LIB_DIR}/common/mem.h
  ${ZSTD_LIB_DIR}/common/pool.h
  ${ZSTD_LIB_DIR}/common/threading.h
  ${ZSTD_LIB_DIR}/common/xxhash.h
  ${ZSTD_LIB_DIR}/common/zstd_errors.h
  ${ZSTD_LIB_DIR}/common/zstd_internal.h
  ${ZSTD_LIB_DIR}/compress/zstd_compress_internal.h
  ${ZSTD_LIB_DIR}/compress/zstd_double_fast.h
  ${ZSTD_LIB_DIR}/compress/zstd_fast.h
  ${ZSTD_LIB_DIR}/compress/zstd_lazy.h
  ${ZSTD_LIB_DIR}/compress/zstd_ldm.h
  ${ZSTD_LIB_DIR}/compress/zstdmt_compress.h
  ${ZSTD_LIB_DIR}/compress/zstd_opt.h
  ${ZSTD_LIB_DIR}/deprecated/zbuff.h
  ${ZSTD_LIB_DIR}/dictBuilder/divsufsort.h
  ${ZSTD_LIB_DIR}/dictBuilder/zdict.h
)

ADD_CONVENIENCE_LIBRARY(zstd
  ${ZSTD_SRCS} ${ZSTD_PUBLIC_HDRS})
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
