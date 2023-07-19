/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

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

/** @file lock/lock0lock.cc
 The transaction lock system

 Created 5/7/1996 Heikki Tuuri
 *******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include <mysql/service_thd_engine_lock.h>
#include <sys/types.h>

#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "btr0btr.h"
#include "current_thd.h"
#include "debug_sync.h" /* CONDITIONAL_SYNC_POINT */
#include "dict0boot.h"
#include "dict0mem.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "lock0priv.h"
#include "os0thread.h"
#include "pars0pars.h"
#include "row0mysql.h"
#include "row0sel.h"
#include "srv0mon.h"
#include "trx0purge.h"
#include "trx0sys.h"
#include "usr0sess.h"
#include "ut0new.h"
#include "ut0vec.h"

#include "my_dbug.h"
#include "my_psi_config.h"
#include "mysql/plugin.h"
#include "mysql/psi/psi_thread.h"

/* Flag to enable/disable deadlock detector. */
bool innobase_deadlock_detect = true;

/** Total number of cached record locks */
static const ulint REC_LOCK_CACHE = 8;

/** Maximum record lock size in bytes */
static const ulint REC_LOCK_SIZE = sizeof(ib_lock_t) + 256;

/** Total number of cached table locks */
static const ulint TABLE_LOCK_CACHE = 8;

/** Size in bytes, of the table lock instance */
static const ulint TABLE_LOCK_SIZE = sizeof(ib_lock_t);

template <typename T>
using Locks = std::vector<T, mem_heap_allocator<T>>;

/** Used by lock_get_mode_str to build a lock mode description */
static const std::map<uint, const char *> lock_constant_names{
    {LOCK_GAP, "GAP"},
    {LOCK_REC_NOT_GAP, "REC_NOT_GAP"},
    {LOCK_INSERT_INTENTION, "INSERT_INTENTION"},
    {LOCK_PREDICATE, "PREDICATE"},
    {LOCK_PRDT_PAGE, "PRDT_PAGE"},
};
/** Used by lock_get_mode_str to cache results. Strings pointed by these
pointers might be in use by performance schema and thus can not be freed
until the very end.
Protected by exclusive global lock_sys latch.
*/
static std::unordered_map<uint, const char *> lock_cached_lock_mode_names;

/** A static class for reporting notifications about deadlocks */
class Deadlock_notifier {
 public:
  Deadlock_notifier() = delete;

  /** Handles writing the information about found deadlock to the log files
  and caches it for future lock_latest_err_file() calls (for example used by
  SHOW ENGINE INNODB STATUS)
  @param[in] trxs_on_cycle  trxs causing deadlock, i-th waits for i+1-th
  @param[in] victim_trx     the trx from trx_on_cycle which will be rolled back
  */
  static void notify(const ut::vector<const trx_t *> &trxs_on_cycle,
                     const trx_t *victim_trx);

 private:
#ifdef UNIV_DEBUG
  /** Determines if a situation in which the lock takes part in a deadlock
  cycle is expected (as in: handled correctly) or not (say because it is on a DD
  table, for which there is no reason to expect a deadlock and we don't handle
  deadlocks correctly). The purpose of the function is to use it in an assertion
  failing as soon as the deadlock is identified, to give developer a chance to
  investigate the root cause of the situation (without such assertion, the code
  might continue to run and either fail at later stage when the data useful for
  debugging is no longer on stack, or not fail at all, which is risky).
  @param[in] lock lock found in a deadlock cycle
  @return true if we expect that this lock can take part in a deadlock cycle */
  static bool is_allowed_to_be_on_cycle(const lock_t *lock);
#endif /* UNIV_DEBUG */

  /** Print transaction data to the deadlock file and possibly to stderr.
  @param trx transaction
  @param max_query_len max query length to print */
  static void print(const trx_t *trx, ulint max_query_len);

  /** rewind(3) the file used for storing the latest detected deadlock
  and print a heading message to stderr if printing of all deadlocks to
  stderr is enabled. */
  static void start_print();

  /** Print lock data to the deadlock file and possibly to stderr.
  @param lock record or table type lock */
  static void print(const lock_t *lock);

  /** Print a message to the deadlock file and possibly to stderr.
  @param msg message to print */
  static void print(const char *msg);

  /** Prints a numbered section title to the deadlock file and possibly to
  stderr. Numbers do not have to be unique, as they are used to identify
  transactions on the cycle, and there are multiple sections per transaction.
  @param[in]    pos_on_cycle    The zero-based position of trx on deadlock cycle
  @param[in]    title           The title of the section */
  static void print_title(size_t pos_on_cycle, const char *title);
};

#ifdef UNIV_DEBUG
namespace locksys {

bool owns_exclusive_global_latch() {
  return lock_sys->latches.owns_exclusive_global_latch();
}

bool owns_shared_global_latch() {
  return lock_sys->latches.owns_shared_global_latch();
}

bool owns_page_shard(const page_id_t &page_id) {
  return lock_sys->latches.owns_page_shard(page_id);
}

bool owns_table_shard(const dict_table_t &table) {
  return lock_sys->latches.owns_table_shard(table);
}

bool owns_lock_shard(const lock_t *lock) {
  if (lock->is_record_lock()) {
    return lock_sys->latches.owns_page_shard(lock->rec_lock.page_id);
  } else {
    return lock_sys->latches.owns_table_shard(*lock->tab_lock.table);
  }
}
}  // namespace locksys

/** Validates the record lock queues on a page.
 @return true if ok */
[[nodiscard]] static bool lock_rec_validate_page(
    const buf_block_t *block); /*!< in: buffer block */
#endif                         /* UNIV_DEBUG */

/* The lock system */
lock_sys_t *lock_sys = nullptr;

/** We store info on the latest deadlock error to this buffer. InnoDB
Monitor will then fetch it and print */
static bool lock_deadlock_found = false;

/** Only created if !srv_read_only_mode. I/O operations on this file require
exclusive lock_sys latch */
static FILE *lock_latest_err_file;

void lock_report_trx_id_insanity(trx_id_t trx_id, const rec_t *rec,
                                 const dict_index_t *index,
                                 const ulint *offsets, trx_id_t next_trx_id) {
  ib::error(ER_IB_MSG_634) << "Transaction id " << trx_id
                           << " associated with record"
                           << rec_offsets_print(rec, offsets) << " in index "
                           << index->name << " of table " << index->table->name
                           << " is greater or equal than the global counter "
                           << next_trx_id << "! The table is corrupted.";
}

bool lock_check_trx_id_sanity(trx_id_t trx_id, const rec_t *rec,
                              const dict_index_t *index, const ulint *offsets) {
  ut_ad(rec_offs_validate(rec, index, offsets));

  trx_id_t next_trx_id = trx_sys_get_next_trx_id_or_no();
  bool is_ok = trx_id < next_trx_id;

  if (!is_ok) {
    lock_report_trx_id_insanity(trx_id, rec, index, offsets, next_trx_id);
  }

  return (is_ok);
}

/** Checks that a record is seen in a consistent read.
 @return true if sees, or false if an earlier version of the record
 should be retrieved */
bool lock_clust_rec_cons_read_sees(
    const rec_t *rec,     /*!< in: user record which should be read or
                          passed over by a read cursor */
    dict_index_t *index,  /*!< in: clustered index */
    const ulint *offsets, /*!< in: rec_get_offsets(rec, index) */
    ReadView *view)       /*!< in: consistent read view */
{
  ut_ad(index->is_clustered());
  ut_ad(page_rec_is_user_rec(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));

  /* Temp-tables are not shared across connections and multiple
  transactions from different connections cannot simultaneously
  operate on same temp-table and so read of temp-table is
  always consistent read. */
  if (srv_read_only_mode || index->table->is_temporary()) {
    ut_ad(view == nullptr || index->table->is_temporary());
    return (true);
  }

  /* NOTE that we call this function while holding the search
  system latch. */

  trx_id_t trx_id = row_get_rec_trx_id(rec, index, offsets);

  return (view->changes_visible(trx_id, index->table->name));
}

/** Checks that a non-clustered index record is seen in a consistent read.

 NOTE that a non-clustered index page contains so little information on
 its modifications that also in the case false, the present version of
 rec may be the right, but we must check this from the clustered index
 record.

 @return true if certainly sees, or false if an earlier version of the
 clustered index record might be needed */
bool lock_sec_rec_cons_read_sees(
    const rec_t *rec,          /*!< in: user record which
                               should be read or passed over
                               by a read cursor */
    const dict_index_t *index, /*!< in: index */
    const ReadView *view)      /*!< in: consistent read view */
{
  ut_ad(page_rec_is_user_rec(rec));

  /* NOTE that we might call this function while holding the search
  system latch. */

  if (recv_recovery_is_on()) {
    return (false);

  } else if (index->table->is_temporary()) {
    /* Temp-tables are not shared across connections and multiple
    transactions from different connections cannot simultaneously
    operate on same temp-table and so read of temp-table is
    always consistent read. */

    return (true);
  }

  trx_id_t max_trx_id = page_get_max_trx_id(page_align(rec));

  ut_ad(max_trx_id > 0);

  return (view->sees(max_trx_id));
}

/** Creates the lock system at database start. */
void lock_sys_create(
    ulint n_cells) /*!< in: number of slots in lock hash table */
{
  ulint lock_sys_sz;

  lock_sys_sz = sizeof(*lock_sys) + srv_max_n_threads * sizeof(srv_slot_t);

  lock_sys = static_cast<lock_sys_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, lock_sys_sz));

  new (lock_sys) lock_sys_t{};

  void *ptr = &lock_sys[1];

  lock_sys->waiting_threads = static_cast<srv_slot_t *>(ptr);

  lock_sys->last_slot = lock_sys->waiting_threads;

  mutex_create(LATCH_ID_LOCK_SYS_WAIT, &lock_sys->wait_mutex);

  lock_sys->timeout_event = os_event_create();

  lock_sys->rec_hash = ut::new_<hash_table_t>(n_cells);
  lock_sys->prdt_hash = ut::new_<hash_table_t>(n_cells);
  lock_sys->prdt_page_hash = ut::new_<hash_table_t>(n_cells);

  if (!srv_read_only_mode) {
    lock_latest_err_file = os_file_create_tmpfile();
    ut_a(lock_latest_err_file);
  }
}

/** Calculates the hash value of a lock: used in migrating the hash table.
@param[in]	lock	record lock object
@return	hashed value */
static uint64_t lock_rec_lock_hash_value(const lock_t *lock) {
  return lock_rec_hash_value(lock->rec_lock.page_id);
}

/** Resize the lock hash tables.
@param[in]	n_cells	number of slots in lock hash table */
void lock_sys_resize(ulint n_cells) {
  hash_table_t *old_hash;

  /* We will rearrange locks between cells and change the parameters of hash
  function used in sharding of latches, so we have to prevent everyone from
  accessing lock sys queues, or even computing shard id. */
  locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};

  old_hash = lock_sys->rec_hash;
  lock_sys->rec_hash = ut::new_<hash_table_t>(n_cells);
  HASH_MIGRATE(old_hash, lock_sys->rec_hash, lock_t, hash,
               lock_rec_lock_hash_value);
  ut::delete_(old_hash);

  DBUG_EXECUTE_IF("syncpoint_after_lock_sys_resize_rec_hash", {
    /* A workaround for buf_resize_thread() not using create_thd().
    TBD: if buf_resize_thread() were to use create_thd() then should it be
    instrumented (together or instead of os_thread_create instrumentation)? */
    ut_ad(current_thd == nullptr);
    THD *thd = create_internal_thd();
    ut_ad(current_thd == thd);
    CONDITIONAL_SYNC_POINT("after_lock_sys_resize_rec_hash");
    destroy_internal_thd(thd);
    ut_ad(current_thd == nullptr);
  });

  old_hash = lock_sys->prdt_hash;
  lock_sys->prdt_hash = ut::new_<hash_table_t>(n_cells);
  HASH_MIGRATE(old_hash, lock_sys->prdt_hash, lock_t, hash,
               lock_rec_lock_hash_value);
  ut::delete_(old_hash);

  old_hash = lock_sys->prdt_page_hash;
  lock_sys->prdt_page_hash = ut::new_<hash_table_t>(n_cells);
  HASH_MIGRATE(old_hash, lock_sys->prdt_page_hash, lock_t, hash,
               lock_rec_lock_hash_value);
  ut::delete_(old_hash);
}

/** Closes the lock system at database shutdown. */
void lock_sys_close(void) {
  if (lock_latest_err_file != nullptr) {
    fclose(lock_latest_err_file);
    lock_latest_err_file = nullptr;
  }

  ut::delete_(lock_sys->rec_hash);
  ut::delete_(lock_sys->prdt_hash);
  ut::delete_(lock_sys->prdt_page_hash);

  os_event_destroy(lock_sys->timeout_event);

  mutex_destroy(&lock_sys->wait_mutex);

  srv_slot_t *slot = lock_sys->waiting_threads;

  for (uint32_t i = 0; i < srv_max_n_threads; i++, ++slot) {
    if (slot->event != nullptr) {
      os_event_destroy(slot->event);
    }
  }
  for (auto &cached_lock_mode_name : lock_cached_lock_mode_names) {
    ut::free(const_cast<char *>(cached_lock_mode_name.second));
  }
  lock_cached_lock_mode_names.clear();

  lock_sys->~lock_sys_t();

  ut::free(lock_sys);

  lock_sys = nullptr;
}

/** Gets the size of a lock struct.
 @return size in bytes */
ulint lock_get_size(void) { return ((ulint)sizeof(lock_t)); }

/** Sets the wait flag of a lock and the back pointer in trx to lock.
@param[in]  lock  The lock on which a transaction is waiting */
static inline void lock_set_lock_and_trx_wait(lock_t *lock) {
  auto trx = lock->trx;
  ut_ad(trx_mutex_own(trx));
  ut_a(trx->lock.wait_lock == nullptr);
  ut_ad(locksys::owns_lock_shard(lock));

  trx->lock.wait_lock = lock;
  trx->lock.wait_lock_type = lock_get_type_low(lock);
  lock->type_mode |= LOCK_WAIT;
}

/** Gets the gap flag of a record lock.
 @return LOCK_GAP or 0 */
static inline ulint lock_rec_get_gap(const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  return (lock->type_mode & LOCK_GAP);
}

/** Gets the LOCK_REC_NOT_GAP flag of a record lock.
 @return LOCK_REC_NOT_GAP or 0 */
static inline ulint lock_rec_get_rec_not_gap(
    const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  return (lock->type_mode & LOCK_REC_NOT_GAP);
}

/** Gets the waiting insert flag of a record lock.
 @return LOCK_INSERT_INTENTION or 0 */
static inline ulint lock_rec_get_insert_intention(
    const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  return (lock->type_mode & LOCK_INSERT_INTENTION);
}
namespace locksys {

enum class Conflict {
  HAS_TO_WAIT,
  NO_CONFLICT,
  CAN_BYPASS,
};

/** Checks if a new request for a record lock has to wait for existing request.
@param[in]  trx                   The trx requesting the new lock
@param[in]  type_mode             precise mode of the new lock to set: LOCK_S or
                                  LOCK_X, possibly ORed to LOCK_GAP or
                                  LOCK_REC_NOT_GAP, LOCK_INSERT_INTENTION
@param[in]  lock2                 another record lock;
                                  NOTE that it is assumed that this has a lock
                                  bit set on the same record as in the new lock
                                  we are setting
@param[in]  lock_is_on_supremum   true if we are setting the lock on the
                                  'supremum' record of an index page: we know
                                  then that the lock request is really for a
                                  'gap' type lock
@param[in]  trx_locks_cache       An object which can be passed to consecutive
                                  calls to this function for the same trx and
                                  heap_no (which is implicitly the bit common to
                                  all lock2 objects passed) which can be used by
                                  this function to cache some partial results.
@retval NO_CONFLICT the trx does not have to wait for lock2
@retval CAN_BYPASS  the trx does not have to wait for lock2, as it can bypass it
@retval HAS_TO_WAIT the trx has to wait for lock2
*/
static inline Conflict rec_lock_check_conflict(const trx_t *trx,
                                               ulint type_mode,
                                               const lock_t *lock2,
                                               bool lock_is_on_supremum,
                                               Trx_locks_cache &trx_locks_cache)

{
  ut_ad(trx && lock2);
  ut_ad(lock_get_type_low(lock2) == LOCK_REC);

  if (trx == lock2->trx ||
      lock_mode_compatible(static_cast<lock_mode>(LOCK_MODE_MASK & type_mode),
                           lock_get_mode(lock2))) {
    return Conflict::NO_CONFLICT;
  }

  const bool is_hp = trx_is_high_priority(trx);
  /* If our trx is High Priority and the existing lock is WAITING and not
      high priority, then we can ignore it. */
  if (is_hp && lock2->is_waiting() && !trx_is_high_priority(lock2->trx)) {
    return Conflict::NO_CONFLICT;
  }

  /* We have somewhat complex rules when gap type record locks
  cause waits */

  if ((lock_is_on_supremum || (type_mode & LOCK_GAP)) &&
      !(type_mode & LOCK_INSERT_INTENTION)) {
    /* Gap type locks without LOCK_INSERT_INTENTION flag
    do not need to wait for anything. This is because
    different users can have conflicting lock types
    on gaps. */

    return Conflict::NO_CONFLICT;
  }

  if (!(type_mode & LOCK_INSERT_INTENTION) && lock_rec_get_gap(lock2)) {
    /* Record lock (LOCK_ORDINARY or LOCK_REC_NOT_GAP
    does not need to wait for a gap type lock */

    return Conflict::NO_CONFLICT;
  }

  if ((type_mode & LOCK_GAP) && lock_rec_get_rec_not_gap(lock2)) {
    /* Lock on gap does not need to wait for
    a LOCK_REC_NOT_GAP type lock */

    return Conflict::NO_CONFLICT;
  }

  if (lock_rec_get_insert_intention(lock2)) {
    /* No lock request needs to wait for an insert
    intention lock to be removed. This is ok since our
    rules allow conflicting locks on gaps. This eliminates
    a spurious deadlock caused by a next-key lock waiting
    for an insert intention lock; when the insert
    intention lock was granted, the insert deadlocked on
    the waiting next-key lock.

    Also, insert intention locks do not disturb each
    other. */

    return Conflict::NO_CONFLICT;
  }

  /* This is very important that LOCK_INSERT_INTENTION should not overtake a
  WAITING Gap or Next-Key lock on the same heap_no, because the following
  insertion of the record would split the gap duplicating the waiting lock,
  violating the rule that a transaction can have at most one waiting lock. */
  if (!(type_mode & LOCK_INSERT_INTENTION) && lock2->is_waiting() &&
      lock2->mode() == LOCK_X && (type_mode & LOCK_MODE_MASK) == LOCK_X) {
    // We would've already returned false if it was a gap lock.
    ut_ad(!(type_mode & LOCK_GAP));
    // Similarly, since locks on supremum are either LOCK_INSERT_INTENTION or
    // gap locks, we would've already returned false if it's about supremum.
    ut_ad(!lock_is_on_supremum);
    // If lock2 was a gap lock (in particular: insert intention), it could
    // only block LOCK_INSERT_INTENTION, which we've ruled out.
    ut_ad(!lock_rec_get_gap(lock2));
    // So, both locks are REC_NOT_GAP or Next-Key locks
    ut_ad(lock2->is_record_not_gap() || lock2->is_next_key_lock());
    ut_ad((type_mode & LOCK_REC_NOT_GAP) ||
          lock_mode_is_next_key_lock(type_mode));
    /* In this case, we should ignore lock2, if trx already has a GRANTED lock
    blocking lock2 from being granted. */
    if (trx_locks_cache.has_granted_blocker(trx, lock2)) {
      return Conflict::CAN_BYPASS;
    }
  }

  return Conflict::HAS_TO_WAIT;
}

/** Checks if a record lock request lock1 has to wait for request lock2.
@param[in]  lock1         waiting record lock
@param[in]  lock2         another record lock;
                          NOTE that it is assumed that this has a lock bit set
                          on the same record as in lock1
@param[in]  lock1_cache   Cached info gathered during calls with lock1
@return true if lock1 has to wait for lock2 to be removed */
static inline bool rec_lock_has_to_wait(const lock_t *lock1,
                                        const lock_t *lock2,
                                        Trx_locks_cache &lock1_cache) {
  ut_ad(lock1->is_waiting());
  ut_ad(lock_rec_get_nth_bit(lock2, lock_rec_find_set_bit(lock1)));
  return rec_lock_check_conflict(lock1->trx, lock1->type_mode, lock2,
                                 lock1->includes_supremum(),
                                 lock1_cache) == Conflict::HAS_TO_WAIT;
}

bool has_to_wait(const lock_t *lock1, const lock_t *lock2,
                 Trx_locks_cache &lock1_cache) {
  if (lock_get_type_low(lock1) == LOCK_REC) {
    ut_ad(lock_get_type_low(lock2) == LOCK_REC);

    if (lock1->type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE)) {
      return lock_prdt_has_to_wait(lock1->trx, lock1->type_mode,
                                   lock_get_prdt_from_lock(lock1), lock2);
    }
    return rec_lock_has_to_wait(lock1, lock2, lock1_cache);
  }
  // Rules for LOCK_TABLE are much simpler:
  return (lock1->trx != lock2->trx &&
          !lock_mode_compatible(lock_get_mode(lock1), lock_get_mode(lock2)));
}
}  // namespace locksys

bool lock_has_to_wait(const lock_t *lock1, const lock_t *lock2) {
  /* We assume that the caller doesn't expect lock2 to be waiting, or record
  lock or to execute multiple calls for the same lock1, or doesn't care about
  performance too much, thus we create a single-use cache */
  locksys::Trx_locks_cache trx_locks_cache{};
  return locksys::has_to_wait(lock1, lock2, trx_locks_cache);
}

/*============== RECORD LOCK BASIC FUNCTIONS ============================*/

/** A helper function for lock_rec_find_set_bit() which checks if the next S
bits starting from i-th bit of the bitmap are zeros, where S is the
sizeof(T) and T is uint64_t,uint32_t,uint16_t or uint8_t.
This function assumes that i is divisible by S, and bitmap is properly aligned.
@param[in,out]  i       The position of the first bit to check. Will be advanced
                        by sizeof(T), if sizeof(T) bits are zero.
@param[in]      bitmap  The bitmap to scan
@param[in]      n       The size of the bitmap
@return true iff next sizeof(T) bits starting from i-th of bitmap are zeros. In
particular returns false if n is too short.
*/
template <typename T>
static bool lock_bit_skip_if_zero(uint32_t &i, const byte *const bitmap,
                                  const uint32_t n) {
  constexpr size_t SIZE_IN_BITS = sizeof(T) * 8;
  if (n < i + SIZE_IN_BITS ||
      (reinterpret_cast<const T *>(bitmap))[i / SIZE_IN_BITS]) {
    return false;
  }
  i += SIZE_IN_BITS;
  return true;
}

ulint lock_rec_find_set_bit(const lock_t *lock) {
  static_assert(alignof(uint64_t) <= alignof(lock_t),
                "lock_t and thus the bitmap after lock_t should be aligned for "
                "64-bit access");
  const byte *bitmap = (const byte *)&lock[1];
  ut_a(ut::is_aligned_as<uint64_t>(bitmap));
  uint32_t i = 0;
  const uint32_t n = lock_rec_get_n_bits(lock);
  ut_ad(n % 8 == 0);
  while (lock_bit_skip_if_zero<uint64_t>(i, bitmap, n)) {
  }
  lock_bit_skip_if_zero<uint32_t>(i, bitmap, n);
  lock_bit_skip_if_zero<uint16_t>(i, bitmap, n);
  lock_bit_skip_if_zero<byte>(i, bitmap, n);
  ut_ad(i % 8 == 0);
  if (i < n) {
    /* This could use std::countr_zero once we switch to C++20, as n and i are
    guaranteed to be divisible by 8.*/
    byte v = bitmap[i / 8];
    ut_ad(v != 0);
    while (i < n) {
      if (v & 1) {
        return i;
      }
      i++;
      v >>= 1;
    }
  }
  return ULINT_UNDEFINED;
}

/** Looks for the next set bit in the record lock bitmap.
@param[in] lock         record lock with at least one bit set
@param[in] heap_no      current set bit
@return The next bit index  == heap number following heap_no, or ULINT_UNDEFINED
if none found */
ulint lock_rec_find_next_set_bit(const lock_t *lock, ulint heap_no) {
  ut_ad(heap_no != ULINT_UNDEFINED);

  for (ulint i = heap_no + 1; i < lock_rec_get_n_bits(lock); ++i) {
    if (lock_rec_get_nth_bit(lock, i)) {
      return (i);
    }
  }

  return (ULINT_UNDEFINED);
}

/** Reset the nth bit of a record lock.
@param[in,out] lock record lock
@param[in] i index of the bit that will be reset
@return previous value of the bit */
static inline byte lock_rec_reset_nth_bit(lock_t *lock, ulint i) {
  ut_ad(lock_get_type_low(lock) == LOCK_REC);
  ut_ad(i < lock->rec_lock.n_bits);

  byte *b = reinterpret_cast<byte *>(&lock[1]) + (i >> 3);
  byte mask = 1 << (i & 7);
  byte bit = *b & mask;
  *b &= ~mask;

  if (bit != 0) {
    ut_ad(lock->trx->lock.n_rec_locks.load() > 0);
    lock->trx->lock.n_rec_locks.fetch_sub(1, std::memory_order_relaxed);
  }

  return (bit);
}

/** Reset the nth bit of a record lock.
@param[in,out]  lock record lock
@param[in] i    index of the bit that will be reset
@param[in] type whether the lock is in wait mode */
void lock_rec_trx_wait(lock_t *lock, ulint i, ulint type) {
  lock_rec_reset_nth_bit(lock, i);

  if (type & LOCK_WAIT) {
    lock_reset_lock_and_trx_wait(lock);
  }
}

bool lock_rec_expl_exist_on_page(const page_id_t &page_id) {
  lock_t *lock;
  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, page_id};
  /* Only used in ibuf pages, so rec_hash is good enough */
  lock = lock_rec_get_first_on_page_addr(lock_sys->rec_hash, page_id);

  return (lock != nullptr);
}

/** Resets the record lock bitmap to zero. NOTE: does not touch the wait_lock
 pointer in the transaction! This function is used in lock object creation
 and resetting. */
static void lock_rec_bitmap_reset(lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  /* Reset to zero the bitmap which resides immediately after the lock
  struct */

  const auto n_bytes = lock_rec_get_n_bits(lock) / 8;

  ut_ad((lock_rec_get_n_bits(lock) % 8) == 0);

  memset(&lock[1], 0, n_bytes);
}

/** Copies a record lock to heap.
 @return copy of lock */
static lock_t *lock_rec_copy(const lock_t *lock, /*!< in: record lock */
                             mem_heap_t *heap)   /*!< in: memory heap */
{
  ulint size;

  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  size = sizeof(lock_t) + lock_rec_get_n_bits(lock) / 8;

  return (static_cast<lock_t *>(mem_heap_dup(heap, lock, size)));
}

/** Gets the previous record lock set on a record.
 @return previous lock on the same record, NULL if none exists */
const lock_t *lock_rec_get_prev(
    const lock_t *in_lock, /*!< in: record lock */
    ulint heap_no)         /*!< in: heap number of the record */
{
  lock_t *lock;
  lock_t *found_lock = nullptr;
  hash_table_t *hash;

  ut_ad(lock_get_type_low(in_lock) == LOCK_REC);
  const auto page_id = in_lock->rec_lock.page_id;
  ut_ad(locksys::owns_page_shard(page_id));

  hash = lock_hash_get(in_lock->type_mode);

  for (lock = lock_rec_get_first_on_page_addr(hash, page_id);
       /* No op */; lock = lock_rec_get_next_on_page(lock)) {
    ut_ad(lock);

    if (lock == in_lock) {
      return (found_lock);
    }

    if (lock_rec_get_nth_bit(lock, heap_no)) {
      found_lock = lock;
    }
  }
}

/*============= FUNCTIONS FOR ANALYZING RECORD LOCK QUEUE ================*/

/** Checks if a transaction has a GRANTED explicit lock on rec stronger or equal
 to precise_mode.
@param[in]    precise_mode  LOCK_S or LOCK_X possibly ORed to LOCK_GAP or
                            LOCK_REC_NOT_GAP, for a supremum record we regard
                            this always a gap type request
@param[in]    page_id       id of the page containing the record
@param[in]    heap_no       heap number of the record
@param[in]    trx           transaction
@return lock or NULL */
static inline const lock_t *lock_rec_has_expl(ulint precise_mode,
                                              const page_id_t page_id,
                                              uint32_t heap_no,
                                              const trx_t *trx) {
  ut_ad(locksys::owns_page_shard(page_id));
  ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S ||
        (precise_mode & LOCK_MODE_MASK) == LOCK_X);
  ut_ad(
      !(precise_mode & ~(ulint)(LOCK_MODE_MASK | LOCK_GAP | LOCK_REC_NOT_GAP)));
  ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));
  ut_ad(!(precise_mode & LOCK_PREDICATE));
  ut_ad(!(precise_mode & LOCK_PRDT_PAGE));
  const RecID rec_id{page_id, heap_no};
  const bool is_on_supremum = rec_id.is_supremum();
  const bool is_rec_not_gap = 0 != (precise_mode & LOCK_REC_NOT_GAP);
  const bool is_gap = 0 != (precise_mode & LOCK_GAP);
  const auto mode = static_cast<lock_mode>(precise_mode & LOCK_MODE_MASK);
  const auto p_implies_q = [](bool p, bool q) { return q || !p; };
  /* Stop iterating on first matching record or first WAITING lock */
  const auto first = Lock_iter::for_each(rec_id, [&](const lock_t *lock) {
    return !(lock->is_waiting() ||
             (lock->trx == trx && !lock->is_insert_intention() &&
              lock_mode_stronger_or_eq(lock_get_mode(lock), mode) &&
              (is_on_supremum ||
               (p_implies_q(lock->is_record_not_gap(), is_rec_not_gap) &&
                p_implies_q(lock->is_gap(), is_gap)))));
  });
  /* There are no GRANTED locks after the first WAITING lock in the queue. */
  return first == nullptr || first->is_waiting() ? nullptr : first;
}
static inline const lock_t *lock_rec_has_expl(ulint precise_mode,
                                              const buf_block_t *block,
                                              ulint heap_no, const trx_t *trx) {
  return lock_rec_has_expl(precise_mode, block->get_page_id(), heap_no, trx);
}
namespace locksys {
bool Trx_locks_cache::has_granted_blocker(const trx_t *trx,
                                          const lock_t *waiting_lock) {
  ut_ad(waiting_lock->is_waiting());
  ut_ad(waiting_lock->trx != trx);
  /* We only support case where waiting_lock is on a record or record and gap,
  and has mode X. This allows for very simple implementation and state. */
  ut_ad(waiting_lock->is_record_lock());
  ut_ad(waiting_lock->is_next_key_lock() || waiting_lock->is_record_not_gap());
  ut_ad(waiting_lock->mode() == LOCK_X);
  if (!m_computed) {
    const auto page_id = waiting_lock->rec_lock.page_id;
    const auto heap_no = lock_rec_find_set_bit(waiting_lock);
    /* A lock is blocking an X or X|REC_NOT_GAP lock, if and only if it is
    stronger or equal to LOCK_S|LOCK_REC_NOT_GAP */
    m_has_s_lock_on_record =
        lock_rec_has_expl(LOCK_S | LOCK_REC_NOT_GAP, page_id, heap_no, trx);
    m_computed = true;
#ifdef UNIV_DEBUG
    m_cached_trx = trx;
    m_cached_page_id = page_id;
    m_cached_heap_no = heap_no;
#endif /* UNIV_DEBUG*/
  }
  ut_ad(m_cached_trx == trx);
  ut_ad(m_cached_page_id == waiting_lock->rec_lock.page_id);
  ut_ad(lock_rec_get_nth_bit(waiting_lock, m_cached_heap_no));
  return m_has_s_lock_on_record;
}
}  // namespace locksys
#ifdef UNIV_DEBUG
/** Checks if some other transaction has a lock request in the queue.
 @return lock or NULL */
static const lock_t *lock_rec_other_has_expl_req(
    lock_mode mode,           /*!< in: LOCK_S or LOCK_X */
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    bool wait,                /*!< in: whether also waiting locks
                              are taken into account */
    ulint heap_no,            /*!< in: heap number of the record */
    const trx_t *trx)         /*!< in: transaction, or NULL if
                              requests by all transactions
                              are taken into account */
{
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(mode == LOCK_X || mode == LOCK_S);

  /* Only GAP lock can be on SUPREMUM, and we are not looking
  for GAP lock */

  RecID rec_id{block, heap_no};

  if (rec_id.is_supremum()) {
    return (nullptr);
  }

  return (Lock_iter::for_each(rec_id, [=](const lock_t *lock) {
    /* Ignore transactions that are being rolled back. */
    return (!(lock->trx != trx && !lock->is_gap() &&
              (wait || !lock->is_waiting()) &&
              lock_mode_stronger_or_eq(lock->mode(), mode)));
  }));
}
#endif /* UNIV_DEBUG */

namespace locksys {
struct Conflicting {
  /** a conflicting lock or null if no conflicting lock found */
  const lock_t *wait_for;
  /** true iff the trx has bypassed one of waiting locks */
  bool bypassed;
};
} /*namespace locksys*/
/** Checks if some other transaction has a conflicting explicit lock request
 in the queue, so that we have to wait.
 @param[in]     mode        LOCK_S or LOCK_X, possibly ORed to
                            LOCK_GAP or LOC_REC_NOT_GAP, LOCK_INSERT_INTENTION
 @param[in]     block       buffer block containing the record
 @param[in]     heap_no     heap number of the record
 @param[in]     trx         our transaction
 @return a pair, where:
 the first element is a conflicting lock or null if no conflicting lock found,
 the second element indicates if the trx has bypassed one of waiting locks.
*/
static locksys::Conflicting lock_rec_other_has_conflicting(
    ulint mode, const buf_block_t *block, ulint heap_no, const trx_t *trx) {
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(!(mode & ~(ulint)(LOCK_MODE_MASK | LOCK_GAP | LOCK_REC_NOT_GAP |
                          LOCK_INSERT_INTENTION)));
  ut_ad(!(mode & LOCK_PREDICATE));
  ut_ad(!(mode & LOCK_PRDT_PAGE));
  bool bypassed{false};

  RecID rec_id{block, heap_no};
  const bool is_supremum = rec_id.is_supremum();
  locksys::Trx_locks_cache trx_locks_cache{};
  const lock_t *wait_for = Lock_iter::for_each(rec_id, [&](const lock_t *lock) {
    const auto conflict = locksys::rec_lock_check_conflict(
        trx, mode, lock, is_supremum, trx_locks_cache);
    if (conflict == locksys::Conflict::CAN_BYPASS) {
      bypassed = true;
    }
    return conflict != locksys::Conflict::HAS_TO_WAIT;
  });
  return {wait_for, bypassed};
}

/** Checks if the (-infinity,max_old_active_id] range contains an id of
a currently active transaction which has modified a record.
The premise is that the caller has seen a record modified by a trx with
trx->id <= max_old_active_id, and wants to know if it might be still active.
It may err on the safe side.

@remarks
The difficulties to keep in mind here:
  - the caller doesn't hold trx_sys mutex, nor can prevent any transaction
    from committing or starting before the start and after the end of execution
    of this function. Thus the result of this function has to be interpreted
    as if it could have a "one sided error", even if the return value is exact
    during the execution
  - the transactions are assigned ids from increasing sequence, but they are
    added to various structures like lists and shards out of order. This means
    that the answer this function gives only makes sense in the context that
    the caller already saw an effect of the trx modifying some row, which
    means it had to be already added to these structures. In other words,
    calling this function twice for the same number can give false then true.
    Still the false from the first call is "correct" for the purpose of the
    caller (as it must mean that the trx which modified the record had to be
    removed from the structures already, hence is not active anymore).
    Also the true from the second call is "correct" in that indeed some smaller
    transaction id had to be added to the structures meanwhile, even if it's
    not the one which modified the record in question - error on the safe side.

@param[in]    max_old_active_id    The end of the range inclusive. For example
                                   found in the PAGE_MAX_TRX_ID field of a
                                   header of a secondary index page.
@retval   false  the caller may assume that if before the call it saw a
                 record modified by trx_id, and trx_id < max_old_active_id,
                 then it is no longer active
@retval   true   the caller should double check in a synchronized way if
                 the seen trx_id is still active or not
*/
static bool can_older_trx_be_still_active(trx_id_t max_old_active_id) {
  if (mutex_enter_nowait(&trx_sys->mutex) != 0) {
    ut_ad(!trx_sys_mutex_own());
    /* The mutex is currently locked by somebody else. Instead of wasting time
    on spinning and waiting to acquire it, we loop over the shards and check if
    any of them contains a value in the range (-infinity,max_old_active_id].
    NOTE: Do not be tempted to "cache" the minimum, until you also enforce that
    transactions are inserted to shards in a monotone order!
    Current implementation heavily depends on the property that even if we put
    a trx with smaller id to any structure later, it could not have modified a
    row the caller saw earlier. */
    static_assert(TRX_SHARDS_N < 1000, "The loop should be short");
    for (auto &shard : trx_sys->shards) {
      if (shard.active_rw_trxs.peek().min_id() <= max_old_active_id) {
        return true;
      }
    }
    return false;
  }
  ut_ad(trx_sys_mutex_own());
  const trx_t *trx = UT_LIST_GET_LAST(trx_sys->rw_trx_list);
  if (trx == nullptr) {
    trx_sys_mutex_exit();
    return false;
  }
  assert_trx_in_rw_list(trx);
  const trx_id_t min_active_now_id = trx->id;
  trx_sys_mutex_exit();
  return min_active_now_id <= max_old_active_id;
}

/** Checks if some transaction has an implicit x-lock on a record in a secondary
 index.
 @param[in]   rec       user record
 @param[in]   index     secondary index
 @param[in]   offsets   rec_get_offsets(rec, index)
 @return transaction id of the transaction which has the x-lock, or 0;
 NOTE that this function can return false positives but never false
 negatives. The caller must confirm all positive results by checking if the trx
 is still active. */
static trx_t *lock_sec_rec_some_has_impl(const rec_t *rec, dict_index_t *index,
                                         const ulint *offsets) {
  trx_t *trx;
  trx_id_t max_trx_id;
  const page_t *page = page_align(rec);

  ut_ad(!locksys::owns_exclusive_global_latch());
  ut_ad(!trx_sys_mutex_own());
  ut_ad(!index->is_clustered());
  ut_ad(page_rec_is_user_rec(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));

  max_trx_id = page_get_max_trx_id(page);

  /* Some transaction may have an implicit x-lock on the record only
  if the max trx id for the page >= min trx id for the trx list, or
  database recovery is running. We do not write the changes of a page
  max trx id to the log, and therefore during recovery, this value
  for a page may be incorrect. */

  if (!recv_recovery_is_on() && !can_older_trx_be_still_active(max_trx_id)) {
    trx = nullptr;

  } else if (!lock_check_trx_id_sanity(max_trx_id, rec, index, offsets)) {
    /* The page is corrupt: try to avoid a crash by returning 0 */
    trx = nullptr;

    /* In this case it is possible that some transaction has an implicit
    x-lock. We have to look in the clustered index. */

  } else {
    trx = row_vers_impl_x_locked(rec, index, offsets);
  }

  return (trx);
}

#ifdef UNIV_DEBUG
/** Checks if some transaction, other than given trx_id, has an explicit
 lock on the given rec, in the given precise_mode.
@param[in]   precise_mode   LOCK_S or LOCK_X possibly ORed to LOCK_GAP or
                            LOCK_REC_NOT_GAP.
@param[in]   trx            the trx holding implicit lock on rec
@param[in]   rec            user record
@param[in]   block          buffer block containing the record
@return true iff there's a transaction, whose id is not equal to trx_id,
        that has an explicit lock on the given rec, in the given
        precise_mode. */
static bool lock_rec_other_trx_holds_expl(ulint precise_mode, const trx_t *trx,
                                          const rec_t *rec,
                                          const buf_block_t *block) {
  bool holds = false;

  /* We will inspect locks from various shards when inspecting transactions. */
  locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};
  /* If trx_rw_is_active returns non-null impl_trx it only means that impl_trx
  was active at some moment during the call, but might already be in
  TRX_STATE_COMMITTED_IN_MEMORY when we execute the body of the if.
  However, we hold exclusive latch on whole lock_sys, which prevents anyone
  from creating any new explicit locks.
  So, all explicit locks we will see must have been created at the time when
  the transaction was not committed yet. */
  if (trx_t *impl_trx = trx_rw_is_active(trx->id, false)) {
    ulint heap_no = page_rec_get_heap_no(rec);
    mutex_enter(&trx_sys->mutex);

    for (auto t : trx_sys->rw_trx_list) {
      const lock_t *expl_lock =
          lock_rec_has_expl(precise_mode, block, heap_no, t);

      if (expl_lock && expl_lock->trx != impl_trx) {
        /* An explicit lock is held by trx other than
        the trx holding the implicit lock. */
        holds = true;
        break;
      }
    }

    mutex_exit(&trx_sys->mutex);
  }

  return (holds);
}
#endif /* UNIV_DEBUG */

ulint lock_number_of_rows_locked(const trx_lock_t *trx_lock) {
  /* We need exclusive lock_sys access, because trx_lock->n_rec_locks is
  modified while holding sharded lock only, so we need to disable all writers
  for this number to be meaningful */
  ut_ad(locksys::owns_exclusive_global_latch());

  return (trx_lock->n_rec_locks);
}

ulint lock_number_of_tables_locked(const trx_t *trx) {
  ut_ad(trx_mutex_own(trx));
  ulint count = 0;
  for (const lock_t *lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
       lock != nullptr && lock_get_type(lock) == LOCK_TABLE;
       lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
    count++;
  }

  return count;
}

/*============== RECORD LOCK CREATION AND QUEUE MANAGEMENT =============*/

/**
Do some checks and prepare for creating a new record lock */
void RecLock::prepare() const {
  ut_ad(locksys::owns_page_shard(m_rec_id.get_page_id()));
  ut_ad(m_trx == thr_get_trx(m_thr));

  /* Test if there already is some other reason to suspend thread:
  we do not enqueue a lock request if the query thread should be
  stopped anyway */

  if (que_thr_stop(m_thr)) {
    ut_error;
  }

  switch (trx_get_dict_operation(m_trx)) {
    case TRX_DICT_OP_NONE:
      break;
    case TRX_DICT_OP_TABLE:
    case TRX_DICT_OP_INDEX:
      ib::error(ER_IB_MSG_635)
          << "A record lock wait happens in a dictionary"
             " operation. index "
          << m_index->name << " of table " << m_index->table->name << ". "
          << BUG_REPORT_MSG;
      ut_d(ut_error);
  }

  ut_ad(m_index->table->n_ref_count > 0 || !m_index->table->can_be_evicted);
}

/**
Create the lock instance
@param[in, out] trx     The transaction requesting the lock
@param[in, out] index   Index on which record lock is required
@param[in] mode         The lock mode desired
@param[in] rec_id       The record id
@param[in] size         Size of the lock + bitmap requested
@return a record lock instance */
lock_t *RecLock::lock_alloc(trx_t *trx, dict_index_t *index, ulint mode,
                            const RecID &rec_id, ulint size) {
  ut_ad(locksys::owns_page_shard(rec_id.get_page_id()));
  /* We are about to modify structures in trx->lock which needs trx->mutex */
  ut_ad(trx_mutex_own(trx));

  lock_t *lock;

  if (trx->lock.rec_cached >= trx->lock.rec_pool.size() ||
      sizeof(*lock) + size > REC_LOCK_SIZE) {
    ulint n_bytes = size + sizeof(*lock);
    mem_heap_t *heap = trx->lock.lock_heap;
    auto ptr = mem_heap_alloc(heap, n_bytes);
    ut_a(ut::is_aligned_as<lock_t>(ptr));
    lock = reinterpret_cast<lock_t *>(ptr);
  } else {
    lock = trx->lock.rec_pool[trx->lock.rec_cached];
    ++trx->lock.rec_cached;
  }

  lock->trx = trx;

  lock->index = index;

  /* Note the creation timestamp */
  ut_d(lock->m_seq = lock_sys->m_seq.fetch_add(1));

  /* Setup the lock attributes */

  lock->type_mode = LOCK_REC | (mode & ~LOCK_TYPE_MASK);

  lock_rec_t &rec_lock = lock->rec_lock;

  /* Predicate lock always on INFIMUM (0) */

  if (is_predicate_lock(mode)) {
    rec_lock.n_bits = 8;

    memset(&lock[1], 0x0, 1);

  } else {
    ut_ad(8 * size < UINT32_MAX);
    rec_lock.n_bits = static_cast<uint32_t>(8 * size);

    memset(&lock[1], 0x0, size);
  }

  rec_lock.page_id = rec_id.get_page_id();

  /* Set the bit corresponding to rec */

  lock_rec_set_nth_bit(lock, rec_id.m_heap_no);

  MONITOR_INC(MONITOR_NUM_RECLOCK);

  MONITOR_INC(MONITOR_RECLOCK_CREATED);

  return (lock);
}

/** Insert lock record to the tail of the queue where the WAITING locks reside.
@param[in,out]  lock_hash       Hash table containing the locks
@param[in,out]  lock            Record lock instance to insert
@param[in]      rec_id          Record being locked */
static void lock_rec_insert_to_waiting(hash_table_t *lock_hash, lock_t *lock,
                                       const RecID &rec_id) {
  ut_ad(lock->is_waiting());
  ut_ad(rec_id.matches(lock));
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));
  ut_ad(locksys::owns_page_shard(rec_id.get_page_id()));

  HASH_INSERT(lock_t, hash, lock_hash, rec_id.hash_value(), lock);
}

/** Insert lock record to the head of the queue where the GRANTED locks reside.
@param[in,out]  lock_hash       Hash table containing the locks
@param[in,out]  lock            Record lock instance to insert
@param[in]      rec_id          Record being locked */
static void lock_rec_insert_to_granted(hash_table_t *lock_hash, lock_t *lock,
                                       const RecID &rec_id) {
  ut_ad(rec_id.matches(lock));
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));
  ut_ad(locksys::owns_page_shard(rec_id.get_page_id()));
  ut_ad(!lock->is_waiting());

  /* Move the target lock to the head of the list. */
  auto &first_node = hash_get_first(
      lock_hash, hash_calc_cell_id(rec_id.hash_value(), lock_hash));

  ut_ad(lock != first_node);

  auto next = reinterpret_cast<lock_t *>(first_node);

  first_node = lock;
  lock->hash = next;
}
namespace locksys {
/**
Adds the lock to the list of trx's locks.
Requires lock->trx to be already set.
Bumps the trx_lock_version.
@param[in,out]  lock  The lock that we want to add to lock->trx->lock.trx_locks
*/
static void add_to_trx_locks(lock_t *lock) {
  ut_ad(lock->trx != nullptr);
  ut_ad(trx_mutex_own(lock->trx));
  if (lock_get_type_low(lock) == LOCK_REC) {
    UT_LIST_ADD_LAST(lock->trx->lock.trx_locks, lock);
  } else {
    UT_LIST_ADD_FIRST(lock->trx->lock.trx_locks, lock);
  }
  lock->trx->lock.trx_locks_version++;
}

/**
Removes the lock from the list of trx's locks.
Bumps the trx_lock_version.
@param[in,out]  lock  The lock that we want to remove from
                      lock->trx->lock.trx_locks
*/
static void remove_from_trx_locks(lock_t *lock) {
  ut_ad(lock->trx != nullptr);
  ut_ad(trx_mutex_own(lock->trx));
  UT_LIST_REMOVE(lock->trx->lock.trx_locks, lock);
  lock->trx->lock.trx_locks_version++;
}
}  // namespace locksys

void RecLock::lock_add(lock_t *lock) {
  ut_ad((lock->type_mode | LOCK_REC) == (m_mode | LOCK_REC));
  ut_ad(m_rec_id.matches(lock));
  ut_ad(locksys::owns_page_shard(m_rec_id.get_page_id()));
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));
  ut_ad(trx_mutex_own(lock->trx));

  bool wait = m_mode & LOCK_WAIT;

  hash_table_t *lock_hash = lock_hash_get(m_mode);

  lock->index->table->n_rec_locks.fetch_add(1, std::memory_order_relaxed);

  if (!wait) {
    lock_rec_insert_to_granted(lock_hash, lock, m_rec_id);
  } else {
    lock_rec_insert_to_waiting(lock_hash, lock, m_rec_id);
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  /* The performance schema THREAD_ID and EVENT_ID are used only
  when DATA_LOCKS are exposed.  */
  PSI_THREAD_CALL(get_current_thread_event_id)
  (&lock->m_psi_internal_thread_id, &lock->m_psi_event_id);
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
#endif /* HAVE_PSI_THREAD_INTERFACE */

  locksys::add_to_trx_locks(lock);

  if (wait) {
    lock_set_lock_and_trx_wait(lock);
  }
}

/**
Create a lock for a transaction and initialise it.
@param[in, out] trx             Transaction requesting the new lock
@param[in] prdt                 Predicate lock (optional)
@return new lock instance */
lock_t *RecLock::create(trx_t *trx, const lock_prdt_t *prdt) {
  ut_ad(locksys::owns_page_shard(m_rec_id.get_page_id()));

  /* Ensure that another transaction doesn't access the trx
  lock state and lock data structures while we are adding the
  lock and changing the transaction state to LOCK_WAIT.
  In particular it protects the lock_alloc which uses trx's private pool of
  lock structures.
  It might be the case that we already hold trx->mutex because we got here from:
    - lock_rec_convert_impl_to_expl_for_trx
    - add_to_waitq
  */
  ut_ad(trx_mutex_own(trx));

  /* Create the explicit lock instance and initialise it. */

  lock_t *lock = lock_alloc(trx, m_index, m_mode, m_rec_id, m_size);

#ifdef UNIV_DEBUG
  /* GAP lock shouldn't be taken on DD tables with some exceptions */
  if (m_index->table->is_dd_table &&
      strstr(m_index->table->name.m_name,
             "mysql/st_spatial_reference_systems") == nullptr &&
      strstr(m_index->table->name.m_name, "mysql/innodb_table_stats") ==
          nullptr &&
      strstr(m_index->table->name.m_name, "mysql/innodb_index_stats") ==
          nullptr &&
      strstr(m_index->table->name.m_name, "mysql/table_stats") == nullptr &&
      strstr(m_index->table->name.m_name, "mysql/index_stats") == nullptr) {
    ut_ad(lock_rec_get_rec_not_gap(lock));
  }
#endif /* UNIV_DEBUG */

  if (prdt != nullptr && (m_mode & LOCK_PREDICATE)) {
    lock_prdt_set_prdt(lock, prdt);
  }

  lock_add(lock);

  return (lock);
}

/**
Collect the transactions that will need to be rolled back asynchronously
@param[in, out] hit_list    The list of transactions to be rolled back, to which
                            the trx should be appended.
@param[in]      hp_trx_id   The id of the blocked High Priority Transaction
@param[in, out] trx         The blocking transaction to be rolled back */
static void lock_mark_trx_for_rollback(hit_list_t &hit_list, trx_id_t hp_trx_id,
                                       trx_t *trx) {
  trx->abort = true;

  ut_ad(!trx->read_only);
  ut_ad(trx_mutex_own(trx));
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK));
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE));

  trx->in_innodb |= TRX_FORCE_ROLLBACK;

  std::thread::id thread_id = std::this_thread::get_id();

  std::thread::id zero{};
  ut_a(trx->killed_by.compare_exchange_strong(zero, thread_id));

  hit_list.push_back(hit_list_t::value_type(trx));

#ifdef UNIV_DEBUG
  THD *thd = trx->mysql_thd;

  if (thd != nullptr) {
    char buffer[1024];
    ib::info(ER_IB_MSG_636, ulonglong{hp_trx_id}, to_string(thread_id).c_str(),
             ulonglong{trx->id},
             thd_security_context(thd, buffer, sizeof(buffer), 512));
  }
#endif /* UNIV_DEBUG */
}
/**
Checks if the waits-for edge between waiting_lock and blocking_lock may
survive PREPARE of the blocking_lock->trx. For transactions in low
isolation levels we release some of the locks during PREPARE.
@param[in]    waiting_lock    A lock waiting in queue, blocked by blocking_lock
@param[in]    blocking_lock   A lock which is a reason the waiting_lock has to
                              wait
@return if the waiting_lock->trx MAY have to wait for blocking_lock->trx
        even if blocking_lock->trx PREPAREs. The nondeterminism comes from
        situations like when X lock conflicts with S lock on a delete-marked
        record - purgining it might convert both to non-conflicitng gap locks
@retval true    the waiting_lock->trx MAY have to wait for blocking_lock->trx
                even if blocking_lock->trx PREPAREs.
@retval false   the waiting_lock->trx CERTAINLY will not have to wait for
                blocking_lock->trx for this particular reason.
*/
static bool lock_edge_may_survive_prepare(const lock_t *waiting_lock,
                                          const lock_t *blocking_lock) {
  /* Keep in sync with lock_relase_read_lock(blocking_lock, only_gap)
  for the only_gap value currently used in the call from trx_prepare().
  Currently some transactions release locks on gaps and a lock on a gap blocks
  only Insert Intention, and II is only blocked by locks on a gap.
  A "lock on a gap" can be either a LOCK_GAP, or a part of LOCK_ORDINARY. */
  if (blocking_lock->trx->releases_gap_locks_at_prepare() &&
      waiting_lock->is_insert_intention()) {
    ut_ad(blocking_lock->is_record_lock());
    ut_ad(waiting_lock->is_record_lock());

    return false;
  }
  return true;
}
static void lock_report_wait_for_edge_to_server(const lock_t *waiting_lock,
                                                const lock_t *blocking_lock) {
  thd_report_lock_wait(
      waiting_lock->trx->mysql_thd, blocking_lock->trx->mysql_thd,
      lock_edge_may_survive_prepare(waiting_lock, blocking_lock));
}
/** Creates a new edge in wait-for graph, from waiter to blocker
@param[in]    waiting_lock    A lock waiting in queue, blocked by blocking_lock
@param[in]    blocking_lock   A lock which is a reason the waiting_lock has to
                          wait */
static void lock_create_wait_for_edge(const lock_t *waiting_lock,
                                      const lock_t *blocking_lock) {
  trx_t *waiter = waiting_lock->trx;
  trx_t *blocker = blocking_lock->trx;
  ut_ad(trx_mutex_own(waiter));
  ut_ad(waiter->lock.wait_lock != nullptr);
  ut_ad(locksys::owns_lock_shard(waiter->lock.wait_lock));
  ut_ad(waiter->lock.blocking_trx.load() == nullptr);
  /* We don't call lock_wait_request_check_for_cycles() here as it
  would be slightly premature: the trx is not yet inserted into a slot of
  lock_sys->waiting_threads at this point, and thus it would be invisible to
  the thread which analyzes these slots. What we do instead is to let the
  lock_wait_table_reserve_slot() function be responsible for calling
  lock_wait_request_check_for_cycles() once it insert the trx to a
  slot.*/
  waiter->lock.blocking_trx.store(blocker);
  lock_report_wait_for_edge_to_server(waiting_lock, blocking_lock);
}

/**
Setup the requesting transaction state for lock grant
@param[in,out] lock             Lock for which to change state */
void RecLock::set_wait_state(lock_t *lock) {
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));
  ut_ad(m_trx == lock->trx);
  ut_ad(trx_mutex_own(m_trx));
  ut_ad(lock_get_wait(lock));

  m_trx->lock.wait_started =
      std::chrono::system_clock::from_time_t(time(nullptr));

  m_trx->lock.que_state = TRX_QUE_LOCK_WAIT;

  m_trx->lock.was_chosen_as_deadlock_victim = false;

  bool stopped = que_thr_stop(m_thr);
  ut_a(stopped);
}

dberr_t RecLock::add_to_waitq(const lock_t *wait_for, const lock_prdt_t *prdt) {
  ut_ad(locksys::owns_page_shard(m_rec_id.get_page_id()));
  ut_ad(m_trx == thr_get_trx(m_thr));

  /* It is not that the body of this function requires trx->mutex, but some of
  the functions it calls require it and it so happens that we always posses it
  so it makes reasoning about code easier if we simply assert this fact. */
  ut_ad(trx_mutex_own(m_trx));

  DEBUG_SYNC_C("rec_lock_add_to_waitq");

  if (m_trx->in_innodb & TRX_FORCE_ROLLBACK) {
    return (DB_DEADLOCK);
  }

  m_mode |= LOCK_WAIT;

  /* Do the preliminary checks, and set query thread state */

  prepare();

  /* Don't queue the lock to hash table, if high priority transaction. */
  lock_t *lock = create(m_trx, prdt);

  lock_create_wait_for_edge(lock, wait_for);

  ut_ad(lock_get_wait(lock));

  set_wait_state(lock);

  MONITOR_INC(MONITOR_LOCKREC_WAIT);

  return (DB_LOCK_WAIT);
}
/** Moves a granted lock to the front of the queue for a given record by
removing it adding it to the front. As a single lock can correspond to multiple
rows (and thus: queues) this function moves it to the front of whole hash cell.
@param  [in]    lock    a granted lock to be moved
@param  [in]    rec_id  record id which specifies particular queue and hash
cell */
static void lock_rec_move_granted_to_front(lock_t *lock, const RecID &rec_id) {
  ut_ad(!lock->is_waiting());
  ut_ad(rec_id.matches(lock));
  ut_ad(locksys::owns_page_shard(rec_id.get_page_id()));
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));

  const auto hash_table = lock->hash_table();
  HASH_DELETE(lock_t, hash, hash_table, rec_id.hash_value(), lock);
  lock_rec_insert_to_granted(hash_table, lock, rec_id);
}

/** Looks for a suitable type record lock struct by the same trx on the same
page. This can be used to save space when a new record lock should be set on a
page: no new struct is needed, if a suitable old is found.
@param[in]  type_mode                 lock type_mode field
@param[in]  heap_no                   heap number of the record we plan to use.
                                      The lock struct we search for needs to
                                      have a bitmap at least as large.
@param[in]  lock                      lock_rec_get_first_on_page()
@param[in]  trx                       transaction
@param[out] found_waiter_before_lock  true iff there is a waiting lock before
                                      the returned lock
@return lock or nullptr if there is no lock we could reuse*/
static inline lock_t *lock_rec_find_similar_on_page(
    uint32_t type_mode, size_t heap_no, lock_t *lock, const trx_t *trx,
    bool &found_waiter_before_lock) {
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));
  found_waiter_before_lock = false;
  for (/* No op */; lock != nullptr; lock = lock_rec_get_next_on_page(lock)) {
    if (lock->trx == trx && lock->type_mode == type_mode &&
        heap_no < lock_rec_get_n_bits(lock)) {
      return (lock);
    }
    if (lock->is_waiting()) {
      found_waiter_before_lock = true;
    }
  }
  found_waiter_before_lock = false;
  return (nullptr);
}

/** Adds a record lock request in the record queue. The request is normally
 added as the last in the queue, but if the request to be added is not a waiting
 request, we can reuse a suitable record lock object already existing on the
 same page, just setting the appropriate bit in its bitmap. This is a low-level
 function which does NOT check for deadlocks or lock compatibility!
@param[in]      type_mode         lock mode, wait, gap etc. flags; type is
                                  ignored and replaced by LOCK_REC
@param[in]      block             buffer block containing the record
@param[in]      heap_no           heap number of the record
@param[in]      index             index of record
@param[in,out]  trx               transaction
@param[in]      we_own_trx_mutex  true iff the caller own trx->mutex (optional).
                                  Defaults to false. */
static void lock_rec_add_to_queue(ulint type_mode, const buf_block_t *block,
                                  const ulint heap_no, dict_index_t *index,
                                  trx_t *trx,
                                  const bool we_own_trx_mutex = false) {
#ifdef UNIV_DEBUG
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(we_own_trx_mutex == trx_mutex_own(trx));

  ut_ad(index->is_clustered() ||
        dict_index_get_online_status(index) != ONLINE_INDEX_CREATION);
  switch (type_mode & LOCK_MODE_MASK) {
    case LOCK_X:
    case LOCK_S:
      break;
    default:
      ut_error;
  }

  if (!(type_mode & (LOCK_WAIT | LOCK_GAP))) {
    lock_mode mode = (type_mode & LOCK_MODE_MASK) == LOCK_S ? LOCK_X : LOCK_S;
    const lock_t *other_lock =
        lock_rec_other_has_expl_req(mode, block, false, heap_no, trx);
    ut_a(!other_lock);
  }
#endif /* UNIV_DEBUG */

  type_mode |= LOCK_REC;

  /* If rec is the supremum record, then we can reset the gap bit, as
  all locks on the supremum are automatically of the gap type, and we
  try to avoid unnecessary memory consumption of a new record lock
  struct for a gap type lock */

  if (heap_no == PAGE_HEAP_NO_SUPREMUM) {
    ut_ad(!(type_mode & LOCK_REC_NOT_GAP));

    /* There should never be LOCK_REC_NOT_GAP on a supremum
    record, but let us play safe */

    type_mode &= ~(LOCK_GAP | LOCK_REC_NOT_GAP);
  }

  if (!(type_mode & LOCK_WAIT)) {
    hash_table_t *const hash = lock_hash_get(type_mode);
    lock_t *const first_lock = lock_rec_get_first_on_page(hash, block);

    if (first_lock != nullptr) {
      /* Look for a similar record lock on the same page:
      if one is found we can just set the bit */

      bool found_waiter_before_lock = false;
      lock_t *lock = lock_rec_find_similar_on_page(
          type_mode, heap_no, first_lock, trx, found_waiter_before_lock);

      if (lock != nullptr) {
        /* Some B-tree reorganization functions, when moving locks from one
        place to another, can leave a lock_t struct with an empty bitmap. They
        also clear a LOCK_WAIT flag. This means it's possible that `lock` was
        a waiting lock in the past, and if we want to reuse it, we have to move
        it to the front of the queue where granted locks reside.
        We only NEED to do that if there are any waiting locks in front of it.
        We CAN move the lock to front ONLY IF it wasn't part of any queue.
        In other words, moving to front is not safe if it has non-empty bitmap.
        Moving a lock to the front of its queue can create endless loop in the
        caller if it is iterating over the queue.
        Fortunately, the only situation in which a GRANTED lock can be after a
        WAITING lock in the hash cell is if it was WAITING in the past and the
        only bit for the heap_no was cleared, so it no longer belongs to any
        queue.*/
        ut_ad(!found_waiter_before_lock ||
              (ULINT_UNDEFINED == lock_rec_find_set_bit(lock)));

        if (!lock_rec_get_nth_bit(lock, heap_no)) {
          lock_rec_set_nth_bit(lock, heap_no);
          if (found_waiter_before_lock) {
            lock_rec_move_granted_to_front(lock, RecID{lock, heap_no});
          }
        }

        return;
      }
    }
  }

  RecLock rec_lock(index, block, heap_no, type_mode);

  if (!we_own_trx_mutex) {
    trx_mutex_enter(trx);
  }
  rec_lock.create(trx);
  if (!we_own_trx_mutex) {
    trx_mutex_exit(trx);
  }
}

/** This is a fast routine for locking a record in the most common cases:
 there are no explicit locks on the page, or there is just one lock, owned
 by this transaction, and of the right type_mode. This is a low-level function
 which does NOT look at implicit locks! Checks lock compatibility within
 explicit locks. This function sets a normal next-key lock, or in the case of
 a page supremum record, a gap type lock.
 @return whether the locking succeeded LOCK_REC_SUCCESS,
 LOCK_REC_SUCCESS_CREATED, LOCK_REC_FAIL */
static inline lock_rec_req_status lock_rec_lock_fast(
    bool impl,                /*!< in: if true, no lock is set
                              if no wait is necessary: we
                              assume that the caller will
                              set an implicit lock */
    ulint mode,               /*!< in: lock mode: LOCK_X or
                              LOCK_S possibly ORed to either
                              LOCK_GAP or LOCK_REC_NOT_GAP */
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    ulint heap_no,            /*!< in: heap number of record */
    dict_index_t *index,      /*!< in: index of record */
    que_thr_t *thr)           /*!< in: query thread */
{
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(!srv_read_only_mode);
  ut_ad((LOCK_MODE_MASK & mode) != LOCK_S ||
        lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
  ut_ad((LOCK_MODE_MASK & mode) != LOCK_X ||
        lock_table_has(thr_get_trx(thr), index->table, LOCK_IX) ||
        srv_read_only_mode);
  ut_ad((LOCK_MODE_MASK & mode) == LOCK_S || (LOCK_MODE_MASK & mode) == LOCK_X);
  ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP ||
        mode - (LOCK_MODE_MASK & mode) == 0 ||
        mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);
  ut_ad(index->is_clustered() || !dict_index_is_online_ddl(index));
  ut_ad(!(mode & LOCK_PREDICATE));
  ut_ad(!(mode & LOCK_PRDT_PAGE));
  DBUG_EXECUTE_IF("innodb_report_deadlock", return (LOCK_REC_FAIL););

  lock_t *lock = lock_rec_get_first_on_page(lock_sys->rec_hash, block);

  trx_t *trx = thr_get_trx(thr);
  ut_ad(!trx_mutex_own(trx));

  lock_rec_req_status status = LOCK_REC_SUCCESS;

  if (lock == nullptr) {
    if (!impl) {
      RecLock rec_lock(index, block, heap_no, mode);

      trx_mutex_enter(trx);
      rec_lock.create(trx);
      trx_mutex_exit(trx);

      status = LOCK_REC_SUCCESS_CREATED;
    }
  } else {
    trx_mutex_enter(trx);

    if (lock_rec_get_next_on_page(lock) != nullptr || lock->trx != trx ||
        lock->type_mode != (mode | LOCK_REC) ||
        lock_rec_get_n_bits(lock) <= heap_no) {
      status = LOCK_REC_FAIL;
    } else if (!impl) {
      /* If the nth bit of the record lock is already set
      then we do not set a new lock bit, otherwise we do
      set */
      if (!lock_rec_get_nth_bit(lock, heap_no)) {
        lock_rec_set_nth_bit(lock, heap_no);
        status = LOCK_REC_SUCCESS_CREATED;
      }
    }

    trx_mutex_exit(trx);
  }
  ut_ad(status == LOCK_REC_SUCCESS || status == LOCK_REC_SUCCESS_CREATED ||
        status == LOCK_REC_FAIL);
  return (status);
}

/** A helper function for lock_rec_lock_slow(), which grants a Next Key Lock
(either LOCK_X or LOCK_S as specified by `mode`) on <`block`,`heap_no`> in the
`index` to the `trx`, assuming that it already has a granted `held_lock`, which
is at least as strong as mode|LOCK_REC_NOT_GAP. It does so by either reusing the
lock if it already covers the gap, or by ensuring a separate GAP Lock, which in
combination with Record Lock satisfies the request.
@param[in]      held_lock   a lock granted to `trx` which is at least as strong
                            as mode|LOCK_REC_NOT_GAP
@param[in]      mode        requested lock mode: LOCK_X or LOCK_S
@param[in]      block       buffer block containing the record to be locked
@param[in]      heap_no     heap number of the record to be locked
@param[in]      index       index of record to be locked
@param[in]      trx         the transaction requesting the Next Key Lock */
static void lock_reuse_for_next_key_lock(const lock_t *held_lock, ulint mode,
                                         const buf_block_t *block,
                                         ulint heap_no, dict_index_t *index,
                                         trx_t *trx) {
  ut_ad(mode == LOCK_S || mode == LOCK_X);
  ut_ad(lock_mode_is_next_key_lock(mode));

  if (!held_lock->is_record_not_gap()) {
    ut_ad(held_lock->is_next_key_lock());
    return;
  }

  /* We have a Record Lock granted, so we only need a GAP Lock. We assume
  that GAP Locks do not conflict with anything. Therefore a GAP Lock
  could be granted to us right now if we've requested: */
  mode |= LOCK_GAP;
  ut_ad(nullptr ==
        lock_rec_other_has_conflicting(mode, block, heap_no, trx).wait_for);

  /* It might be the case we already have one, so we first check that. */
  if (lock_rec_has_expl(mode, block, heap_no, trx) == nullptr) {
    lock_rec_add_to_queue(LOCK_REC | mode, block, heap_no, index, trx);
  }
}
/** This is the general, and slower, routine for locking a record. This is a
low-level function which does NOT look at implicit locks! Checks lock
compatibility within explicit locks. This function sets a normal next-key
lock, or in the case of a page supremum record, a gap type lock.
@param[in]      impl            if true, no lock might be set if no wait is
                                necessary: we assume that the caller will
                                set an implicit lock
@param[in]      sel_mode        select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOCKED, or SELECT_NO_WAIT
@param[in]      mode            lock mode: LOCK_X or LOCK_S possibly ORed to
                                either LOCK_GAP or LOCK_REC_NOT_GAP
@param[in]      block           buffer block containing the record
@param[in]      heap_no         heap number of record
@param[in]      index           index of record
@param[in,out]  thr             query thread
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
DB_SKIP_LOCKED, or DB_LOCK_NOWAIT */
static dberr_t lock_rec_lock_slow(bool impl, select_mode sel_mode, ulint mode,
                                  const buf_block_t *block, ulint heap_no,
                                  dict_index_t *index, que_thr_t *thr) {
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(!srv_read_only_mode);
  ut_ad((LOCK_MODE_MASK & mode) != LOCK_S ||
        lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
  ut_ad((LOCK_MODE_MASK & mode) != LOCK_X ||
        lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
  ut_ad((LOCK_MODE_MASK & mode) == LOCK_S || (LOCK_MODE_MASK & mode) == LOCK_X);
  ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP ||
        mode - (LOCK_MODE_MASK & mode) == LOCK_ORDINARY ||
        mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);
  ut_ad(index->is_clustered() || !dict_index_is_online_ddl(index));

  DBUG_EXECUTE_IF("innodb_report_deadlock", return (DB_DEADLOCK););

  trx_t *trx = thr_get_trx(thr);

  ut_ad(sel_mode == SELECT_ORDINARY ||
        (sel_mode != SELECT_ORDINARY && !trx_is_high_priority(trx)));

  /* A very common type of lock in InnoDB is "Next Key Lock", which is almost
  equivalent to two locks: Record Lock and GAP Lock separately.
  Thus, in case we need to wait, we check if we already own a Record Lock,
  and if we do, we only need the GAP Lock.
  We don't do the opposite thing (of checking for GAP Lock, and only requesting
  Record Lock), because if Next Key Lock has to wait, then it is because of a
  conflict with someone who locked the record, as locks on gaps are compatible
  with each other, so even if we have a GAP Lock, narrowing the requested mode
  to Record Lock will not make the conflict go away.

  In current implementation locks on supremum are treated like GAP Locks,
  in particular they never have to wait for anything (unless they are Insert
  Intention locks, but we've ruled that out with asserts before getting here),
  so there is no gain in using the above "lock splitting" heuristic for locks on
  supremum, and reasoning becomes a bit simpler without this special case. */

  auto checked_mode =
      (heap_no != PAGE_HEAP_NO_SUPREMUM && lock_mode_is_next_key_lock(mode))
          ? mode | LOCK_REC_NOT_GAP
          : mode;

  const auto *held_lock = lock_rec_has_expl(checked_mode, block, heap_no, trx);

  if (held_lock != nullptr) {
    if (checked_mode == mode) {
      /* The trx already has a strong enough lock on rec: do nothing */
      return (DB_SUCCESS);
    }

    /* As check_mode != mode, the mode is Next Key Lock, which can not be
    emulated by implicit lock (which are LOCK_REC_NOT_GAP only). */
    ut_ad(!impl);

    lock_reuse_for_next_key_lock(held_lock, mode, block, heap_no, index, trx);
    return (DB_SUCCESS);
  }
  const auto conflicting =
      lock_rec_other_has_conflicting(mode, block, heap_no, trx);

  if (conflicting.wait_for != nullptr) {
    switch (sel_mode) {
      case SELECT_SKIP_LOCKED:
        return (DB_SKIP_LOCKED);
      case SELECT_NOWAIT:
        return (DB_LOCK_NOWAIT);
      case SELECT_ORDINARY:
        /* If another transaction has a non-gap conflicting request in the
        queue, as this transaction does not have a lock strong enough already
        granted on the record, we may have to wait. */

        RecLock rec_lock(thr, index, block, heap_no, mode);

        trx_mutex_enter(trx);

        dberr_t err = rec_lock.add_to_waitq(conflicting.wait_for);

        trx_mutex_exit(trx);

        ut_ad(err == DB_SUCCESS_LOCKED_REC || err == DB_LOCK_WAIT ||
              err == DB_DEADLOCK);
        return (err);
    }
  }
  /* In case we've used a heuristic to bypass a conflicting waiter, we prefer to
  create an explicit lock so it is easier to track the wait-for relation.*/
  if (!impl || conflicting.bypassed) {
    /* Set the requested lock on the record. */

    lock_rec_add_to_queue(LOCK_REC | mode, block, heap_no, index, trx);

    return (DB_SUCCESS_LOCKED_REC);
  }
  return (DB_SUCCESS);
}

/** Tries to lock the specified record in the mode requested. If not immediately
possible, enqueues a waiting lock request. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. This function sets a normal next-key lock, or in the case
of a page supremum record, a gap type lock.
@param[in]      impl            if true, no lock is set if no wait is
                                necessary: we assume that the caller will
                                set an implicit lock
@param[in]      sel_mode        select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOCKED, or SELECT_NO_WAIT
@param[in]      mode            lock mode: LOCK_X or LOCK_S possibly ORed to
                                either LOCK_GAP or LOCK_REC_NOT_GAP
@param[in]      block           buffer block containing the record
@param[in]      heap_no         heap number of record
@param[in]      index           index of record
@param[in,out]  thr             query thread
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
DB_SKIP_LOCKED, or DB_LOCK_NOWAIT */
static dberr_t lock_rec_lock(bool impl, select_mode sel_mode, ulint mode,
                             const buf_block_t *block, ulint heap_no,
                             dict_index_t *index, que_thr_t *thr) {
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(!srv_read_only_mode);
  ut_ad((LOCK_MODE_MASK & mode) != LOCK_S ||
        lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
  ut_ad((LOCK_MODE_MASK & mode) != LOCK_X ||
        lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
  ut_ad((LOCK_MODE_MASK & mode) == LOCK_S || (LOCK_MODE_MASK & mode) == LOCK_X);
  ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP ||
        mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP ||
        mode - (LOCK_MODE_MASK & mode) == 0);
  ut_ad(index->is_clustered() || !dict_index_is_online_ddl(index));
  /* Implicit locks are equivalent to LOCK_X|LOCK_REC_NOT_GAP, so we can omit
  creation of explicit lock only if the requested mode was LOCK_REC_NOT_GAP */
  ut_ad(!impl || ((mode & LOCK_REC_NOT_GAP) == LOCK_REC_NOT_GAP));
  /* We try a simplified and faster subroutine for the most
  common cases */
  switch (lock_rec_lock_fast(impl, mode, block, heap_no, index, thr)) {
    case LOCK_REC_SUCCESS:
      return (DB_SUCCESS);
    case LOCK_REC_SUCCESS_CREATED:
      return (DB_SUCCESS_LOCKED_REC);
    case LOCK_REC_FAIL:
      return (
          lock_rec_lock_slow(impl, sel_mode, mode, block, heap_no, index, thr));
    default:
      ut_error;
  }
}

/** Checks if a waiting record lock request still has to wait in a queue.
@param[in]  wait_lock     Waiting record lock
@param[in]  blocking_trx  If not nullptr, it restricts the search to only the
                          locks held by the blocking_trx, which is useful in
                          case when there might be multiple reasons for waiting
                          in queue, but we need to report the specific one.
                          Useful when reporting a deadlock cycle. (optional)
@return The conflicting lock which is the reason wait_lock has to wait
or nullptr if it can be granted now */
static const lock_t *lock_rec_has_to_wait_in_queue(
    const lock_t *wait_lock, const trx_t *blocking_trx = nullptr) {
  const lock_t *lock;
  ulint heap_no;
  ulint bit_mask;
  ulint bit_offset;
  hash_table_t *hash;

  ut_ad(lock_get_type_low(wait_lock) == LOCK_REC);
  const auto page_id = wait_lock->rec_lock.page_id;
  ut_ad(locksys::owns_page_shard(page_id));
  ut_ad(lock_get_wait(wait_lock));

  heap_no = lock_rec_find_set_bit(wait_lock);

  bit_offset = heap_no / 8;
  bit_mask = static_cast<ulint>(1) << (heap_no % 8);

  hash = lock_hash_get(wait_lock->type_mode);
  locksys::Trx_locks_cache wait_lock_cache{};
  for (lock = lock_rec_get_first_on_page_addr(hash, page_id); lock != wait_lock;
       lock = lock_rec_get_next_on_page_const(lock)) {
    const byte *p = (const byte *)&lock[1];

    if ((blocking_trx == nullptr || blocking_trx == lock->trx) &&
        heap_no < lock_rec_get_n_bits(lock) && (p[bit_offset] & bit_mask) &&
        locksys::rec_lock_has_to_wait(wait_lock, lock, wait_lock_cache)) {
      return (lock);
    }
  }

  return (nullptr);
}

/** Grants a lock to a waiting lock request and releases the waiting
transaction. The caller must hold lock_sys latch for the shard containing the
lock, but not the lock->trx->mutex.
@param[in,out]    lock    waiting lock request
 */
static void lock_grant(lock_t *lock) {
  ut_ad(locksys::owns_lock_shard(lock));
  ut_ad(!trx_mutex_own(lock->trx));

  trx_mutex_enter(lock->trx);

  if (lock_get_mode(lock) == LOCK_AUTO_INC) {
    dict_table_t *table = lock->tab_lock.table;

    if (table->autoinc_trx == lock->trx) {
      ib::error(ER_IB_MSG_637) << "Transaction already had an"
                               << " AUTO-INC lock!";
    } else {
      ut_ad(table->autoinc_trx == nullptr);
      table->autoinc_trx = lock->trx;

      ib_vector_push(lock->trx->lock.autoinc_locks, &lock);
    }
  }

  DBUG_PRINT("ib_lock", ("wait for trx " TRX_ID_FMT " ends",
                         trx_get_id_for_print(lock->trx)));

  lock_reset_wait_and_release_thread_if_suspended(lock);
  ut_ad(trx_mutex_own(lock->trx));

  trx_mutex_exit(lock->trx);
}

void lock_make_trx_hit_list(trx_t *hp_trx, hit_list_t &hit_list) {
  trx_mutex_enter(hp_trx);
  const trx_id_t hp_trx_id = hp_trx->id;
  ut_ad(trx_can_be_handled_by_current_thread(hp_trx));
  ut_ad(trx_is_high_priority(hp_trx));
  /* To avoid slow procedure below, we first
  check if this transaction is waiting for a lock at all. It's unsafe to read
  hp->lock.wait_lock without latching whole lock_sys as it might temporarily
  change to NULL during a concurrent B-tree reorganization, even though the
  trx actually is still waiting. Thus we use hp_trx->lock.blocking_trx instead.
  */
  const bool is_waiting = (hp_trx->lock.blocking_trx.load() != nullptr);
  trx_mutex_exit(hp_trx);
  if (!is_waiting) {
    return;
  }
  /* We don't expect hp_trx to commit (change version) as we are the thread
  running the hp_trx */
  locksys::run_if_waiting({hp_trx}, [&]() {
    const lock_t *lock = hp_trx->lock.wait_lock;
    // TBD: could this technique be used for table locks as well?
    if (!lock->is_record_lock()) {
      return;
    }
    trx_mutex_exit(hp_trx);
    Lock_iter::for_each(
        {lock, lock_rec_find_set_bit(lock)},
        [&](lock_t *next) {
          trx_t *trx = next->trx;
          /* Check only for conflicting, granted locks on the current
          row. Currently, we don't rollback read only transactions,
          transactions owned by background threads. */
          if (trx == hp_trx || next->is_waiting() || trx->read_only ||
              trx->mysql_thd == nullptr || !lock_has_to_wait(lock, next)) {
            return true;
          }

          trx_mutex_enter(trx);

          /* Skip high priority transactions, if already marked for
          abort by some other transaction or if ASYNC rollback is
          disabled. A transaction must complete kill/abort of a
          victim transaction once marked and added to hit list. */
          if (!trx_is_high_priority(trx) &&
              (trx->in_innodb & TRX_FORCE_ROLLBACK) == 0 &&
              (trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE) == 0 &&
              !trx->abort) {
            /* Mark for ASYNC Rollback and add to hit list. */
            lock_mark_trx_for_rollback(hit_list, hp_trx_id, trx);
          }

          trx_mutex_exit(trx);
          return true;
        },
        lock->hash_table());
    // the run_if_waiting expects the hp_trx to be held after callback
    trx_mutex_enter(hp_trx);
  });
}

/** Cancels a waiting record lock request and releases the waiting transaction
 that requested it. NOTE: does NOT check if waiting lock requests behind this
 one can now be granted! */
static void lock_rec_cancel(
    lock_t *lock) /*!< in: waiting record lock request */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));

  /* Reset the bit (there can be only one set bit) in the lock bitmap */
  lock_rec_reset_nth_bit(lock, lock_rec_find_set_bit(lock));

  trx_mutex_enter(lock->trx);

  lock_reset_wait_and_release_thread_if_suspended(lock);

  trx_mutex_exit(lock->trx);
}

/** Given a waiting_lock, and blocking_lock which is the reason it has to wait,
makes sure that the (only) edge in the wait-for graph outgoing from the
waiting_lock->trx points to blocking_lock->trx
@param[in]    waiting_lock    A lock waiting in queue, blocked by blocking_lock
@param[in]    blocking_lock   A lock which is a reason the waiting_lock has to
                              wait */
static void lock_update_wait_for_edge(const lock_t *waiting_lock,
                                      const lock_t *blocking_lock) {
  ut_ad(locksys::owns_lock_shard(waiting_lock));
  ut_ad(locksys::owns_lock_shard(blocking_lock));
  ut_ad(waiting_lock->is_waiting());
  ut_ad(lock_has_to_wait(waiting_lock, blocking_lock));
  /* Still needs to wait, but perhaps the reason has changed */
  if (waiting_lock->trx->lock.blocking_trx.load() != blocking_lock->trx) {
    waiting_lock->trx->lock.blocking_trx.store(blocking_lock->trx);
    /* We call lock_wait_request_check_for_cycles() because the outgoing edge of
    wait_lock->trx has changed it's endpoint and we need to analyze the
    wait-for-graph again. */
    lock_wait_request_check_for_cycles();
    lock_report_wait_for_edge_to_server(waiting_lock, blocking_lock);
  }
}

/** Checks if a waiting record lock request still has to wait for granted locks.
@param[in]      wait_lock               Waiting record lock
@param[in]      granted                 Granted record locks
@param[in]      new_granted_index       Start of new granted locks
@return The conflicting lock which is the reason wait_lock has to wait
or nullptr if it can be granted now */
template <typename Container>
static const lock_t *lock_rec_has_to_wait_for_granted(
    const typename Container::value_type &wait_lock, const Container &granted,
    const size_t new_granted_index)

{
  ut_ad(locksys::owns_page_shard(wait_lock->rec_lock.page_id));
  ut_ad(wait_lock->is_record_lock());

  ut_ad(new_granted_index <= granted.size());

  /* We iterate over granted locks in reverse order.
  Conceptually this corresponds to chronological order.
  This way, we pick as blocking_trx the oldest reason for waiting we haven't
  yet analyzed in deadlock checker. Our hope is that eventually (perhaps after
  several such updates) we will set blocking_trx to the real cause of the
  deadlock, which is the next node on the deadlock cycle. */
  for (size_t i = new_granted_index; i--;) {
    const auto granted_lock = granted[i];
    if (lock_has_to_wait(wait_lock, granted_lock)) {
      return (granted_lock);
    }
  }

  for (size_t i = new_granted_index; i < granted.size(); ++i) {
    const auto granted_lock = granted[i];
    ut_ad(granted_lock->trx->error_state != DB_DEADLOCK);
    ut_ad(!granted_lock->trx->lock.was_chosen_as_deadlock_victim);

    if (lock_has_to_wait(wait_lock, granted_lock)) {
      return (granted_lock);
    }
  }

  return (nullptr);
}

/** Grant a lock to waiting transactions. This function scans the queue of locks
in which in_lock resides (or resided) paying attention only to locks on
heap_no-th bit. For each waiting lock which was blocked by in_lock->trx it
checks if it can be granted now. It iterates on waiting locks in order favoring
high-priority transactions and then transactions of high
trx->lock.schedule_weight.
@param[in]    in_lock   Lock which was released, or
                        partially released by modifying its type/mode
                        (see lock_trx_release_read_locks) or
                        resetting heap_no-th bit in the bitmap
                        (see lock_rec_release)
@param[in]    heap_no   Heap number within the page on which the
lock was (or still is) held */
static void lock_rec_grant_by_heap_no(lock_t *in_lock, ulint heap_no) {
  const auto hash_table = in_lock->hash_table();

  ut_ad(in_lock->is_record_lock());
  ut_ad(locksys::owns_page_shard(in_lock->rec_lock.page_id));

  using LockDescriptorEx = std::pair<trx_schedule_weight_t, lock_t *>;
  /* Preallocate for 4 lists with 32 locks. */
  Scoped_heap heap((sizeof(lock_t *) * 3 + sizeof(LockDescriptorEx)) * 32,
                   UT_LOCATION_HERE);

  RecID rec_id{in_lock, heap_no};
  Locks<lock_t *> low_priority_light{heap.get()};
  Locks<lock_t *> waiting{heap.get()};
  Locks<lock_t *> granted{heap.get()};
  Locks<LockDescriptorEx> low_priority_heavier{heap.get()};

  const auto in_trx = in_lock->trx;
#ifdef UNIV_DEBUG
  bool seen_waiting_lock = false;
#endif
  Lock_iter::for_each(
      rec_id,
      [&](lock_t *lock) {
        /* Split the relevant locks in the queue into:
        - granted = granted locks
        - waiting = waiting locks of high priority transactions
        - low_priority_heavier = waiting locks of low priority, but heavy weight
        - low_priority_light = waiting locks of low priority and light weight
        */
        if (!lock->is_waiting()) {
          /* Granted locks should be before waiting locks. */
          ut_ad(!seen_waiting_lock);
          granted.push_back(lock);
          return (true);
        }
        ut_d(seen_waiting_lock = true);
        const auto trx = lock->trx;
        if (trx->error_state == DB_DEADLOCK ||
            trx->lock.was_chosen_as_deadlock_victim) {
          return (true);
        }
        /* We read blocking_trx while holding this lock_sys queue latched, and
        each write to blocking_trx is done while holding the latch. So, even
        though we use memory_order_relaxed we will see modifications performed
        before we acquired the latch. */
        const auto blocking_trx =
            trx->lock.blocking_trx.load(std::memory_order_relaxed);
        /* No one should be WAITING without good reason! */
        ut_ad(blocking_trx);
        /* We will only consider granting the `lock`, if we are the reason it
        was waiting. */
        if (blocking_trx != in_trx) {
          return (true);
        }
        if (trx_is_high_priority(trx)) {
          waiting.push_back(lock);
          return (true);
        }
        /* The values of schedule_weight are read with memory_order_relaxed as
        we do not care neither about having the most recent value, nor about any
        relative order between this load and other operations.
        As std::sort requires the order to be consistent during execution we
        have to take a snapshot of all schedule_weight atomics, so they don't
        change during call to stable_sort in a way which causes the algorithm to
        crash. */
        const auto schedule_weight =
            trx->lock.schedule_weight.load(std::memory_order_relaxed);
        if (schedule_weight <= 1) {
          low_priority_light.push_back(lock);
          return (true);
        }
        low_priority_heavier.push_back(LockDescriptorEx{schedule_weight, lock});

        return (true);
      },
      hash_table);

  if (waiting.empty() && low_priority_light.empty() &&
      low_priority_heavier.empty()) {
    /* Nothing to grant. */
    return;
  }
  /* We want high schedule weight to be in front, and break ties by position */
  std::stable_sort(low_priority_heavier.begin(), low_priority_heavier.end(),
                   [](const LockDescriptorEx &a, const LockDescriptorEx &b) {
                     return (a.first > b.first);
                   });
  for (const auto &descriptor : low_priority_heavier) {
    waiting.push_back(descriptor.second);
  }
  waiting.insert(waiting.end(), low_priority_light.begin(),
                 low_priority_light.end());

  /* New granted locks will be added from this index. */
  const auto new_granted_index = granted.size();

  granted.reserve(granted.size() + waiting.size());

  for (lock_t *wait_lock : waiting) {
    /* Check if the transactions in the waiting queue have
    to wait for locks granted above. If they don't have to
    wait then grant them the locks and add them to the granted
    queue. */

    /* We don't expect to be a waiting trx, and we can't grant to ourselves as
    that would require entering trx->mutex while holding in_trx->mutex. */
    ut_ad(wait_lock->trx != in_trx);

    const lock_t *blocking_lock =
        lock_rec_has_to_wait_for_granted(wait_lock, granted, new_granted_index);
    if (blocking_lock == nullptr) {
      lock_grant(wait_lock);

      lock_rec_move_granted_to_front(wait_lock, rec_id);

      granted.push_back(wait_lock);
    } else {
      lock_update_wait_for_edge(wait_lock, blocking_lock);
    }
  }
}

/* Forward declaration to minimize the diff */
static const lock_t *lock_has_to_wait_in_queue(const lock_t *wait_lock,
                                               const trx_t *blocking_trx);

/** Given a lock, which was found in waiting queue, checks if it still has to
wait in queue, and either grants it, or makes sure that the reason it has to
wait is reflected in the wait-for graph.
@param[in]  lock  A lock in WAITING state, which perhaps can be granted now */
static void lock_grant_or_update_wait_for_edge(lock_t *lock) {
  ut_ad(lock->is_waiting());
  const lock_t *blocking_lock = lock_has_to_wait_in_queue(lock, nullptr);
  if (blocking_lock == nullptr) {
    /* Grant the lock */
    lock_grant(lock);
  } else {
    ut_ad(lock->trx != blocking_lock->trx);
    lock_update_wait_for_edge(lock, blocking_lock);
  }
}

/** Given a lock, and a transaction which is releasing another lock from the
same queue, makes sure that if the lock was waiting for this transaction, then
it will either be granted, or another reason for waiting is reflected in the
wait-for graph. */
static void lock_grant_or_update_wait_for_edge_if_waiting(
    lock_t *lock, const trx_t *releasing_trx) {
  if (lock->is_waiting() && lock->trx->lock.blocking_trx == releasing_trx) {
    ut_ad(lock->trx != releasing_trx);
    lock_grant_or_update_wait_for_edge(lock);
  }
}

/** Grant lock to waiting requests that no longer conflicts.
The in_lock might be modified before call to this function by clearing some flag
(see for example lock_trx_release_read_locks). It also might already be removed
from the hash cell (a.k.a. waiting queue) or still reside in it. However the
content of bitmap should not be changed prior to calling this function, as the
bitmap will be inspected to see which heap_no at all were blocked by this
in_lock, and only locks waiting for those heap_no's will be checked.
@param[in,out]  in_lock         record lock object: grant all non-conflicting
                          locks waiting behind this lock object */
static void lock_rec_grant(lock_t *in_lock) {
  const auto page_id = in_lock->rec_lock.page_id;
  auto lock_hash = in_lock->hash_table();

  /* In some scenarios, in particular in replication appliers, it is often the
  case, that there are no WAITING locks, and in such situation iterating over
  all bits, and calling lock_rec_grant_by_heap_no() slows down the execution
  noticeably. (I guess that checking bits is not the costly part, but rather the
  allocation of vectors inside lock_rec_grant_by_heap_no). Therefore we first
  check if there is any lock which is waiting at all.
  Note: This condition could be further narrowed to check if the `lock` is
  waiting for the `in_lock` and/or `lock->trx` is blocked by the `in_lock->trx`,
  and we could optimize lock_rec_grant_by_heap_no() to allocate vectors only if
  there are at least two waiters to arbitrate among, but in practice the current
  simple heuristic is good enough. */
  bool found_waiter = false;
  for (auto lock = lock_rec_get_first_on_page_addr(lock_hash, page_id);
       lock != nullptr; lock = lock_rec_get_next_on_page(lock)) {
    if (lock->is_waiting()) {
      found_waiter = true;
      break;
    }
  }
  if (found_waiter) {
    mon_type_t grant_attempts = 0;
    for (ulint heap_no = 0; heap_no < lock_rec_get_n_bits(in_lock); ++heap_no) {
      if (lock_rec_get_nth_bit(in_lock, heap_no)) {
        lock_rec_grant_by_heap_no(in_lock, heap_no);
        ++grant_attempts;
      }
    }
    MONITOR_INC_VALUE(MONITOR_RECLOCK_GRANT_ATTEMPTS, grant_attempts);
  }
  MONITOR_INC(MONITOR_RECLOCK_RELEASE_ATTEMPTS);
}

/** Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed.
@param[in,out]  in_lock         record lock object: all record locks which
                                are contained in this lock object are removed;
                                transactions waiting behind will get their
                                lock requests granted, if they are now
                                qualified to it */
static void lock_rec_dequeue_from_page(lock_t *in_lock) {
  lock_rec_discard(in_lock);
  lock_rec_grant(in_lock);
}

/** Removes a record lock request, waiting or granted, from the queue.
@param[in]      in_lock         record lock object: all record locks
                                which are contained in this lock object
                                are removed */
void lock_rec_discard(lock_t *in_lock) {
  ut_ad(lock_get_type_low(in_lock) == LOCK_REC);
  const auto page_id = in_lock->rec_lock.page_id;
  ut_ad(locksys::owns_page_shard(page_id));

  ut_ad(in_lock->index->table->n_rec_locks.load() > 0);
  in_lock->index->table->n_rec_locks.fetch_sub(1, std::memory_order_relaxed);

  /* We want the state of lock queue and trx_locks list to be synchronized
  atomically from the point of view of people using trx->mutex, so we perform
  HASH_DELETE and UT_LIST_REMOVE while holding trx->mutex. */

  ut_ad(trx_mutex_own(in_lock->trx));

  locksys::remove_from_trx_locks(in_lock);

  HASH_DELETE(lock_t, hash, lock_hash_get(in_lock->type_mode),
              lock_rec_hash_value(page_id), in_lock);

  MONITOR_INC(MONITOR_RECLOCK_REMOVED);
  MONITOR_DEC(MONITOR_NUM_RECLOCK);
}

/** Removes record lock objects set on an index page which is discarded. This
 function does not move locks, or check for waiting locks, therefore the
 lock bitmaps must already be reset when this function is called. */
static void lock_rec_free_all_from_discard_page_low(page_id_t page_id,
                                                    hash_table_t *lock_hash) {
  lock_t *lock;
  lock_t *next_lock;

  lock = lock_rec_get_first_on_page_addr(lock_hash, page_id);

  while (lock != nullptr) {
    ut_ad(lock_rec_find_set_bit(lock) == ULINT_UNDEFINED);
    ut_ad(!lock_get_wait(lock));

    next_lock = lock_rec_get_next_on_page(lock);

    trx_t *trx = lock->trx;
    trx_mutex_enter(trx);
    lock_rec_discard(lock);
    trx_mutex_exit(trx);

    lock = next_lock;
  }
}

/** Removes record lock objects set on an index page which is discarded. This
 function does not move locks, or check for waiting locks, therefore the
 lock bitmaps must already be reset when this function is called. */
void lock_rec_free_all_from_discard_page(
    const buf_block_t *block) /*!< in: page to be discarded */
{
  const auto page_id = block->get_page_id();
  ut_ad(locksys::owns_page_shard(page_id));

  lock_rec_free_all_from_discard_page_low(page_id, lock_sys->rec_hash);
  lock_rec_free_all_from_discard_page_low(page_id, lock_sys->prdt_hash);
  lock_rec_free_all_from_discard_page_low(page_id, lock_sys->prdt_page_hash);
}

/*============= RECORD LOCK MOVING AND INHERITING ===================*/

/** Resets the lock bits for a single record. Releases transactions waiting for
 lock requests here. */
static void lock_rec_reset_and_release_wait_low(
    hash_table_t *hash,       /*!< in: hash table */
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    ulint heap_no)            /*!< in: heap number of record */
{
  lock_t *lock;

  ut_ad(locksys::owns_page_shard(block->get_page_id()));

  for (lock = lock_rec_get_first(hash, block, heap_no); lock != nullptr;
       lock = lock_rec_get_next(heap_no, lock)) {
    if (lock_get_wait(lock)) {
      lock_rec_cancel(lock);
    } else {
      lock_rec_reset_nth_bit(lock, heap_no);
    }
  }
}

/** Resets the lock bits for a single record. Releases transactions waiting for
 lock requests here. */
static void lock_rec_reset_and_release_wait(
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    ulint heap_no)            /*!< in: heap number of record */
{
  lock_rec_reset_and_release_wait_low(lock_sys->rec_hash, block, heap_no);

  lock_rec_reset_and_release_wait_low(lock_sys->prdt_hash, block,
                                      PAGE_HEAP_NO_INFIMUM);
  lock_rec_reset_and_release_wait_low(lock_sys->prdt_page_hash, block,
                                      PAGE_HEAP_NO_INFIMUM);
}

void lock_on_statement_end(trx_t *trx) { trx->lock.inherit_all.store(false); }

/* Used to store information that `thr` requested a lock asking for protection
at least till the end of the current statement which requires it to be inherited
as gap locks even in READ COMMITTED isolation level.
@param[in]  thr     the requesting thread */
static inline void lock_protect_locks_till_statement_end(que_thr_t *thr) {
  thr_get_trx(thr)->lock.inherit_all.store(true);
}

/** Makes a record to inherit the locks (except LOCK_INSERT_INTENTION type)
 of another record as gap type locks, but does not reset the lock bits of
 the other record. Also waiting lock requests on rec are inherited as
 GRANTED gap locks. */
static void lock_rec_inherit_to_gap(
    const buf_block_t *heir_block, /*!< in: block containing the
                                   record which inherits */
    const buf_block_t *block,      /*!< in: block containing the
                                   record from which inherited;
                                   does NOT reset the locks on
                                   this record */
    ulint heir_heap_no,            /*!< in: heap_no of the
                                   inheriting record */
    ulint heap_no)                 /*!< in: heap_no of the
                                   donating record */
{
  lock_t *lock;

  ut_ad(locksys::owns_page_shard(heir_block->get_page_id()));
  ut_ad(locksys::owns_page_shard(block->get_page_id()));

  /* If session is using READ COMMITTED or READ UNCOMMITTED isolation
  level, we do not want locks set by an UPDATE or a DELETE to be
  inherited as gap type locks.  But we DO want S-locks/X-locks(taken for
  replace) set by a consistency constraint to be inherited also then. */

  /* We also dont inherit these locks as gap type locks for DD tables
  because the serialization is guaranteed by MDL on DD tables. */

  /* Constraint checks place LOCK_S or (in case of INSERT ... ON DUPLICATE
  UPDATE... or REPLACE INTO..) LOCK_X on records.
  If such a record is delete-marked, it may then become purged, and
  lock_rec_inheirt_to_gap will be called to decide the fate of each lock on it:
  either it will be inherited as gap lock, or discarded.
  In READ COMMITTED and less restricitve isolation levels we generally avoid gap
  locks, but we make an exception for precisely this situation: we want to
  inherit locks created for constraint checks.
  More precisely we need to keep inheriting them only for the duration of the
  query which has requested them, as such inserts have two phases : first they
  check for constraints, then they do actual row insert, and they trust that
  the locks set in the first phase will survive till the second phase.
  It is not easy to tell if a particular lock was created for constraint check
  or not, because we do not store this bit of information on it.
  What we do, is we use a heuristic: whenever a trx requests a lock with
  lock_duration_t::AT_LEAST_STATEMENT we set trx->lock.inherit_all, meaning that
  locks of this trx need to be inherited.
  And we clear trx->lock.inherit_all on statement end. */

  for (lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);
       lock != nullptr; lock = lock_rec_get_next(heap_no, lock)) {
    /* Skip inheriting lock if set */
    if (lock->trx->skip_lock_inheritance) {
      continue;
    }

    if (!lock_rec_get_insert_intention(lock) &&
        !lock->index->table->skip_gap_locks() &&
        (!lock->trx->skip_gap_locks() || lock->trx->lock.inherit_all.load())) {
      lock_rec_add_to_queue(LOCK_REC | LOCK_GAP | lock_get_mode(lock),
                            heir_block, heir_heap_no, lock->index, lock->trx);
    }
  }
}

/** Makes a record to inherit the gap locks (except LOCK_INSERT_INTENTION type)
 of another record as gap type locks, but does not reset the lock bits of the
 other record. Also waiting lock requests are inherited as GRANTED gap locks. */
static void lock_rec_inherit_to_gap_if_gap_lock(
    const buf_block_t *block, /*!< in: buffer block */
    ulint heir_heap_no,       /*!< in: heap_no of
                              record which inherits */
    ulint heap_no)            /*!< in: heap_no of record
                              from which inherited;
                              does NOT reset the locks
                              on this record */
{
  lock_t *lock;

  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

  for (lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);
       lock != nullptr; lock = lock_rec_get_next(heap_no, lock)) {
    /* Skip inheriting lock if set */
    if (lock->trx->skip_lock_inheritance) {
      continue;
    }

    if (!lock_rec_get_insert_intention(lock) &&
        (heap_no == PAGE_HEAP_NO_SUPREMUM || !lock_rec_get_rec_not_gap(lock))) {
      lock_rec_add_to_queue(LOCK_REC | LOCK_GAP | lock_get_mode(lock), block,
                            heir_heap_no, lock->index, lock->trx);
    }
  }
}

/** Moves the locks of a record to another record and resets the lock bits of
 the donating record. */
static void lock_rec_move_low(
    hash_table_t *lock_hash,     /*!< in: hash table to use */
    const buf_block_t *receiver, /*!< in: buffer block containing
                                 the receiving record */
    const buf_block_t *donator,  /*!< in: buffer block containing
                                 the donating record */
    ulint receiver_heap_no,      /*!< in: heap_no of the record
                                which gets the locks; there
                                must be no lock requests
                                on it! */
    ulint donator_heap_no)       /*!< in: heap_no of the record
                                 which gives the locks */
{
  lock_t *lock;

  ut_ad(locksys::owns_page_shard(receiver->get_page_id()));
  ut_ad(locksys::owns_page_shard(donator->get_page_id()));

  /* If the lock is predicate lock, it resides on INFIMUM record */
  ut_ad(lock_rec_get_first(lock_hash, receiver, receiver_heap_no) == nullptr ||
        lock_hash == lock_sys->prdt_hash ||
        lock_hash == lock_sys->prdt_page_hash);

  for (lock = lock_rec_get_first(lock_hash, donator, donator_heap_no);
       lock != nullptr; lock = lock_rec_get_next(donator_heap_no, lock)) {
    const ulint type_mode = lock->type_mode;

    lock_rec_reset_nth_bit(lock, donator_heap_no);

    if (type_mode & LOCK_WAIT) {
      lock_reset_lock_and_trx_wait(lock);
    }

    /* Note that we FIRST reset the bit, and then set the lock:
    the function works also if donator == receiver */

    lock_rec_add_to_queue(type_mode, receiver, receiver_heap_no, lock->index,
                          lock->trx);
  }

  ut_ad(lock_rec_get_first(lock_sys->rec_hash, donator, donator_heap_no) ==
        nullptr);
}

/** Move all the granted locks to the front of the given lock list.
All the waiting locks will be at the end of the list.
@param[in,out]  lock_list       the given lock list.  */
static void lock_move_granted_locks_to_front(trx_lock_list_t &lock_list) {
  bool seen_waiting_lock = false;
  /* Note: We need iterator to removable container, as the ut_list_move_to_front
  effectively removes the current element as part of its operation. */
  for (auto lock : lock_list.removable()) {
    if (!seen_waiting_lock) {
      if (lock->is_waiting()) {
        seen_waiting_lock = true;
      }
      continue;
    }

    ut_ad(seen_waiting_lock);

    if (!lock->is_waiting()) {
      ut_list_move_to_front(lock_list, lock);
    }
  }
}

/** Moves the locks of a record to another record and resets the lock bits of
 the donating record. */
static inline void lock_rec_move(
    const buf_block_t *receiver, /*!< in: buffer block containing
                                 the receiving record */
    const buf_block_t *donator,  /*!< in: buffer block containing
                                 the donating record */
    ulint receiver_heap_no,      /*!< in: heap_no of the record
                        which gets the locks; there
                                must be no lock requests
                                on it! */
    ulint donator_heap_no)       /*!< in: heap_no of the record
                                 which gives the locks */
{
  lock_rec_move_low(lock_sys->rec_hash, receiver, donator, receiver_heap_no,
                    donator_heap_no);
}

/** Updates the lock table when we have reorganized a page. NOTE: we copy
 also the locks set on the infimum of the page; the infimum may carry
 locks if an update of a record is occurring on the page, and its locks
 were temporarily stored on the infimum. */
void lock_move_reorganize_page(
    const buf_block_t *block,  /*!< in: old index page, now
                               reorganized */
    const buf_block_t *oblock) /*!< in: copy of the old, not
                               reorganized page */
{
  lock_t *lock;
  trx_lock_list_t old_locks;
  mem_heap_t *heap = nullptr;
  {
    /* We only process locks on block, not oblock */
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

    /* FIXME: This needs to deal with predicate lock too */
    lock = lock_rec_get_first_on_page(lock_sys->rec_hash, block);

    if (lock == nullptr) {
      return;
    }

    heap = mem_heap_create(256, UT_LOCATION_HERE);

    /* Copy first all the locks on the page to heap and reset the
    bitmaps in the original locks; chain the copies of the locks
    using the trx_locks field in them. */

    do {
      /* Make a copy of the lock */
      lock_t *old_lock = lock_rec_copy(lock, heap);

      UT_LIST_ADD_LAST(old_locks, old_lock);

      /* Reset bitmap of lock */
      lock_rec_bitmap_reset(lock);

      if (lock_get_wait(lock)) {
        lock_reset_lock_and_trx_wait(lock);
      }

      lock = lock_rec_get_next_on_page(lock);
    } while (lock != nullptr);

    auto comp = page_is_comp(block->frame);
    ut_ad(comp == page_is_comp(oblock->frame));

    lock_move_granted_locks_to_front(old_locks);

    DBUG_EXECUTE_IF("do_lock_reverse_page_reorganize",
                    UT_LIST_REVERSE(old_locks););

    for (auto lock : old_locks) {
      /* NOTE: we copy also the locks set on the infimum and
      supremum of the page; the infimum may carry locks if an
      update of a record is occurring on the page, and its locks
      were temporarily stored on the infimum */
      const rec_t *rec1 = page_get_infimum_rec(buf_block_get_frame(block));
      const rec_t *rec2 = page_get_infimum_rec(buf_block_get_frame(oblock));

      /* Set locks according to old locks */
      for (;;) {
        ulint old_heap_no;
        ulint new_heap_no;

        if (comp) {
          old_heap_no = rec_get_heap_no_new(rec2);
          new_heap_no = rec_get_heap_no_new(rec1);

          rec1 = page_rec_get_next_low(rec1, true);
          rec2 = page_rec_get_next_low(rec2, true);
        } else {
          old_heap_no = rec_get_heap_no_old(rec2);
          new_heap_no = rec_get_heap_no_old(rec1);
          ut_ad(!memcmp(rec1, rec2, rec_get_data_size_old(rec2)));

          rec1 = page_rec_get_next_low(rec1, false);
          rec2 = page_rec_get_next_low(rec2, false);
        }

        /* Clear the bit in old_lock. */
        if (old_heap_no < lock->rec_lock.n_bits &&
            lock_rec_reset_nth_bit(lock, old_heap_no)) {
          /* NOTE that the old lock bitmap could be too
          small for the new heap number! */

          lock_rec_add_to_queue(lock->type_mode, block, new_heap_no,
                                lock->index, lock->trx);
        }

        if (new_heap_no == PAGE_HEAP_NO_SUPREMUM) {
          ut_ad(old_heap_no == PAGE_HEAP_NO_SUPREMUM);
          break;
        }
      }

      ut_ad(lock_rec_find_set_bit(lock) == ULINT_UNDEFINED);
    }
  } /* Shard_latch_guard */

  mem_heap_free(heap);

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
#endif /* UNIV_DEBUG_LOCK_VALIDATE */
}

/** Moves the explicit locks on user records to another page if a record
 list end is moved to another page.
@param[in] new_block Index page to move to
@param[in] block Index page
@param[in,out] rec Record on page: this is the first record moved */
void lock_move_rec_list_end(const buf_block_t *new_block,
                            const buf_block_t *block, const rec_t *rec) {
  lock_t *lock;
  const auto comp = page_rec_is_comp(rec);

  ut_ad(buf_block_get_frame(block) == page_align(rec));
  ut_ad(comp == page_is_comp(buf_block_get_frame(new_block)));

  {
    locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *block, *new_block};

    for (lock = lock_rec_get_first_on_page(lock_sys->rec_hash, block); lock;
         lock = lock_rec_get_next_on_page(lock)) {
      const rec_t *rec1 = rec;
      const rec_t *rec2;
      const ulint type_mode = lock->type_mode;

      if (comp) {
        if (page_offset(rec1) == PAGE_NEW_INFIMUM) {
          rec1 = page_rec_get_next_low(rec1, true);
        }

        rec2 = page_rec_get_next_low(
            buf_block_get_frame(new_block) + PAGE_NEW_INFIMUM, true);
      } else {
        if (page_offset(rec1) == PAGE_OLD_INFIMUM) {
          rec1 = page_rec_get_next_low(rec1, false);
        }

        rec2 = page_rec_get_next_low(
            buf_block_get_frame(new_block) + PAGE_OLD_INFIMUM, false);
      }

      /* Copy lock requests on user records to new page and
      reset the lock bits on the old */

      for (;;) {
        ulint rec1_heap_no;
        ulint rec2_heap_no;

        if (comp) {
          rec1_heap_no = rec_get_heap_no_new(rec1);

          if (rec1_heap_no == PAGE_HEAP_NO_SUPREMUM) {
            break;
          }

          rec2_heap_no = rec_get_heap_no_new(rec2);
          rec1 = page_rec_get_next_low(rec1, true);
          rec2 = page_rec_get_next_low(rec2, true);
        } else {
          rec1_heap_no = rec_get_heap_no_old(rec1);

          if (rec1_heap_no == PAGE_HEAP_NO_SUPREMUM) {
            break;
          }

          rec2_heap_no = rec_get_heap_no_old(rec2);

          ut_ad(!memcmp(rec1, rec2, rec_get_data_size_old(rec2)));

          rec1 = page_rec_get_next_low(rec1, false);
          rec2 = page_rec_get_next_low(rec2, false);
        }

        if (rec1_heap_no < lock->rec_lock.n_bits &&
            lock_rec_reset_nth_bit(lock, rec1_heap_no)) {
          if (type_mode & LOCK_WAIT) {
            lock_reset_lock_and_trx_wait(lock);
          }

          lock_rec_add_to_queue(type_mode, new_block, rec2_heap_no, lock->index,
                                lock->trx);
        }
      }
    }
  } /* Shard_latches_guard */

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
  ut_ad(lock_rec_validate_page(new_block));
#endif /* UNIV_DEBUG_LOCK_VALIDATE */
}

/** Moves the explicit locks on user records to another page if a record
 list start is moved to another page.
@param[in] new_block Index page to move to
@param[in] block Index page
@param[in,out] rec Record on page: this is the first record not copied
@param[in] old_end Old previous-to-last record on new_page before the records
were copied */
void lock_move_rec_list_start(const buf_block_t *new_block,
                              const buf_block_t *block, const rec_t *rec,
                              const rec_t *old_end) {
  lock_t *lock;
  const auto comp = page_rec_is_comp(rec);

  ut_ad(block->frame == page_align(rec));
  ut_ad(new_block->frame == page_align(old_end));
  ut_ad(comp == page_rec_is_comp(old_end));

  {
    locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *block, *new_block};

    for (lock = lock_rec_get_first_on_page(lock_sys->rec_hash, block); lock;
         lock = lock_rec_get_next_on_page(lock)) {
      const rec_t *rec1;
      const rec_t *rec2;
      const ulint type_mode = lock->type_mode;

      if (comp) {
        rec1 = page_rec_get_next_low(
            buf_block_get_frame(block) + PAGE_NEW_INFIMUM, true);
        rec2 = page_rec_get_next_low(old_end, true);
      } else {
        rec1 = page_rec_get_next_low(
            buf_block_get_frame(block) + PAGE_OLD_INFIMUM, false);
        rec2 = page_rec_get_next_low(old_end, false);
      }

      /* Copy lock requests on user records to new page and
      reset the lock bits on the old */

      while (rec1 != rec) {
        ulint rec1_heap_no;
        ulint rec2_heap_no;

        if (comp) {
          rec1_heap_no = rec_get_heap_no_new(rec1);
          rec2_heap_no = rec_get_heap_no_new(rec2);

          rec1 = page_rec_get_next_low(rec1, true);
          rec2 = page_rec_get_next_low(rec2, true);
        } else {
          rec1_heap_no = rec_get_heap_no_old(rec1);
          rec2_heap_no = rec_get_heap_no_old(rec2);

          ut_ad(!memcmp(rec1, rec2, rec_get_data_size_old(rec2)));

          rec1 = page_rec_get_next_low(rec1, false);
          rec2 = page_rec_get_next_low(rec2, false);
        }

        if (rec1_heap_no < lock->rec_lock.n_bits &&
            lock_rec_reset_nth_bit(lock, rec1_heap_no)) {
          if (type_mode & LOCK_WAIT) {
            lock_reset_lock_and_trx_wait(lock);
          }

          lock_rec_add_to_queue(type_mode, new_block, rec2_heap_no, lock->index,
                                lock->trx);
        }
      }

#ifdef UNIV_DEBUG
      if (page_rec_is_supremum(rec)) {
        ulint i;

        for (i = PAGE_HEAP_NO_USER_LOW; i < lock_rec_get_n_bits(lock); i++) {
          ut_a(!lock_rec_get_nth_bit(lock, i));
        }
      }
#endif /* UNIV_DEBUG */
    }
  } /* Shard_latches_guard */

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
#endif /* UNIV_DEBUG_LOCK_VALIDATE */
}

/** Moves the explicit locks on user records to another page if a record
 list start is moved to another page.
@param[in] new_block Index page to move to
@param[in] block Index page
@param[in] rec_move Recording records moved
@param[in] num_move Num of rec to move */
void lock_rtr_move_rec_list(const buf_block_t *new_block,
                            const buf_block_t *block, rtr_rec_move_t *rec_move,
                            ulint num_move) {
  lock_t *lock;

  if (!num_move) {
    return;
  }

  auto comp = page_rec_is_comp(rec_move[0].old_rec);

  ut_ad(block->frame == page_align(rec_move[0].old_rec));
  ut_ad(new_block->frame == page_align(rec_move[0].new_rec));
  ut_ad(comp == page_rec_is_comp(rec_move[0].new_rec));

  {
    locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *new_block, *block};

    for (lock = lock_rec_get_first_on_page(lock_sys->rec_hash, block); lock;
         lock = lock_rec_get_next_on_page(lock)) {
      ulint moved = 0;
      const rec_t *rec1;
      const rec_t *rec2;
      const ulint type_mode = lock->type_mode;

      /* Copy lock requests on user records to new page and
      reset the lock bits on the old */

      while (moved < num_move) {
        ulint rec1_heap_no;
        ulint rec2_heap_no;

        rec1 = rec_move[moved].old_rec;
        rec2 = rec_move[moved].new_rec;

        if (comp) {
          rec1_heap_no = rec_get_heap_no_new(rec1);
          rec2_heap_no = rec_get_heap_no_new(rec2);

        } else {
          rec1_heap_no = rec_get_heap_no_old(rec1);
          rec2_heap_no = rec_get_heap_no_old(rec2);

          ut_ad(!memcmp(rec1, rec2, rec_get_data_size_old(rec2)));
        }

        if (rec1_heap_no < lock->rec_lock.n_bits &&
            lock_rec_reset_nth_bit(lock, rec1_heap_no)) {
          if (type_mode & LOCK_WAIT) {
            lock_reset_lock_and_trx_wait(lock);
          }

          lock_rec_add_to_queue(type_mode, new_block, rec2_heap_no, lock->index,
                                lock->trx);

          rec_move[moved].moved = true;
        }

        moved++;
      }
    }
  } /* Shard_latches_guard */

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
#endif
}

/** Updates the lock table when a page is split to the right.
@param[in] right_block Right page
@param[in] left_block Left page */
void lock_update_split_right(const buf_block_t *right_block,
                             const buf_block_t *left_block) {
  const auto heap_no = lock_get_min_heap_no(right_block);

  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *left_block,
                                     *right_block};

  /* Move the locks on the supremum of the left page to the supremum
  of the right page */

  lock_rec_move(right_block, left_block, PAGE_HEAP_NO_SUPREMUM,
                PAGE_HEAP_NO_SUPREMUM);

  /* Inherit the locks to the supremum of left page from the successor
  of the infimum on right page */

  lock_rec_inherit_to_gap(left_block, right_block, PAGE_HEAP_NO_SUPREMUM,
                          heap_no);
}

/** Updates the lock table when a page is merged to the right.
@param[in] right_block Right page to which merged
@param[in] orig_succ Original successor of infimum on the right page before
merge
@param[in] left_block Merged index page which will be discarded */
void lock_update_merge_right(const buf_block_t *right_block,
                             const rec_t *orig_succ,
                             const buf_block_t *left_block) {
  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *left_block,
                                     *right_block};

  /* Inherit the locks from the supremum of the left page to the original
  successor of infimum on the right page, to which the left page was merged. */

  lock_rec_inherit_to_gap(right_block, left_block,
                          page_rec_get_heap_no(orig_succ),
                          PAGE_HEAP_NO_SUPREMUM);

  /* Reset the locks on the supremum of the left page, releasing waiting
  transactions. */

  lock_rec_reset_and_release_wait_low(lock_sys->rec_hash, left_block,
                                      PAGE_HEAP_NO_SUPREMUM);

  /* There should exist no page lock on the left page, otherwise, it will be
  blocked from merge. */
  ut_ad(lock_rec_get_first_on_page_addr(lock_sys->prdt_page_hash,
                                        left_block->get_page_id()) == nullptr);

  lock_rec_free_all_from_discard_page(left_block);
}

/** Updates the lock table when the root page is copied to another in
 btr_root_raise_and_insert. Note that we leave lock structs on the
 root page, even though they do not make sense on other than leaf
 pages: the reason is that in a pessimistic update the infimum record
 of the root page will act as a dummy carrier of the locks of the record
 to be updated. */
void lock_update_root_raise(
    const buf_block_t *block, /*!< in: index page to which copied */
    const buf_block_t *root)  /*!< in: root page */
{
  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *block, *root};

  /* Move the locks on the supremum of the root to the supremum
  of block */

  lock_rec_move(block, root, PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
}

/** Updates the lock table when a page is copied to another and the original
 page is removed from the chain of leaf pages, except if page is the root!
@param[in] new_block Index page to which copied
@param[in] block Index page; not the root! */
void lock_update_copy_and_discard(const buf_block_t *new_block,
                                  const buf_block_t *block) {
  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *new_block, *block};

  /* Move the locks on the supremum of the old page to the supremum
  of new_page */

  lock_rec_move(new_block, block, PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
  lock_rec_free_all_from_discard_page(block);
}

void lock_update_split_point(const buf_block_t *right_block,
                             const buf_block_t *left_block) {
  const auto heap_no = lock_get_min_heap_no(right_block);

  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *left_block,
                                     *right_block};

  /* Inherit locks from the gap before supremum of the left page to the gap
  before the successor of the infimum of the right page */
  lock_rec_inherit_to_gap(right_block, left_block, heap_no,
                          PAGE_HEAP_NO_SUPREMUM);
}

/** Updates the lock table when a page is split to the left.
@param[in] right_block Right page
@param[in] left_block Left page */
void lock_update_split_left(const buf_block_t *right_block,
                            const buf_block_t *left_block) {
  const auto heap_no = lock_get_min_heap_no(right_block);

  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *left_block,
                                     *right_block};

  /* Inherit the locks to the supremum of the left page from the
  successor of the infimum on the right page */

  lock_rec_inherit_to_gap(left_block, right_block, PAGE_HEAP_NO_SUPREMUM,
                          heap_no);
}

/** Updates the lock table when a page is merged to the left.
@param[in] left_block Left page to which merged
@param[in] orig_pred Original predecessor of supremum on the left page before
merge
@param[in] right_block Merged index page which will be discarded */
void lock_update_merge_left(const buf_block_t *left_block,
                            const rec_t *orig_pred,
                            const buf_block_t *right_block) {
  const rec_t *left_next_rec;

  ut_ad(left_block->frame == page_align(orig_pred));

  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *left_block,
                                     *right_block};

  left_next_rec = page_rec_get_next_const(orig_pred);

  if (!page_rec_is_supremum(left_next_rec)) {
    /* Inherit the locks on the supremum of the left page to the
    first record which was moved from the right page */

    lock_rec_inherit_to_gap(left_block, left_block,
                            page_rec_get_heap_no(left_next_rec),
                            PAGE_HEAP_NO_SUPREMUM);

    /* Reset the locks on the supremum of the left page,
    releasing waiting transactions */

    lock_rec_reset_and_release_wait_low(lock_sys->rec_hash, left_block,
                                        PAGE_HEAP_NO_SUPREMUM);
  }

  /* Move the locks from the supremum of right page to the supremum
  of the left page */

  lock_rec_move(left_block, right_block, PAGE_HEAP_NO_SUPREMUM,
                PAGE_HEAP_NO_SUPREMUM);

  /* there should exist no page lock on the right page,
  otherwise, it will be blocked from merge */
  ut_ad(lock_rec_get_first_on_page_addr(lock_sys->prdt_page_hash,
                                        right_block->get_page_id()) == nullptr);

  lock_rec_free_all_from_discard_page(right_block);
}

/** Resets the original locks on heir and replaces them with gap type locks
 inherited from rec.
@param[in] heir_block Block containing the record which inherits
@param[in] block Block containing the record from which inherited; does not
reset the locks on this record
@param[in] heir_heap_no Heap_no of the inheriting record
@param[in] heap_no Heap_no of the donating record */
void lock_rec_reset_and_inherit_gap_locks(const buf_block_t *heir_block,
                                          const buf_block_t *block,
                                          ulint heir_heap_no, ulint heap_no) {
  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *heir_block, *block};

  lock_rec_reset_and_release_wait(heir_block, heir_heap_no);

  lock_rec_inherit_to_gap(heir_block, block, heir_heap_no, heap_no);
}

/** Updates the lock table when a page is discarded.
@param[in] heir_block Index page which will inherit the locks
@param[in] heir_heap_no Heap_no of the record which will inherit the locks
@param[in] block Index page which will be discarded */
void lock_update_discard(const buf_block_t *heir_block, ulint heir_heap_no,
                         const buf_block_t *block) {
  const rec_t *rec;
  ulint heap_no;
  const page_t *page = block->frame;

  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *heir_block, *block};

  if (!lock_rec_get_first_on_page(lock_sys->rec_hash, block) &&
      (!lock_rec_get_first_on_page(lock_sys->prdt_page_hash, block)) &&
      (!lock_rec_get_first_on_page(lock_sys->prdt_hash, block))) {
    /* No locks exist on page, nothing to do */

    return;
  }

  /* Inherit all the locks on the page to the record and reset all
  the locks on the page */

  if (page_is_comp(page)) {
    rec = page + PAGE_NEW_INFIMUM;

    do {
      heap_no = rec_get_heap_no_new(rec);

      lock_rec_inherit_to_gap(heir_block, block, heir_heap_no, heap_no);

      lock_rec_reset_and_release_wait(block, heap_no);

      rec = page + rec_get_next_offs(rec, true);
    } while (heap_no != PAGE_HEAP_NO_SUPREMUM);
  } else {
    rec = page + PAGE_OLD_INFIMUM;

    do {
      heap_no = rec_get_heap_no_old(rec);

      lock_rec_inherit_to_gap(heir_block, block, heir_heap_no, heap_no);

      lock_rec_reset_and_release_wait(block, heap_no);

      rec = page + rec_get_next_offs(rec, false);
    } while (heap_no != PAGE_HEAP_NO_SUPREMUM);
  }

  lock_rec_free_all_from_discard_page(block);
}

/** Updates the lock table when a new user record is inserted. */
void lock_update_insert(
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec)         /*!< in: the inserted record */
{
  ulint receiver_heap_no;
  ulint donator_heap_no;

  ut_ad(block->frame == page_align(rec));

  /* Inherit the gap-locking locks for rec, in gap mode, from the next
  record */

  if (page_rec_is_comp(rec)) {
    receiver_heap_no = rec_get_heap_no_new(rec);
    donator_heap_no = rec_get_heap_no_new(page_rec_get_next_low(rec, true));
  } else {
    receiver_heap_no = rec_get_heap_no_old(rec);
    donator_heap_no = rec_get_heap_no_old(page_rec_get_next_low(rec, false));
  }

  lock_rec_inherit_to_gap_if_gap_lock(block, receiver_heap_no, donator_heap_no);
}

/** Updates the lock table when a record is removed.
@param[in] block Buffer block containing rec
@param[in] rec The record to be removed */
void lock_update_delete(const buf_block_t *block, const rec_t *rec) {
  const page_t *page = block->frame;
  ulint heap_no;
  ulint next_heap_no;

  ut_ad(page == page_align(rec));

  if (page_is_comp(page)) {
    heap_no = rec_get_heap_no_new(rec);
    next_heap_no = rec_get_heap_no_new(page + rec_get_next_offs(rec, true));
  } else {
    heap_no = rec_get_heap_no_old(rec);
    next_heap_no = rec_get_heap_no_old(page + rec_get_next_offs(rec, false));
  }

  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

  /* Let the next record inherit the locks from rec, in gap mode */

  lock_rec_inherit_to_gap(block, block, next_heap_no, heap_no);

  /* Reset the lock bits on rec and release waiting transactions */

  lock_rec_reset_and_release_wait(block, heap_no);
}

/** Stores on the page infimum record the explicit locks of another record.
 This function is used to store the lock state of a record when it is
 updated and the size of the record changes in the update. The record
 is moved in such an update, perhaps to another page. The infimum record
 acts as a dummy carrier record, taking care of lock releases while the
 actual record is being moved. */
void lock_rec_store_on_page_infimum(
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec)         /*!< in: record whose lock state
                              is stored on the infimum
                              record of the same page; lock
                              bits are reset on the
                              record */
{
  ulint heap_no = page_rec_get_heap_no(rec);

  ut_ad(block->frame == page_align(rec));

  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

  lock_rec_move(block, block, PAGE_HEAP_NO_INFIMUM, heap_no);
}

/** Restores the state of explicit lock requests on a single record, where the
 state was stored on the infimum of the page.
@param[in] block Buffer block containing rec
@param[in] rec Record whose lock state is restored
@param[in] donator Page (rec is not necessarily on this page) whose infimum
stored the lock state; lock bits are reset on the infimum */
void lock_rec_restore_from_page_infimum(const buf_block_t *block,
                                        const rec_t *rec,
                                        const buf_block_t *donator) {
  DEBUG_SYNC_C("lock_rec_restore_from_page_infimum_will_latch");
  ulint heap_no = page_rec_get_heap_no(rec);

  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *block, *donator};

  lock_rec_move(block, donator, heap_no, PAGE_HEAP_NO_INFIMUM);
}

/*========================= TABLE LOCKS ==============================*/
struct TableLockGetNode {
  /** Functor for accessing the embedded node within a table lock. */
  static const ut_list_node<lock_t> &get_node(const lock_t &lock) {
    return lock.tab_lock.locks;
  }
};

/** Creates a table lock object and adds it as the last in the lock queue
 of the table. Does NOT check for deadlocks or lock compatibility.
 @return own: new lock object */
static inline lock_t *lock_table_create(
    dict_table_t *table, /*!< in/out: database table
                         in dictionary cache */
    ulint type_mode,     /*!< in: lock mode possibly ORed with
                       LOCK_WAIT */
    trx_t *trx)          /*!< in: trx */
{
  lock_t *lock;

  ut_ad(table && trx);
  ut_ad(locksys::owns_table_shard(*table));
  ut_ad(trx_mutex_own(trx));
  ut_ad(trx_can_be_handled_by_current_thread(trx));

  check_trx_state(trx);
  ++table->count_by_mode[type_mode & LOCK_MODE_MASK];
  /* For AUTOINC locking we reuse the lock instance only if
  there is no wait involved else we allocate the waiting lock
  from the transaction lock heap. */
  if (type_mode == LOCK_AUTO_INC) {
    lock = table->autoinc_lock;
    ut_ad(table->autoinc_trx == nullptr);
    table->autoinc_trx = trx;

    ib_vector_push(trx->lock.autoinc_locks, &lock);

  } else if (trx->lock.table_cached < trx->lock.table_pool.size()) {
    lock = trx->lock.table_pool[trx->lock.table_cached++];
  } else {
    auto ptr = mem_heap_alloc(trx->lock.lock_heap, sizeof(*lock));
    ut_a(ut::is_aligned_as<lock_t>(ptr));
    lock = static_cast<lock_t *>(ptr);
  }
  lock->type_mode = uint32_t(type_mode | LOCK_TABLE);
  lock->trx = trx;
  ut_d(lock->m_seq = lock_sys->m_seq.fetch_add(1));

  lock->tab_lock.table = table;

  ut_ad(table->n_ref_count > 0 || !table->can_be_evicted);

#ifdef HAVE_PSI_THREAD_INTERFACE
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  /* The performance schema THREAD_ID and EVENT_ID
  are used only when DATA_LOCKS are exposed.  */
  PSI_THREAD_CALL(get_current_thread_event_id)
  (&lock->m_psi_internal_thread_id, &lock->m_psi_event_id);
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
#endif /* HAVE_PSI_THREAD_INTERFACE */

  locksys::add_to_trx_locks(lock);

  ut_list_append(table->locks, lock);

  if (type_mode & LOCK_WAIT) {
    lock_set_lock_and_trx_wait(lock);
  }

  MONITOR_INC(MONITOR_TABLELOCK_CREATED);
  MONITOR_INC(MONITOR_NUM_TABLELOCK);

  return (lock);
}

/** Pops autoinc lock requests from the transaction's autoinc_locks. We
 handle the case where there are gaps in the array and they need to
 be popped off the stack. */
static inline void lock_table_pop_autoinc_locks(
    trx_t *trx) /*!< in/out: transaction that owns the AUTOINC locks */
{
  /* We will access and modify trx->lock.autoinc_locks so we need trx->mutex */
  ut_ad(trx_mutex_own(trx));
  ut_ad(!ib_vector_is_empty(trx->lock.autoinc_locks));

  /* Skip any gaps, gaps are NULL lock entries in the
  trx->autoinc_locks vector. */

  do {
    ib_vector_pop(trx->lock.autoinc_locks);

    if (ib_vector_is_empty(trx->lock.autoinc_locks)) {
      return;
    }

  } while (*(lock_t **)ib_vector_get_last(trx->lock.autoinc_locks) == nullptr);
}

/** Removes an autoinc lock request from the transaction's autoinc_locks. */
static inline void lock_table_remove_autoinc_lock(
    lock_t *lock, /*!< in: table lock */
    trx_t *trx)   /*!< in/out: transaction that owns the lock */
{
  /* We will access and modify trx->lock.autoinc_locks so we need trx->mutex */
  ut_ad(trx_mutex_own(trx));
  lock_t *autoinc_lock;
  lint i = ib_vector_size(trx->lock.autoinc_locks) - 1;

  ut_ad(lock_get_mode(lock) == LOCK_AUTO_INC);
  ut_ad(lock_get_type_low(lock) & LOCK_TABLE);
  ut_ad(locksys::owns_table_shard(*lock->tab_lock.table));
  ut_ad(!ib_vector_is_empty(trx->lock.autoinc_locks));

  /* With stored functions and procedures the user may drop
  a table within the same "statement". This special case has
  to be handled by deleting only those AUTOINC locks that were
  held by the table being dropped. */

  autoinc_lock =
      *static_cast<lock_t **>(ib_vector_get(trx->lock.autoinc_locks, i));

  /* This is the default fast case. */

  if (autoinc_lock == lock) {
    lock_table_pop_autoinc_locks(trx);
  } else {
    /* The last element should never be NULL */
    ut_a(autoinc_lock != nullptr);

    /* Handle freeing the locks from within the stack. */

    while (--i >= 0) {
      autoinc_lock =
          *static_cast<lock_t **>(ib_vector_get(trx->lock.autoinc_locks, i));

      if (autoinc_lock == lock) {
        void *null_var = nullptr;
        ib_vector_set(trx->lock.autoinc_locks, i, &null_var);
        return;
      }
    }

    /* Must find the autoinc lock. */
    ut_error;
  }
}

/** Removes a table lock request from the queue and the trx list of locks;
 this is a low-level function which does NOT check if waiting requests
 can now be granted. */
static inline void lock_table_remove_low(
    lock_t *lock) /*!< in/out: table lock */
{
  trx_t *trx;
  dict_table_t *table;

  trx = lock->trx;
  /* We will modify trx->lock.trx_locks so we need trx->mutex */
  ut_ad(trx_mutex_own(trx));
  table = lock->tab_lock.table;
  ut_ad(locksys::owns_table_shard(*table));
  const auto lock_mode = lock_get_mode(lock);
  /* Remove the table from the transaction's AUTOINC vector, if
  the lock that is being released is an AUTOINC lock. */
  if (lock_mode == LOCK_AUTO_INC) {
    /* The table's AUTOINC lock could not be granted to us yet. */
    ut_ad(table->autoinc_trx == trx || lock->is_waiting());
    if (table->autoinc_trx == trx) {
      table->autoinc_trx = nullptr;
    }

    /* The locks must be freed in the reverse order from
    the one in which they were acquired. This is to avoid
    traversing the AUTOINC lock vector unnecessarily.

    We only store locks that were granted in the
    trx->autoinc_locks vector (see lock_table_create()
    and lock_grant()). */

    if (!lock_get_wait(lock)) {
      lock_table_remove_autoinc_lock(lock, trx);
    }
  }
  ut_a(0 < table->count_by_mode[lock_mode]);
  --table->count_by_mode[lock_mode];

  locksys::remove_from_trx_locks(lock);

  ut_list_remove(table->locks, lock);

  MONITOR_INC(MONITOR_TABLELOCK_REMOVED);
  MONITOR_DEC(MONITOR_NUM_TABLELOCK);
}

/** Enqueues a waiting request for a table lock which cannot be granted
 immediately. Checks for deadlocks.
 @param[in] mode           lock mode this transaction is requesting
 @param[in] table          the table to be locked
 @param[in] thr            the query thread requesting the lock
 @param[in] blocking_lock  the lock which is the reason this request has to wait
 @return DB_LOCK_WAIT or DB_DEADLOCK */
static dberr_t lock_table_enqueue_waiting(ulint mode, dict_table_t *table,
                                          que_thr_t *thr,
                                          const lock_t *blocking_lock) {
  trx_t *trx;

  ut_ad(locksys::owns_table_shard(*table));
  ut_ad(!srv_read_only_mode);

  trx = thr_get_trx(thr);
  ut_ad(trx_mutex_own(trx));

  /* Test if there already is some other reason to suspend thread:
  we do not enqueue a lock request if the query thread should be
  stopped anyway */

  if (que_thr_stop(thr)) {
    ut_error;
  }

  switch (trx_get_dict_operation(trx)) {
    case TRX_DICT_OP_NONE:
      break;
    case TRX_DICT_OP_TABLE:
    case TRX_DICT_OP_INDEX:
      ib::error(ER_IB_MSG_642) << "A table lock wait happens in a dictionary"
                                  " operation. Table "
                               << table->name << ". " << BUG_REPORT_MSG;
      ut_d(ut_error);
  }

  if (trx->in_innodb & TRX_FORCE_ROLLBACK) {
    return (DB_DEADLOCK);
  }

  /* Enqueue the lock request that will wait to be granted */
  lock_t *lock = lock_table_create(table, mode | LOCK_WAIT, trx);

  trx->lock.que_state = TRX_QUE_LOCK_WAIT;

  trx->lock.wait_started =
      std::chrono::system_clock::from_time_t(time(nullptr));
  trx->lock.was_chosen_as_deadlock_victim = false;

  auto stopped = que_thr_stop(thr);
  ut_a(stopped);

  MONITOR_INC(MONITOR_TABLELOCK_WAIT);
  lock_create_wait_for_edge(lock, blocking_lock);
  return (DB_LOCK_WAIT);
}

/** Checks if other transactions have an incompatible mode lock request in
 the lock queue.
 @return lock or NULL */
static inline const lock_t *lock_table_other_has_incompatible(
    const trx_t *trx,          /*!< in: transaction, or NULL if all
                               transactions should be included */
    ulint wait,                /*!< in: LOCK_WAIT if also
                               waiting locks are taken into
                               account, or 0 if not */
    const dict_table_t *table, /*!< in: table */
    lock_mode mode)            /*!< in: lock mode */
{
  const lock_t *lock;

  ut_ad(locksys::owns_table_shard(*table));

  // According to lock_compatibility_matrix, an intention lock can wait only
  // for LOCK_S or LOCK_X. If there are no LOCK_S nor LOCK_X locks in the queue,
  // then we can avoid iterating through the list and return immediately.
  // This might help in OLTP scenarios, with no DDL queries,
  // as then there are almost no LOCK_S nor LOCK_X, but many DML queries still
  // need to get an intention lock to perform their action - while this never
  // causes them to wait for a "data lock", it might cause them to wait for
  // lock_sys table shard latch for the duration of table lock queue operation.

  if ((mode == LOCK_IS || mode == LOCK_IX) &&
      table->count_by_mode[LOCK_S] == 0 && table->count_by_mode[LOCK_X] == 0) {
    return nullptr;
  }

  for (lock = UT_LIST_GET_LAST(table->locks); lock != nullptr;
       lock = UT_LIST_GET_PREV(tab_lock.locks, lock)) {
    if (lock->trx != trx && !lock_mode_compatible(lock_get_mode(lock), mode) &&
        (wait || !lock_get_wait(lock))) {
      return (lock);
    }
  }

  return (nullptr);
}

/** Locks the specified database table in the mode given. If the lock cannot
 be granted immediately, the query thread is put to wait.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_table(ulint flags, /*!< in: if BTR_NO_LOCKING_FLAG bit is set,
                                does nothing */
                   dict_table_t *table, /*!< in/out: database table
                                        in dictionary cache */
                   lock_mode mode,      /*!< in: lock mode */
                   que_thr_t *thr)      /*!< in: query thread */
{
  trx_t *trx;
  dberr_t err;
  const lock_t *wait_for;

  ut_ad(table && thr);

  /* Given limited visibility of temp-table we can avoid
  locking overhead */
  if ((flags & BTR_NO_LOCKING_FLAG) || srv_read_only_mode ||
      table->is_temporary()) {
    return (DB_SUCCESS);
  }

  ut_a(flags == 0);

  trx = thr_get_trx(thr);

  /* Look for equal or stronger locks the same trx already has on the table.
  Even though lock_table_has() takes trx->mutex internally, it does not protect
  us at all from "higher-level" races - for instance the state could change in
  theory after we exit lock_table_has() and before we return DB_SUCCESS, or
  before somebody who called us reacts to the DB_SUCCESS.
  In theory table locks can be modified in:
    lock_release_autoinc_last_lock
      lock_release_autoinc_locks
        lock_cancel_waiting_and_release
          (this one seems to be called only when trx is waiting and not running)
        lock_unlock_table_autoinc
          (this one seems to be called from the thread running the transaction)
    lock_remove_all_on_table_for_trx
      lock_remove_all_on_table
        row_drop_table_for_mysql
          (this one is mysterious, as it is not obvious to me why do we expect
          that someone will drop a table while there are locks on it)
        row_mysql_table_id_reassign
          row_discard_tablespace
            (there is some long explanation starting with "How do we prevent
            crashes caused by ongoing operations...")
    lock_remove_recovered_trx_record_locks
      (this seems to be used to remove locks of recovered transactions from
      table being dropped, and recovered transactions shouldn't call lock_table)
  Also the InnoDB Memcached plugin causes a callchain:
  innodb_store -> innodb_conn_init -> innodb_api_begin -> innodb_cb_cursor_lock
  -> ib_cursor_set_lock_mode -> ib_cursor_lock -> ib_trx_lock_table_with_retry
  -> lock_table_for_trx -> lock_table -> lock_table_has
  in which lock_table_has sees trx->mysqld_thd different than current_thd.
  In practice this call to lock_table_has was never protected in any way before,
  so the situation now, after protecting it with trx->mutex, can't be worse. */

  if (lock_table_has(trx, table, mode)) {
    /* In Debug mode we assert the same condition again, to help catch cases of
    race condition, if it is possible at all, for further analysis. */
    ut_ad(lock_table_has(trx, table, mode));
    return (DB_SUCCESS);
  }

  /* Read only transactions can write to temp tables, we don't want
  to promote them to RW transactions. Their updates cannot be visible
  to other transactions. Therefore we can keep them out
  of the read views. */

  if ((mode == LOCK_IX || mode == LOCK_X) && !trx->read_only &&
      trx->rsegs.m_redo.rseg == nullptr) {
    trx_set_rw_mode(trx);
  }

  locksys::Shard_latch_guard table_latch_guard{UT_LOCATION_HERE, *table};

  /* We have to check if the new lock is compatible with any locks
  other transactions have in the table lock queue. */

  wait_for = lock_table_other_has_incompatible(trx, LOCK_WAIT, table, mode);

  trx_mutex_enter(trx);

  /* Another trx has a request on the table in an incompatible
  mode: this trx may have to wait */

  if (wait_for != nullptr) {
    err = lock_table_enqueue_waiting(mode | flags, table, thr, wait_for);
  } else {
    lock_table_create(table, mode | flags, trx);

    ut_a(!flags || mode == LOCK_S || mode == LOCK_X);

    err = DB_SUCCESS;
  }

  trx_mutex_exit(trx);

  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

/** Creates a table IX lock object for a resurrected transaction.
@param[in,out] table Table
@param[in,out] trx Transaction */
void lock_table_ix_resurrect(dict_table_t *table, trx_t *trx) {
  ut_ad(trx->is_recovered);

  if (lock_table_has(trx, table, LOCK_IX)) {
    return;
  }
  locksys::Shard_latch_guard table_latch_guard{UT_LOCATION_HERE, *table};
  /* We have to check if the new lock is compatible with any locks
  other transactions have in the table lock queue. */

  ut_ad(!lock_table_other_has_incompatible(trx, LOCK_WAIT, table, LOCK_IX));

  trx_mutex_enter(trx);
  lock_table_create(table, LOCK_IX, trx);
  trx_mutex_exit(trx);
}

/** Checks if a waiting table lock request still has to wait in a queue.
@param[in]  wait_lock     Waiting table lock
@param[in]  blocking_trx  If not nullptr, it restricts the search to only the
                          locks held by the blocking_trx, which is useful in
                          case when there might be multiple reasons for waiting
                          in queue, but we need to report the specific one.
                          Useful when reporting a deadlock cycle. (optional)
@return The conflicting lock which is the reason wait_lock has to wait
or nullptr if it can be granted now */
static const lock_t *lock_table_has_to_wait_in_queue(
    const lock_t *wait_lock, const trx_t *blocking_trx = nullptr) {
  const dict_table_t *table;

  ut_ad(lock_get_wait(wait_lock));

  table = wait_lock->tab_lock.table;
  ut_ad(locksys::owns_table_shard(*table));

  const auto mode = lock_get_mode(wait_lock);

  // According to lock_compatibility_matrix, an intention lock can wait only
  // for LOCK_S or LOCK_X. If there are no LOCK_S nor LOCK_X locks in the queue,
  // then we can avoid iterating through the list and return immediately.
  // This might help in OLTP scenarios, with no DDL queries,
  // as then there are almost no LOCK_S nor LOCK_X, but many DML queries still
  // need to get an intention lock to perform their action. When an occasional
  // DDL finishes and releases the LOCK_S or LOCK_X, it has to scan the queue
  // and grant any locks which were blocked by it. This can take Omega(n^2) if
  // each of intention locks has to verify that all the other locks.

  if ((mode == LOCK_IS || mode == LOCK_IX) &&
      table->count_by_mode[LOCK_S] == 0 && table->count_by_mode[LOCK_X] == 0) {
    return (nullptr);
  }
  for (auto lock : table->locks) {
    if (lock == wait_lock) break;
    if ((blocking_trx == nullptr || blocking_trx == lock->trx) &&
        lock_has_to_wait(wait_lock, lock)) {
      return (lock);
    }
  }

  return (nullptr);
}

/** Checks if a waiting lock request still has to wait in a queue.
@param[in]  wait_lock     Waiting lock
@param[in]  blocking_trx  If not nullptr, it restricts the search to only the
                          locks held by the blocking_trx, which is useful in
                          case when there might be multiple reasons for waiting
                          in queue, but we need to report the specific one.
                          Useful when reporting a deadlock cycle.
@return The conflicting lock which is the reason wait_lock has to wait
or nullptr if it can be granted now */
static const lock_t *lock_has_to_wait_in_queue(const lock_t *wait_lock,
                                               const trx_t *blocking_trx) {
  if (lock_get_type_low(wait_lock) == LOCK_REC) {
    return lock_rec_has_to_wait_in_queue(wait_lock, blocking_trx);
  } else {
    return lock_table_has_to_wait_in_queue(wait_lock, blocking_trx);
  }
}

/** Removes a table lock request, waiting or granted, from the queue and grants
 locks to other transactions in the queue, if they now are entitled to a
 lock. */
static void lock_table_dequeue(
    lock_t *in_lock) /*!< in/out: table lock object; transactions waiting
                     behind will get their lock requests granted, if
                     they are now qualified to it */
{
  /* This is needed for lock_table_remove_low(), but it's easier to understand
  the code if we assert it here as well */
  ut_ad(trx_mutex_own(in_lock->trx));
  ut_ad(locksys::owns_table_shard(*in_lock->tab_lock.table));
  ut_a(lock_get_type_low(in_lock) == LOCK_TABLE);

  const auto mode = lock_get_mode(in_lock);
  const auto table = in_lock->tab_lock.table;

  lock_t *lock = UT_LIST_GET_NEXT(tab_lock.locks, in_lock);
  /* This call can remove the last lock on the table, in which case it's unsafe
  to access the table object in the code below, because it can get freed as soon
  as the last lock on it is removed (@see lock_table_has_locks). */
  lock_table_remove_low(in_lock);

  // According to lock_compatibility_matrix, an intention lock can block only
  // LOCK_S or LOCK_X from being granted, and thus, releasing of an intention
  // lock can help in granting only LOCK_S or LOCK_X. If there are no LOCK_S nor
  // LOCK_X locks in the queue, then we can avoid iterating through the list and
  // return immediately. This might help in OLTP scenarios, with no DDL queries,
  // as then there are almost no LOCK_S nor LOCK_X, but many DML queries still
  // need to get an intention lock to perform their action - while this never
  // causes them to wait for a "data lock", it might cause them to wait for
  // lock_sys table shard latch for the duration of table lock queue operation.
  if (!lock || ((mode == LOCK_IS || mode == LOCK_IX) &&
                table->count_by_mode[LOCK_S] == 0 &&
                table->count_by_mode[LOCK_X] == 0)) {
    return;
  }

  /* Check if waiting locks in the queue can now be granted: grant
  locks if there are no conflicting locks ahead. */

  for (/* No op */; lock != nullptr;
       lock = UT_LIST_GET_NEXT(tab_lock.locks, lock)) {
    lock_grant_or_update_wait_for_edge_if_waiting(lock, in_lock->trx);
  }
}

/** Sets a lock on a table based on the given mode.
@param[in]      table   table to lock
@param[in,out]  trx     transaction
@param[in]      mode    LOCK_X or LOCK_S
@return error code or DB_SUCCESS. */
dberr_t lock_table_for_trx(dict_table_t *table, trx_t *trx,
                           enum lock_mode mode) {
  mem_heap_t *heap;
  que_thr_t *thr;
  dberr_t err;
  sel_node_t *node;
  heap = mem_heap_create(512, UT_LOCATION_HERE);

  node = sel_node_create(heap);
  thr = pars_complete_graph_for_exec(node, trx, heap, nullptr);
  thr->graph->state = QUE_FORK_ACTIVE;

  /* We use the select query graph as the dummy graph needed
  in the lock module call */

  thr = static_cast<que_thr_t *>(que_fork_get_first_thr(
      static_cast<que_fork_t *>(que_node_get_parent(thr))));

  que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
  thr->run_node = thr;
  thr->prev_node = thr->common.parent;

  err = lock_table(0, table, mode, thr);

  trx->error_state = err;

  if (err == DB_SUCCESS) {
    que_thr_stop_for_mysql_no_error(thr, trx);
  } else {
    que_thr_stop_for_mysql(thr);

    auto was_lock_wait = row_mysql_handle_errors(&err, trx, thr, nullptr);

    if (was_lock_wait) {
      goto run_again;
    }
  }

  que_graph_free(thr->graph);
  trx->op_info = "";

  return (err);
}

/*=========================== LOCK RELEASE ==============================*/

/** Grant a lock to waiting transactions.
@param[in]      lock            Lock that was unlocked
@param[in]      heap_no         Heap no within the page for the lock. */
static void lock_rec_release(lock_t *lock, ulint heap_no) {
  ut_ad(locksys::owns_page_shard(lock->rec_lock.page_id));
  ut_ad(!lock_get_wait(lock));
  ut_ad(lock_get_type_low(lock) == LOCK_REC);
  ut_ad(lock_rec_get_nth_bit(lock, heap_no));
  lock_rec_reset_nth_bit(lock, heap_no);

  lock_rec_grant_by_heap_no(lock, heap_no);
  MONITOR_INC(MONITOR_RECLOCK_GRANT_ATTEMPTS);
}

/** Removes a granted record lock of a transaction from the queue and grants
 locks to other transactions waiting in the queue if they now are entitled
 to a lock.
 This function is meant to be used only by row_try_unlock, and it assumes
 that the lock we are looking for has LOCK_REC_NOT_GAP flag.
 */
void lock_rec_unlock(
    trx_t *trx,               /*!< in/out: transaction that has
                              set a record lock */
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec,         /*!< in: record */
    lock_mode lock_mode)      /*!< in: LOCK_S or LOCK_X */
{
  ut_ad(block->frame == page_align(rec));
  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
  ut_ad(lock_mode == LOCK_S || lock_mode == LOCK_X);

  ulint heap_no = page_rec_get_heap_no(rec);

  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};
    trx_mutex_enter_first_of_two(trx);
    ut_ad(!trx->lock.wait_lock);

    lock_t *first_lock;

    first_lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);

    /* Find the last lock with the same lock_mode and transaction
    on the record. */

    for (auto lock = first_lock; lock != nullptr;
         lock = lock_rec_get_next(heap_no, lock)) {
      if (lock->trx == trx && lock_get_mode(lock) == lock_mode &&
          lock_rec_get_rec_not_gap(lock)) {
#ifdef UNIV_DEBUG
        /* Since we actually found the first, not the last lock, lets check
           that it is also the last one */
        for (auto lock2 = lock_rec_get_next(heap_no, lock); lock2 != nullptr;
             lock2 = lock_rec_get_next(heap_no, lock2)) {
          ut_ad(!(lock2->trx == trx && lock_get_mode(lock2) == lock_mode &&
                  lock_rec_get_rec_not_gap(lock2)));
        }
#endif
        lock_rec_release(lock, heap_no);

        trx_mutex_exit(trx);

        return;
      }
    }

    trx_mutex_exit(trx);
  } /* Shard_latch_guard */

  {
    size_t stmt_len;

    auto stmt = innobase_get_stmt_unsafe(trx->mysql_thd, &stmt_len);

    ib::error err(ER_IB_MSG_1228);

    err << "Unlock row could not find a " << lock_mode
        << " mode lock on the record. Current statement: ";

    err.write(stmt, stmt_len);
  }
}

/** Unlock the GAP Lock part of a Next Key Lock and grant it to waiters (if any)
@param[in,out]  lock    lock object */
static void lock_release_gap_lock(lock_t *lock) {
  /* 1. Remove GAP lock for all records */
  lock->unlock_gap_lock();

  /* 2. Grant locks for all records */
  lock_rec_grant(lock);

  /* 3. Release explicitly all locks on supremum record. This is required
  because supremum record lock is always considered a GAP Lock, but the lock
  mode can be set to Next Key Lock for sharing lock objects with other records.

  We could not release all locks on supremum record in step [1] & [2] because
  currently lock_rec_grant accepts `lock` object as input which is also part of
  the lock queue. If we unlock supremum record (reset the BIT) in step-1, then
  step-2 would fail to grant locks because SUPREMUM record would be missing from
  input `lock` record bit set. */
  if (lock->includes_supremum()) {
    lock_rec_release(lock, PAGE_HEAP_NO_SUPREMUM);
  }
}

/** Used to release a lock during PREPARE. The lock is only
released if rules permit it.
@param[in]   lock       the lock that we consider releasing
@param[in]   only_gap   true if we don't want to release records,
                        just the gaps between them
@return true iff the function did release (maybe a part of) a lock
*/
static bool lock_release_read_lock(lock_t *lock, bool only_gap) {
  /* Keep in sync with lock_edge_may_survive_prepare() */
  if (!lock->is_record_lock() || lock->is_insert_intention() ||
      lock->is_predicate()) {
    /* DO NOTHING */
    return false;
  } else if (lock->is_gap()) {
    /* Release any GAP only lock. */
    lock_rec_dequeue_from_page(lock);
    return true;
  } else if (lock->is_record_not_gap() && only_gap) {
    /* Don't release any non-GAP lock if not asked.*/
    return false;
  } else if (lock->mode() == LOCK_S && !only_gap) {
    /* Release Shared Next Key Lock(SH + GAP) if asked for */
    lock_rec_dequeue_from_page(lock);
    return true;
  } else {
    /* Release GAP lock from Next Key lock */
    lock_release_gap_lock(lock);
    return true;
  }
}

namespace locksys {

/** A helper function which solves a chicken-and-egg problem occurring when one
needs to iterate over trx's locks and perform some actions on them. Iterating
over this list requires trx->mutex (or exclusive global lock_sys latch), and
operating on a lock requires lock_sys latches, yet the latching order requires
lock_sys latches to be taken before trx->mutex.
One way around it is to use exclusive global lock_sys latch, which heavily
deteriorates concurrency. Another is to try to reacquire the latches in needed
order, veryfing that the list wasn't modified meanwhile.
This function performs following steps:
1. releases trx->mutex,
2. acquires proper lock_sys shard latch for given lock,
3. reaquires trx->mutex
4. executes f unless trx's locks list has changed
Before and after this function following should hold:
- the shared global lock_sys latch is held
- the trx->mutex is held
@param[in]    lock    the lock we are interested in
@param[in]    f       the function to execute when the shard is latched
@return true if f was called, false if it couldn't be called because trx locks
        have changed while relatching trx->mutex
*/
template <typename F>
static bool try_relatch_trx_and_shard_and_do(const lock_t *lock, F &&f) {
  ut_ad(locksys::owns_shared_global_latch());
  const trx_t *trx = lock->trx;
  ut_ad(trx_mutex_own(trx));

  const auto expected_version = trx->lock.trx_locks_version;
  return latch_peeked_shard_and_do(lock, [&]() {
    ut_ad(trx_mutex_own(trx));
    /* Check that list was not modified while we were reacquiring latches */
    if (expected_version != trx->lock.trx_locks_version) {
      /* Someone has modified the list while we were re-acquiring the latches
      so, it is unsafe to operate on the lock. It might have been released, or
      maybe even assigned to another transaction (in case of AUTOINC lock). More
      importantly, we need to let know the caller that the list it is iterating
      over has been modified, which affects next/prev pointers. */
      return false;
    }
    std::forward<F>(f)();
    ut_ad(trx_mutex_own(trx));
    return true;
  });
}

/** We don't want to hold the Global latch for too long, even in S mode, not to
starve threads waiting for X-latch on it such as lock_wait_timeout_thread().
This defines the longest allowed critical section duration. */
constexpr auto MAX_CS_DURATION = std::chrono::seconds{1};

/** Tries to release read locks of a transaction without latching the whole
lock sys. This may fail, if there are many concurrent threads editing the
list of locks of this transaction (for example due to B-tree pages being
merged or split, or due to implicit-to-explicit conversion).
It is called during XA prepare to release locks early.
@param[in,out]  trx             transaction
@param[in]      only_gap        release only GAP locks
@return true if and only if it succeeded to do the job*/
[[nodiscard]] static bool try_release_read_locks_in_s_mode(trx_t *trx,
                                                           bool only_gap) {
  /* In order to access trx->lock.trx_locks safely we need to hold trx->mutex.
  So, conceptually we'd love to hold trx->mutex while iterating through
  trx->lock.trx_locks.
  However the latching order only allows us to obtain trx->mutex AFTER any
  lock_sys latch.
  One way around this problem is to simply latch the whole lock_sys in exclusive
  mode (which also prevents any changes to trx->lock.trx_locks), however this
  impacts performance in appliers (TPS drops by up to 10%).
  Here we use a different approach:
  1. we extract lock from the list when holding the trx->mutex,
  2. identify the shard of lock_sys it belongs to,
  3. store the current version of trx->lock.trx_locks
  4. release the trx->mutex,
  5. acquire the lock_sys shard's latch,
  6. and reacquire the trx->mutex,
  7. verify that the version of trx->lock.trx_locks has not changed
  8. and only then perform any action on the lock.
  */
  locksys::Global_shared_latch_guard shared_latch_guard{UT_LOCATION_HERE};
  trx_mutex_enter(trx);
  ut_ad(trx->lock.wait_lock == nullptr);

  bool made_progress{false};
  for (auto lock : trx->lock.trx_locks.removable()) {
    ut_ad(trx_mutex_own(trx));
    /* We didn't latch the lock_sys shard this `lock` is in, so we only read a
    bare minimum set of information from the `lock`, such as the type, space,
    page_no, and next pointer, which, as long as we hold trx->mutex, should be
    immutable.
    */
    const auto release_read_lock = [lock, only_gap, &made_progress]() {
      /* Note: The |= does not short-circut. We want the RHS called.*/
      made_progress |= lock_release_read_lock(lock, only_gap);
    };
    if (lock_get_type_low(lock) == LOCK_REC) {
      /* Following call temporarily releases trx->mutex */
      if (!try_relatch_trx_and_shard_and_do(lock, release_read_lock) ||
          (made_progress && shared_latch_guard.is_x_blocked_by_us())) {
        /* Someone has modified the list while we were re-acquiring the latches,
        or someone is waiting for x-latch and we've already made some progress,
        so we need to start over again. */
        trx_mutex_exit(trx);
        return false;
      }
    }
    /* As we have verified that the version was not changed by another thread,
    we can safely continue iteration even if we have removed the lock.*/
  }
  trx_mutex_exit(trx);
  return true;
}

/** Release read locks of a transaction latching the whole lock-sys in
exclusive mode, which is a bit too expensive to do by default.
It is called during XA prepare to release locks early.
@param[in,out]  trx             transaction
@param[in]      only_gap        release only GAP locks
@return true if and only if it succeeded to do the job*/
[[nodiscard]] static bool try_release_read_locks_in_x_mode(trx_t *trx,
                                                           bool only_gap) {
  ut_ad(!trx_mutex_own(trx));
  /* We will iterate over locks from various shards. */
  Global_exclusive_latch_guard guard{UT_LOCATION_HERE};
  const auto started_at = std::chrono::steady_clock::now();
  trx_mutex_enter_first_of_two(trx);

  for (auto lock : trx->lock.trx_locks.removable()) {
    if (MAX_CS_DURATION < std::chrono::steady_clock::now() - started_at) {
      trx_mutex_exit(trx);
      return false;
    }
    DEBUG_SYNC_C("lock_trx_release_read_locks_in_x_mode_will_release");

    lock_release_read_lock(lock, only_gap);
  }

  trx_mutex_exit(trx);
  return true;
}
}  // namespace locksys

void lock_trx_release_read_locks(trx_t *trx, bool only_gap) {
  ut_ad(trx_can_be_handled_by_current_thread(trx));

  const size_t MAX_FAILURES = 5;

  for (size_t failures = 0; failures < MAX_FAILURES; ++failures) {
    if (locksys::try_release_read_locks_in_s_mode(trx, only_gap)) {
      return;
    }
    std::this_thread::yield();
  }

  while (!locksys::try_release_read_locks_in_x_mode(trx, only_gap)) {
    std::this_thread::yield();
  }
}

namespace locksys {
/** Releases transaction locks, and releases possible other transactions waiting
 because of these locks.
@param[in,out]  trx   transaction
@return true if and only if it succeeded to do the job*/
[[nodiscard]] static bool try_release_all_locks(trx_t *trx) {
  lock_t *lock;
  ut_ad(!locksys::owns_exclusive_global_latch());
  ut_ad(!trx_mutex_own(trx));
  ut_ad(!trx->is_dd_trx);
  /* The length of the list is an atomic and the number of locks can't change
  from zero to non-zero or vice-versa, see explanation below. */
  if (UT_LIST_GET_LEN(trx->lock.trx_locks) == 0) {
    return true;
  }
  Global_shared_latch_guard shared_latch_guard{UT_LOCATION_HERE};
  /* In order to access trx->lock.trx_locks safely we need to hold trx->mutex.
  The transaction is already in TRX_STATE_COMMITTED_IN_MEMORY state and is no
  longer referenced, so we are not afraid of implicit-to-explicit conversions,
  nor a cancellation of a wait_lock (we are running, not waiting). Still, there
  might be some B-tree merge or split operations running in parallel which cause
  locks to be moved from one page to another, which at the low level means that
  a new lock is created (and added to trx->lock.trx_locks) and the old one is
  removed (also from trx->lock.trx_locks) in that specific order.
  So, conceptually we'd love to hold trx->mutex while iterating through
  trx->lock.trx_locks.
  However the latching order only allows us to obtain trx->mutex AFTER any
  lock_sys latch. One way around this problem is to simply latch the whole
  lock_sys in exclusive mode (which also prevents any changes to
  trx->lock.trx_locks), however this impacts performance (TPS drops on
  sysbench {pareto,uniform}-2S-{128,1024}-usrs tests by 3% to 11%) Here we
  use a different approach:
  1. we extract lock from the list when holding the trx->mutex,
  2. identify the shard of lock_sys it belongs to,
  3. release the trx->mutex,
  4. acquire the lock_sys shard's latch,
  5. and reacquire the trx->mutex,
  6. verify that the lock pointer is still in trx->lock.trx_locks (so it is
  safe to access it),
  7. and only then perform any action on the lock.
  */
  trx_mutex_enter(trx);

  ut_ad(trx->lock.wait_lock == nullptr);
  while ((lock = UT_LIST_GET_LAST(trx->lock.trx_locks)) != nullptr) {
    /* Following call temporarily releases trx->mutex */
    try_relatch_trx_and_shard_and_do(lock, [=]() {
      if (lock_get_type_low(lock) == LOCK_REC) {
        lock_rec_dequeue_from_page(lock);
      } else {
        lock_table_dequeue(lock);
      }
    });
    if (shared_latch_guard.is_x_blocked_by_us()) {
      trx_mutex_exit(trx);
      return false;
    }
  }

  trx_mutex_exit(trx);
  return true;
}
}  // namespace locksys

/* True if a lock mode is S or X */
static inline bool IS_LOCK_S_OR_X(lock_t *lock) {
  return lock_get_mode(lock) == LOCK_S || lock_get_mode(lock) == LOCK_X;
}

/** Removes locks of a transaction on a table to be dropped.
 If remove_also_table_sx_locks is true then table-level S and X locks are
 also removed in addition to other table-level and record-level locks.
 No lock that is going to be removed is allowed to be a wait lock. */
static void lock_remove_all_on_table_for_trx(
    dict_table_t *table,             /*!< in: table to be dropped */
    trx_t *trx,                      /*!< in: a transaction */
    bool remove_also_table_sx_locks) /*!< in: also removes
                                   table S and X locks */
{
  lock_t *lock;
  lock_t *prev_lock;

  /* This is used when we drop a table and indeed have exclusive lock_sys
  access. */
  ut_ad(locksys::owns_exclusive_global_latch());
  /* We need trx->mutex to iterate over trx->lock.trx_lock and it is needed by
  lock_table_remove_low() but we haven't acquired it yet. */
  ut_ad(!trx_mutex_own(trx));
  trx_mutex_enter(trx);

  for (lock = UT_LIST_GET_LAST(trx->lock.trx_locks); lock != nullptr;
       lock = prev_lock) {
    prev_lock = UT_LIST_GET_PREV(trx_locks, lock);

    if (lock_get_type_low(lock) == LOCK_REC && lock->index->table == table) {
      ut_a(!lock_get_wait(lock));

      lock_rec_discard(lock);
    } else if (lock_get_type_low(lock) & LOCK_TABLE &&
               lock->tab_lock.table == table &&
               (remove_also_table_sx_locks || !IS_LOCK_S_OR_X(lock))) {
      ut_a(!lock_get_wait(lock));

      lock_table_remove_low(lock);
    }
  }

  trx_mutex_exit(trx);
}

/** Remove any explicit record locks held by recovering transactions on
 the table.
 @return number of recovered transactions examined */
static ulint lock_remove_recovered_trx_record_locks(
    dict_table_t *table) /*!< in: check if there are any locks
                         held on records in this table or on the
                         table itself */
{
  ut_a(table != nullptr);
  /* We need exclusive lock_sys latch, as we are about to iterate over locks
  held by multiple transactions while they might be operating. */
  ut_ad(locksys::owns_exclusive_global_latch());

  ulint n_recovered_trx = 0;

  mutex_enter(&trx_sys->mutex);

  for (trx_t *trx : trx_sys->rw_trx_list) {
    assert_trx_in_rw_list(trx);

    if (!trx->is_recovered) {
      continue;
    }
    /* We need trx->mutex to iterate over trx->lock.trx_lock and it is needed by
    lock_table_remove_low() but we haven't acquired it yet. */
    ut_ad(!trx_mutex_own(trx));
    trx_mutex_enter(trx);
    /* Because we are holding the exclusive global lock_sys latch,
    implicit locks cannot be converted to explicit ones
    while we are scanning the explicit locks. */

    for (auto lock : trx->lock.trx_locks.removable()) {
      ut_a(lock->trx == trx);

      /* Recovered transactions can't wait on a lock. */

      ut_a(!lock_get_wait(lock));

      switch (lock_get_type_low(lock)) {
        default:
          ut_error;
        case LOCK_TABLE:
          if (lock->tab_lock.table == table) {
            lock_table_remove_low(lock);
          }
          break;
        case LOCK_REC:
          if (lock->index->table == table) {
            lock_rec_discard(lock);
          }
      }
    }

    trx_mutex_exit(trx);
    ++n_recovered_trx;
  }

  mutex_exit(&trx_sys->mutex);

  return (n_recovered_trx);
}

/** Removes locks on a table to be dropped.
 If remove_also_table_sx_locks is true then table-level S and X locks are
 also removed in addition to other table-level and record-level locks.
 No lock, that is going to be removed, is allowed to be a wait lock. */
void lock_remove_all_on_table(
    dict_table_t *table,             /*!< in: table to be dropped
                                     or discarded */
    bool remove_also_table_sx_locks) /*!< in: also removes
                                   table S and X locks */
{
  /* We will iterate over locks (including record locks) from various shards */
  locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};

  for (auto lock : table->locks.removable()) {
    /* If we should remove all locks (remove_also_table_sx_locks
    is true), or if the lock is not table-level S or X lock,
    then check we are not going to remove a wait lock. */
    if (remove_also_table_sx_locks ||
        !(lock_get_type(lock) == LOCK_TABLE && IS_LOCK_S_OR_X(lock))) {
      ut_a(!lock_get_wait(lock));
    }

    lock_remove_all_on_table_for_trx(table, lock->trx,
                                     remove_also_table_sx_locks);
  }

  /* Note: Recovered transactions don't have table level IX or IS locks
  but can have implicit record locks that have been converted to explicit
  record locks. Such record locks cannot be freed by traversing the
  transaction lock list in dict_table_t (as above). */

  if (!lock_sys->rollback_complete &&
      lock_remove_recovered_trx_record_locks(table) == 0) {
    lock_sys->rollback_complete = true;
  }
}

/*===================== VALIDATION AND DEBUGGING ====================*/

/** Prints info of a table lock. */
static void lock_table_print(FILE *file,         /*!< in: file where to print */
                             const lock_t *lock) /*!< in: table type lock */
{
  ut_a(lock_get_type_low(lock) == LOCK_TABLE);
  /* We actually hold exclusive latch here, but we require just the shard */
  ut_ad(locksys::owns_table_shard(*lock->tab_lock.table));

  fputs("TABLE LOCK table ", file);
  ut_print_name(file, lock->trx, lock->tab_lock.table->name.m_name);
  fprintf(file, " trx id " TRX_ID_FMT, trx_get_id_for_print(lock->trx));

  if (lock_get_mode(lock) == LOCK_S) {
    fputs(" lock mode S", file);
  } else if (lock_get_mode(lock) == LOCK_X) {
    ut_ad(lock->trx->id != 0);
    fputs(" lock mode X", file);
  } else if (lock_get_mode(lock) == LOCK_IS) {
    fputs(" lock mode IS", file);
  } else if (lock_get_mode(lock) == LOCK_IX) {
    ut_ad(lock->trx->id != 0);
    fputs(" lock mode IX", file);
  } else if (lock_get_mode(lock) == LOCK_AUTO_INC) {
    fputs(" lock mode AUTO-INC", file);
  } else {
    fprintf(file, " unknown lock mode %lu", (ulong)lock_get_mode(lock));
  }

  if (lock_get_wait(lock)) {
    fputs(" waiting", file);
  }

  putc('\n', file);
}

/** Prints info of a record lock. */
static void lock_rec_print(FILE *file,         /*!< in: file where to print */
                           const lock_t *lock) /*!< in: record type lock */
{
  mtr_t mtr;
  Rec_offsets offsets;

  ut_a(lock_get_type_low(lock) == LOCK_REC);
  const auto page_id = lock->rec_lock.page_id;
  /* We actually hold exclusive latch here, but we require just the shard */
  ut_ad(locksys::owns_page_shard(page_id));

  fprintf(file,
          "RECORD LOCKS space id %lu page no %lu n bits %llu "
          "index %s of table ",
          ulong{page_id.space()}, ulong{page_id.page_no()},
          ulonglong{lock_rec_get_n_bits(lock)}, lock->index->name());
  ut_print_name(file, lock->trx, lock->index->table_name);
  fprintf(file, " trx id " TRX_ID_FMT, trx_get_id_for_print(lock->trx));

  if (lock_get_mode(lock) == LOCK_S) {
    fputs(" lock mode S", file);
  } else if (lock_get_mode(lock) == LOCK_X) {
    fputs(" lock_mode X", file);
  } else {
    ut_error;
  }

  if (lock_rec_get_gap(lock)) {
    fputs(" locks gap before rec", file);
  }

  if (lock_rec_get_rec_not_gap(lock)) {
    fputs(" locks rec but not gap", file);
  }

  if (lock_rec_get_insert_intention(lock)) {
    fputs(" insert intention", file);
  }

  if (lock_get_wait(lock)) {
    fputs(" waiting", file);
  }

  mtr_start(&mtr);

  putc('\n', file);

  const buf_block_t *block;

  block = buf_page_try_get(page_id, UT_LOCATION_HERE, &mtr);

  for (ulint i = 0; i < lock_rec_get_n_bits(lock); ++i) {
    if (!lock_rec_get_nth_bit(lock, i)) {
      continue;
    }

    fprintf(file, "Record lock, heap no %lu", (ulong)i);

    if (block) {
      const rec_t *rec;

      rec = page_find_rec_with_heap_no(buf_block_get_frame(block), i);

      putc(' ', file);
      rec_print_new(file, rec, offsets.compute(rec, lock->index));
    }

    putc('\n', file);
  }

  mtr_commit(&mtr);
}

#ifdef UNIV_DEBUG
/* Print the number of lock structs from lock_print_info_summary() only
in non-production builds for performance reasons, see
http://bugs.mysql.com/36942 */
#define PRINT_NUM_OF_LOCK_STRUCTS
#endif /* UNIV_DEBUG */

#ifdef PRINT_NUM_OF_LOCK_STRUCTS
/** Calculates the number of record lock structs in the record lock hash table.
 @return number of record locks */
static ulint lock_get_n_rec_locks(void) {
  ulint n_locks = 0;
  ulint i;

  /* We need exclusive access to lock_sys to iterate over all hash cells. */
  ut_ad(locksys::owns_exclusive_global_latch());

  for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
    const lock_t *lock;

    for (lock =
             static_cast<const lock_t *>(hash_get_first(lock_sys->rec_hash, i));
         lock != nullptr;
         lock = static_cast<const lock_t *>(HASH_GET_NEXT(hash, lock))) {
      n_locks++;
    }
  }

  return (n_locks);
}
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */

void lock_print_info_summary(FILE *file) {
  ut_ad(locksys::owns_exclusive_global_latch());

  if (lock_deadlock_found) {
    fputs(
        "------------------------\n"
        "LATEST DETECTED DEADLOCK\n"
        "------------------------\n",
        file);

    if (!srv_read_only_mode) {
      ut_copy_file(file, lock_latest_err_file);
    }
  }

  fputs(
      "------------\n"
      "TRANSACTIONS\n"
      "------------\n",
      file);

  fprintf(file, "Trx id counter " TRX_ID_FMT "\n",
          trx_sys_get_next_trx_id_or_no());

  fprintf(file,
          "Purge done for trx's n:o < " TRX_ID_FMT " undo n:o < " TRX_ID_FMT
          " state: ",
          purge_sys->iter.trx_no, purge_sys->iter.undo_no);

  /* Note: We are reading the state without the latch. One because it
  will violate the latching order and two because we are merely querying
  the state of the variable for display. */

  switch (purge_sys->state) {
    case PURGE_STATE_INIT:
      /* Should never be in this state while the system is running. */
      fprintf(file, "initializing");
      break;

    case PURGE_STATE_EXIT:
      fprintf(file, "exited");
      break;

    case PURGE_STATE_DISABLED:
      fprintf(file, "disabled");
      break;

    case PURGE_STATE_RUN:
      fprintf(file, "running");
      /* Check if it is waiting for more data to arrive. */
      if (!purge_sys->running) {
        fprintf(file, " but idle");
      }
      break;

    case PURGE_STATE_STOP:
      fprintf(file, "stopped");
      break;
  }

  fprintf(file, "\n");

  fprintf(file, "History list length " UINT64PF "\n",
          trx_sys->rseg_history_len.load());

#ifdef PRINT_NUM_OF_LOCK_STRUCTS
  fprintf(file, "Total number of lock structs in row lock hash table %lu\n",
          (ulong)lock_get_n_rec_locks());
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */
}

/** Functor to print not-started transaction from the mysql_trx_list. */
struct PrintNotStarted {
  PrintNotStarted(FILE *file) : m_file(file) {}

  void operator()(const trx_t *trx) {
    /* We require exclusive access to lock_sys */
    ut_ad(locksys::owns_exclusive_global_latch());
    ut_ad(trx->in_mysql_trx_list);
    ut_ad(mutex_own(&trx_sys->mutex));

    /* See state transitions and locking rules in trx0trx.h */

    trx_mutex_enter(trx);
    if (trx_state_eq(trx, TRX_STATE_NOT_STARTED)) {
      fputs("---", m_file);
      trx_print_latched(m_file, trx, 600);
    }
    trx_mutex_exit(trx);
  }

  FILE *m_file;
};

/** Iterate over a transaction's locks. Keeping track of the
iterator using an ordinal value. */
class TrxLockIterator {
 public:
  TrxLockIterator() { rewind(); }

  /** Get the m_index(th) lock of a transaction.
  @return current lock or 0 */
  const lock_t *current(const trx_t *trx) const {
    ulint i = 0;
    /* Writes to trx->lock.trx_locks are protected by trx->mutex combined with a
    shared lock_sys global latch, and we assume we have the exclusive latch on
    lock_sys here. */
    ut_ad(locksys::owns_exclusive_global_latch());
    for (auto lock : trx->lock.trx_locks) {
      if (i++ == m_index) {
        return lock;
      }
    }
    return nullptr;
  }

  /** Set the ordinal value to 0 */
  void rewind() { m_index = 0; }

  /** Increment the ordinal value.
  @return the current index value */
  ulint next() { return (++m_index); }

 private:
  /** Current iterator position */
  ulint m_index;
};

/** This iterates over RW trx_sys lists only. We need to keep
track where the iterator was up to and we do that using an ordinal value. */

class TrxListIterator {
 public:
  TrxListIterator() : m_index() {
    /* We iterate over the RW trx list only. */

    m_trx_list = &trx_sys->rw_trx_list;
  }

  /** Get the current transaction whose ordinality is m_index.
  @return current transaction or 0 */

  const trx_t *current() { return (reposition()); }

  /** Advance the transaction current ordinal value and reset the
  transaction lock ordinal value */

  void next() {
    ++m_index;
    m_lock_iter.rewind();
  }

  TrxLockIterator &lock_iter() { return (m_lock_iter); }

 private:
  /** Reposition the "cursor" on the current transaction. If it
  is the first time then the "cursor" will be positioned on the
  first transaction.

  @return transaction instance or 0 */
  const trx_t *reposition() const {
    ulint i = 0;

    /* Make the transaction at the ordinal value of m_index
    the current transaction. ie. reposition/restore */

    for (auto trx : *m_trx_list) {
      if (i++ == m_index) {
        return trx;
      }
      check_trx_state(trx);
    }

    return nullptr;
  }

  /** Ordinal value of the transaction in the current transaction list */
  ulint m_index;

  /** Current transaction list */
  decltype(trx_sys->rw_trx_list) *m_trx_list;

  /** For iterating over a transaction's locks */
  TrxLockIterator m_lock_iter;
};

/** Prints transaction lock wait and MVCC state.
@param[in,out]  file    file where to print
@param[in]      trx     transaction */
void lock_trx_print_wait_and_mvcc_state(FILE *file, const trx_t *trx) {
  /* We require exclusive lock_sys access so that trx->lock.wait_lock is
  not being modified, and to access trx->lock.wait_started without trx->mutex.*/
  ut_ad(locksys::owns_exclusive_global_latch());
  fprintf(file, "---");

  trx_print_latched(file, trx, 600);

  const ReadView *read_view = trx_get_read_view(trx);

  if (read_view != nullptr) {
    read_view->print_limits(file);
  }

  if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
    fprintf(file,
            "------- TRX HAS BEEN WAITING %" PRId64
            " SEC FOR THIS LOCK TO BE GRANTED:\n",
            static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - trx->lock.wait_started)
                    .count()));

    if (lock_get_type_low(trx->lock.wait_lock) == LOCK_REC) {
      lock_rec_print(file, trx->lock.wait_lock);
    } else {
      lock_table_print(file, trx->lock.wait_lock);
    }

    fprintf(file, "------------------\n");
  }
}

/** Reads the page containing the record protected by the given lock.
This function will temporarily release the exclusive global latch and the
trx_sys_t::mutex if the page was read from disk.
@param[in]  lock  the record lock
@return true if a page was successfully read from the tablespace */
static bool lock_rec_fetch_page(const lock_t *lock) {
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  const page_id_t page_id = lock->rec_lock.page_id;
  const space_id_t space_id = page_id.space();
  fil_space_t *space;
  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space_id, &found);

  /* Check if the .ibd file exists. */
  if (found) {
    mtr_t mtr;

    locksys::Unsafe_global_latch_manipulator::exclusive_unlatch();

    mutex_exit(&trx_sys->mutex);

    DEBUG_SYNC_C("innodb_monitor_before_lock_page_read");

    /* Check if the space is exists or not. only
    when the space is valid, try to get the page. */
    space = fil_space_acquire_silent(space_id);
    if (space) {
      mtr_start(&mtr);
      buf_page_get_gen(page_id, page_size, RW_NO_LATCH, nullptr,
                       Page_fetch::POSSIBLY_FREED, UT_LOCATION_HERE, &mtr);
      mtr_commit(&mtr);
      fil_space_release(space);
    }

    locksys::Unsafe_global_latch_manipulator::exclusive_latch(UT_LOCATION_HERE);

    mutex_enter(&trx_sys->mutex);

    return (true);
  }

  return (false);
}

/** Prints info of locks for a transaction.
 @return true if all printed, false if latches were released. */
static bool lock_trx_print_locks(
    FILE *file,            /*!< in/out: File to write */
    const trx_t *trx,      /*!< in: current transaction */
    TrxLockIterator &iter, /*!< in: transaction lock iterator */
    bool load_block)       /*!< in: if true then read block
                           from disk */
{
  const lock_t *lock;
  /* We require exclusive access to lock_sys */
  ut_ad(locksys::owns_exclusive_global_latch());

  /* Iterate over the transaction's locks. */
  while ((lock = iter.current(trx)) != nullptr) {
    if (lock_get_type_low(lock) == LOCK_REC) {
      if (load_block) {
        /* Note: lock_rec_fetch_page() will release both the exclusive global
        latch and the trx_sys_t::mutex if it does a read from disk. */

        if (lock_rec_fetch_page(lock)) {
          /* We need to resync the
          current transaction. */
          return (false);
        }

        /* It is a single table tablespace
        and the .ibd file is missing
        (DISCARD TABLESPACE probably stole the
        locks): just print the lock without
        attempting to load the page in the
        buffer pool. */

        fprintf(file,
                "RECORD LOCKS on non-existing"
                " space %u\n",
                lock->rec_lock.page_id.space());
      }

      /* Print all the record locks on the page from
      the record lock bitmap */

      lock_rec_print(file, lock);

      load_block = true;

    } else {
      ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

      lock_table_print(file, lock);
    }

    if (iter.next() >= 10) {
      fprintf(file,
              "10 LOCKS PRINTED FOR THIS TRX:"
              " SUPPRESSING FURTHER PRINTS\n");

      break;
    }
  }

  return (true);
}

void lock_print_info_all_transactions(FILE *file) {
  /* We require exclusive access to lock_sys */
  ut_ad(locksys::owns_exclusive_global_latch());

  fprintf(file, "LIST OF TRANSACTIONS FOR EACH SESSION:\n");

  mutex_enter(&trx_sys->mutex);

  /* First print info on non-active transactions */

  /* NOTE: information of auto-commit non-locking read-only
  transactions will be omitted here. The information will be
  available from INFORMATION_SCHEMA.INNODB_TRX. */

  PrintNotStarted print_not_started(file);
  ut_list_map(trx_sys->mysql_trx_list, print_not_started);

  const trx_t *trx;
  TrxListIterator trx_iter;
  const trx_t *prev_trx = nullptr;

  /* Control whether a block should be fetched from the buffer pool. */
  bool load_block = true;
  bool monitor = srv_print_innodb_lock_monitor;

  while ((trx = trx_iter.current()) != nullptr) {
    check_trx_state(trx);

    if (trx != prev_trx) {
      lock_trx_print_wait_and_mvcc_state(file, trx);
      prev_trx = trx;

      /* The transaction that read in the page is no
      longer the one that read the page in. We need to
      force a page read. */
      load_block = true;
    }

    /* If we need to print the locked record contents then we
    need to fetch the containing block from the buffer pool. */
    if (monitor) {
      /* Print the locks owned by the current transaction. */
      TrxLockIterator &lock_iter = trx_iter.lock_iter();

      if (!lock_trx_print_locks(file, trx, lock_iter, load_block)) {
        /* Resync trx_iter, the trx_sys->mutex and exclusive global latch were
        temporarily released. A page was successfully read in. We need to print
        its contents on the next call to lock_trx_print_locks(). On the next
        call to lock_trx_print_locks() we should simply print the contents of
        the page just read in.*/
        load_block = false;

        continue;
      }
    }

    load_block = true;

    /* All record lock details were printed without fetching
    a page from disk, or we didn't need to print the detail. */
    trx_iter.next();
  }

  mutex_exit(&trx_sys->mutex);
}

#ifdef UNIV_DEBUG

/** Validates the lock queue on a table.
 @return true if ok */
static bool lock_table_queue_validate(
    const dict_table_t *table) /*!< in: table */
{
  /* We actually hold exclusive latch here, but we require just the shard */
  ut_ad(locksys::owns_table_shard(*table));
  ut_ad(trx_sys_mutex_own());

  for (auto lock : table->locks) {
    /* lock->trx->state cannot change to NOT_STARTED until transaction released
    its table locks and that is prevented here by the locksys shard's mutex. */
    ut_ad(trx_assert_started(lock->trx));

    if (!lock_get_wait(lock)) {
      ut_a(!lock_table_other_has_incompatible(lock->trx, 0, table,
                                              lock_get_mode(lock)));
    } else {
      ut_a(lock_table_has_to_wait_in_queue(lock));
    }
  }

  return (true);
}
namespace locksys {
/** Validates the lock queue on a single record.
@param[in]  block     buffer block containing rec
@param[in]  rec       record to look at
@param[in]  index     index, or NULL if not known
@param[in]  offsets   rec_get_offsets(rec, index) */
static void rec_queue_validate_latched(const buf_block_t *block,
                                       const rec_t *rec,
                                       const dict_index_t *index,
                                       const ulint *offsets) {
  ut_ad(owns_page_shard(block->get_page_id()));
  ut_ad(mutex_own(&trx_sys->mutex));
  ut_a(rec);
  ut_a(block->frame == page_align(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));
  ut_ad(!index || index->is_clustered() || !dict_index_is_online_ddl(index));

  ulint heap_no = page_rec_get_heap_no(rec);
  RecID rec_id{block, heap_no};

  if (!page_rec_is_user_rec(rec)) {
    Lock_iter::for_each(rec_id, [&](lock_t *lock) {
      ut_ad(!trx_is_ac_nl_ro(lock->trx));

      if (lock->is_waiting()) {
        ut_a(lock_rec_has_to_wait_in_queue(lock));
      }

      if (index != nullptr) {
        ut_a(lock->index == index);
      }

      return (true);
    });

    return;
  }

  if (index == nullptr) {
    /* Nothing we can do */

  } else if (index->is_clustered()) {
    trx_id_t trx_id;

    /* Unlike the non-debug code, this invariant can only succeed
    if the check and assertion are covered by the lock_sys latch. */

    trx_id = lock_clust_rec_some_has_impl(rec, index, offsets);

    trx_sys->latch_and_execute_with_active_trx(
        trx_id,
        [&](const trx_t *impl_trx) {
          if (impl_trx != nullptr) {
            ut_ad(owns_page_shard(block->get_page_id()));
            /* impl_trx cannot become TRX_STATE_COMMITTED_IN_MEMORY nor removed
            from active_rw_trxs.by_id until we release Trx_shard's mutex, which
            means that currently all other threads in the system consider this
            impl_trx active and thus should respect implicit locks held by
            impl_trx*/

            const lock_t *other_lock = lock_rec_other_has_expl_req(
                LOCK_S, block, true, heap_no, impl_trx);

            /* The impl_trx is holding an implicit lock on the given 'rec'.
            So there cannot be another explicit granted lock. Also, there can
            be another explicit waiting lock only if the impl_trx has an
            explicit granted lock. */

            if (other_lock != nullptr) {
              ut_a(lock_get_wait(other_lock));
              ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block, heap_no,
                                     impl_trx));
            }
          }
        },
        UT_LOCATION_HERE);
  }

  Lock_iter::for_each(rec_id, [&](lock_t *lock) {
    ut_ad(!trx_is_ac_nl_ro(lock->trx));

    if (index != nullptr) {
      ut_a(lock->index == index);
    }

    if (!lock->is_gap() && !lock->is_waiting()) {
      lock_mode mode;

      if (lock_get_mode(lock) == LOCK_S) {
        mode = LOCK_X;
      } else {
        mode = LOCK_S;
      }

      const lock_t *other_lock =
          lock_rec_other_has_expl_req(mode, block, false, heap_no, lock->trx);

      ut_a(!other_lock);

    } else if (lock->is_waiting() && !lock->is_gap()) {
      ut_a(lock_rec_has_to_wait_in_queue(lock));
    }

    return (true);
  });
}

/** Validates the lock queue on a single record.
@param[in]  block     buffer block containing rec
@param[in]  rec       record to look at
@param[in]  index     index, or NULL if not known
@param[in]  offsets   rec_get_offsets(rec, index) */
static void rec_queue_latch_and_validate(const buf_block_t *block,
                                         const rec_t *rec,
                                         const dict_index_t *index,
                                         const ulint *offsets) {
  ut_ad(!owns_exclusive_global_latch());
  ut_ad(!mutex_own(&trx_sys->mutex));

  Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};
  mutex_enter(&trx_sys->mutex);
  rec_queue_validate_latched(block, rec, index, offsets);
  mutex_exit(&trx_sys->mutex);
}

/** Validates the lock queue on a single record.
@param[in]  block     buffer block containing rec
@param[in]  rec       record to look at
@param[in]  index     index, or NULL if not known */
static void rec_queue_latch_and_validate(const buf_block_t *block,
                                         const rec_t *rec,
                                         const dict_index_t *index) {
  rec_queue_latch_and_validate(block, rec, index,
                               Rec_offsets().compute(rec, index));
}
}  // namespace locksys

/** Validates the record lock queues on a page.
 @return true if ok */
static bool lock_rec_validate_page(
    const buf_block_t *block) /*!< in: buffer block */
{
  const lock_t *lock;
  const rec_t *rec;
  ulint nth_lock = 0;
  ulint nth_bit = 0;
  ulint i;
  Rec_offsets offsets;

  ut_ad(!locksys::owns_exclusive_global_latch());

  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};
  mutex_enter(&trx_sys->mutex);
loop:
  lock =
      lock_rec_get_first_on_page_addr(lock_sys->rec_hash, block->get_page_id());

  if (!lock) {
    goto function_exit;
  }

  ut_ad(!block->page.file_page_was_freed);

  for (i = 0; i < nth_lock; i++) {
    lock = lock_rec_get_next_on_page_const(lock);

    if (!lock) {
      goto function_exit;
    }
  }

  ut_ad(!trx_is_ac_nl_ro(lock->trx));

  if (!sync_check_find(SYNC_FSP))
    for (i = nth_bit; i < lock_rec_get_n_bits(lock); i++) {
      if (i == 1 || lock_rec_get_nth_bit(lock, i)) {
        rec = page_find_rec_with_heap_no(block->frame, i);
        ut_a(rec);

        /* If this thread is holding the file space
        latch (fil_space_t::latch), the following
        check WILL break the latching order and may
        cause a deadlock of threads. */

        locksys::rec_queue_validate_latched(block, rec, lock->index,
                                            offsets.compute(rec, lock->index));

        nth_bit = i + 1;

        goto loop;
      }
    }

  nth_bit = 0;
  nth_lock++;

  goto loop;

function_exit:
  mutex_exit(&trx_sys->mutex);

  return (true);
}

/** Validates the table locks. */
static void lock_validate_table_locks() {
  /* We need exclusive access to lock_sys to iterate over trxs' locks */
  ut_ad(locksys::owns_exclusive_global_latch());
  ut_ad(trx_sys_mutex_own());

  for (const trx_t *trx : trx_sys->rw_trx_list) {
    check_trx_state(trx);

    for (const lock_t *lock : trx->lock.trx_locks) {
      if (lock_get_type_low(lock) & LOCK_TABLE) {
        lock_table_queue_validate(lock->tab_lock.table);
      }
    }
  }
}

/** Validate a record lock's block */
static void lock_rec_block_validate(const page_id_t &page_id) {
  /* The lock and the block that it is referring to may be freed at
  this point. We pass Page_fetch::POSSIBLY_FREED to skip a debug check.
  If the lock exists in lock_rec_validate_page() we assert
  !block->page.file_page_was_freed. */

  buf_block_t *block;
  mtr_t mtr;

  /* Make sure that the tablespace is not deleted while we are
  trying to access the page. */
  if (fil_space_t *space = fil_space_acquire_silent(page_id.space())) {
    mtr_start(&mtr);

    block = buf_page_get_gen(page_id, page_size_t(space->flags), RW_X_LATCH,
                             nullptr, Page_fetch::POSSIBLY_FREED,
                             UT_LOCATION_HERE, &mtr);

    buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

    ut_ad(lock_rec_validate_page(block));
    mtr_commit(&mtr);

    fil_space_release(space);
  }
}

bool lock_validate() {
  typedef std::set<page_id_t, std::less<page_id_t>, ut::allocator<page_id_t>>
      page_addr_set;

  page_addr_set pages;
  {
    /* lock_validate_table_locks() needs exclusive global latch, and we will
    inspect record locks from all shards */
    locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};
    mutex_enter(&trx_sys->mutex);

    lock_validate_table_locks();

    /* Iterate over all the record locks and validate the locks. We
    don't want to hog the lock_sys global latch and the trx_sys_t::mutex.
    Thus we release both latches before the validation check. */

    for (ulint i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
      for (const lock_t *lock = static_cast<const lock_t *>(
               hash_get_first(lock_sys->rec_hash, i));
           lock != nullptr;
           lock = static_cast<const lock_t *>(HASH_GET_NEXT(hash, lock))) {
        ut_ad(!trx_is_ac_nl_ro(lock->trx));
        ut_ad(lock_get_type(lock) == LOCK_REC);
        pages.emplace(lock->rec_lock.page_id);
      }
    }

    mutex_exit(&trx_sys->mutex);
  }
  std::for_each(pages.cbegin(), pages.cend(), lock_rec_block_validate);

  return (true);
}
#endif /* UNIV_DEBUG */
/*============ RECORD LOCK CHECKS FOR ROW OPERATIONS ====================*/

/** Checks if locks of other transactions prevent an immediate insert of
 a record. If they do, first tests if the query thread should anyway
 be suspended for some reason; if not, then puts the transaction and
 the query thread to the lock wait state and inserts a waiting request
 for a gap x-lock to the lock queue.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_rec_insert_check_and_lock(
    ulint flags,         /*!< in: if BTR_NO_LOCKING_FLAG bit is
                         set, does nothing */
    const rec_t *rec,    /*!< in: record after which to insert */
    buf_block_t *block,  /*!< in/out: buffer block of rec */
    dict_index_t *index, /*!< in: index */
    que_thr_t *thr,      /*!< in: query thread */
    mtr_t *mtr,          /*!< in/out: mini-transaction */
    bool *inherit)       /*!< out: set to true if the new
                          inserted record maybe should inherit
                          LOCK_GAP type locks from the successor
                          record */
{
  ut_ad(block->frame == page_align(rec));
  ut_ad(!dict_index_is_online_ddl(index) || index->is_clustered() ||
        (flags & BTR_CREATE_FLAG));

  if (flags & BTR_NO_LOCKING_FLAG) {
    return (DB_SUCCESS);
  }

  ut_ad(!index->table->is_temporary());

  dberr_t err = DB_SUCCESS;
  lock_t *lock;
  auto inherit_in = *inherit;
  trx_t *trx = thr_get_trx(thr);
  const rec_t *next_rec = page_rec_get_next_const(rec);
  ulint heap_no = page_rec_get_heap_no(next_rec);

  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

    /* When inserting a record into an index, the table must be at
    least IX-locked. When we are building an index, we would pass
    BTR_NO_LOCKING_FLAG and skip the locking altogether. */
    ut_ad(lock_table_has(trx, index->table, LOCK_IX));

    /* Spatial index does not use GAP lock protection. It uses
    "predicate lock" to protect the "range" */
    ut_ad(!dict_index_is_spatial(index));

    lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);

    if (lock == nullptr) {
      *inherit = false;
    } else {
      *inherit = true;

      /* If another transaction has an explicit lock request which locks
      the gap, waiting or granted, on the successor, the insert has to wait.

      An exception is the case where the lock by the another transaction
      is a gap type lock which it placed to wait for its turn to insert. We
      do not consider that kind of a lock conflicting with our insert. This
      eliminates an unnecessary deadlock which resulted when 2 transactions
      had to wait for their insert. Both had waiting gap type lock requests
      on the successor, which produced an unnecessary deadlock. */

      const ulint type_mode = LOCK_X | LOCK_GAP | LOCK_INSERT_INTENTION;

      const auto conflicting =
          lock_rec_other_has_conflicting(type_mode, block, heap_no, trx);

      /* LOCK_INSERT_INTENTION locks can not be allowed to bypass waiting locks,
      because they allow insertion of a record which splits the gap which would
      lead to duplication of the waiting lock, violating the constraint that
      each transaction can wait for at most one lock at any given time */
      ut_a(!conflicting.bypassed);

      if (conflicting.wait_for != nullptr) {
        RecLock rec_lock(thr, index, block, heap_no, type_mode);

        trx_mutex_enter(trx);

        err = rec_lock.add_to_waitq(conflicting.wait_for);

        trx_mutex_exit(trx);
      }
    }
  } /* Shard_latch_guard */

  switch (err) {
    case DB_SUCCESS_LOCKED_REC:
      err = DB_SUCCESS;
      [[fallthrough]];
    case DB_SUCCESS:
      if (!inherit_in || index->is_clustered()) {
        break;
      }

      /* Update the page max trx id field */
      page_update_max_trx_id(block, buf_block_get_page_zip(block), trx->id,
                             mtr);
    default:
      /* We only care about the two return values. */
      break;
  }

  ut_d(locksys::rec_queue_latch_and_validate(block, next_rec, index));
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);

  return (err);
}

/** Creates an explicit record lock for a running transaction that currently
 only has an implicit lock on the record. The transaction instance must have a
 reference count > 0 so that it can't be committed and freed before this
 function has completed. */
static void lock_rec_convert_impl_to_expl_for_trx(
    const buf_block_t *block, /*!< in: buffer block of rec */
    const rec_t *rec,         /*!< in: user record on page */
    dict_index_t *index,      /*!< in: index of record */
    const ulint *offsets,     /*!< in: rec_get_offsets(rec, index) */
    trx_t *trx,               /*!< in/out: active transaction */
    ulint heap_no)            /*!< in: rec heap number to lock */
{
  ut_ad(trx_is_referenced(trx));

  DEBUG_SYNC_C("before_lock_rec_convert_impl_to_expl_for_trx");
  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};
    /* This trx->mutex acquisition here is not really needed.
    Its purpose is to prevent a state transition between calls to trx_state_eq()
    and lock_rec_add_to_queue().
    But one can prove, that even if the state did change, it is not
    a big problem, because we still keep reference count from dropping
    to zero, so the trx object is still in use, and we hold the shard latched,
    so trx can not release its explicit lock (if it has any) so we will
    notice the explicit lock in lock_rec_has_expl.
    On the other hand if trx does not have explicit lock, then we would create
    one on its behalf, which is wasteful, but does not cause a problem, as once
    the reference count drops to zero the trx will notice and remove this new
    explicit lock. Also, even if some other trx had observed that trx is already
    removed from rw trxs list and thus ignored the implicit lock and decided to
    add its own lock, it will still have to wait for shard latch before adding
    her lock. However it does not cost us much to simply take the trx->mutex
    and avoid this whole shaky reasoning. */
    trx_mutex_enter(trx);

    ut_ad(!index->is_clustered() ||
          trx->id ==
              lock_clust_rec_some_has_impl(
                  rec, index,
                  offsets ? offsets : Rec_offsets().compute(rec, index)));

    ut_ad(!trx_state_eq(trx, TRX_STATE_NOT_STARTED));

    if (!trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY) &&
        !lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block, heap_no, trx)) {
      ulint type_mode;

      type_mode = (LOCK_REC | LOCK_X | LOCK_REC_NOT_GAP);

      lock_rec_add_to_queue(type_mode, block, heap_no, index, trx, true);
    }

    trx_mutex_exit(trx);
  }

  trx_release_reference(trx);

  DEBUG_SYNC_C("after_lock_rec_convert_impl_to_expl_for_trx");
}

void lock_rec_convert_impl_to_expl(const buf_block_t *block, const rec_t *rec,
                                   dict_index_t *index, const ulint *offsets) {
  trx_t *trx;

  ut_ad(!locksys::owns_exclusive_global_latch());
  ut_ad(page_rec_is_user_rec(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));

  DEBUG_SYNC_C("lock_rec_convert_impl_to_expl");

  if (index->is_clustered()) {
    trx_id_t trx_id;

    trx_id = lock_clust_rec_some_has_impl(rec, index, offsets);

    trx = trx_rw_is_active(trx_id, true);
  } else {
    ut_ad(!dict_index_is_online_ddl(index));

    trx = lock_sec_rec_some_has_impl(rec, index, offsets);
    if (trx) {
      DEBUG_SYNC_C("lock_rec_convert_impl_to_expl_will_validate");
      ut_ad(!lock_rec_other_trx_holds_expl(LOCK_S | LOCK_REC_NOT_GAP, trx, rec,
                                           block));
    }
  }

  if (trx != nullptr) {
    ulint heap_no = page_rec_get_heap_no(rec);

    ut_ad(trx_is_referenced(trx));

    /* If the transaction is still active and has no
    explicit x-lock set on the record, set one for it.
    trx cannot be committed until the ref count is zero. */

    lock_rec_convert_impl_to_expl_for_trx(block, rec, index, offsets, trx,
                                          heap_no);
  }
}

/** Checks if locks of other transactions prevent an immediate modify (update,
 delete mark, or delete unmark) of a clustered index record. If they do,
 first tests if the query thread should anyway be suspended for some
 reason; if not, then puts the transaction and the query thread to the
 lock wait state and inserts a waiting request for a record x-lock to the
 lock queue.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_clust_rec_modify_check_and_lock(
    ulint flags,              /*!< in: if BTR_NO_LOCKING_FLAG
                              bit is set, does nothing */
    const buf_block_t *block, /*!< in: buffer block of rec */
    const rec_t *rec,         /*!< in: record which should be
                              modified */
    dict_index_t *index,      /*!< in: clustered index */
    const ulint *offsets,     /*!< in: rec_get_offsets(rec, index) */
    que_thr_t *thr)           /*!< in: query thread */
{
  dberr_t err;
  ulint heap_no;

  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(index->is_clustered());
  ut_ad(block->frame == page_align(rec));

  if (flags & BTR_NO_LOCKING_FLAG) {
    return (DB_SUCCESS);
  }
  ut_ad(!index->table->is_temporary());

  heap_no = rec_offs_comp(offsets) ? rec_get_heap_no_new(rec)
                                   : rec_get_heap_no_old(rec);

  /* If a transaction has no explicit x-lock set on the record, set one
  for it */

  lock_rec_convert_impl_to_expl(block, rec, index, offsets);

  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};
    ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

    err = lock_rec_lock(true, SELECT_ORDINARY, LOCK_X | LOCK_REC_NOT_GAP, block,
                        heap_no, index, thr);

    MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);
  }

  ut_d(locksys::rec_queue_latch_and_validate(block, rec, index, offsets));

  if (err == DB_SUCCESS_LOCKED_REC) {
    err = DB_SUCCESS;
  }
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

/** Checks if locks of other transactions prevent an immediate modify (delete
 mark or delete unmark) of a secondary index record.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_sec_rec_modify_check_and_lock(
    ulint flags,         /*!< in: if BTR_NO_LOCKING_FLAG
                         bit is set, does nothing */
    buf_block_t *block,  /*!< in/out: buffer block of rec */
    const rec_t *rec,    /*!< in: record which should be
                         modified; NOTE: as this is a secondary
                         index, we always have to modify the
                         clustered index record first: see the
                         comment below */
    dict_index_t *index, /*!< in: secondary index */
    que_thr_t *thr,      /*!< in: query thread
                         (can be NULL if BTR_NO_LOCKING_FLAG) */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  dberr_t err;
  ulint heap_no;

  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(index) || (flags & BTR_CREATE_FLAG));
  ut_ad(block->frame == page_align(rec));

  if (flags & BTR_NO_LOCKING_FLAG) {
    return (DB_SUCCESS);
  }
  ut_ad(!index->table->is_temporary());

  heap_no = page_rec_get_heap_no(rec);

  /* Another transaction cannot have an implicit lock on the record,
  because when we come here, we already have modified the clustered
  index record, and this would not have been possible if another active
  transaction had modified this secondary index record. */
  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

    ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

    err = lock_rec_lock(true, SELECT_ORDINARY, LOCK_X | LOCK_REC_NOT_GAP, block,
                        heap_no, index, thr);

    MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);
  }

  ut_d(locksys::rec_queue_latch_and_validate(block, rec, index));

  if (err == DB_SUCCESS || err == DB_SUCCESS_LOCKED_REC) {
    /* Update the page max trx id field */
    /* It might not be necessary to do this if
    err == DB_SUCCESS (no new lock created),
    but it should not cost too much performance. */
    page_update_max_trx_id(block, buf_block_get_page_zip(block),
                           thr_get_trx(thr)->id, mtr);
    err = DB_SUCCESS;
  }
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

dberr_t lock_sec_rec_read_check_and_lock(
    const lock_duration_t duration, const buf_block_t *block, const rec_t *rec,
    dict_index_t *index, const ulint *offsets, const select_mode sel_mode,
    const lock_mode mode, const ulint gap_mode, que_thr_t *thr) {
  dberr_t err;
  ulint heap_no;

  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(index));
  ut_ad(block->frame == page_align(rec));
  ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(mode == LOCK_X || mode == LOCK_S);

  if (srv_read_only_mode || index->table->is_temporary()) {
    return (DB_SUCCESS);
  }

  heap_no = page_rec_get_heap_no(rec);

  if (!page_rec_is_supremum(rec)) {
    lock_rec_convert_impl_to_expl(block, rec, index, offsets);
  }
  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

    if (duration == lock_duration_t::AT_LEAST_STATEMENT) {
      lock_protect_locks_till_statement_end(thr);
    }

    ut_ad(mode != LOCK_X ||
          lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
    ut_ad(mode != LOCK_S ||
          lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

    err = lock_rec_lock(false, sel_mode, mode | gap_mode, block, heap_no, index,
                        thr);

    MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);
  }
  DEBUG_SYNC_C("lock_sec_rec_read_check_and_lock_has_locked");

  ut_d(locksys::rec_queue_latch_and_validate(block, rec, index, offsets));
  ut_ad(err == DB_SUCCESS || err == DB_SUCCESS_LOCKED_REC ||
        err == DB_LOCK_WAIT || err == DB_DEADLOCK || err == DB_SKIP_LOCKED ||
        err == DB_LOCK_NOWAIT);
  return (err);
}

dberr_t lock_clust_rec_read_check_and_lock(
    const lock_duration_t duration, const buf_block_t *block, const rec_t *rec,
    dict_index_t *index, const ulint *offsets, const select_mode sel_mode,
    const lock_mode mode, const ulint gap_mode, que_thr_t *thr) {
  dberr_t err;
  ulint heap_no;
  DEBUG_SYNC_C("before_lock_clust_rec_read_check_and_lock");
  ut_ad(index->is_clustered());
  ut_ad(block->frame == page_align(rec));
  ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
  ut_ad(gap_mode == LOCK_ORDINARY || gap_mode == LOCK_GAP ||
        gap_mode == LOCK_REC_NOT_GAP);
  ut_ad(rec_offs_validate(rec, index, offsets));

  if (srv_read_only_mode || index->table->is_temporary()) {
    return (DB_SUCCESS);
  }

  heap_no = page_rec_get_heap_no(rec);

  if (heap_no != PAGE_HEAP_NO_SUPREMUM) {
    lock_rec_convert_impl_to_expl(block, rec, index, offsets);
  }

  DEBUG_SYNC_C("after_lock_clust_rec_read_check_and_lock_impl_to_expl");
  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

    if (duration == lock_duration_t::AT_LEAST_STATEMENT) {
      lock_protect_locks_till_statement_end(thr);
    }

    ut_ad(mode != LOCK_X ||
          lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
    ut_ad(mode != LOCK_S ||
          lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

    err = lock_rec_lock(false, sel_mode, mode | gap_mode, block, heap_no, index,
                        thr);

    MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);
  }
  DEBUG_SYNC_C("after_lock_clust_rec_read_check_and_lock");

  ut_d(locksys::rec_queue_latch_and_validate(block, rec, index, offsets));

  ut_ad(err == DB_SUCCESS || err == DB_SUCCESS_LOCKED_REC ||
        err == DB_LOCK_WAIT || err == DB_DEADLOCK || err == DB_SKIP_LOCKED ||
        err == DB_LOCK_NOWAIT);
  return (err);
}
/** Checks if locks of other transactions prevent an immediate read, or passing
 over by a read cursor, of a clustered index record. If they do, first tests
 if the query thread should anyway be suspended for some reason; if not, then
 puts the transaction and the query thread to the lock wait state and inserts a
 waiting request for a record lock to the lock queue. Sets the requested mode
 lock on the record. This is an alternative version of
 lock_clust_rec_read_check_and_lock() that does not require the parameter
 "offsets".
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_clust_rec_read_check_and_lock_alt(
    const buf_block_t *block, /*!< in: buffer block of rec */
    const rec_t *rec,         /*!< in: user record or page
                              supremum record which should
                              be read or passed over by a
                              read cursor */
    dict_index_t *index,      /*!< in: clustered index */
    lock_mode mode,           /*!< in: mode of the lock which
                              the read cursor should set on
                              records: LOCK_S or LOCK_X; the
                              latter is possible in
                              SELECT FOR UPDATE */
    ulint gap_mode,           /*!< in: LOCK_ORDINARY, LOCK_GAP, or
                             LOCK_REC_NOT_GAP */
    que_thr_t *thr)           /*!< in: query thread */
{
  dberr_t err = lock_clust_rec_read_check_and_lock(
      lock_duration_t::REGULAR, block, rec, index,
      Rec_offsets().compute(rec, index), SELECT_ORDINARY, mode, gap_mode, thr);

  if (err == DB_SUCCESS_LOCKED_REC) {
    err = DB_SUCCESS;
  }
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

/** Release the last lock from the transaction's autoinc locks.
@param[in]  trx   trx which vector of AUTOINC locks to modify */
static inline void lock_release_autoinc_last_lock(trx_t *trx) {
  ulint last;
  lock_t *lock;

  /* We will access trx->lock.autoinc_locks which requires trx->mutex */
  ut_ad(trx_mutex_own(trx));
  ib_vector_t *autoinc_locks = trx->lock.autoinc_locks;

  /* Since we do not know for which table the trx has created the last lock
  we can not narrow the required latch to any particular shard, and thus we
  require exclusive access to lock_sys here */
  ut_ad(locksys::owns_exclusive_global_latch());
  ut_a(!ib_vector_is_empty(autoinc_locks));

  /* The lock to be release must be the last lock acquired. */
  last = ib_vector_size(autoinc_locks) - 1;
  lock = *static_cast<lock_t **>(ib_vector_get(autoinc_locks, last));

  /* Should have only AUTOINC locks in the vector. */
  ut_a(lock_get_mode(lock) == LOCK_AUTO_INC);
  ut_a(lock_get_type(lock) == LOCK_TABLE);

  ut_a(lock->tab_lock.table != nullptr);

  /* This will remove the lock from the trx autoinc_locks too. */
  lock_table_dequeue(lock);
}

/** Check if a transaction holds any autoinc locks.
 @return true if the transaction holds any AUTOINC locks. */
static bool lock_trx_holds_autoinc_locks(
    const trx_t *trx) /*!< in: transaction */
{
  /* We will access trx->lock.autoinc_locks which requires trx->mutex */
  ut_ad(trx_mutex_own(trx));
  ut_a(trx->lock.autoinc_locks != nullptr);

  return (!ib_vector_is_empty(trx->lock.autoinc_locks));
}

/** Release all the transaction's autoinc locks. */
static void lock_release_autoinc_locks(trx_t *trx) /*!< in/out: transaction */
{
  /* Since we do not know for which table(s) the trx has created the lock(s)
  we can not narrow the required latch to any particular shard, and thus we
  require exclusive access to lock_sys here */
  ut_ad(locksys::owns_exclusive_global_latch());
  ut_ad(trx_mutex_own(trx));

  ut_a(trx->lock.autoinc_locks != nullptr);

  /* We release the locks in the reverse order. This is to
  avoid searching the vector for the element to delete at
  the lower level. See (lock_table_remove_low()) for details. */
  while (!ib_vector_is_empty(trx->lock.autoinc_locks)) {
    /* lock_table_remove_low() will also remove the lock from
    the transaction's autoinc_locks vector. */
    lock_release_autoinc_last_lock(trx);
  }

  /* Should release all locks. */
  ut_a(ib_vector_is_empty(trx->lock.autoinc_locks));
}

/** Gets the type of a lock. Non-inline version for using outside of the
 lock module.
 @return LOCK_TABLE or LOCK_REC */
uint32_t lock_get_type(const lock_t *lock) /*!< in: lock */
{
  return (lock_get_type_low(lock));
}

uint64_t lock_get_trx_immutable_id(const lock_t *lock) {
  return (trx_immutable_id(lock->trx));
}

trx_id_t lock_get_trx_id(const lock_t *lock) {
  return (trx_get_id_for_print(lock->trx));
}

uint64_t lock_get_immutable_id(const lock_t *lock) {
  return (uint64_t{reinterpret_cast<uintptr_t>(lock)});
}

/** Get the performance schema event (thread_id, event_id)
that created the lock.
@param[in]      lock            Lock
@param[out]     thread_id       Thread ID that created the lock
@param[out]     event_id        Event ID that created the lock
*/
void lock_get_psi_event(const lock_t *lock, ulonglong *thread_id,
                        ulonglong *event_id) {
#if defined(HAVE_PSI_THREAD_INTERFACE) && defined(HAVE_PSI_DATA_LOCK_INTERFACE)
  *thread_id = lock->m_psi_internal_thread_id;
  *event_id = lock->m_psi_event_id;
#else
  *thread_id = 0;
  *event_id = 0;
#endif
}

/** Get the first lock of a trx lock list.
@param[in]      trx_lock        the trx lock
@return The first lock
*/
const lock_t *lock_get_first_trx_locks(const trx_lock_t *trx_lock) {
  /* Writes to trx->lock.trx_locks are protected by trx->mutex combined with a
  shared global lock_sys latch, and we assume we have the exclusive latch on
  lock_sys here */
  ut_ad(locksys::owns_exclusive_global_latch());
  const lock_t *result = UT_LIST_GET_FIRST(trx_lock->trx_locks);
  return (result);
}

/** Get the next lock of a trx lock list.
@param[in]      lock    the current lock
@return The next lock
*/
const lock_t *lock_get_next_trx_locks(const lock_t *lock) {
  /* Writes to trx->lock.trx_locks are protected by trx->mutex combined with a
  shared global lock_sys latch, and we assume we have the exclusive latch on
  lock_sys here */
  ut_ad(locksys::owns_exclusive_global_latch());
  const lock_t *result = UT_LIST_GET_NEXT(trx_locks, lock);
  return (result);
}

/** Gets the mode of a lock in a human readable string.
 The string should not be free()'d or modified.
 This functions is a bit complex for following reasons:
  - the way it is used in performance schema requires that the memory pointed
    by the return value is accessible for a long time
  - the caller never frees the memory
  - so, we need to maintain a pool of these strings or use string literals
  - there are many possible combinations of flags and thus it is impractical
    to maintain the list of all possible literals and if/else logic
  - moreover, sometimes performance_schema.data_locks is used precisely to
    investigate some unexpected situation, thus limiting output of this function
    only to expected combinations of flags might be misleading
 @return lock mode */
const char *lock_get_mode_str(const lock_t *lock) /*!< in: lock */
{
  /* We use exclusive global lock_sys latch to protect the global
  lock_cached_lock_mode_names mapping. */
  ut_ad(locksys::owns_exclusive_global_latch());

  const auto type_mode = lock->type_mode;
  const auto mode = lock->mode();
  const auto type = lock->type();
  /* type_mode is type + mode + flags actually.
    We are interested in flags here.
    And we are not interested in LOCK_WAIT. */
  const auto flags = (type_mode & (~(uint)LOCK_WAIT)) - mode - type;

  /* Search for a cached string */
  const auto key = flags | mode;
  const auto found = lock_cached_lock_mode_names.find(key);
  if (found != lock_cached_lock_mode_names.end()) {
    return (found->second);
  }
  /* A new, unseen yet, mode of lock. We need to create new string. */
  ut::ostringstream name_stream;
  /* lock_mode_string can be used to describe mode, however the LOCK_ prefix in
  return mode name makes the string a bit too verbose for our purpose, as
  performance_schema.data_locks LOCK_MODE is a varchar(32), so we strip the
  prefix */
  const char *mode_string = lock_mode_string(mode);
  const char *LOCK_PREFIX = "LOCK_";
  if (!strncmp(mode_string, LOCK_PREFIX, strlen(LOCK_PREFIX))) {
    mode_string = mode_string + strlen(LOCK_PREFIX);
  }
  name_stream << mode_string;
  /* We concatenate constants in ascending order. */
  uint recognized_flags = 0;
  for (const auto &lock_constant : lock_constant_names) {
    const auto value = lock_constant.first;
    /* Constants have to be single bit only for this algorithm to work */
    ut_ad((value & (value - 1)) == 0);
    if (flags & value) {
      recognized_flags += value;
      name_stream << ',' << lock_constant.second;
    }
  }
  if (flags != recognized_flags) {
    return "UNKNOWN";
  }
  auto name_string = name_stream.str();
  char *name_buffer = (char *)ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                                 name_string.length() + 1);
  strcpy(name_buffer, name_string.c_str());
  lock_cached_lock_mode_names[key] = name_buffer;
  return (name_buffer);
}

/** Gets the type of a lock in a human readable string.
 The string should not be free()'d or modified.
 @return lock type */
const char *lock_get_type_str(const lock_t *lock) /*!< in: lock */
{
  switch (lock_get_type_low(lock)) {
    case LOCK_REC:
      return ("RECORD");
    case LOCK_TABLE:
      return ("TABLE");
    default:
      return ("UNKNOWN");
  }
}

/** Gets the table on which the lock is.
 @return table */
static inline dict_table_t *lock_get_table(const lock_t *lock) /*!< in: lock */
{
  switch (lock_get_type_low(lock)) {
    case LOCK_REC:
      ut_ad(lock->index->is_clustered() ||
            !dict_index_is_online_ddl(lock->index));
      return (lock->index->table);
    case LOCK_TABLE:
      return (lock->tab_lock.table);
    default:
      ut_error;
  }
}

/** Gets the id of the table on which the lock is.
 @return id of the table */
table_id_t lock_get_table_id(const lock_t *lock) /*!< in: lock */
{
  dict_table_t *table;

  table = lock_get_table(lock);

  return (table->id);
}

/** Determine which table a lock is associated with.
@param[in]      lock    the lock
@return name of the table */
const table_name_t &lock_get_table_name(const lock_t *lock) {
  return (lock_get_table(lock)->name);
}

/** For a record lock, gets the index on which the lock is.
 @return index */
const dict_index_t *lock_rec_get_index(const lock_t *lock) /*!< in: lock */
{
  ut_a(lock_get_type_low(lock) == LOCK_REC);
  ut_ad(lock->index->is_clustered() || !dict_index_is_online_ddl(lock->index));

  return (lock->index);
}

/** For a record lock, gets the name of the index on which the lock is.
 The string should not be free()'d or modified.
 @return name of the index */
const char *lock_rec_get_index_name(const lock_t *lock) /*!< in: lock */
{
  ut_a(lock_get_type_low(lock) == LOCK_REC);
  ut_ad(lock->index->is_clustered() || !dict_index_is_online_ddl(lock->index));

  return (lock->index->name);
}

page_id_t lock_rec_get_page_id(const lock_t *lock) {
  ut_a(lock_get_type_low(lock) == LOCK_REC);
  return lock->rec_lock.page_id;
}

void lock_cancel_waiting_and_release(trx_t *trx) {
  ut_ad(trx_mutex_own(trx));
  const auto lock = trx->lock.wait_lock.load();
  ut_ad(locksys::owns_lock_shard(lock));

  if (lock_get_type_low(lock) == LOCK_REC) {
    lock_rec_dequeue_from_page(lock);
  } else {
    ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

    lock_table_dequeue(lock);
  }

  lock_reset_wait_and_release_thread_if_suspended(lock);
}

/** Unlocks AUTO_INC type locks that were possibly reserved by a trx. This
 function should be called at the the end of an SQL statement, by the
 connection thread that owns the transaction (trx->mysql_thd). */
void lock_unlock_table_autoinc(trx_t *trx) /*!< in/out: transaction */
{
  ut_ad(!locksys::owns_exclusive_global_latch());
  ut_ad(!trx_mutex_own(trx));

  /* This can be invoked on NOT_STARTED, ACTIVE, PREPARED,
  but not COMMITTED transactions. */

  ut_ad(trx_state_eq(trx, TRX_STATE_NOT_STARTED) ||
        trx_state_eq(trx, TRX_STATE_FORCED_ROLLBACK) ||
        !trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));

  /* The trx->lock.autoinc_locks are protected by trx->mutex and in principle
  can be modified by other threads:
    1. When the other thread calls lock_grant on trx->lock.wait_lock.
      (This is impossible here, because we've verified !trx->lock.wait_lock)
    2. During recovery lock_remove_recoverd_trx_record_locks ->
       lock_table_remove_low -> lock_table_remove_autoinc_lock ->
       lock_table_pop_autoinc_lock.
       (But AFAIK recovery is a single-threaded process)
    3. During DROP TABLE lock_remove_all_on_table_for_trx ->
      lock_table_remove_low ...
      (I'm unsure if this is possible to happen in parallel to our trx)
  Please note, that from this list only lock_grant tries to add something
  to the trx->lock.autoinc_locks (namely the granted AUTOINC lock), and the
  others try to remove something. This means that we can treat the result of
  lock_trx_holds_autoinc_locks(trx) as a heuristic. If it returns true,
  then it might or (with small probability) might not hold locks, so we better
  call lock_release_autoinc_locks with proper latching.
  If it returns false, then it is guaranteed that the vector will remain empty.
  If we like risk, we could even call lock_trx_holds_autoinc_locks without
  trx->mutex protection, but:
    1. why risk? It is not obvious how thread-safe our vector implementation is
    2. trx->mutex is cheap
  */
  trx_mutex_enter(trx);
  ut_ad(!trx->lock.wait_lock);
  bool might_have_autoinc_locks = lock_trx_holds_autoinc_locks(trx);
  trx_mutex_exit(trx);

  if (might_have_autoinc_locks) {
    /* lock_release_autoinc_locks() requires exclusive global latch as the
    AUTOINC locks might be on tables from different shards. Identifying and
    latching them in correct order would complicate this rarely-taken path. */
    locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};
    trx_mutex_enter(trx);
    lock_release_autoinc_locks(trx);
    trx_mutex_exit(trx);
  }
}

/** Releases a transaction's locks, and releases possible other transactions
 waiting because of these locks. Change the state of the transaction to
 TRX_STATE_COMMITTED_IN_MEMORY. */
void lock_trx_release_locks(trx_t *trx) /*!< in/out: transaction */
{
  DEBUG_SYNC_C("before_lock_trx_release_locks");

  trx_mutex_enter(trx);

  check_trx_state(trx);
  ut_ad(trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));
  ut_ad(!trx->in_rw_trx_list);

  if (trx_is_referenced(trx)) {
    while (trx_is_referenced(trx)) {
      trx_mutex_exit(trx);

      DEBUG_SYNC_C("waiting_trx_is_not_referenced");

      /** Doing an implicit to explicit conversion
      should not be expensive. */
      ut_delay(ut::random_from_interval_fast(0, srv_spin_wait_delay));

      trx_mutex_enter(trx);
    }
  }

  ut_ad(!trx_is_referenced(trx));
  trx_mutex_exit(trx);

  while (!locksys::try_release_all_locks(trx)) {
    std::this_thread::yield();
  }

  /* We don't free the locks one by one for efficiency reasons.
  We simply empty the heap one go. Similarly we reset n_rec_locks count to 0.
  At this point there should be no one else interested in our trx's
  locks as we've released and removed all of them, and the trx is no longer
  referenced so nobody will attempt implicit to explicit conversion neither.
  Please note that we are either the thread which runs the transaction, or we
  are the thread of a high priority transaction which decided to kill trx, in
  which case it had to first make sure that it is no longer running in InnoDB.
  So no race is expected to happen.
  All that being said, it does not cost us anything in terms of performance to
  protect these operations with trx->mutex, which makes some class of errors
  impossible even if the above reasoning was wrong. */
  trx_mutex_enter(trx);
  trx->lock.n_rec_locks.store(0);

  ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);
  ut_a(ib_vector_is_empty(trx->lock.autoinc_locks));

  mem_heap_empty(trx->lock.lock_heap);
  trx_mutex_exit(trx);
}

bool lock_cancel_if_waiting_and_release(const TrxVersion trx_version) {
  trx_t &trx{*trx_version.m_trx};
  bool realeased = false;
  locksys::run_if_waiting(trx_version, [&]() {
    ut_ad(trx_mutex_own(&trx));
    ut_a(trx_version.m_version == trx.version.load());
    if ((trx.in_innodb & TRX_FORCE_ROLLBACK) != 0) {
      /* A HP transaction wants to wake up and rollback trx by pretending it
      has been chosen a deadlock victim while waiting for a lock. */
#ifdef UNIV_DEBUG
      ib::info(ER_IB_MSG_639, to_string(trx.killed_by).c_str(),
               ulonglong{trx.id});
#endif /* UNIV_DEBUG */
      trx.lock.was_chosen_as_deadlock_victim = true;
    } else {
      /* This case is currently used by kill_connection. Canceling the
      wait and waking up the transaction will have the effect that its
      thread will continue without the lock acquired, which is unsafe,
      unless it will notice that it has been interrupted and give up. */
      ut_ad(trx_is_interrupted(&trx));
    }
    lock_cancel_waiting_and_release(&trx);
    realeased = true;
  });
  return realeased;
}

#ifdef UNIV_DEBUG
/** Scans all locks of all transactions in the rw_trx_list searching for any
lock (table or rec) against the table.
@param[in]  table   the table for which we perform the search
@return lock if found */
static const lock_t *lock_table_locks_lookup(const dict_table_t *table) {
  ut_a(table != nullptr);
  /* We are going to iterate over multiple transactions, so even though we know
  which table we are looking for we can not narrow required latch to just the
  shard which contains the table, because accessing trx->lock.trx_locks would be
  unsafe */
  ut_ad(locksys::owns_exclusive_global_latch());
  ut_ad(trx_sys_mutex_own());

  for (auto trx : trx_sys->rw_trx_list) {
    check_trx_state(trx);

    for (auto lock : trx->lock.trx_locks) {
      ut_a(lock->trx == trx);

      if (lock_get_type_low(lock) == LOCK_REC) {
        ut_ad(!dict_index_is_online_ddl(lock->index) ||
              lock->index->is_clustered());
        if (lock->index->table == table) {
          return (lock);
        }
      } else if (lock->tab_lock.table == table) {
        return (lock);
      }
    }
  }

  return (nullptr);
}
#endif /* UNIV_DEBUG */

bool lock_table_has_locks(const dict_table_t *table) {
  /** The n_rec_locks field might be modified by operation on any page shard.
  This function is called in contexts where we believe that the number of
  locks should either be zero or decreasing.
  For such scenario of usage, we can read the n_rec_locks without any latch
  and restrict latch just to the table's shard and release it before return,
  which means `true` could be a false-positive, but `false` is certain. */

  bool has_locks = table->n_rec_locks.load() > 0;
  if (!has_locks) {
    /* As soon as we return false the caller might free the table object, so it
    is crucial that when lock_table_dequeue() removes the last lock on the table
    then the thread calling it won't dereference the table pointer anymore. */
    has_locks = UT_LIST_GET_LEN(table->locks) > 0;
  }

#ifdef UNIV_DEBUG
  if (!has_locks) {
    locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};
    mutex_enter(&trx_sys->mutex);

    ut_ad(!lock_table_locks_lookup(table));

    mutex_exit(&trx_sys->mutex);
  }
#endif /* UNIV_DEBUG */

  return (has_locks);
}
/** Set the lock system timeout event. */
void lock_set_timeout_event() { os_event_set(lock_sys->timeout_event); }

#ifdef UNIV_DEBUG

bool lock_trx_has_rec_x_lock(que_thr_t *thr, const dict_table_t *table,
                             const buf_block_t *block, ulint heap_no) {
  ut_ad(heap_no > PAGE_HEAP_NO_SUPREMUM);

  const trx_t *trx = thr_get_trx(thr);
  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};
  ut_a(lock_table_has(trx, table, LOCK_IX) || table->is_temporary());
  ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block, heap_no, trx) ||
       table->is_temporary());
  return (true);
}
#endif /* UNIV_DEBUG */

/** rewind(3) the file used for storing the latest detected deadlock and
print a heading message to stderr if printing of all deadlocks to stderr
is enabled. */
void Deadlock_notifier::start_print() {
  /* I/O operations on lock_latest_err_file require exclusive latch on
  lock_sys */
  ut_ad(locksys::owns_exclusive_global_latch());

  rewind(lock_latest_err_file);
  ut_print_timestamp(lock_latest_err_file);

  if (srv_print_all_deadlocks) {
    ib::info(ER_IB_MSG_643) << "Transactions deadlock detected, dumping"
                            << " detailed information.";
  }
}

/** Print a message to the deadlock file and possibly to stderr.
@param msg message to print */
void Deadlock_notifier::print(const char *msg) {
  /* I/O operations on lock_latest_err_file require exclusive latch on
  lock_sys */
  ut_ad(locksys::owns_exclusive_global_latch());
  fputs(msg, lock_latest_err_file);

  if (srv_print_all_deadlocks) {
    ib::info(ER_IB_MSG_644) << msg;
  }
}

/** Print transaction data to the deadlock file and possibly to stderr.
@param trx transaction
@param max_query_len max query length to print */
void Deadlock_notifier::print(const trx_t *trx, ulint max_query_len) {
  /* We need exclusive latch on lock_sys because:
    1. I/O operations on lock_latest_err_file
    2. lock_number_of_rows_locked()
    3. Accessing trx->lock fields requires either holding trx->mutex or latching
    the lock sys. */
  ut_ad(locksys::owns_exclusive_global_latch());

  trx_mutex_enter(trx);
  ulint n_rec_locks = lock_number_of_rows_locked(&trx->lock);
  ulint n_trx_locks = UT_LIST_GET_LEN(trx->lock.trx_locks);
  ulint heap_size = mem_heap_get_size(trx->lock.lock_heap);
  trx_mutex_exit(trx);

  mutex_enter(&trx_sys->mutex);

  trx_print_low(lock_latest_err_file, trx, max_query_len, n_rec_locks,
                n_trx_locks, heap_size);

  if (srv_print_all_deadlocks) {
    trx_print_low(stderr, trx, max_query_len, n_rec_locks, n_trx_locks,
                  heap_size);
  }

  mutex_exit(&trx_sys->mutex);
}

/** Print lock data to the deadlock file and possibly to stderr.
@param lock record or table type lock */
void Deadlock_notifier::print(const lock_t *lock) {
  /* I/O operations on lock_latest_err_file require exclusive latch on
  lock_sys. */
  ut_ad(locksys::owns_exclusive_global_latch());

  if (lock_get_type_low(lock) == LOCK_REC) {
    lock_rec_print(lock_latest_err_file, lock);

    if (srv_print_all_deadlocks) {
      lock_rec_print(stderr, lock);
    }
  } else {
    lock_table_print(lock_latest_err_file, lock);

    if (srv_print_all_deadlocks) {
      lock_table_print(stderr, lock);
    }
  }
}

void Deadlock_notifier::print_title(size_t pos_on_cycle, const char *title) {
  /* I/O operations on lock_latest_err_file require exclusive latch on
  lock_sys */
  ut_ad(locksys::owns_exclusive_global_latch());
  ut::ostringstream buff;
  buff << "\n*** (" << (pos_on_cycle + 1) << ") " << title << ":\n";
  print(buff.str().c_str());
}

void Deadlock_notifier::notify(const ut::vector<const trx_t *> &trxs_on_cycle,
                               const trx_t *victim_trx) {
  ut_ad(locksys::owns_exclusive_global_latch());

  start_print();
  const auto n = trxs_on_cycle.size();
  for (size_t i = 0; i < n; ++i) {
    const trx_t *trx = trxs_on_cycle[i];
    const trx_t *blocked_trx = trxs_on_cycle[0 < i ? i - 1 : n - 1];
    const lock_t *blocking_lock =
        lock_has_to_wait_in_queue(blocked_trx->lock.wait_lock, trx);
    ut_a(blocking_lock);

    print_title(i, "TRANSACTION");
    print(trx, 3000);

    print_title(i, "HOLDS THE LOCK(S)");
    print(blocking_lock);

    print_title(i, "WAITING FOR THIS LOCK TO BE GRANTED");
    print(trx->lock.wait_lock);
  }
  const auto victim_it =
      std::find(trxs_on_cycle.begin(), trxs_on_cycle.end(), victim_trx);
  ut_ad(victim_it != trxs_on_cycle.end());
  const auto victim_pos = std::distance(trxs_on_cycle.begin(), victim_it);
  ut::ostringstream buff;
  buff << "*** WE ROLL BACK TRANSACTION (" << (victim_pos + 1) << ")\n";
  print(buff.str().c_str());
  DBUG_PRINT("ib_lock", ("deadlock detected"));

#ifdef UNIV_DEBUG
  /* We perform this check only after information is output, to give a
  developer as much information as we can for debugging the problem */
  for (const trx_t *trx : trxs_on_cycle) {
    ut_ad(is_allowed_to_be_on_cycle(trx->lock.wait_lock));
  }
#endif /* UNIV_DEBUG */

  lock_deadlock_found = true;
}

#ifdef UNIV_DEBUG

bool Deadlock_notifier::is_allowed_to_be_on_cycle(const lock_t *lock) {
  /* The original purpose of this validation is to check record locks from
  DD & SDI tables only, because we think a deadlock for these locks should be
  prevented by MDL and proper updating order, but later, some exemptions were
  introduced (for more context see comment to this function).
  In particular, we don't check table locks here, since there never was any
  guarantee saying a deadlock is impossible for table locks. */
  if (!lock->is_record_lock()) {
    return (true);
  }
  /* The only places where we don't expect deadlocks are in handling DD
  tables, and since WL#9538 also in code handling SDI tables.
  Therefore the second condition is that we only pay attention to DD and SDI
  tables. */
  const bool is_dd_or_sdi = (lock->index->table->is_dd_table ||
                             dict_table_is_sdi(lock->index->table->id));
  if (!is_dd_or_sdi) {
    return (true);
  }

  /* If we are still here, the lock is a record lock on some DD or SDI table.
  There are some such tables though, for which a deadlock is somewhat expected,
  for various reasons specific to these particular tables.
  So, we have a list of exceptions here:

  innodb_table_stats and innodb_index_stats
      These two tables are visible to the end user, so can take part in
      quite arbitrary queries and transactions, so deadlock is possible.
      Therefore we need to allow such deadlocks, as otherwise a user
      could crash a debug build of a server by issuing a specific sequence of
      queries. DB_DEADLOCK error in dict0stats is either handled (see for
      example dict_stats_rename_table), or ignored silently (for example in
      dict_stats_process_entry_from_recalc_pool), but I am not aware of any
      situation in which DB_DEADLOCK could cause a serious problem.
      Most such queries are performed via dict_stats_exec_sql() which logs an
      ERROR in case of a DB_DEADLOCK, and also returns error code to the caller,
      so both the end user and a developer should be aware of a problem in case
      they want to do something about it.

  table_stats and index_stats
      These two tables take part in queries which are issued by background
      threads, and the code which performs these queries can handle failures
      such as deadlocks, because they were expected at design phase. */

  const char *name = lock->index->table->name.m_name;
  return (!strcmp(name, "mysql/innodb_table_stats") ||
          !strcmp(name, "mysql/innodb_index_stats") ||
          !strcmp(name, "mysql/table_stats") ||
          !strcmp(name, "mysql/index_stats"));
}
#endif /* UNIV_DEBUG */

/**
Allocate cached locks for the transaction.
@param trx              allocate cached record locks for this transaction */
void lock_trx_alloc_locks(trx_t *trx) {
  /* We will create trx->lock.table_pool and rec_pool which are protected by
  trx->mutex. In theory nobody else should use the trx object while it is being
  constructed, but how can we (the lock-sys) "know" about it and why risk? */
  trx_mutex_enter(trx);
  ulint sz = REC_LOCK_SIZE * REC_LOCK_CACHE;
  byte *ptr = reinterpret_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sz));

  /* We allocate one big chunk and then distribute it among
  the rest of the elements. The allocated chunk pointer is always
  at index 0. */

  for (ulint i = 0; i < REC_LOCK_CACHE; ++i, ptr += REC_LOCK_SIZE) {
    ut_a(ut::is_aligned_as<lock_t>(ptr));
    trx->lock.rec_pool.push_back(reinterpret_cast<ib_lock_t *>(ptr));
  }

  sz = TABLE_LOCK_SIZE * TABLE_LOCK_CACHE;
  ptr = reinterpret_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sz));

  for (ulint i = 0; i < TABLE_LOCK_CACHE; ++i, ptr += TABLE_LOCK_SIZE) {
    ut_a(ut::is_aligned_as<lock_t>(ptr));
    trx->lock.table_pool.push_back(reinterpret_cast<ib_lock_t *>(ptr));
  }
  trx_mutex_exit(trx);
}

void lock_notify_about_deadlock(const ut::vector<const trx_t *> &trxs_on_cycle,
                                const trx_t *victim_trx) {
  Deadlock_notifier::notify(trxs_on_cycle, victim_trx);
}
