/*****************************************************************************

Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

/** @file include/detail/ut0new.h
 Implementation bits and pieces of include/ut0new.h */

#ifndef detail_ut0new_h
#define detail_ut0new_h

#include <cstddef>
#include <utility>

#include "storage/innobase/include/detail/ut/aligned_alloc.h"
#include "storage/innobase/include/detail/ut/alloc.h"
#include "storage/innobase/include/detail/ut/large_page_alloc.h"
#include "storage/innobase/include/detail/ut/page_alloc.h"
#include "storage/innobase/include/ut0tuple.h"

namespace ut {
namespace detail {

template <typename T, typename Tuple, size_t... Args_index_seq>
inline void construct_impl(void *mem, size_t offset, Tuple &&tuple,
                           std::index_sequence<Args_index_seq...>) {
  new (reinterpret_cast<uint8_t *>(mem) + offset)
      T{std::get<Args_index_seq>(std::forward<Tuple>(tuple))...};
}

template <typename T, typename Tuple>
inline void construct(void *mem, size_t offset, Tuple &&tuple) {
  using N_args_seq = std::make_index_sequence<std::tuple_size<Tuple>::value>;
  construct_impl<T>(mem, offset, tuple, N_args_seq{});
}
/** Generic utility function which invokes placement-new statement on type T.
    Arguments to be passed to T constructor are contained within tuple-like
    container. To be able to unpack arguments from tuple-like container at
    compile-time, index sequence is used.
 */
template <typename T, typename Tuple, size_t... Args_index_seq>
inline void invoke_placement_new(void *mem, size_t offset, Tuple &&args,
                                 std::index_sequence<Args_index_seq...>) {
  new (reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(mem) + offset))
      T(std::get<Args_index_seq>(std::forward<Tuple>(args))...);
}

/** Compile-time for-loop which generates Count number of placement-new
    statements.
 */
template <size_t I, const size_t Count, typename T, size_t Offset,
          const size_t N_args_per_T, size_t Tuple_idx, typename Tuple>
struct Loop {
  static void run(void *mem, Tuple &&args) {
    using N_args_seq = std::make_index_sequence<N_args_per_T>;
    invoke_placement_new<T>(
        mem, Offset,
        select_from_tuple<Tuple_idx, Tuple_idx + N_args_per_T>(
            std::forward<Tuple>(args)),
        N_args_seq{});
    Loop<I + 1, Count, T, Offset + sizeof(T), N_args_per_T,
         Tuple_idx + N_args_per_T, Tuple>::run(mem, std::forward<Tuple>(args));
  }
};

/** Specialization which denotes that we are at the end of the loop. */
template <size_t Count, typename T, size_t Offset, size_t N_args_per_T,
          size_t Tuple_idx, typename Tuple>
struct Loop<Count, Count, T, Offset, N_args_per_T, Tuple_idx, Tuple> {
  static void run(void *mem, Tuple &&args) {}
};
}  // namespace detail
}  // namespace ut

#endif /* detail_ut0new_h */
