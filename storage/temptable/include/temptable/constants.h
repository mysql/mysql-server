/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/constants.h
TempTable constants. */

#ifndef TEMPTABLE_CONSTANTS_H
#define TEMPTABLE_CONSTANTS_H

#include "my_config.h"

namespace temptable {

/** Multiply a number by 1024.
 * @return n * 1024. */
inline constexpr unsigned long long operator"" _KiB(
    /** [in] Number to multiply. */
    unsigned long long n) {
  return n << 10;
}

/** Multiply a number by 1024 * 1024.
 * @return n * 1024 * 1024. */
inline constexpr unsigned long long operator"" _MiB(
    /** [in] Number to multiply. */
    unsigned long long n) {
  return n << 20;
}

/** Multiply a number by 1024 * 1024 * 1024.
 * @return n * 1024 * 1024 * 1024. */
inline constexpr unsigned long long operator"" _GiB(
    /** [in] Number to multiply. */
    unsigned long long n) {
  return n << 30;
}

/** log2(allocator max block size in MiB). Ie.
 * 2 ^ ALLOCATOR_MAX_BLOCK_MB_EXP * 1024^2 = ALLOCATOR_MAX_BLOCK_BYTES. */
constexpr size_t ALLOCATOR_MAX_BLOCK_MB_EXP = 9;

/** Limit on the size of a block created by `Allocator` (in bytes). A larger
 * block could still be created if a single allocation request with bigger size
 * is received. */
constexpr size_t ALLOCATOR_MAX_BLOCK_BYTES = 1_MiB
                                             << ALLOCATOR_MAX_BLOCK_MB_EXP;

/** `Storage` page size. */
constexpr size_t STORAGE_PAGE_SIZE = 64_KiB;

/** Number of buckets to have by default in a hash index. */
constexpr size_t INDEX_DEFAULT_HASH_TABLE_BUCKETS = 1024;

/** Store build-type information into the constexpr expression. */
#ifndef NDEBUG
constexpr bool DEBUG_BUILD = true;
#else
constexpr bool DEBUG_BUILD = false;
#endif /* NDEBUG */

/** Store L1-dcache size information into the constexpr expression. */
constexpr size_t L1_DCACHE_SIZE = CPU_LEVEL1_DCACHE_LINESIZE;

/** Number of shards in key-value store. */
constexpr size_t KV_STORE_SHARDS_COUNT = 16 * 1024;

/** Size of a pool containing shared-blocks. */
constexpr size_t SHARED_BLOCK_POOL_SIZE = 16 * 1024;

} /* namespace temptable */

#endif /* TEMPTABLE_CONSTANTS_H */
