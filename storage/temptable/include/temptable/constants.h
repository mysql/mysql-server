/* Copyright (c) 2017, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file storage/temptable/include/temptable/constants.h
TempTable constants. */

#ifndef TEMPTABLE_CONSTANTS_H
#define TEMPTABLE_CONSTANTS_H

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

} /* namespace temptable */

#endif /* TEMPTABLE_CONSTANTS_H */
