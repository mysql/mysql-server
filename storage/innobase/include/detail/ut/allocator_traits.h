/*****************************************************************************

Copyright (c) 2021, Oracle and/or its affiliates.

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

namespace ut {
namespace detail {

/** Simple allocator traits. */
template <bool Pfs_instrumented>
struct allocator_traits {
  // Is allocator PFS instrumented or not
  static constexpr auto is_pfs_instrumented_v = Pfs_instrumented;
};

/** Simple wrapping type around malloc, calloc and friends.*/
template <bool Zero_initialized>
struct Alloc_fn {
  static void *alloc(size_t nbytes) { return std::malloc(nbytes); }
};
template <>
struct Alloc_fn<true> {
  static void *alloc(size_t nbytes) { return std::calloc(1, nbytes); }
};

}  // namespace detail
}  // namespace ut

#endif
