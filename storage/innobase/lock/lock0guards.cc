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

#define LOCK_MODULE_IMPLEMENTATION

#include "lock0guards.h"
#include "lock0priv.h"
#include "sync0rw.h"

namespace locksys {

/* Global_exclusive_latch_guard */

Global_exclusive_latch_guard::Global_exclusive_latch_guard(
    ut::Location location) {
  lock_sys->latches.global_latch.x_lock(location);
}

Global_exclusive_latch_guard::~Global_exclusive_latch_guard() {
  lock_sys->latches.global_latch.x_unlock();
}

/* Global_exclusive_try_latch */

Global_exclusive_try_latch::Global_exclusive_try_latch(ut::Location location) {
  m_owns_exclusive_global_latch =
      lock_sys->latches.global_latch.try_x_lock(location);
}

Global_exclusive_try_latch::~Global_exclusive_try_latch() {
  if (m_owns_exclusive_global_latch) {
    lock_sys->latches.global_latch.x_unlock();
    m_owns_exclusive_global_latch = false;
  }
}

/* Shard_naked_latch_guard */

Shard_naked_latch_guard::Shard_naked_latch_guard(ut::Location location,
                                                 Lock_mutex &shard_mutex)
    : m_shard_mutex{shard_mutex} {
  ut_ad(owns_shared_global_latch());
  mutex_enter_inline(&m_shard_mutex, location);
}

Shard_naked_latch_guard::Shard_naked_latch_guard(ut::Location location,
                                                 const table_id_t &table_id)
    : Shard_naked_latch_guard{
          location, lock_sys->latches.table_shards.get_mutex(table_id)} {}

Shard_naked_latch_guard::Shard_naked_latch_guard(ut::Location location,
                                                 const page_id_t &page_id)
    : Shard_naked_latch_guard{
          location, lock_sys->latches.page_shards.get_mutex(page_id)} {}

Shard_naked_latch_guard::~Shard_naked_latch_guard() {
  mutex_exit(&m_shard_mutex);
}

/* Global_shared_latch_guard */

Global_shared_latch_guard::Global_shared_latch_guard(ut::Location location) {
  lock_sys->latches.global_latch.s_lock(location);
}

Global_shared_latch_guard::~Global_shared_latch_guard() {
  lock_sys->latches.global_latch.s_unlock();
}
bool Global_shared_latch_guard::is_x_blocked_by_us() {
  return lock_sys->latches.global_latch.is_x_blocked_by_our_s();
}

/* Shard_naked_latches_guard */

Shard_naked_latches_guard::Shard_naked_latches_guard(Lock_mutex &shard_mutex_a,
                                                     Lock_mutex &shard_mutex_b)
    : m_shard_mutex_1{*std::min(&shard_mutex_a, &shard_mutex_b, MUTEX_ORDER)},
      m_shard_mutex_2{*std::max(&shard_mutex_a, &shard_mutex_b, MUTEX_ORDER)} {
  ut_ad(owns_shared_global_latch());
  if (&m_shard_mutex_1 != &m_shard_mutex_2) {
    mutex_enter(&m_shard_mutex_1);
  }
  mutex_enter(&m_shard_mutex_2);
}

Shard_naked_latches_guard::Shard_naked_latches_guard(const buf_block_t &block_a,
                                                     const buf_block_t &block_b)
    : Shard_naked_latches_guard{
          lock_sys->latches.page_shards.get_mutex(block_a.get_page_id()),
          lock_sys->latches.page_shards.get_mutex(block_b.get_page_id())} {}

Shard_naked_latches_guard::~Shard_naked_latches_guard() {
  mutex_exit(&m_shard_mutex_2);
  if (&m_shard_mutex_1 != &m_shard_mutex_2) {
    mutex_exit(&m_shard_mutex_1);
  }
}

}  // namespace locksys
