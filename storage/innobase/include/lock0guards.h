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

#ifndef lock0guards_h
#define lock0guards_h

#include "lock0lock.h"
#include "ut0class_life_cycle.h"

namespace locksys {
/**
A RAII helper which latches global_latch in exclusive mode during constructor,
and unlatches it during destruction, preventing any other threads from activity
within lock_sys for it's entire scope.
*/
class Global_exclusive_latch_guard : private ut::Non_copyable {
 public:
  Global_exclusive_latch_guard(ut::Location location);
  ~Global_exclusive_latch_guard();
};

/**
A RAII helper which tries to exclusively latch the global_lach in constructor
and unlatches it, if needed, during destruction, preventing any other threads
from activity within lock_sys for it's entire scope, if owns_lock().
*/
class Global_exclusive_try_latch : private ut::Non_copyable {
 public:
  Global_exclusive_try_latch(ut::Location location);
  ~Global_exclusive_try_latch();
  /** Checks if succeeded to latch the global_latch during construction.
  @return true iff the current thread owns (through this instance) the exclusive
          global lock_sys latch */
  bool owns_lock() const noexcept { return m_owns_exclusive_global_latch; }

 private:
  /** Did the constructor succeed to acquire exclusive global lock_sys latch? */
  bool m_owns_exclusive_global_latch;
};

/**
A RAII helper which latches global_latch in shared mode during constructor,
and unlatches it during destruction, preventing any other thread from acquiring
exclusive latch. This should be used in combination Shard_naked_latch_guard,
preferably by simply using Shard_latch_guard which combines the two for you.
*/
class Global_shared_latch_guard : private ut::Non_copyable {
 public:
  Global_shared_latch_guard(ut::Location location);
  ~Global_shared_latch_guard();
  /** Checks if there is a thread requesting the global_latch in exclusive mode
  blocked by our thread.
  @return true iff there is an x-latcher blocked by our s-latch. */
  bool is_x_blocked_by_us();
};

/**
A RAII helper which latches the mutex protecting given shard during constructor,
and unlatches it during destruction.
You quite probably don't want to use this class, which only takes a shard's
latch, without acquiring global_latch - which gives no protection from threads
which latch only the global_latch exclusively to prevent any activity.
You should use it in combination with Global_shared_latch_guard, so that you
first obtain an s-latch on the global_latch, or simply use the Shard_latch_guard
class which already combines the two for you.
*/
class Shard_naked_latch_guard : private ut::Non_copyable {
  explicit Shard_naked_latch_guard(ut::Location location,
                                   Lock_mutex &shard_mutex);

 public:
  explicit Shard_naked_latch_guard(ut::Location location,
                                   const table_id_t &table_id);

  explicit Shard_naked_latch_guard(ut::Location location,
                                   const page_id_t &page_id);

  ~Shard_naked_latch_guard();

 private:
  /** The mutex protecting the shard requested in constructor */
  Lock_mutex &m_shard_mutex;
};

/**
A RAII wrapper class which combines Global_shared_latch_guard and
Shard_naked_latch_guard to s-latch the global lock_sys latch and latch the mutex
protecting the specified shard for the duration of its scope.
The order of initialization is important: we have to take shared global latch
BEFORE we attempt to use hash function to compute correct shard and latch it. */
class Shard_latch_guard {
  Global_shared_latch_guard m_global_shared_latch_guard;
  Shard_naked_latch_guard m_shard_naked_latch_guard;

 public:
  explicit Shard_latch_guard(ut::Location location, const dict_table_t &table)
      : m_global_shared_latch_guard{location},
        m_shard_naked_latch_guard{location, table.id} {}

  explicit Shard_latch_guard(ut::Location location, const page_id_t &page_id)
      : m_global_shared_latch_guard{location},
        m_shard_naked_latch_guard{location, page_id} {}
};

/**
A RAII helper which latches the mutexes protecting specified shards for the
duration of its scope.
It makes sure to take the latches in correct order and handles the case where
both pages are in the same shard correctly.
You quite probably don't want to use this class, which only takes a shard's
latch, without acquiring global_latch - which gives no protection from threads
which latch only the global_latch exclusively to prevent any activity.
You should use it in combination with Global_shared_latch_guard, so that you
first obtain an s-latch on the global_latch, or simply use the
Shard_latches_guard class which already combines the two for you.
*/
class Shard_naked_latches_guard {
  explicit Shard_naked_latches_guard(Lock_mutex &shard_mutex_a,
                                     Lock_mutex &shard_mutex_b);

 public:
  explicit Shard_naked_latches_guard(const buf_block_t &block_a,
                                     const buf_block_t &block_b);

  ~Shard_naked_latches_guard();

 private:
  /** The "smallest" of the two shards' mutexes in the latching order */
  Lock_mutex &m_shard_mutex_1;
  /** The "largest" of the two shards' mutexes in the latching order */
  Lock_mutex &m_shard_mutex_2;
  /** The ordering on shard mutexes used to avoid deadlocks */
  static constexpr std::less<Lock_mutex *> MUTEX_ORDER{};
};

/**
A RAII wrapper class which s-latches the global lock_sys shard, and mutexes
protecting specified shards for the duration of its scope.
It makes sure to take the latches in correct order and handles the case where
both pages are in the same shard correctly.
The order of initialization is important: we have to take shared global latch
BEFORE we attempt to use hash function to compute correct shard and latch it.
*/
class Shard_latches_guard {
 public:
  explicit Shard_latches_guard(ut::Location location,
                               const buf_block_t &block_a,
                               const buf_block_t &block_b)
      : m_global_shared_latch_guard{location},
        m_shard_naked_latches_guard{block_a, block_b} {}

 private:
  Global_shared_latch_guard m_global_shared_latch_guard;
  Shard_naked_latches_guard m_shard_naked_latches_guard;
};

}  // namespace locksys

#endif /* lock0guards_h */
