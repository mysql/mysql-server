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
#ifndef lock0latches_h
#define lock0latches_h

#include "dict0types.h"
#include "sync0sharded_rw.h"
#include "ut0cpu_cache.h"
#include "ut0mutex.h"

/* Forward declarations */
struct dict_table_t;
class page_id_t;

namespace locksys {
/**
The class which handles the logic of latching of lock_sys queues themselves.
The lock requests for table locks and record locks are stored in queues, and to
allow concurrent operations on these queues, we need a mechanism to latch these
queues in safe and quick fashion.
In the past we had a single latch which protected access to all of them.
Now, we use more granular approach.
In extreme, one could imagine protecting each queue with a separate latch.
To avoid having too many latch objects, and having to create and remove them on
demand, we use a more conservative approach.
The queues are grouped into a fixed number of shards, and each shard is
protected by its own mutex.

However, there are several rare events in which we need to "stop the world" -
latch all queues, to prevent any activity inside lock-sys.
One way to accomplish this would be to simply latch all the shards one by one,
but it turns out to be way too slow in debug runs, where such "stop the world"
events are very frequent due to lock_sys validation.

To allow for efficient latching of everything, we've introduced a global_latch,
which is a read-write latch.
Most of the time, we operate on one or two shards, in which case it is
sufficient to s-latch the global_latch and then latch shard's mutex.
For the "stop the world" operations, we x-latch the global_latch, which prevents
any other thread from latching any shard.

However, it turned out that on ARM architecture, the default implementation of
read-write latch (rw_lock_t) is too slow because increments and decrements of
the number of s-latchers is implemented as read-update-try-to-write loop, which
means multiple threads try to modify the same cache line disrupting each other.
Therefore, we use a sharded version of read-write latch (Sharded_rw_lock), which
internally uses multiple instances of rw_lock_t, spreading the load over several
cache lines. Note that this sharding is a technical internal detail of the
global_latch, which for all other purposes can be treated as a single entity.

This his how this conceptually looks like:
```
  [                           global latch                                ]
                                  |
                                  v
  [table shard 1] ... [table shard 512] [page shard 1] ... [page shard 512]

```

So, for example access two queues for two records involves following steps:
1. s-latch the global_latch
2. identify the 2 pages to which the records belong
3. identify the lock_sys 2 hash cells which contain the queues for given pages
4. identify the 2 shard ids which contain these two cells
5. latch mutexes for the two shards in the order of their addresses

All of the steps above (except 2, as we usually know the page already) are
accomplished with the help of single line:

    locksys::Shard_latches_guard guard{*block_a, *block_b};

And to "stop the world" one can simply x-latch the global latch by using:

    locksys::Global_exclusive_latch_guard guard{};

This class does not expose too many public functions, as the intention is to
rather use friend guard classes, like the Shard_latches_guard demonstrated.
*/
class Latches {
 private:
  using Lock_mutex = ib_mutex_t;

  /** A helper wrapper around Shared_rw_lock which simplifies:
    - lifecycle by providing constructor and destructor, and
    - s-latching and s-unlatching by keeping track of the shard id used for
      spreading the contention.
  There must be at most one instance of this class (the one in the lock_sys), as
  it uses thread_local-s to remember which shard of sharded rw lock was used by
  this thread to perform s-latching (so, hypothetical other instances would
  share this field, overwriting it and leading to errors). */
  class Unique_sharded_rw_lock {
    /** The actual rw_lock implementation doing the heavy lifting */
    Sharded_rw_lock rw_lock;

    /** The value used for m_shard_id to indicate that current thread did not
    s-latch any of the rw_lock's shards */
    static constexpr size_t NOT_IN_USE = std::numeric_limits<size_t>::max();

    /** The id of the rw_lock's shard which this thread has s-latched, or
    NOT_IN_USE if it has not s-latched any*/
    static thread_local size_t m_shard_id;

   public:
    Unique_sharded_rw_lock();
    ~Unique_sharded_rw_lock();
    bool try_x_lock(ut::Location location) {
      return rw_lock.try_x_lock(location);
    }
    /** Checks if there is a thread requesting an x-latch waiting for our
    thread to release its s-latch.
    Must be called while holding an s-latch.
    @return true iff there is an x-latcher blocked by our s-latch. */
    bool is_x_blocked_by_our_s() {
      ut_ad(m_shard_id != NOT_IN_USE);
      return rw_lock.is_x_blocked_by_s(m_shard_id);
    }
    void x_lock(ut::Location location) { rw_lock.x_lock(location); }
    void x_unlock() { rw_lock.x_unlock(); }
    void s_lock(ut::Location location) {
      ut_ad(m_shard_id == NOT_IN_USE);
      m_shard_id = rw_lock.s_lock(location);
    }
    void s_unlock() {
      ut_ad(m_shard_id != NOT_IN_USE);
      rw_lock.s_unlock(m_shard_id);
      m_shard_id = NOT_IN_USE;
    }
#ifdef UNIV_DEBUG
    bool x_own() const { return rw_lock.x_own(); }
    bool s_own() const {
      return m_shard_id != NOT_IN_USE && rw_lock.s_own(m_shard_id);
    }
#endif
  };

  using Padded_mutex = ut::Cacheline_padded<Lock_mutex>;

  /** Number of page shards, and also number of table shards.
  Must be a power of two */
  static constexpr size_t SHARDS_COUNT = 512;

  /*
  Functions related to sharding by page (containing records to lock).

  This must be done in such a way that two pages which share a single lock
  queue fall into the same shard. We accomplish this by reusing hash function
  used to determine lock queue, and then group multiple queues into single
  shard.
  */
  class Page_shards {
    /** Each shard is protected by a separate mutex. Mutexes are padded to avoid
    false sharing issues with cache. */
    Padded_mutex mutexes[SHARDS_COUNT];
    /**
    Identifies the page shard which contains record locks for records from the
    given page.
    @param[in]    page_id    The space_id and page_no of the page
    @return Integer in the range [0..lock_sys_t::SHARDS_COUNT)
    */
    static size_t get_shard(const page_id_t &page_id);

   public:
    Page_shards();
    ~Page_shards();

    /**
    Returns the mutex which (together with the global latch) protects the page
    shard which contains record locks for records from the given page.
    @param[in]    page_id    The space_id and page_no of the page
    @return The mutex responsible for the shard containing the page
    */
    const Lock_mutex &get_mutex(const page_id_t &page_id) const;

    /**
    Returns the mutex which (together with the global latch) protects the page
    shard which contains record locks for records from the given page.
    @param[in]    page_id    The space_id and page_no of the page
    @return The mutex responsible for the shard containing the page
    */
    Lock_mutex &get_mutex(const page_id_t &page_id);
  };

  /*
  Functions related to sharding by table

  We identify tables by their id. Each table has its own lock queue, so we
  simply group several such queues into single shard.
  */
  class Table_shards {
    /** Each shard is protected by a separate mutex. Mutexes are padded to avoid
    false sharing issues with cache. */
    Padded_mutex mutexes[SHARDS_COUNT];
    /**
    Identifies the table shard which contains locks for the given table.
    @param[in]    table_id    The id of the table
    @return Integer in the range [0..lock_sys_t::SHARDS_COUNT)
    */
    static size_t get_shard(const table_id_t table_id);

   public:
    Table_shards();
    ~Table_shards();

    /** Returns the mutex which (together with the global latch) protects the
    table shard which contains table locks for the given table.
    @param[in]    table_id    The id of the table
    @return The mutex responsible for the shard containing the table
    */
    Lock_mutex &get_mutex(const table_id_t table_id);

    /** Returns the mutex which (together with the global latch) protects the
    table shard which contains table locks for the given table.
    @param[in]    table_id    The id of the table
    @return The mutex responsible for the shard containing the table
    */
    const Lock_mutex &get_mutex(const table_id_t table_id) const;

    /** Returns the mutex which (together with the global latch) protects the
    table shard which contains table locks for the given table.
    @param[in]    table    The table
    @return The mutex responsible for the shard containing the table
    */
    const Lock_mutex &get_mutex(const dict_table_t &table) const;
  };

  /** padding to prevent other memory update hotspots from residing on the same
  memory cache line */
  char pad1[ut::INNODB_CACHE_LINE_SIZE] = {};

  Unique_sharded_rw_lock global_latch;

  Page_shards page_shards;

  Table_shards table_shards;

 public:
  /* You should use following RAII guards to modify the state of Latches. */
  friend class Global_exclusive_latch_guard;
  friend class Global_exclusive_try_latch;
  friend class Global_shared_latch_guard;
  friend class Shard_naked_latch_guard;
  friend class Shard_naked_latches_guard;

  /** You should not use this functionality in new code.
  Instead use Global_exclusive_latch_guard.
  This is intended only to be use within lock0* module, thus this class is only
  accessible through lock0priv.h.
  It is only used by lock_rec_fetch_page() as a workaround. */
  friend class Unsafe_global_latch_manipulator;

  /** For some reason clang 6.0.0 and 7.0.0 (but not 8.0.0) fail at linking
  stage if we completely omit the destructor declaration, or use

  ~Latches() = default;

  This might be somehow connected to one of these:
     https://bugs.llvm.org/show_bug.cgi?id=28280
     https://github.com/android/ndk/issues/143
     https://reviews.llvm.org/D45898
  So, this declaration is just to make clang 6.0.0 and 7.0.0 happy.
  */
#if defined(__clang__) && (__clang_major__ < 8)
  ~Latches() {}  // NOLINT(modernize-use-equals-default)
#else
  ~Latches() = default;
#endif

#ifdef UNIV_DEBUG
  /**
  Tests if lock_sys latch is exclusively owned by the current thread.
  @return true iff the current thread owns exclusive global lock_sys latch
  */
  bool owns_exclusive_global_latch() const { return global_latch.x_own(); }

  /**
  Tests if lock_sys latch is owned in shared mode by the current thread.
  @return true iff the current thread owns shared global lock_sys latch
  */
  bool owns_shared_global_latch() const { return global_latch.s_own(); }

  /**
  Tests if given page shard can be safely accessed by the current thread.
  @param[in]    page_id    The space_id and page_no of the page
  @return true iff the current thread owns exclusive global lock_sys latch or
  both a shared global lock_sys latch and mutex protecting the page shard
  */
  bool owns_page_shard(const page_id_t &page_id) const {
    return owns_exclusive_global_latch() ||
           (page_shards.get_mutex(page_id).is_owned() &&
            owns_shared_global_latch());
  }

  /**
  Tests if given table shard can be safely accessed by the current thread.
  @param  table   the table
  @return true iff the current thread owns exclusive global lock_sys latch or
  both a shared global lock_sys latch and mutex protecting the table shard
  */
  bool owns_table_shard(const dict_table_t &table) const {
    return owns_exclusive_global_latch() ||
           (table_shards.get_mutex(table).is_owned() &&
            owns_shared_global_latch());
  }
#endif /* UNIV_DEBUG */
};
}  // namespace locksys

#endif /* lock0latches_h */
