/*****************************************************************************

Copyright (c) 2020, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0cpu_cache.h
Utilities related to CPU cache. */

#ifndef ut0cpu_cache_h
#define ut0cpu_cache_h

#include "ut0ut.h"
namespace ut {

/** CPU cache line size */
#ifdef __powerpc__
constexpr size_t INNODB_CACHE_LINE_SIZE = 128;
#else
constexpr size_t INNODB_CACHE_LINE_SIZE = 64;
#endif /* __powerpc__ */

/** Default kernel page size (not assuming huge pages support). */
constexpr size_t INNODB_KERNEL_PAGE_SIZE_DEFAULT = 4 * 1024;

/**
A utility wrapper class, which adds padding at the end of the wrapped structure,
so that the next object after it is guaranteed to be in the next cache line.
This is to avoid false-sharing.
Use this, as opposed to alignas(), to avoid problems with allocators which do
not handle over-aligned types.
 */
template <typename T>
struct Cacheline_padded : public T {
  char pad[INNODB_CACHE_LINE_SIZE];
  // "Inherit" constructors
  using T::T;
};

/**
A utility wrapper class, which aligns T to cacheline boundary.
This is to avoid false-sharing.
*/
template <typename T>
struct alignas(INNODB_CACHE_LINE_SIZE) Cacheline_aligned : public T {
  // "Inherit" constructors
  using T::T;
};

} /* namespace ut */

#endif /* ut0cpu_cache_h */
