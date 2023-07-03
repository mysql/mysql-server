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

/** @file include/ut0guarded.h
The ut::Guarded template which protects access to another class with mutex. */

#ifndef ut0guarded_h
#define ut0guarded_h

#ifndef UNIV_LIBRARY
#include "ut0cpu_cache.h"
#include "ut0mutex.h"
#endif

namespace ut {
// TBD: should latch_id be specified at runtime?
template <typename Inner, latch_id_t latch_id>
class Guarded {
#ifndef UNIV_LIBRARY
  Cacheline_padded<IB_mutex> mutex{latch_id};
#endif
  Inner inner;

 public:
  template <typename F>
  auto latch_and_execute(F &&f, const ut::Location &loc) {
#ifndef UNIV_LIBRARY
    IB_mutex_guard guard{&mutex, loc};
#endif
    return std::forward<F>(f)(inner);
  }

  const Inner &peek() const { return inner; }
};
}  // namespace ut
#endif /* ut0guarded_h */
