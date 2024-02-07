/*****************************************************************************

Copyright (c) 2017, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

*****************************************************************************/

/**************************************************/ /**
 @file include/sync0sharded_rw.h

 The sharded read-write lock (for threads).

 The s-lock scales better than in single rw-lock,
 but the x-lock is much slower.

 *******************************************************/

#ifndef sync0sharded_rw_h
#define sync0sharded_rw_h

#include "sync0rw.h"
#include "ut0cpu_cache.h"
#include "ut0rnd.h"
#include "ut0ut.h"

#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY

/** Rw-lock with very fast, highly concurrent s-lock but slower x-lock.
It's basically array of rw-locks. When s-lock is being acquired, single
rw-lock from array is selected randomly and s-locked. Therefore, all
rw-locks from array has to be x-locked when x-lock is being acquired.

Purpose of this data structure is to reduce contention on single atomic
in single rw-lock when a lot of threads need to acquire s-lock very often,
but x-lock is very rare. */
class Sharded_rw_lock {
 public:
  void create(
#ifdef UNIV_PFS_RWLOCK
      mysql_pfs_key_t pfs_key,
#endif
      latch_id_t latch_id, size_t n_shards) {
    ut_ad(ut_is_2pow(n_shards));
    m_n_shards = n_shards;

    m_shards = static_cast<Shard *>(
        ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(Shard) * n_shards));

    for_each([
#ifdef UNIV_PFS_RWLOCK
                 pfs_key,
#endif
                 latch_id](rw_lock_t &lock) {
      static_cast<void>(latch_id);  // clang -Wunused-lambda-capture
      rw_lock_create(pfs_key, &lock, latch_id);
    });
  }

  void free() {
    ut_a(m_shards != nullptr);

    for_each([](rw_lock_t &lock) { rw_lock_free(&lock); });

    ut::free(m_shards);
    m_shards = nullptr;
    m_n_shards = 0;
  }

  size_t s_lock(ut::Location location) {
    const size_t shard_no =
        default_indexer_t<>::get_rnd_index() & (m_n_shards - 1);
    rw_lock_s_lock_gen(&m_shards[shard_no], 0, location);
    return shard_no;
  }

  void s_unlock(size_t shard_no) {
    ut_a(shard_no < m_n_shards);
    rw_lock_s_unlock(&m_shards[shard_no]);
  }
  /** Checks if there is a thread requesting an x-latch waiting for threads to
  release their s-latches on given shard.
  @param[in]  shard_no  The shard to check.
  @return true iff there is an x-latcher blocked by s-latchers on shard_no. */
  bool is_x_blocked_by_s(size_t shard_no) {
    ut_a(shard_no < m_n_shards);
    return m_shards[shard_no].is_x_blocked_by_s();
  }
  /**
  Tries to obtain exclusive latch - similar to x_lock(), but non-blocking, and
  thus can fail.
  @return true iff succeeded to acquire the exclusive latch
  */
  bool try_x_lock(ut::Location location) {
    for (size_t shard_no = 0; shard_no < m_n_shards; ++shard_no) {
      if (!rw_lock_x_lock_nowait(&m_shards[shard_no], location)) {
        while (0 < shard_no--) {
          rw_lock_x_unlock(&m_shards[shard_no]);
        }
        return (false);
      }
    }
    return (true);
  }

  void x_lock(ut::Location location) {
    for_each([location](rw_lock_t &lock) {
      rw_lock_x_lock_gen(&lock, 0, location);
    });
  }

  void x_unlock() {
    for_each([](rw_lock_t &lock) { rw_lock_x_unlock(&lock); });
  }

#ifdef UNIV_DEBUG
  bool s_own(size_t shard_no) const {
    return rw_lock_own(&m_shards[shard_no], RW_LOCK_S);
  }

  bool x_own() const { return rw_lock_own(&m_shards[0], RW_LOCK_X); }
#endif /* !UNIV_DEBUG */

 private:
  using Shard = ut::Cacheline_padded<rw_lock_t>;

  template <typename F>
  void for_each(F f) {
    std::for_each(m_shards, m_shards + m_n_shards, f);
  }

  Shard *m_shards = nullptr;

  size_t m_n_shards = 0;
};

#else /* !UNIV_LIBRARY */

/* For UNIV_LIBRARY, rw_lock is no-op, so sharded rw-lock is also no-op. */

class Sharded_rw_lock {
 public:
  void create(
#ifdef UNIV_PFS_RWLOCK
      mysql_pfs_key_t pfs_key,
#endif
      latch_id_t latch_id, size_t n_shards) {
  }

  void free() {}

  size_t s_lock() { return 0; }

  void s_unlock(size_t shard_no) { ut_a(shard_no == 0); }

  void x_lock() {}

  void x_unlock() {}
};

#endif /* UNIV_LIBRARY */
#endif /* UNIV_HOTBACKUP */

#endif /* sync0sharded_rw.h */
