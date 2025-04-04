/*****************************************************************************

Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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

/** @file lock/lock0iter.cc
 Lock queue iterator. Can iterate over table and record
 lock queues.

 Created July 16, 2007 Vasil Dimov
 *******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include "lock0iter.h"

#include "dict0dd.h"
#include "lock0lock.h"
#include "lock0priv.h"
#include "univ.i"

template <typename F>
bool All_locks_iterator::iterate_over_current_table(F &&f) {
  if (m_table_ids.size() == m_bucket_id) {
    return false;
  }
  ut_ad(m_bucket_id < m_table_ids.size());
  const table_id_t table_id = m_table_ids[m_bucket_id];
  locksys::find_on_table(table_id, [&](const lock_t &lock) {
    std::forward<F>(f)(lock);
    return false;
  });
  m_bucket_id++;
  return true;
}

template <typename F>
bool All_locks_iterator::iterate_over_current_cell(Locks_hashtable &hash_table,
                                                   F &&f) {
  locksys::Global_shared_latch_guard shared_latch_guard{UT_LOCATION_HERE};

  if (m_bucket_id == 0) {
    m_lock_sys_n_resizes_at_the_beginning = lock_sys->n_resizes;
  }
  /*
    Current implementation does not crash in case of lock_sys_resize() executed
    concurrently with iterating over locks, instead returning incomplete data.
    This is better than reporting some locks twice, which would violate primary
    key constraint, and could happen if we blindly report all locks from
    m_bucket_id-th cell, without first checking if hash table was resized
    causing reshuffling of entries among cells.
    For now, the only use of this iterator is in performance_schema.data_locks
    and performance_schema.data_wait_locks which both provide no guarantee that
    the view of the locks is consistent. I consider current implementation a
    good trade-off between simplicity of implementation and correctness, as any
    problems can only occur during dynamically resizing the buffer pool (which
    causes resize of lock-sys hash tables) and the only manifestation will be
    that some locks are not reported (which is always possible anyway given
    that we don't hold any latch permanently).
    A more complicated solution would be to have a dedicated rwlock x-acquired
    for lock_sys_resize() and s-acquired by the iterator constructor and
    released in the destructor.
    Having long-lasting latches, and non-trivial life-cycle of this class seems
    to be introducing too much complexity to me (for one thing, reasoning about
    latching order is very complicated then).
  */
  if (m_lock_sys_n_resizes_at_the_beginning != lock_sys->n_resizes ||
      hash_table.get_n_cells() <= m_bucket_id) {
    return false;
  }
  const size_t shard_id = m_bucket_id % locksys::Latches::SHARDS_COUNT;
  /* We need to latch the shard of lock-sys which contains the locks from
  hash_get_nth_cell(hash_table, m_bucket_id). We know that they must be in a
  single shard, as otherwise lock-sys wouldn't be able to iterate over bucket.*/
  locksys::Shard_naked_latch_guard shard_guard{UT_LOCATION_HERE, nullptr,
                                               m_bucket_id};
  m_bucket_id = hash_table.find_set_in_this_shard(m_bucket_id);
  if (m_bucket_id < hash_table.get_n_cells()) {
    hash_table.find_in_cell(m_bucket_id, [&](lock_t *lock) {
      std::forward<F>(f)(*lock);
      return false;
    });

    m_bucket_id += locksys::Latches::SHARDS_COUNT;
  }
  if (m_bucket_id < hash_table.get_n_cells()) {
    return true;
  }
  m_bucket_id = shard_id + 1;
  return m_bucket_id != locksys::Latches::SHARDS_COUNT;
}

bool All_locks_iterator::iterate_over_next_batch(
    const std::function<void(const lock_t &lock)> &f) {
  /*
    We want to report at least one lock.
    We will search for it in:
    - table locks, one table at a time
    - predicate page locks, one hash table cell at a time
    - predicate locks, one hash table cell at a time
    - record locks, one hash table cell at a time
    When inspecting each of this places, we report all locks found there.
    We stop as soon as we found something.
  */
  bool found_at_least_one_lock = false;

  auto report_lock = [&found_at_least_one_lock, &f](const lock_t &lock) {
    f(lock);
    found_at_least_one_lock = true;
  };

  while (!found_at_least_one_lock && m_stage != stage_t::DONE) {
    bool is_stage_finished;

    switch (m_stage) {
      case stage_t::NOT_STARTED: {
        m_table_ids = dict_get_all_table_ids();
        is_stage_finished = true;
        break;
      }

      case stage_t::TABLE_LOCKS: {
        is_stage_finished = !iterate_over_current_table(report_lock);
        break;
      }

      case stage_t::PRDT_PAGE_LOCKS: {
        is_stage_finished =
            !iterate_over_current_cell(lock_sys->prdt_page_hash, report_lock);
        break;
      }

      case stage_t::PRDT_LOCKS: {
        is_stage_finished =
            !iterate_over_current_cell(lock_sys->prdt_hash, report_lock);
        break;
      }

      case stage_t::REC_LOCKS: {
        is_stage_finished =
            !iterate_over_current_cell(lock_sys->rec_hash, report_lock);

        if (found_at_least_one_lock) {
          DEBUG_SYNC_C("all_locks_iterator_found_record_lock");
        }
        break;
      }

      default:
        ut_error;
    }

    if (is_stage_finished) {
      m_stage = static_cast<stage_t>(to_int(m_stage) + 1);
      m_bucket_id = 0;
    }
  }

  return m_stage == stage_t::DONE;
}

namespace locksys {
const lock_t *find_blockers(const lock_t &wait_lock,
                            std::function<bool(const lock_t &)> visitor) {
  ut_ad(locksys::owns_lock_shard(&wait_lock));
  ut_a(wait_lock.is_waiting());
  locksys::Trx_locks_cache wait_lock_cache{};
  if (lock_get_type_low(&wait_lock) == LOCK_REC) {
    const uint16_t heap_no = lock_rec_find_set_bit(&wait_lock);
    const auto found = wait_lock.hash_table().find_on_record(
        RecID{&wait_lock, heap_no}, [&](lock_t *lock) {
          if (lock == &wait_lock) {
            return true;
          }
          if (locksys::has_to_wait(&wait_lock, lock, wait_lock_cache)) {
            return visitor(*lock);
          }
          return false;
        });
    return found == &wait_lock ? nullptr : found;
  }
  for (auto lock : wait_lock.tab_lock.table->locks) {
    if (lock == &wait_lock) {
      return nullptr;
    }
    if (locksys::has_to_wait(&wait_lock, lock, wait_lock_cache)) {
      if (visitor(*lock)) {
        return lock;
      }
    }
  }
  return nullptr;
}

void find_on_table(const table_id_t table_id,
                   std::function<bool(const lock_t &)> visitor) {
  /* A thread which is dropping the table is not expecting the n_ref_count
  to be above 0 (or 1 if we count the thread itself), because it holds exclusive
  MDL on the table, and so nobody else should be accessing it. Yet, this method
  here will call dd_table_open_on_id_in_mem(..) which is incrementing
  n_ref_count. However, before releasing dict_sys->mutex it will decrement
  n_ref_count again by calling table->release(), so the dropping thread should
  not observe the state with elevated n_ref_count, because it checks it while
  holding the dict_sys->mutex. Because visitor might be quite heavy, we do so,
  before calling visitor. This, however, means visitor is called without holding
  dict_sys->mutex nor even n_ref_count protection, thus it is crucial, that we
  ensure table is not freed by other means: namely we verified that there is at
  least one lock in table->locks, and we keep the shard mutex which prevents
  anyone from releasing it - this is sufficient, and a technique already used in
  other parts of the lock_sys, because one can not free a dict_table_t unless
  all locks are released first. One drawback of this approach is that we acquire
  lock_sys shard mutex, while holding dict_sys->mutex. To minimize the impact of
  doing so, we only do so if table->locks appears non-empty - which we can check
  without any mutex because it is an atomic. Of course, because we don't hold
  any mutex, there's nothing preventing anyone from adding or removing locks on
  this table, but this is fine, as we do not promise at which exact moment
  during the call table->locks are inspected, so we can pick a convenient
  linearization moment, or even check it twice, and the caller can't complain.
  There are important reasons we use table->release() instead of a more regular
  dict_table_close or dd_table_close:
     - The dict_table_close, as a side effect might call dict_stats_deinit()
       when the n_ref_count drops to zero, which could interfere with
       create_table_info_t::create_table_update_dict() which is calling
       dict_stats_update() without properly bumping the n_ref_count.
       In worst case, our thread could deinitialize the stats just before
       dict_stats_save() is called from dict_stats_update() to persist them.
     - The dict_table_close() calls table->lock() which would violate latching
       order if done while holding lock sys shard mutex.
     - It's a bit cheaper and we try to not hog dict_sys->mutex */
  dict_sys_mutex_enter();
  dict_table_t *table = dd_table_open_on_id_in_mem(table_id, true);
  if (table != nullptr) {
    /* avoid waiting for shard mutex if there are no locks to report */
    if (table->locks.get_length()) {
      Shard_latch_guard table_latch_guard{UT_LOCATION_HERE, *table};
      const bool any_lock_exists = (0 < table->locks.get_length());
      table->release();
      dict_sys_mutex_exit();
      if (any_lock_exists) {
        for (auto lock : table->locks) {
          if (visitor(*lock)) {
            return;
          }
        }
      } /* else: table might be dangling, so do not even try to access it! */
      return;
    }
    table->release();
  }
  dict_sys_mutex_exit();
}
}  // namespace locksys
