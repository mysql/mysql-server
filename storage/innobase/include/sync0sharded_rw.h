/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

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
  void create(mysql_pfs_key_t pfs_key, latch_level_t latch_level,
              size_t n_shards) {
    m_n_shards = n_shards;

    m_shards = static_cast<Shard *>(ut_zalloc_nokey(sizeof(Shard) * n_shards));

    for_each([pfs_key, latch_level](rw_lock_t &lock) {
      static_cast<void>(latch_level);  // clang -Wunused-lambda-capture
      rw_lock_create(pfs_key, &lock, latch_level);
    });
  }

  void free() {
    ut_a(m_shards != nullptr);

    for_each([](rw_lock_t &lock) { rw_lock_free(&lock); });

    ut_free(m_shards);
    m_shards = nullptr;
    m_n_shards = 0;
  }

  size_t s_lock() {
    const size_t shard_no = ut_rnd_interval(0, m_n_shards - 1);
    rw_lock_s_lock(&m_shards[shard_no].lock);
    return shard_no;
  }

  ibool s_lock_nowait(size_t &shard_no, const char *file, ulint line) {
    shard_no = ut_rnd_interval(0, m_n_shards - 1);
    return rw_lock_s_lock_nowait(&m_shards[shard_no].lock, file, line);
  }

  void s_unlock(size_t shard_no) {
    ut_a(shard_no < m_n_shards);
    rw_lock_s_unlock(&m_shards[shard_no].lock);
  }

  void x_lock() {
    for_each([](rw_lock_t &lock) { rw_lock_x_lock(&lock); });
  }

  void x_unlock() {
    for_each([](rw_lock_t &lock) { rw_lock_x_unlock(&lock); });
  }

#ifdef UNIV_DEBUG
  bool s_own(size_t shard_no) const {
    return rw_lock_own(&m_shards[shard_no].lock, RW_LOCK_S);
  }

  bool x_own() const { return rw_lock_own(&m_shards[0].lock, RW_LOCK_X); }
#endif /* !UNIV_DEBUG */

 private:
  struct Shard {
    rw_lock_t lock;

    char pad[INNOBASE_CACHE_LINE_SIZE];
  };

  template <typename F>
  void for_each(F f) {
    std::for_each(m_shards, m_shards + m_n_shards,
                  [&f](Shard &shard) { f(shard.lock); });
  }

  Shard *m_shards = nullptr;

  size_t m_n_shards = 0;
};

#else /* !UNIV_LIBRARY */

/* For UNIV_LIBRARY, rw_lock is no-op, so sharded rw-lock is also no-op. */

class Sharded_rw_lock {
 public:
  void create(mysql_pfs_key_t pfs_key, latch_level_t latch_level,
              size_t n_shards) {}

  void free() {}

  size_t s_lock() { return 0; }

  void s_unlock(size_t shard_no) { ut_a(shard_no == 0); }

  void x_lock() {}

  void x_unlock() {}
};

#endif /* UNIV_LIBRARY */
#endif /* UNIV_HOTBACKUP */

#endif /* sync0sharded_rw.h */
