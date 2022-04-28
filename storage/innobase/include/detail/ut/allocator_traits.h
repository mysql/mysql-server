/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

/** @file include/detail/ut/allocator_traits.h
    Simple allocator traits.
 */

#ifndef detail_ut_allocator_traits_h
#define detail_ut_allocator_traits_h

#if defined(_WIN32) && defined(MYSQL_SERVER)
#include <mutex>
#include "jemalloc_win.h"
#endif /* _WIN32 && MYSQL_SERVER */

namespace ut {
namespace detail {

#if defined(_WIN32) && defined(MYSQL_SERVER)
/** Wrapper functions for using jemalloc on Windows.
If jemalloc.dll is available and its use is enabled, the
init_malloc_pointers function will set up the pfn_malloc, pfn_calloc,
pfn_realloc and pfn_free function pointers to point to the corresponding
jemalloc functions. Otherwise they will point to the standard MSVC library
implementations of the malloc, calloc etc. */

inline void *malloc(size_t nbytes) {
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
  return mysys::detail::pfn_malloc(nbytes);
}
inline void *calloc(size_t nbytes) {
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
  return mysys::detail::pfn_calloc(1, nbytes);
}
inline void *realloc(void *ptr, size_t nbytes) {
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
  return mysys::detail::pfn_realloc(ptr, nbytes);
}
inline void free(void *ptr) {
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
  mysys::detail::pfn_free(ptr);
}
#else
inline void *malloc(size_t nbytes) { return std::malloc(nbytes); }
inline void *calloc(size_t nbytes) { return std::calloc(1, nbytes); }
inline void *realloc(void *ptr, size_t nbytes) {
  return std::realloc(ptr, nbytes);
}
inline void free(void *ptr) { std::free(ptr); }
#endif /* _WIN32 && MYSQL_SERVER */

/** Simple allocator traits. */
template <bool Pfs_instrumented>
struct allocator_traits {
  // Is allocator PFS instrumented or not
  static constexpr auto is_pfs_instrumented_v = Pfs_instrumented;
};

/** Simple wrapping type around malloc, calloc and friends.*/
struct Alloc_fn {
  static void *malloc(size_t nbytes) { return ut::detail::malloc(nbytes); }

  static void *calloc(size_t nbytes) { return ut::detail::calloc(nbytes); }

  template <bool Zero_initialized>
  static void *alloc(size_t size) {
    if constexpr (Zero_initialized)
      return Alloc_fn::calloc(size);
    else
      return Alloc_fn::malloc(size);
  }

  static void *realloc(void *ptr, size_t nbytes) {
    return ut::detail::realloc(ptr, nbytes);
  }

  static void free(void *ptr) { ut::detail::free(ptr); }
};

}  // namespace detail
}  // namespace ut

#endif
