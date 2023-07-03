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

#define LOCK_MODULE_IMPLEMENTATION

#include "lock0latches.h"
#include "lock0lock.h"
#include "lock0priv.h"

namespace locksys {

size_t Latches::Page_shards::get_shard(const page_id_t &page_id) {
  /* We always use lock_sys->rec_hash regardless of the exact type of the
  lock. It may happen that the lock is a predicate lock, in which case, it would
  make more sense to use hash_calc_cell_id with proper hash table size. The
  current implementation works, because the size of all three hashmaps is always
  the same. This allows an interface with less arguments. */
  ut_ad(lock_sys->rec_hash->get_n_cells() ==
        lock_sys->prdt_hash->get_n_cells());
  ut_ad(lock_sys->rec_hash->get_n_cells() ==
        lock_sys->prdt_page_hash->get_n_cells());
  /* We need a property that if two pages are mapped to the same bucket of the
  hash table, and thus their lock queues are merged, then these two lock queues
  are protected by the same shard. This is why to compute the shard we use the
  cell_id as the input and not the original lock_rec_hash_value's result. */
  return hash_calc_cell_id(lock_rec_hash_value(page_id), lock_sys->rec_hash) %
         SHARDS_COUNT;
}

const Lock_mutex &Latches::Page_shards::get_mutex(
    const page_id_t &page_id) const {
  return mutexes[get_shard(page_id)];
}

Lock_mutex &Latches::Page_shards::get_mutex(const page_id_t &page_id) {
  /* See "Effective C++ item 3: Use const whenever possible" for explanation of
  this pattern, which avoids code duplication by reusing const version. */
  return const_cast<Lock_mutex &>(
      const_cast<const Latches::Page_shards *>(this)->get_mutex(page_id));
}

size_t Latches::Table_shards::get_shard(const table_id_t table_id) {
  return table_id % SHARDS_COUNT;
}

const Lock_mutex &Latches::Table_shards::get_mutex(
    const table_id_t table_id) const {
  return mutexes[get_shard(table_id)];
}

Lock_mutex &Latches::Table_shards::get_mutex(const table_id_t table_id) {
  /* See "Effective C++ item 3: Use const whenever possible" for explanation of
  this pattern, which avoids code duplication by reusing const version. */
  return const_cast<Lock_mutex &>(
      const_cast<const Latches::Table_shards *>(this)->get_mutex(table_id));
}

const Lock_mutex &Latches::Table_shards::get_mutex(
    const dict_table_t &table) const {
  return get_mutex(table.id);
}

thread_local size_t Latches::Unique_sharded_rw_lock::m_shard_id{NOT_IN_USE};

Latches::Unique_sharded_rw_lock::Unique_sharded_rw_lock() {
  rw_lock.create(
#ifdef UNIV_PFS_RWLOCK
      lock_sys_global_rw_lock_key,
#endif
      LATCH_ID_LOCK_SYS_GLOBAL, 64);
}

Latches::Unique_sharded_rw_lock::~Unique_sharded_rw_lock() { rw_lock.free(); }

Latches::Page_shards::Page_shards() {
  for (size_t i = 0; i < SHARDS_COUNT; ++i) {
    mutex_create(LATCH_ID_LOCK_SYS_PAGE, mutexes + i);
  }
}

Latches::Page_shards::~Page_shards() {
  for (size_t i = 0; i < SHARDS_COUNT; ++i) {
    mutex_destroy(mutexes + i);
  }
}

Latches::Table_shards::Table_shards() {
  for (size_t i = 0; i < SHARDS_COUNT; ++i) {
    mutex_create(LATCH_ID_LOCK_SYS_TABLE, mutexes + i);
  }
}

Latches::Table_shards::~Table_shards() {
  for (size_t i = 0; i < SHARDS_COUNT; ++i) {
    mutex_destroy(mutexes + i);
  }
}

}  // namespace locksys
