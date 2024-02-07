/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

}  // namespace detail
}  // namespace ut

#endif /* detail_ut0new_h */
