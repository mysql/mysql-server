/*****************************************************************************

Copyright (c) 1996, 2019, Oracle and/or its affiliates. All Rights Reserved.

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
#include "dict0boot.h"
#include "dict0mem.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "lock0priv.h"
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

/** Switch to CATS if the number of threads waiting is above this threshold. */
static const int LOCK_CATS_THRESHOLD = 32;

using Locks = std::vector<std::pair<lock_t *, size_t>,
                          mem_heap_allocator<std::pair<lock_t *, size_t>>>;

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
until the very end */
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
/** Validates the lock system.
 @return true if ok */
static bool lock_validate();

/** Validates the record lock queues on a page.
 @return true if ok */
static bool lock_rec_validate_page(
    const buf_block_t *block) /*!< in: buffer block */
    MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */

/* The lock system */
lock_sys_t *lock_sys = NULL;

/** We store info on the latest deadlock error to this buffer. InnoDB
Monitor will then fetch it and print */
static bool lock_deadlock_found = false;

/** Only created if !srv_read_only_mode. I/O operations on this file require
exclusive lock_sys latch */
static FILE *lock_latest_err_file;

/** Reports that a transaction id is insensible, i.e., in the future. */
void lock_report_trx_id_insanity(
    trx_id_t trx_id,           /*!< in: trx id */
    const rec_t *rec,          /*!< in: user record */
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec, index) */
    trx_id_t max_trx_id)       /*!< in: trx_sys_get_max_trx_id() */
{
  ib::error(ER_IB_MSG_634) << "Transaction id " << trx_id
                           << " associated with record"
                           << rec_offsets_print(rec, offsets) << " in index "
                           << index->name << " of table " << index->table->name
                           << " is greater than the global counter "
                           << max_trx_id << "! The table is corrupted.";
}

/** Checks that a transaction id is sensible, i.e., not in the future.
 @return true if ok */
#ifdef UNIV_DEBUG

#else
static MY_ATTRIBUTE((warn_unused_result))
#endif
bool lock_check_trx_id_sanity(
    trx_id_t trx_id,           /*!< in: trx id */
    const rec_t *rec,          /*!< in: user record */
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets)      /*!< in: rec_get_offsets(rec, index) */
{
  ut_ad(rec_offs_validate(rec, index, offsets));

  trx_id_t max_trx_id = trx_sys_get_max_trx_id();
  bool is_ok = trx_id < max_trx_id;

  if (!is_ok) {
    lock_report_trx_id_insanity(trx_id, rec, index, offsets, max_trx_id);
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
    ut_ad(view == 0 || index->table->is_temporary());
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

  lock_sys = static_cast<lock_sys_t *>(ut_zalloc_nokey(lock_sys_sz));

  void *ptr = &lock_sys[1];

  lock_sys->waiting_threads = static_cast<srv_slot_t *>(ptr);

  lock_sys->last_slot = lock_sys->waiting_threads;

  mutex_create(LATCH_ID_LOCK_SYS, &lock_sys->mutex);

  mutex_create(LATCH_ID_LOCK_SYS_WAIT, &lock_sys->wait_mutex);

  lock_sys->timeout_event = os_event_create(0);

  lock_sys->rec_hash = hash_create(n_cells);
  lock_sys->prdt_hash = hash_create(n_cells);
  lock_sys->prdt_page_hash = hash_create(n_cells);

  if (!srv_read_only_mode) {
    lock_latest_err_file = os_file_create_tmpfile(NULL);
    ut_a(lock_latest_err_file);
  }
}

/** Calculates the fold value of a lock: used in migrating the hash table.
@param[in]	lock	record lock object
@return	folded value */
static ulint lock_rec_lock_fold(const lock_t *lock) {
  return (lock_rec_fold(lock->rec_lock.space, lock->rec_lock.page_no));
}

/** Resize the lock hash tables.
@param[in]	n_cells	number of slots in lock hash table */
void lock_sys_resize(ulint n_cells) {
  hash_table_t *old_hash;

  lock_mutex_enter();

  old_hash = lock_sys->rec_hash;
  lock_sys->rec_hash = hash_create(n_cells);
  HASH_MIGRATE(old_hash, lock_sys->rec_hash, lock_t, hash, lock_rec_lock_fold);
  hash_table_free(old_hash);

  old_hash = lock_sys->prdt_hash;
  lock_sys->prdt_hash = hash_create(n_cells);
  HASH_MIGRATE(old_hash, lock_sys->prdt_hash, lock_t, hash, lock_rec_lock_fold);
  hash_table_free(old_hash);

  old_hash = lock_sys->prdt_page_hash;
  lock_sys->prdt_page_hash = hash_create(n_cells);
  HASH_MIGRATE(old_hash, lock_sys->prdt_page_hash, lock_t, hash,
               lock_rec_lock_fold);
  hash_table_free(old_hash);

  /* need to update block->lock_hash_val */
  for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);

    mutex_enter(&buf_pool->LRU_list_mutex);
    buf_page_t *bpage;
    bpage = UT_LIST_GET_FIRST(buf_pool->LRU);

    while (bpage != NULL) {
      if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE) {
        buf_block_t *block;
        block = reinterpret_cast<buf_block_t *>(bpage);

        block->lock_hash_val =
            lock_rec_hash(bpage->id.space(), bpage->id.page_no());
      }
      bpage = UT_LIST_GET_NEXT(LRU, bpage);
    }
    mutex_exit(&buf_pool->LRU_list_mutex);
  }

  lock_mutex_exit();
}

/** Closes the lock system at database shutdown. */
void lock_sys_close(void) {
  if (lock_latest_err_file != NULL) {
    fclose(lock_latest_err_file);
    lock_latest_err_file = NULL;
  }

  hash_table_free(lock_sys->rec_hash);
  hash_table_free(lock_sys->prdt_hash);
  hash_table_free(lock_sys->prdt_page_hash);

  os_event_destroy(lock_sys->timeout_event);

  mutex_destroy(&lock_sys->mutex);
  mutex_destroy(&lock_sys->wait_mutex);

  srv_slot_t *slot = lock_sys->waiting_threads;

  for (ulint i = 0; i < srv_max_n_threads; i++, ++slot) {
    if (slot->event != NULL) {
      os_event_destroy(slot->event);
    }
  }
  for (auto &cached_lock_mode_name : lock_cached_lock_mode_names) {
    ut_free(const_cast<char *>(cached_lock_mode_name.second));
  }
  lock_cached_lock_mode_names.clear();
  ut_free(lock_sys);

  lock_sys = NULL;
}

/** Gets the size of a lock struct.
 @return size in bytes */
ulint lock_get_size(void) { return ((ulint)sizeof(lock_t)); }

/** Sets the wait flag of a lock and the back pointer in trx to lock.
@param[in]  lock  The lock on which a transaction is waiting */
UNIV_INLINE
void lock_set_lock_and_trx_wait(lock_t *lock) {
  auto trx = lock->trx;
  ut_a(trx->lock.wait_lock == NULL);
  ut_ad(lock_mutex_own());
  ut_ad(trx_mutex_own(trx));

  trx->lock.wait_lock = lock;
  trx->lock.wait_lock_type = lock_get_type_low(lock);
  lock->type_mode |= LOCK_WAIT;
}

/** Gets the gap flag of a record lock.
 @return LOCK_GAP or 0 */
UNIV_INLINE
ulint lock_rec_get_gap(const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  return (lock->type_mode & LOCK_GAP);
}

/** Gets the LOCK_REC_NOT_GAP flag of a record lock.
 @return LOCK_REC_NOT_GAP or 0 */
UNIV_INLINE
ulint lock_rec_get_rec_not_gap(const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  return (lock->type_mode & LOCK_REC_NOT_GAP);
}

/** Gets the waiting insert flag of a record lock.
 @return LOCK_INSERT_INTENTION or 0 */
UNIV_INLINE
ulint lock_rec_get_insert_intention(const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  return (lock->type_mode & LOCK_INSERT_INTENTION);
}

/** Checks if a lock request for a new lock has to wait for request lock2.
 @return true if new lock has to wait for lock2 to be removed */
UNIV_INLINE
bool lock_rec_has_to_wait(
    const trx_t *trx,    /*!< in: trx of new lock */
    ulint type_mode,     /*!< in: precise mode of the new lock
                       to set: LOCK_S or LOCK_X, possibly
                       ORed to LOCK_GAP or LOCK_REC_NOT_GAP,
                       LOCK_INSERT_INTENTION */
    const lock_t *lock2, /*!< in: another record lock; NOTE that
                         it is assumed that this has a lock bit
                         set on the same record as in the new
                         lock we are setting */
    bool lock_is_on_supremum)
/*!< in: true if we are setting the
lock on the 'supremum' record of an
index page: we know then that the lock
request is really for a 'gap' type lock */
{
  ut_ad(trx && lock2);
  ut_ad(lock_get_type_low(lock2) == LOCK_REC);

  if (trx != lock2->trx &&
      !lock_mode_compatible(static_cast<lock_mode>(LOCK_MODE_MASK & type_mode),
                            lock_get_mode(lock2))) {
    /* We have somewhat complex rules when gap type record locks
    cause waits */

    if ((lock_is_on_supremum || (type_mode & LOCK_GAP)) &&
        !(type_mode & LOCK_INSERT_INTENTION)) {
      /* Gap type locks without LOCK_INSERT_INTENTION flag
      do not need to wait for anything. This is because
      different users can have conflicting lock types
      on gaps. */

      return (false);
    }

    if (!(type_mode & LOCK_INSERT_INTENTION) && lock_rec_get_gap(lock2)) {
      /* Record lock (LOCK_ORDINARY or LOCK_REC_NOT_GAP
      does not need to wait for a gap type lock */

      return (false);
    }

    if ((type_mode & LOCK_GAP) && lock_rec_get_rec_not_gap(lock2)) {
      /* Lock on gap does not need to wait for
      a LOCK_REC_NOT_GAP type lock */

      return (false);
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

      return (false);
    }

    return (true);
  }

  return (false);
}

/** Checks if a lock request lock1 has to wait for request lock2.
 @return true if lock1 has to wait for lock2 to be removed */
bool lock_has_to_wait(const lock_t *lock1, /*!< in: waiting lock */
                      const lock_t *lock2) /*!< in: another lock; NOTE that it
                                           is assumed that this has a lock bit
                                           set on the same record as in lock1 if
                                           the locks are record locks */
{
  if (lock1->trx != lock2->trx &&
      !lock_mode_compatible(lock_get_mode(lock1), lock_get_mode(lock2))) {
    if (lock_get_type_low(lock1) == LOCK_REC) {
      ut_ad(lock_get_type_low(lock2) == LOCK_REC);

      /* If this lock request is for a supremum record
      then the second bit on the lock bitmap is set */

      if (lock1->type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE)) {
        return (lock_prdt_has_to_wait(lock1->trx, lock1->type_mode,
                                      lock_get_prdt_from_lock(lock1), lock2));
      } else {
        return (lock_rec_has_to_wait(
            lock1->trx, lock1->type_mode, lock2,
            lock_rec_get_nth_bit(lock1, PAGE_HEAP_NO_SUPREMUM)));
      }
    }

    return (true);
  }

  return (false);
}

/*============== RECORD LOCK BASIC FUNCTIONS ============================*/

/** Looks for a set bit in a record lock bitmap. Returns ULINT_UNDEFINED,
 if none found.
 @return bit index == heap number of the record, or ULINT_UNDEFINED if
 none found */
ulint lock_rec_find_set_bit(
    const lock_t *lock) /*!< in: record lock with at least one bit set */
{
  for (ulint i = 0; i < lock_rec_get_n_bits(lock); ++i) {
    if (lock_rec_get_nth_bit(lock, i)) {
      return (i);
    }
  }

  return (ULINT_UNDEFINED);
}

/** Looks for the next set bit in the record lock bitmap.
@param[in] lock		record lock with at least one bit set
@param[in] heap_no	current set bit
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
UNIV_INLINE
byte lock_rec_reset_nth_bit(lock_t *lock, ulint i) {
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
@param[in,out]	lock record lock
@param[in] i	index of the bit that will be reset
@param[in] type	whether the lock is in wait mode */
void lock_rec_trx_wait(lock_t *lock, ulint i, ulint type) {
  lock_rec_reset_nth_bit(lock, i);

  if (type & LOCK_WAIT) {
    lock_reset_lock_and_trx_wait(lock);
  }
}

/** Determines if there are explicit record locks on a page.
 @return an explicit record lock on the page, or NULL if there are none */
lock_t *lock_rec_expl_exist_on_page(space_id_t space,  /*!< in: space id */
                                    page_no_t page_no) /*!< in: page number */
{
  lock_t *lock;

  lock_mutex_enter();
  /* Only used in ibuf pages, so rec_hash is good enough */
  lock = lock_rec_get_first_on_page_addr(lock_sys->rec_hash, space, page_no);
  lock_mutex_exit();

  return (lock);
}

/** Resets the record lock bitmap to zero. NOTE: does not touch the wait_lock
 pointer in the transaction! This function is used in lock object creation
 and resetting. */
static void lock_rec_bitmap_reset(lock_t *lock) /*!< in: record lock */
{
  ulint n_bytes;

  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  /* Reset to zero the bitmap which resides immediately after the lock
  struct */

  n_bytes = lock_rec_get_n_bits(lock) / 8;

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
  space_id_t space;
  page_no_t page_no;
  lock_t *found_lock = NULL;
  hash_table_t *hash;

  ut_ad(lock_mutex_own());
  ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

  space = in_lock->rec_lock.space;
  page_no = in_lock->rec_lock.page_no;

  hash = lock_hash_get(in_lock->type_mode);

  for (lock = lock_rec_get_first_on_page_addr(hash, space, page_no);
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
@param[in]    block         buffer block containing the record
@param[in]    heap_no       heap number of the record
@param[in]    trx           transaction
@return lock or NULL */
UNIV_INLINE
const lock_t *lock_rec_has_expl(ulint precise_mode, const buf_block_t *block,
                                ulint heap_no, const trx_t *trx) {
  ut_ad(lock_mutex_own());
  ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S ||
        (precise_mode & LOCK_MODE_MASK) == LOCK_X);
  ut_ad(
      !(precise_mode & ~(ulint)(LOCK_MODE_MASK | LOCK_GAP | LOCK_REC_NOT_GAP)));
  ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));
  ut_ad(!(precise_mode & LOCK_PREDICATE));
  ut_ad(!(precise_mode & LOCK_PRDT_PAGE));
  const RecID rec_id{block, heap_no};
  const bool is_on_supremum = rec_id.is_supremum();
  const bool is_rec_not_gap = 0 != (precise_mode & LOCK_REC_NOT_GAP);
  const bool is_gap = 0 != (precise_mode & LOCK_GAP);
  const auto mode = static_cast<lock_mode>(precise_mode & LOCK_MODE_MASK);
  const auto p_implies_q = [](bool p, bool q) { return q || !p; };

  return (Lock_iter::for_each(rec_id, [&](const lock_t *lock) {
    return (!(lock->trx == trx && !lock->is_insert_intention() &&
              lock_mode_stronger_or_eq(lock_get_mode(lock), mode) &&
              !lock->is_waiting() &&
              (is_on_supremum ||
               (p_implies_q(lock->is_record_not_gap(), is_rec_not_gap) &&
                p_implies_q(lock->is_gap(), is_gap)))));
  }));
}

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
  ut_ad(lock_mutex_own());
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

/** Checks if some other transaction has a conflicting explicit lock request
 in the queue, so that we have to wait.
 @return lock or NULL */
static const lock_t *lock_rec_other_has_conflicting(
    ulint mode,               /*!< in: LOCK_S or LOCK_X,
                              possibly ORed to LOCK_GAP or
                              LOC_REC_NOT_GAP,
                              LOCK_INSERT_INTENTION */
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    ulint heap_no,            /*!< in: heap number of the record */
    const trx_t *trx)         /*!< in: our transaction */
{
  ut_ad(lock_mutex_own());
  ut_ad(!(mode & ~(ulint)(LOCK_MODE_MASK | LOCK_GAP | LOCK_REC_NOT_GAP |
                          LOCK_INSERT_INTENTION)));
  ut_ad(!(mode & LOCK_PREDICATE));
  ut_ad(!(mode & LOCK_PRDT_PAGE));

  RecID rec_id{block, heap_no};
  const bool is_supremum = rec_id.is_supremum();

  return (Lock_iter::for_each(rec_id, [=](const lock_t *lock) {
    return (!(lock_rec_has_to_wait(trx, mode, lock, is_supremum)));
  }));
}

/** Checks if some transaction has an implicit x-lock on a record in a secondary
 index.
 @return transaction id of the transaction which has the x-lock, or 0;
 NOTE that this function can return false positives but never false
 negatives. The caller must confirm all positive results by calling
 trx_is_active(). */
static trx_t *lock_sec_rec_some_has_impl(
    const rec_t *rec,     /*!< in: user record */
    dict_index_t *index,  /*!< in: secondary index */
    const ulint *offsets) /*!< in: rec_get_offsets(rec, index) */
{
  trx_t *trx;
  trx_id_t max_trx_id;
  const page_t *page = page_align(rec);

  ut_ad(!lock_mutex_own());
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

  if (max_trx_id < trx_rw_min_trx_id() && !recv_recovery_is_on()) {
    trx = 0;

  } else if (!lock_check_trx_id_sanity(max_trx_id, rec, index, offsets)) {
    /* The page is corrupt: try to avoid a crash by returning 0 */
    trx = 0;

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

  lock_mutex_enter();
  /* If trx_rw_is_active returns non-null impl_trx it only means that impl_trx
  was active at some moment during the call, but might already be in
  TRX_STATE_COMMITTED_IN_MEMORY when we execute the body of the if.
  However, we hold exclusive latch on whole lock_sys, which prevents anyone
  from creating any new explicit locks.
  So, all explicit locks we will see must have been created at the time when
  the transaction was not committed yet. */
  if (trx_t *impl_trx = trx_rw_is_active(trx->id, nullptr, false)) {
    ulint heap_no = page_rec_get_heap_no(rec);
    mutex_enter(&trx_sys->mutex);

    for (const trx_t *t = UT_LIST_GET_FIRST(trx_sys->rw_trx_list); t != nullptr;
         t = UT_LIST_GET_NEXT(trx_list, t)) {
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

  lock_mutex_exit();

  return (holds);
}
#endif /* UNIV_DEBUG */

/** Return approximate number or record locks (bits set in the bitmap) for
 this transaction. Since delete-marked records may be removed, the
 record count will not be precise.
 The caller must be holding lock_sys->mutex. */
ulint lock_number_of_rows_locked(
    const trx_lock_t *trx_lock) /*!< in: transaction locks */
{
  ut_ad(lock_mutex_own());

  return (trx_lock->n_rec_locks);
}

ulint lock_number_of_tables_locked(const trx_t *trx) {
  ut_ad(trx_mutex_own(trx));

  return (trx->lock.table_locks.size());
}

/*============== RECORD LOCK CREATION AND QUEUE MANAGEMENT =============*/

/**
Check of the lock is on m_rec_id.
@param[in] lock			Lock to compare with
@return true if the record lock is on m_rec_id*/
bool RecLock::is_on_row(const lock_t *lock) const {
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  const lock_rec_t &other = lock->rec_lock;

  return (other.space == m_rec_id.m_space_id &&
          other.page_no == m_rec_id.m_page_no &&
          lock_rec_get_nth_bit(lock, m_rec_id.m_heap_no));
}

/**
Do some checks and prepare for creating a new record lock */
void RecLock::prepare() const {
  ut_ad(lock_mutex_own());
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
      ut_ad(0);
  }

  ut_ad(m_index->table->n_ref_count > 0 || !m_index->table->can_be_evicted);
}

/**
Create the lock instance
@param[in, out] trx	The transaction requesting the lock
@param[in, out] index	Index on which record lock is required
@param[in] mode		The lock mode desired
@param[in] rec_id	The record id
@param[in] size		Size of the lock + bitmap requested
@return a record lock instance */
lock_t *RecLock::lock_alloc(trx_t *trx, dict_index_t *index, ulint mode,
                            const RecID &rec_id, ulint size) {
  ut_ad(lock_mutex_own());
  /* We are about to modify structures in trx->lock which needs trx->mutex */
  ut_ad(trx_mutex_own(trx));

  lock_t *lock;

  if (trx->lock.rec_cached >= trx->lock.rec_pool.size() ||
      sizeof(*lock) + size > REC_LOCK_SIZE) {
    ulint n_bytes = size + sizeof(*lock);
    mem_heap_t *heap = trx->lock.lock_heap;

    lock = reinterpret_cast<lock_t *>(mem_heap_alloc(heap, n_bytes));
  } else {
    lock = trx->lock.rec_pool[trx->lock.rec_cached];
    ++trx->lock.rec_cached;
  }

  lock->trx = trx;

  lock->index = index;

  /* Note the creation timestamp */
  ut_d(lock->m_seq = ++lock_sys->m_seq);

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

  rec_lock.space = rec_id.m_space_id;

  rec_lock.page_no = rec_id.m_page_no;

  /* Set the bit corresponding to rec */

  lock_rec_set_nth_bit(lock, rec_id.m_heap_no);

  MONITOR_INC(MONITOR_NUM_RECLOCK);

  MONITOR_INC(MONITOR_RECLOCK_CREATED);

  return (lock);
}

/** Predicate to check if we should use FCFS or CATS for a particular lock of
particular transaction under current server load.

Currently we support CATS algorithm only for LOCK_REC locks which are not
LOCK_PRDT_PAGE nor LOCK_PREDICATE. In particular, methods such as
lock_update_trx_age, lock_update_age, lock_grant_cats all assume that the locks
they operate on are in lock_sys->rec_hash, so it would require some refactoring
to make them work for LOCK_TABLE, LOCK_PRDT_PAGE or LOCK_PREDICATE, and the
benefit is probably not worth the cost here.

Also, the theory described in the paper about CATS assumes a very simple locking
model in which objects can either be S-locked or X-locked, which simplifies
greatly the issue of deciding which lock request is blocked by which. In
reality, our LOCK_REC locks have a very rich structure including gap locks,
locks simultaneously on records and gaps, etc. which means that the "wait for"
relation is neither symmetric (for example insert intention locks have to wait
for gap locks, but not the other way around), nor transitive (for example an
insert-intention lock, has to wait for a gap+record lock, which has to wait for
a record-only lock, but the insert-intention lock does not have to wait for the
record-only lock).
Our current implementation of CATS simply uses a conservative simplification,
that if two lock requests are for the same <space_id, page_id, heap_no> and one
of them is granted and the other is waiting, then we can (for the purpose of
computing "weight" a.k.a. "age") pretend that the latter is waiting for the
former. This seems a bit rough, but works well enough in practice. This
approach, could perhaps be ported for LOCK_TABLE locks without loosing to much
precision, as well. However, for LOCK_PRDT_PAGE and LOCK_PREDICATE, which are
always set for the same INFIMUM heap_no, similar simplification could lead to a
conclusion that each waiting predicate lock is blocked by each granted predicate
lock, which might be too cumbersome. It is also not clear how to make it more
fine-grained, as predicate locks have quite rich structure themselves (in
essence it is about relations of geometrical shapes, mostly rectangles), and
testing each pair for conflict is on the one hand a bit complex, and on the
other hand does not lead to a clear cut separation into independent queues, but
rather a complicated graph of relations. Note, that I strategically avoided if I
mean LOCK_PRDT_PAGE or LOCK_PREDICATE. This is because the two interact with
each other which makes it all even more complicated to refactor/implement, as
the current CATS implementation for LOCK_REC simply iterates over a single
bucket of a single hash map, but for predicate locks we might need to scan both
lock_sys->prdt_page_hash and lock_sys->prdt_hash.

@param[in]	lock		Lock to check
@return true if FCFS algorithm should be used */
static bool lock_use_fcfs(const lock_t *lock) {
  ut_ad(lock_mutex_own());
  /* We read n_waiting without holding lock_wait_mutex_enter/exit, so we use
  atomic read, to avoid torn read. Because `lock_use_fcfs` is just a heuristic
  which can tolerate a slightly desynchronized (w.r.t. other variables)
  information called quite often during trx->age updating, we use relaxed
  memory ordering */
  return (thd_is_replication_slave_thread(lock->trx->mysql_thd) ||
          lock_sys->n_waiting.load(std::memory_order_relaxed) <
              LOCK_CATS_THRESHOLD ||
          !lock->is_record_lock() || lock->is_predicate());
}

/** Insert lock record to the head of the queue.
@param[in,out]	lock_hash	Hash table containing the locks
@param[in,out]	lock		Record lock instance to insert
@param[in]	rec_id	        Record being locked */
static void lock_rec_insert_cats(hash_table_t *lock_hash, lock_t *lock,
                                 const RecID &rec_id) {
  ut_ad(rec_id.matches(lock));
  ut_ad(lock_mutex_own());

  /* Move the target lock to the head of the list. */
  auto cell =
      hash_get_nth_cell(lock_hash, hash_calc_hash(rec_id.fold(), lock_hash));

  ut_ad(lock != cell->node);

  auto next = reinterpret_cast<lock_t *>(cell->node);

  cell->node = lock;
  lock->hash = next;
}

/** Recursively update the trx_t::age for all transactions in the
lock waits for graph.
@param[in,out]	trx		Transaction to update
@param[in]	age		delta to add or subtract */
static void lock_update_trx_age(trx_t *trx, int32_t age) {
  ut_ad(lock_mutex_own());

  if (age == 0 || trx->age_updated == lock_sys->mark_age_updated) {
    return;
  }

  ut_ad(trx->age_updated < lock_sys->mark_age_updated);

  /* In an incorrect implementation the `trx->age` could grow exponentially due
  to double-counting trx's own weight when a cycle is formed in the
  wait-for graph. A correct implementation should keep the `trx->age` value
  close to its theoretical meaning of "number of other trxs waiting for me"
  which is no more than N in the test scenario. The threshold in the assertion
  below (`trx->age<100`) was chosen as a number which satisfies N < 100 < 2^N,
  where N is the number of transactions involved in this test case (N=10).
  There is also another test case which creates N=100 transactions each of which
  in a correct implementation should not affect the trx->age, yet in an
  incorrect implementation each caused a constant increment (~4) - to detect
  that the assertion threshold needs to be between 0 and 100*constant.
  There are many other ways in which current implementation of CATS can cause
  the trx->age to grow arbitrary large or negative, as it is incapable of
  properly accounting for multiple paths in wait-for-graph connecting the same
  pair of transactions. Therefore, instead of naive:
     trx->age += age;
  we use a temporary workaround to avoid over- and under-flows.
  The RHS in the += assignment will be equal to one of three possible values,
  which make the end result equal to MAX_REASONABLE_AGE, 0 or trx->age+age, and
  the third option is used if and only if it is inside the valid range of
  <0,MAX_REASONABLE_AGE>. */

  const int32_t MAX_REASONABLE_AGE = std::min<ulint>(srv_max_n_threads, 100000);
  trx->age += std::min(MAX_REASONABLE_AGE - trx->age, std::max(-trx->age, age));

  DBUG_EXECUTE_IF("lock_update_trx_age_check_age_limit", ut_a(trx->age < 100););
  ut_ad(0 <= trx->age);
  ut_ad(trx->age <= MAX_REASONABLE_AGE);

  trx->age_updated = lock_sys->mark_age_updated;

  if (trx->state != TRX_STATE_ACTIVE || trx->lock.wait_lock == nullptr) {
    return;
  }

  const auto wait_lock = trx->lock.wait_lock;

  /* could be table level lock like autoinc or predicate lock */
  if (lock_use_fcfs(wait_lock)) {
    return;
  }

  auto heap_no = lock_rec_find_set_bit(wait_lock);
  auto space = wait_lock->rec_lock.space;
  auto page_no = wait_lock->rec_lock.page_no;

  RecID rec_id(space, page_no, heap_no);

  ut_ad(!wait_lock->is_predicate());

  Lock_iter::for_each(rec_id, [&](const lock_t *lock) {
    if (!lock->is_waiting() && trx != lock->trx && !lock_use_fcfs(lock) &&
        lock->trx->age_updated < lock_sys->mark_age_updated) {
      lock_update_trx_age(lock->trx, age);
    }

    return (true);
  });
}

/** Update the age of the transactions in the queue, when a new
record lock is created.
@param[in,out]	new_lock	The lock that was just created
@param[in]	heap_no		The heap number of the lock in the page */
static void lock_update_age(lock_t *new_lock, ulint heap_no) {
  ut_ad(lock_mutex_own());

  if (lock_use_fcfs(new_lock) || new_lock->trx->state != TRX_STATE_ACTIVE) {
    return;
  }

  using Trxs = std::unordered_set<trx_t *>;

  Trxs trxs;
  int32_t age = 0;
  auto space = new_lock->rec_lock.space;
  auto page_no = new_lock->rec_lock.page_no;

  ut_ad(!new_lock->is_predicate());

  RecID rec_id{space, page_no, heap_no};
  const auto wait = lock_get_wait(new_lock);

  Lock_iter::for_each(rec_id, [&](const lock_t *lock) {
    const auto trx = lock->trx;

    if (new_lock->trx != trx) {
      if (wait) {
        if (!lock->is_waiting() && !lock_use_fcfs(lock) &&
            trx->state == TRX_STATE_ACTIVE) {
          trxs.insert(trx);
        }

      } else if (lock->is_waiting()) {
        age += trx->age + 1;
      }
    }

    return (true);
  });

  ++lock_sys->mark_age_updated;

  if (wait) {
    for (auto trx : trxs) {
      lock_update_trx_age(trx, new_lock->trx->age + 1);
    }

  } else if (age > 0) {
    DEBUG_SYNC_C("lock_update_age_will_check_state_again");

    lock_update_trx_age(new_lock->trx, age);
  }
}

/** Add the lock to the record lock hash and the transaction's lock list
@param[in,out] lock	Newly created record lock to add to the rec hash
@param[in] add_to_hash	If the lock should be added to the hash table */
void RecLock::lock_add(lock_t *lock, bool add_to_hash) {
  ut_ad(lock_mutex_own());
  ut_ad(trx_mutex_own(lock->trx));

  bool wait = m_mode & LOCK_WAIT;

  if (add_to_hash) {
    hash_table_t *lock_hash = lock_hash_get(m_mode);

    ++lock->index->table->n_rec_locks;

    if (!lock_use_fcfs(lock) && !wait) {
      lock_rec_insert_cats(lock_hash, lock, m_rec_id);
    } else {
      ulint key = m_rec_id.fold();
      HASH_INSERT(lock_t, hash, lock_hash, key, lock);
    }
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  /* The performance schema THREAD_ID and EVENT_ID are used only
  when DATA_LOCKS are exposed.  */
  PSI_THREAD_CALL(get_current_thread_event_id)
  (&lock->m_psi_internal_thread_id, &lock->m_psi_event_id);
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
#endif /* HAVE_PSI_THREAD_INTERFACE */

  UT_LIST_ADD_LAST(lock->trx->lock.trx_locks, lock);

  if (wait) {
    lock_set_lock_and_trx_wait(lock);
  } else {
    lock_update_age(lock, lock_rec_find_set_bit(lock));
  }
}

/** Create a new lock.
@param[in,out] trx		Transaction requesting the lock
@param[in] add_to_hash		add the lock to hash table
@param[in] prdt			Predicate lock (optional)
@return a new lock instance */
lock_t *RecLock::create(trx_t *trx, bool add_to_hash, const lock_prdt_t *prdt) {
  ut_ad(lock_mutex_own());

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

  if (prdt != NULL && (m_mode & LOCK_PREDICATE)) {
    lock_prdt_set_prdt(lock, prdt);
  }

  lock_add(lock, add_to_hash);

  return (lock);
}

/**
Collect the transactions that will need to be rolled back asynchronously
@param[in, out] hit_list    The list of transactions to be rolled back, to which
                            the trx should be appended.
@param[in]      hp_trx_id   The id of the blocked High Priority Transaction
@param[in, out] trx	    The blocking transaction to be rolled back */
static void lock_mark_trx_for_rollback(hit_list_t &hit_list, trx_id_t hp_trx_id,
                                       trx_t *trx) {
  trx->abort = true;

  ut_ad(!trx->read_only);
  ut_ad(trx_mutex_own(trx));
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK));
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK_ASYNC));
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE));

  /* Note that we will attempt an async rollback. The _ASYNC
  flag will be cleared if the transaction is rolled back
  synchronously before we get a chance to do it. */

  trx->in_innodb |= TRX_FORCE_ROLLBACK | TRX_FORCE_ROLLBACK_ASYNC;

  bool cas;
  os_thread_id_t thread_id = os_thread_get_curr_id();

  cas = os_compare_and_swap_thread_id(&trx->killed_by, 0, thread_id);

  ut_a(cas);

  hit_list.push_back(hit_list_t::value_type(trx));

#ifdef UNIV_DEBUG
  THD *thd = trx->mysql_thd;

  if (thd != NULL) {
    char buffer[1024];
    ib::info(ER_IB_MSG_636)
        << "Blocking transaction: ID: " << trx->id << " - "
        << " Blocked transaction ID: " << hp_trx_id << " - "
        << thd_security_context(thd, buffer, sizeof(buffer), 512);
  }
#endif /* UNIV_DEBUG */
}

/** Creates a new edge in wait-for graph, from waiter to blocker
@param[in]  waiter    The transaction that has to wait for blocker
@param[in]  blocker   The transaction wich causes waiter to wait */
static void lock_create_wait_for_edge(trx_t *waiter, trx_t *blocker) {
  ut_ad(trx_mutex_own(waiter));
  ut_ad(waiter->lock.wait_lock != nullptr);
  ut_ad(lock_mutex_own());
  ut_ad(waiter->lock.blocking_trx.load() == nullptr);
  /* We don't call lock_wait_request_check_for_cycles() here as it
  would be slightly premature: the trx is not yet inserted into a slot of
  lock_sys->waiting_threads at this point, and thus it would be invisible to
  the thread which analyzes these slots. What we do instead is to let the
  lock_wait_table_reserve_slot() function be responsible for calling
  lock_wait_request_check_for_cycles() once it insert the trx to a
  slot.*/
  waiter->lock.blocking_trx.store(blocker);
}

/**
Setup the requesting transaction state for lock grant
@param[in,out] lock		Lock for which to change state */
void RecLock::set_wait_state(lock_t *lock) {
  ut_ad(lock_mutex_own());
  ut_ad(m_trx == lock->trx);
  ut_ad(trx_mutex_own(m_trx));
  ut_ad(lock_get_wait(lock));

  m_trx->lock.wait_started = ut_time();

  m_trx->lock.que_state = TRX_QUE_LOCK_WAIT;

  m_trx->lock.was_chosen_as_deadlock_victim = false;

  bool stopped = que_thr_stop(m_thr);
  ut_a(stopped);
}

#ifdef UNIV_DEBUG
UNIV_INLINE bool lock_current_thread_handles_trx(const trx_t *trx) {
  return (!trx->mysql_thd || trx->mysql_thd == current_thd);
}
#endif

dberr_t RecLock::add_to_waitq(const lock_t *wait_for, const lock_prdt_t *prdt) {
  ut_ad(lock_mutex_own());
  ut_ad(m_trx == thr_get_trx(m_thr));

  /* It is not that the body of this function requires trx->mutex, but some of
  the functions it calls require it and it so happens that we always posses it
  so it makes reasoning about code easier if we simply assert this fact. */
  ut_ad(trx_mutex_own(m_trx));

  DEBUG_SYNC_C("rec_lock_add_to_waitq");

  if (m_trx->in_innodb & TRX_FORCE_ROLLBACK_ASYNC) {
    return (DB_DEADLOCK);
  }

  m_mode |= LOCK_WAIT;

  /* Do the preliminary checks, and set query thread state */

  prepare();

  bool high_priority = trx_is_high_priority(m_trx);

  /* Don't queue the lock to hash table, if high priority transaction. */
  lock_t *lock = create(m_trx, !high_priority, prdt);

  /* Attempt to jump over the low priority waiting locks. */
  if (high_priority && jump_queue(lock, wait_for)) {
    /* Lock is granted */
    return (DB_SUCCESS_LOCKED_REC);
  }
  lock_create_wait_for_edge(m_trx, wait_for->trx);

  ut_ad(lock_get_wait(lock));

  set_wait_state(lock);

  MONITOR_INC(MONITOR_LOCKREC_WAIT);

  lock_update_age(lock, m_rec_id.m_heap_no);

  /* m_trx->mysql_thd is NULL if it's an internal trx. So current_thd
   is used */

  thd_report_row_lock_wait(current_thd, wait_for->trx->mysql_thd);

  return (DB_LOCK_WAIT);
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
                                  Defaults to false.
@return lock where the bit was set */
static void lock_rec_add_to_queue(ulint type_mode, const buf_block_t *block,
                                  const ulint heap_no, dict_index_t *index,
                                  trx_t *trx,
                                  const bool we_own_trx_mutex = false) {
#ifdef UNIV_DEBUG
  ut_ad(lock_mutex_own());
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

      lock_t *lock =
          lock_rec_find_similar_on_page(type_mode, heap_no, first_lock, trx);

      if (lock != NULL) {
        lock_rec_set_nth_bit(lock, heap_no);
        lock_update_age(lock, heap_no);

        return;
      }
    }
  }

  RecLock rec_lock(index, block, heap_no, type_mode);

  if (!we_own_trx_mutex) {
    trx_mutex_enter(trx);
  }
  rec_lock.create(trx, true);
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
UNIV_INLINE
lock_rec_req_status lock_rec_lock_fast(
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
  ut_ad(lock_mutex_own());
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

  if (lock == NULL) {
    if (!impl) {
      RecLock rec_lock(index, block, heap_no, mode);

      trx_mutex_enter(trx);
      rec_lock.create(trx, true);
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
        lock_update_age(lock, heap_no);
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
@param[in]      mode	    requested lock mode: LOCK_X or LOCK_S
@param[in]      block	    buffer block containing the record to be locked
@param[in]      heap_no	    heap number of the record to be locked
@param[in]      index	    index of record to be locked
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
  ut_ad(nullptr == lock_rec_other_has_conflicting(mode, block, heap_no, trx));

  /* It might be the case we already have one, so we first check that. */
  if (lock_rec_has_expl(mode, block, heap_no, trx) == nullptr) {
    lock_rec_add_to_queue(LOCK_REC | mode, block, heap_no, index, trx);
  }
}
/** This is the general, and slower, routine for locking a record. This is a
low-level function which does NOT look at implicit locks! Checks lock
compatibility within explicit locks. This function sets a normal next-key
lock, or in the case of a page supremum record, a gap type lock.
@param[in]	impl		if true, no lock is set	if no wait is
                                necessary: we assume that the caller will
                                set an implicit lock
@param[in]	sel_mode	select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOCKED, or SELECT_NO_WAIT
@param[in]	mode		lock mode: LOCK_X or LOCK_S possibly ORed to
                                either LOCK_GAP or LOCK_REC_NOT_GAP
@param[in]	block		buffer block containing	the record
@param[in]	heap_no		heap number of record
@param[in]	index		index of record
@param[in,out]	thr		query thread
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
DB_SKIP_LOCKED, or DB_LOCK_NOWAIT */
static dberr_t lock_rec_lock_slow(bool impl, select_mode sel_mode, ulint mode,
                                  const buf_block_t *block, ulint heap_no,
                                  dict_index_t *index, que_thr_t *thr) {
  ut_ad(lock_mutex_own());
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

  const lock_t *wait_for =
      lock_rec_other_has_conflicting(mode, block, heap_no, trx);

  if (wait_for != nullptr) {
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

        dberr_t err = rec_lock.add_to_waitq(wait_for);

        trx_mutex_exit(trx);

        ut_ad(err == DB_SUCCESS_LOCKED_REC || err == DB_LOCK_WAIT ||
              err == DB_DEADLOCK);
        return (err);
    }
  }
  if (!impl) {
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
@param[in]	impl		if true, no lock is set	if no wait is
                                necessary: we assume that the caller will
                                set an implicit lock
@param[in]	sel_mode	select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOCKED, or SELECT_NO_WAIT
@param[in]	mode		lock mode: LOCK_X or LOCK_S possibly ORed to
                                either LOCK_GAP or LOCK_REC_NOT_GAP
@param[in]	block		buffer block containing	the record
@param[in]	heap_no		heap number of record
@param[in]	index		index of record
@param[in,out]	thr		query thread
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
DB_SKIP_LOCKED, or DB_LOCK_NOWAIT */
static dberr_t lock_rec_lock(bool impl, select_mode sel_mode, ulint mode,
                             const buf_block_t *block, ulint heap_no,
                             dict_index_t *index, que_thr_t *thr) {
  ut_ad(lock_mutex_own());
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
  space_id_t space;
  page_no_t page_no;
  ulint heap_no;
  ulint bit_mask;
  ulint bit_offset;
  hash_table_t *hash;

  ut_ad(lock_mutex_own());
  ut_ad(lock_get_wait(wait_lock));
  ut_ad(lock_get_type_low(wait_lock) == LOCK_REC);

  space = wait_lock->rec_lock.space;
  page_no = wait_lock->rec_lock.page_no;
  heap_no = lock_rec_find_set_bit(wait_lock);

  bit_offset = heap_no / 8;
  bit_mask = static_cast<ulint>(1) << (heap_no % 8);

  hash = lock_hash_get(wait_lock->type_mode);

  for (lock = lock_rec_get_first_on_page_addr(hash, space, page_no);
       lock != wait_lock; lock = lock_rec_get_next_on_page_const(lock)) {
    const byte *p = (const byte *)&lock[1];

    if ((blocking_trx == nullptr || blocking_trx == lock->trx) &&
        heap_no < lock_rec_get_n_bits(lock) && (p[bit_offset] & bit_mask) &&
        lock_has_to_wait(wait_lock, lock)) {
      return (lock);
    }
  }

  return (nullptr);
}

/** Grants a lock to a waiting lock request and releases the waiting
 transaction. The caller must hold lock_sys->mutex but not lock->trx->mutex. */
static void lock_grant(lock_t *lock) /*!< in/out: waiting lock request */
{
  ut_ad(lock_mutex_own());
  ut_ad(!trx_mutex_own(lock->trx));

  trx_mutex_enter(lock->trx);

  if (lock_get_mode(lock) == LOCK_AUTO_INC) {
    dict_table_t *table = lock->tab_lock.table;

    if (table->autoinc_trx == lock->trx) {
      ib::error(ER_IB_MSG_637) << "Transaction already had an"
                               << " AUTO-INC lock!";
    } else {
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

/**
Jump the queue for the record over all low priority transactions and
add the lock. If all current granted locks are compatible, grant the
lock. Otherwise, mark all granted transaction for asynchronous
rollback and add to hit list.
@param[in, out]	lock		Lock being requested
@param[in]	conflict_lock	First conflicting lock from the head
@return true if the lock is granted */
bool RecLock::jump_queue(lock_t *lock, const lock_t *conflict_lock) {
  ut_ad(m_trx == lock->trx);
  ut_ad(trx_mutex_own(m_trx));
  ut_ad(conflict_lock->trx != m_trx);
  ut_ad(trx_is_high_priority(m_trx));
  ut_ad(m_rec_id.m_heap_no != UINT32_UNDEFINED);

  /* Find out the position to add the lock. If there are other high
  priority transactions in waiting state then we should add it after
  the last high priority transaction. Otherwise, we can add it after
  the last granted lock jumping over the wait queue. */
  bool grant_lock = lock_add_priority(lock, conflict_lock);

  if (grant_lock) {
    ut_ad(conflict_lock->trx->lock.que_state == TRX_QUE_LOCK_WAIT);
    ut_ad(conflict_lock->trx->lock.wait_lock == conflict_lock);

#ifdef UNIV_DEBUG
    ib::info(ER_IB_MSG_638)
        << "Granting High Priority Transaction (ID): " << lock->trx->id
        << " the lock jumping over"
        << " waiting Transaction (ID): " << conflict_lock->trx->id;
#endif /* UNIV_DEBUG */

    lock_reset_lock_and_trx_wait(lock);
    return (true);
  }

  return (false);
}

bool RecLock::lock_add_priority(lock_t *lock, const lock_t *conflict_lock) {
  /* If the first conflicting lock is waiting for the current row,
  then all other granted locks are compatible and the lock can be
  directly granted if no other high priority transactions are
  waiting. We need to recheck with all granted transaction as there
  could be granted GAP or Intention locks down the queue. */
  bool grant_lock = (conflict_lock->is_waiting());
  lock_t *lock_head = NULL;
  lock_t *grant_position = NULL;
  lock_t *add_position = NULL;

  /* Different lock (such as predicate lock) are on different hash */
  hash_table_t *lock_hash = lock_hash_get(m_mode);

  HASH_SEARCH(hash, lock_hash, m_rec_id.fold(), lock_t *, lock_head,
              ut_ad(lock_head->is_record_lock()), true);

  ut_ad(lock_head);

  for (lock_t *next = lock_head; next != NULL; next = next->hash) {
    /* check only for locks on the current row */
    if (!is_on_row(next)) {
      continue;
    }

    if (next->is_waiting()) {
      /* grant lock position is the granted lock just before
      the first wait lock in the queue. */
      if (grant_position == NULL) {
        grant_position = add_position;
      }

      if (trx_is_high_priority(next->trx)) {
        grant_lock = false;
        add_position = next;
      }
    } else {
      add_position = next;
      /* Cannot grant lock if there is any conflicting
      granted lock. */
      if (grant_lock && lock_has_to_wait(lock, next)) {
        grant_lock = false;
      }
    }
  }

  /* If the lock is to be granted it is safe to add before the first
  waiting lock in the queue. */
  if (grant_lock) {
    ut_ad(!lock_has_to_wait(lock, grant_position));
    add_position = grant_position;
  }

  ut_ad(add_position != NULL);

  /* Add the lock to lock hash table. */
  lock->hash = add_position->hash;
  add_position->hash = lock;
  ++lock->index->table->n_rec_locks;

  return (grant_lock);
}

void lock_make_trx_hit_list(trx_t *hp_trx, hit_list_t &hit_list) {
  trx_mutex_enter(hp_trx);
  const trx_id_t hp_trx_id = hp_trx->id;
  ut_ad(lock_current_thread_handles_trx(hp_trx));
  ut_ad(trx_is_high_priority(hp_trx));
  const lock_t *lock = hp_trx->lock.wait_lock;
  bool waits_for_record = (nullptr != lock && lock->is_record_lock());
  trx_mutex_exit(hp_trx);
  if (!waits_for_record) {
    return;
  }

  lock_mutex_enter();

  /* Check again */
  if (lock != hp_trx->lock.wait_lock) {
    lock_mutex_exit();
    return;
  }
  RecID rec_id{lock, lock_rec_find_set_bit(lock)};
  Lock_iter::for_each(
      rec_id,
      [&](lock_t *next) {
        trx_t *trx = next->trx;
        /* Check only for conflicting, granted locks on the current
        row. Currently, we don't rollback read only transactions,
        transactions owned by background threads. */
        if (trx == hp_trx || next->is_waiting() || trx->read_only ||
            trx->mysql_thd == NULL || !lock_has_to_wait(lock, next)) {
          return true;
        }

        trx_mutex_enter(trx);

        /* Skip high priority transactions, if already marked for
        abort by some other transaction or if ASYNC rollback is
        disabled. A transaction must complete kill/abort of a
        victim transaction once marked and added to hit list. */
        if (trx_is_high_priority(trx) ||
            (trx->in_innodb & TRX_FORCE_ROLLBACK) != 0 ||
            (trx->in_innodb & TRX_FORCE_ROLLBACK_ASYNC) != 0 ||
            (trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE) != 0 || trx->abort) {
          trx_mutex_exit(trx);

          return true;
        }

        /* If the transaction is waiting on some other resource then
        wake it up with DEAD_LOCK error so that it can rollback. */
        if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
          /* Assert that it is not waiting for current record. */
          ut_ad(trx->lock.wait_lock != next);
#ifdef UNIV_DEBUG
          ib::info(ER_IB_MSG_639)
              << "High Priority Transaction (ID): " << lock->trx->id
              << " waking up blocking"
              << " transaction (ID): " << trx->id;
#endif /* UNIV_DEBUG */
          trx->lock.was_chosen_as_deadlock_victim = true;

          lock_cancel_waiting_and_release(trx->lock.wait_lock, true);

          trx_mutex_exit(trx);
          return true;
        }

        /* Mark for ASYNC Rollback and add to hit list. */
        lock_mark_trx_for_rollback(hit_list, hp_trx_id, trx);

        trx_mutex_exit(trx);
        return true;
      },
      lock->hash_table());

  lock_mutex_exit();
}

/** Cancels a waiting record lock request and releases the waiting transaction
 that requested it. NOTE: does NOT check if waiting lock requests behind this
 one can now be granted! */
static void lock_rec_cancel(
    lock_t *lock) /*!< in: waiting record lock request */
{
  ut_ad(lock_mutex_own());
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

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
  ut_ad(lock_mutex_own());
  ut_ad(waiting_lock->is_waiting());
  ut_ad(lock_has_to_wait(waiting_lock, blocking_lock));
  /* Still needs to wait, but perhaps the reason has changed */
  if (waiting_lock->trx->lock.blocking_trx.load() != blocking_lock->trx) {
    waiting_lock->trx->lock.blocking_trx.store(blocking_lock->trx);
    /* We call lock_wait_request_check_for_cycles() because the outgoing edge of
    wait_lock->trx has changed it's endpoint and we need to analyze the
    wait-for-graph again. */
    lock_wait_request_check_for_cycles();
  }
}

/** Checks if a waiting record lock request still has to wait for granted locks.
@param[in]	wait_lock		Waiting record lock
@param[in]	granted			Granted record locks
@param[in]	new_granted_index	Start of new granted locks
@return The conflicting lock which is the reason wait_lock has to wait
or nullptr if it can be granted now */
template <typename Container>
static const lock_t *lock_rec_has_to_wait_cats(
    const typename Container::value_type &wait_lock, const Container &granted,
    size_t new_granted_index)

{
  ut_ad(lock_mutex_own());
  ut_ad(wait_lock.first->is_record_lock());

  ut_ad(new_granted_index <= granted.size());

  for (size_t i = 0; i < new_granted_index; ++i) {
    const auto granted_lock = granted[i].first;

    ut_ad(i <= granted[i].second - 1 &&
          (i == 0 || granted[i - 1].second - granted[i].second));

    ut_ad(wait_lock.second != granted[i].second);

    if (wait_lock.second < granted[i].second) {
      break;

    } else if (lock_has_to_wait(wait_lock.first, granted_lock)) {
      return (granted_lock);
    }
  }

  for (size_t i = new_granted_index; i < granted.size(); ++i) {
    const auto granted_lock = granted[i].first;

    ut_ad(granted[i].second == 0);
    ut_ad(granted_lock->trx->error_state != DB_DEADLOCK);
    ut_ad(!granted_lock->trx->lock.was_chosen_as_deadlock_victim);

    if (lock_has_to_wait(wait_lock.first, granted_lock)) {
      return (granted_lock);
    }
  }

  return (nullptr);
}

/** Lock priority comparator. */
struct CATS_Lock_priority {
  /** Check if LHS has higher priority than RHS.
  @param[in]	lhs	Lock to compare priority
  @param[in]	rhs	Lock to compare priority
  1. If neither of them is a wait lock, the LHS one has higher priority.
  2. If only one of them is a wait lock, it has lower priority.
  3. If both are high priority transactions, the one with a lower seq
     number has higher priority.
  4. High priority transaction has higher priority.
  5. Otherwise, the one with an older transaction has higher priority.
  The first two cases can not happen because we only sort waiting locks.
  @returns true if lhs has higher priority, false otherwise. */
  bool operator()(const Locks::value_type &lhs,
                  const Locks::value_type &rhs) const {
    ut_ad(lhs.first->is_record_lock());
    ut_ad(rhs.first->is_record_lock());
    ut_ad(lhs.first->is_waiting());
    ut_ad(rhs.first->is_waiting());

    if (trx_is_high_priority(lhs.first->trx) &&
        trx_is_high_priority(rhs.first->trx)) {
      return (lhs.second < rhs.second);

    } else if (trx_is_high_priority(lhs.first->trx)) {
      return (true);

    } else if (trx_is_high_priority(rhs.first->trx)) {
      return (false);
    }

    return (lhs.first->trx->age > rhs.first->trx->age);
  }
};

/** Grant a lock to waiting transactions.
@param[in,out]		hash		Record lock hash table
@param[in]		in_lock		Lock to check
@param[in]		heap_no		Heap number within the page on which
                                        the lock was held */
static void lock_grant_cats(hash_table_t *hash, lock_t *in_lock,
                            ulint heap_no) {
  ut_ad(lock_mutex_own());
  ut_ad(in_lock->is_record_lock());

  /* Preallocate for 4 lists with 32 locks. */
  std::unique_ptr<mem_heap_t, decltype(&mem_heap_free)> heap(
      mem_heap_create(sizeof(Locks::value_type) * 32 * 4), mem_heap_free);

  RecID rec_id{in_lock, heap_no};
  Locks waiting{Locks::allocator_type{heap.get()}};
  Locks granted{Locks::allocator_type{heap.get()}};

  ulint seq = 0;
  const auto in_trx = in_lock->trx;

  ut_ad(!in_lock->is_predicate());

  Lock_iter::for_each(rec_id, [&](lock_t *lock) {
    /* Split the locks in the queue into waiting and
    granted queues, additionally set the ordinal value
    of the waiting locks in the original/current wait queue. */

    ++seq;

    if (!lock->is_waiting()) {
      granted.push_back(std::make_pair(lock, seq));

    } else {
      waiting.push_back(std::make_pair(lock, seq));
    }

    return (true);
  });

  if (waiting.empty() && granted.empty()) {
    /* Nothing to grant. */
    return;
  }

  /* Reorder the record lock wait queue on the CATS priority. */
  std::stable_sort(waiting.begin(), waiting.end(), CATS_Lock_priority());

  int32_t sub_age = 0;
  int32_t add_age = 0;

  Locks new_granted{Locks::allocator_type{heap.get()}};
  Locks granted_all{granted, Locks::allocator_type{heap.get()}};

  /* New granted locks will be added from this index. */
  auto new_granted_index = granted.size();

#ifdef UNIV_DEBUG
  /* We rely on the sequence (ordinal) number during lock
  grant in lock_rec_has_to_wait(). */

  for (size_t i = 0; i < granted_all.size(); ++i) {
    auto curr = granted_all[i].second;
    auto prev = (i == 0) ? 0 : granted_all[i - 1].second;

    ut_ad(i <= granted_all[i].second - 1 && (i == 0 || prev < curr));
  }
#endif /* UNIV_DEBUG */

  granted_all.reserve(granted_all.capacity() + waiting.size());

  for (const auto &wait_lock : waiting) {
    /* Check if the transactions in the waiting queue have
    to wait for locks granted above. If they don't have to
    wait then grant them the locks and add them to the granted
    queue. */

    auto lock = wait_lock.first;
    const auto trx = lock->trx;
    const auto age = trx->age + 1;

    /* We don't expect to be a waiting trx, and we can't grant to ourselves as
    that would require entering trx->mutex while holding in_trx->mutex. */
    ut_ad(trx != in_trx);
    const lock_t *blocking_lock =
        lock_rec_has_to_wait_cats(wait_lock, granted_all, new_granted_index);
    if (blocking_lock == nullptr) {
      lock_grant(lock);

      HASH_DELETE(lock_t, hash, hash, rec_id.fold(), lock);

      lock_rec_insert_cats(hash, lock, rec_id);

      new_granted.push_back(wait_lock);
      granted_all.push_back(std::make_pair(lock, 0));

      sub_age -= age;

    } else {
      lock_update_wait_for_edge(lock, blocking_lock);
      add_age += age;
    }
  }

  ut_ad(!granted_all.empty());

  ++lock_sys->mark_age_updated;

  if (in_lock->is_waiting()) {
    sub_age -= in_trx->age + 1;
  }

  for (const auto &elem : granted) {
    auto lock = elem.first;
    const auto trx = lock->trx;
    int32_t age_compensate = 0;

    for (const auto new_granted_lock : new_granted) {
      if (lock->trx == new_granted_lock.first->trx) {
        age_compensate += trx->age + 1;
      }
    }

    if (lock->trx != in_trx) {
      lock_update_trx_age(trx, sub_age + age_compensate);
    }
  }

  for (const auto &elem : new_granted) {
    auto lock = elem.first;
    const auto trx = lock->trx;
    int32_t age_compensate = 0;

    for (const auto wait_lock : waiting) {
      if (wait_lock.first->is_waiting() && lock->trx == wait_lock.first->trx) {
        age_compensate -= trx->age + 1;
      }
    }

    if (lock->trx != in_trx) {
      lock_update_trx_age(trx, add_age + age_compensate);
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

/** Grant lock to waiting requests that no longer conflicts
@param[in,out]	in_lock		record lock object: grant all non-conflicting
                                locks waiting behind this lock object
@param[in]	use_fcfs	true -> use first come first served strategy */
static void lock_rec_grant(lock_t *in_lock, bool use_fcfs) {
  auto space = in_lock->space_id();
  auto page_no = in_lock->page_no();
  auto lock_hash = in_lock->hash_table();

  if (use_fcfs || lock_use_fcfs(in_lock)) {
    /* Check if waiting locks in the queue can now be granted:
    grant locks if there are no conflicting locks ahead. Stop at
    the first X lock that is waiting or has been granted. */

    for (auto lock = lock_rec_get_first_on_page_addr(lock_hash, space, page_no);
         lock != nullptr; lock = lock_rec_get_next_on_page(lock)) {
      lock_grant_or_update_wait_for_edge_if_waiting(lock, in_lock->trx);
    }

  } else {
    for (ulint heap_no = 0; heap_no < lock_rec_get_n_bits(in_lock); ++heap_no) {
      if (lock_rec_get_nth_bit(in_lock, heap_no)) {
        lock_grant_cats(lock_hash, in_lock, heap_no);
      }
    }
  }
}

/** Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed.
@param[in,out]	in_lock		record lock object: all record locks which
                                are contained in this lock object are removed;
                                transactions waiting behind will get their
                                lock requests granted, if they are now
                                qualified to it
@param[in]	use_fcfs	true -> use first come first served strategy */
static void lock_rec_dequeue_from_page(lock_t *in_lock, bool use_fcfs) {
  lock_rec_discard(in_lock);
  lock_rec_grant(in_lock, use_fcfs);
}

/** Removes a record lock request, waiting or granted, from the queue.
@param[in]	in_lock		record lock object: all record locks
                                which are contained in this lock object
                                are removed */
void lock_rec_discard(lock_t *in_lock) {
  space_id_t space;
  page_no_t page_no;
  trx_lock_t *trx_lock;

  ut_ad(lock_mutex_own());
  ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

  trx_lock = &in_lock->trx->lock;

  space = in_lock->rec_lock.space;
  page_no = in_lock->rec_lock.page_no;

  ut_ad(in_lock->index->table->n_rec_locks > 0);
  in_lock->index->table->n_rec_locks--;

  /* We want the state of lock queue and trx_locks list to be synchronized
  atomically from the point of view of people using trx->mutex, so we perform
  HASH_DELETE and UT_LIST_REMOVE while holding trx->mutex.
  It might be the case that we already hold trx->mutex here, for example if we
  came here from lock_release(trx). */

  ut_ad(trx_mutex_own(in_lock->trx));

  HASH_DELETE(lock_t, hash, lock_hash_get(in_lock->type_mode),
              lock_rec_fold(space, page_no), in_lock);

  UT_LIST_REMOVE(trx_lock->trx_locks, in_lock);

  MONITOR_INC(MONITOR_RECLOCK_REMOVED);
  MONITOR_DEC(MONITOR_NUM_RECLOCK);
}

/** Removes record lock objects set on an index page which is discarded. This
 function does not move locks, or check for waiting locks, therefore the
 lock bitmaps must already be reset when this function is called. */
static void lock_rec_free_all_from_discard_page_low(space_id_t space,
                                                    page_no_t page_no,
                                                    hash_table_t *lock_hash) {
  lock_t *lock;
  lock_t *next_lock;

  lock = lock_rec_get_first_on_page_addr(lock_hash, space, page_no);

  while (lock != NULL) {
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
  space_id_t space;
  page_no_t page_no;

  ut_ad(lock_mutex_own());

  space = block->page.id.space();
  page_no = block->page.id.page_no();

  lock_rec_free_all_from_discard_page_low(space, page_no, lock_sys->rec_hash);
  lock_rec_free_all_from_discard_page_low(space, page_no, lock_sys->prdt_hash);
  lock_rec_free_all_from_discard_page_low(space, page_no,
                                          lock_sys->prdt_page_hash);
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

  ut_ad(lock_mutex_own());

  for (lock = lock_rec_get_first(hash, block, heap_no); lock != NULL;
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
UNIV_INLINE
void lock_protect_locks_till_statement_end(que_thr_t *thr) {
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

  ut_ad(lock_mutex_own());

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
  In READ COMMITTED and less restricitve isolation levels we generaly avoid gap
  locks, but we make an exception for precisely this situation: we want to
  inherit locks created for constraint checks.
  More precisely we need to keep inheriting them only for the duration of the
  query which has requested them, as such inserts have two phases : first they
  check for constraints, then they do actuall row insert, and they trust that
  the locks set in the first phase will survive till the second phase.
  It is not easy to tell if a particular lock was created for constraint check
  or not, because we do not store this bit of information on it.
  What we do, is we use a heuristic: whenever a trx requests a lock with
  lock_duration_t::AT_LEAST_STATEMENT we set trx->lock.inherit_all, meaning that
  locks of this trx need to be inherited.
  And we clear trx->lock.inherit_all on statement end. */

  for (lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);
       lock != NULL; lock = lock_rec_get_next(heap_no, lock)) {
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

  lock_mutex_enter();

  for (lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);
       lock != NULL; lock = lock_rec_get_next(heap_no, lock)) {
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

  lock_mutex_exit();
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

  ut_ad(lock_mutex_own());

  /* If the lock is predicate lock, it resides on INFIMUM record */
  ut_ad(lock_rec_get_first(lock_hash, receiver, receiver_heap_no) == NULL ||
        lock_hash == lock_sys->prdt_hash ||
        lock_hash == lock_sys->prdt_page_hash);

  for (lock = lock_rec_get_first(lock_hash, donator, donator_heap_no);
       lock != NULL; lock = lock_rec_get_next(donator_heap_no, lock)) {
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
        NULL);
}

/** Move all the granted locks to the front of the given lock list.
All the waiting locks will be at the end of the list.
@param[in,out]	lock_list	the given lock list.  */
static void lock_move_granted_locks_to_front(UT_LIST_BASE_NODE_T(lock_t) &
                                             lock_list) {
  lock_t *lock;

  bool seen_waiting_lock = false;

  for (lock = UT_LIST_GET_FIRST(lock_list); lock != nullptr;
       lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
    if (!seen_waiting_lock) {
      if (lock->is_waiting()) {
        seen_waiting_lock = true;
      }
      continue;
    }

    ut_ad(seen_waiting_lock);

    if (!lock->is_waiting()) {
      lock_t *prev = UT_LIST_GET_PREV(trx_locks, lock);
      ut_a(prev);
      UT_LIST_MOVE_TO_FRONT(lock_list, lock);
      lock = prev;
    }
  }
}

/** Moves the locks of a record to another record and resets the lock bits of
 the donating record. */
UNIV_INLINE
void lock_rec_move(const buf_block_t *receiver, /*!< in: buffer block containing
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
  UT_LIST_BASE_NODE_T(lock_t) old_locks;
  mem_heap_t *heap = NULL;
  ulint comp;

  lock_mutex_enter();

  /* FIXME: This needs to deal with predicate lock too */
  lock = lock_rec_get_first_on_page(lock_sys->rec_hash, block);

  if (lock == NULL) {
    lock_mutex_exit();

    return;
  }

  heap = mem_heap_create(256);

  /* Copy first all the locks on the page to heap and reset the
  bitmaps in the original locks; chain the copies of the locks
  using the trx_locks field in them. */

  UT_LIST_INIT(old_locks, &lock_t::trx_locks);

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
  } while (lock != NULL);

  comp = page_is_comp(block->frame);
  ut_ad(comp == page_is_comp(oblock->frame));

  lock_move_granted_locks_to_front(old_locks);

  DBUG_EXECUTE_IF("do_lock_reverse_page_reorganize",
                  UT_LIST_REVERSE(old_locks););

  for (lock = UT_LIST_GET_FIRST(old_locks); lock != nullptr;
       lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
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

        lock_rec_add_to_queue(lock->type_mode, block, new_heap_no, lock->index,
                              lock->trx);
      }

      if (new_heap_no == PAGE_HEAP_NO_SUPREMUM) {
        ut_ad(old_heap_no == PAGE_HEAP_NO_SUPREMUM);
        break;
      }
    }

#ifdef UNIV_DEBUG
    {
      ulint i = lock_rec_find_set_bit(lock);

      /* Check that all locks were moved. */
      if (i != ULINT_UNDEFINED) {
        ib::fatal(ER_IB_MSG_640) << "lock_move_reorganize_page(): " << i
                                 << " not moved in " << (void *)lock;
      }
    }
#endif /* UNIV_DEBUG */
  }

  lock_mutex_exit();

  mem_heap_free(heap);

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
#endif /* UNIV_DEBUG_LOCK_VALIDATE */
}

/** Moves the explicit locks on user records to another page if a record
 list end is moved to another page. */
void lock_move_rec_list_end(
    const buf_block_t *new_block, /*!< in: index page to move to */
    const buf_block_t *block,     /*!< in: index page */
    const rec_t *rec)             /*!< in: record on page: this
                                  is the first record moved */
{
  lock_t *lock;
  const ulint comp = page_rec_is_comp(rec);

  ut_ad(buf_block_get_frame(block) == page_align(rec));
  ut_ad(comp == page_is_comp(buf_block_get_frame(new_block)));

  lock_mutex_enter();

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

  lock_mutex_exit();

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
  ut_ad(lock_rec_validate_page(new_block));
#endif /* UNIV_DEBUG_LOCK_VALIDATE */
}

/** Moves the explicit locks on user records to another page if a record
 list start is moved to another page. */
void lock_move_rec_list_start(const buf_block_t *new_block, /*!< in: index page
                                                            to move to */
                              const buf_block_t *block, /*!< in: index page */
                              const rec_t *rec,         /*!< in: record on page:
                                                        this is the first
                                                        record NOT copied */
                              const rec_t *old_end)     /*!< in: old
                                                        previous-to-last
                                                        record on new_page
                                                        before the records
                                                        were copied */
{
  lock_t *lock;
  const ulint comp = page_rec_is_comp(rec);

  ut_ad(block->frame == page_align(rec));
  ut_ad(new_block->frame == page_align(old_end));
  ut_ad(comp == page_rec_is_comp(old_end));

  lock_mutex_enter();

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
        if (lock_rec_get_nth_bit(lock, i)) {
          ib::fatal(ER_IB_MSG_641) << "lock_move_rec_list_start():" << i
                                   << " not moved in " << (void *)lock;
        }
      }
    }
#endif /* UNIV_DEBUG */
  }

  lock_mutex_exit();

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
#endif /* UNIV_DEBUG_LOCK_VALIDATE */
}

/** Moves the explicit locks on user records to another page if a record
 list start is moved to another page. */
void lock_rtr_move_rec_list(const buf_block_t *new_block, /*!< in: index page to
                                                          move to */
                            const buf_block_t *block,     /*!< in: index page */
                            rtr_rec_move_t *rec_move, /*!< in: recording records
                                                      moved */
                            ulint num_move) /*!< in: num of rec to move */
{
  lock_t *lock;
  ulint comp;

  if (!num_move) {
    return;
  }

  comp = page_rec_is_comp(rec_move[0].old_rec);

  ut_ad(block->frame == page_align(rec_move[0].old_rec));
  ut_ad(new_block->frame == page_align(rec_move[0].new_rec));
  ut_ad(comp == page_rec_is_comp(rec_move[0].new_rec));

  lock_mutex_enter();

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

  lock_mutex_exit();

#ifdef UNIV_DEBUG_LOCK_VALIDATE
  ut_ad(lock_rec_validate_page(block));
#endif
}
/** Updates the lock table when a page is split to the right. */
void lock_update_split_right(
    const buf_block_t *right_block, /*!< in: right page */
    const buf_block_t *left_block)  /*!< in: left page */
{
  ulint heap_no = lock_get_min_heap_no(right_block);

  lock_mutex_enter();

  /* Move the locks on the supremum of the left page to the supremum
  of the right page */

  lock_rec_move(right_block, left_block, PAGE_HEAP_NO_SUPREMUM,
                PAGE_HEAP_NO_SUPREMUM);

  /* Inherit the locks to the supremum of left page from the successor
  of the infimum on right page */

  lock_rec_inherit_to_gap(left_block, right_block, PAGE_HEAP_NO_SUPREMUM,
                          heap_no);

  lock_mutex_exit();
}

/** Updates the lock table when a page is merged to the right. */
void lock_update_merge_right(
    const buf_block_t *right_block, /*!< in: right page
                                    to which merged */
    const rec_t *orig_succ,         /*!< in: original
                                    successor of infimum
                                    on the right page
                                    before merge */
    const buf_block_t *left_block)  /*!< in: merged
                                    index  page which
                                    will be  discarded */
{
  lock_mutex_enter();

  /* Inherit the locks from the supremum of the left page to the
  original successor of infimum on the right page, to which the left
  page was merged */

  lock_rec_inherit_to_gap(right_block, left_block,
                          page_rec_get_heap_no(orig_succ),
                          PAGE_HEAP_NO_SUPREMUM);

  /* Reset the locks on the supremum of the left page, releasing
  waiting transactions */

  lock_rec_reset_and_release_wait_low(lock_sys->rec_hash, left_block,
                                      PAGE_HEAP_NO_SUPREMUM);

#ifdef UNIV_DEBUG
  /* there should exist no page lock on the left page,
  otherwise, it will be blocked from merge */
  space_id_t space = left_block->page.id.space();
  page_no_t page_no = left_block->page.id.page_no();
  ut_ad(lock_rec_get_first_on_page_addr(lock_sys->prdt_page_hash, space,
                                        page_no) == NULL);
#endif /* UNIV_DEBUG */

  lock_rec_free_all_from_discard_page(left_block);

  lock_mutex_exit();
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
  lock_mutex_enter();

  /* Move the locks on the supremum of the root to the supremum
  of block */

  lock_rec_move(block, root, PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
  lock_mutex_exit();
}

/** Updates the lock table when a page is copied to another and the original
 page is removed from the chain of leaf pages, except if page is the root! */
void lock_update_copy_and_discard(
    const buf_block_t *new_block, /*!< in: index page to
                                  which copied */
    const buf_block_t *block)     /*!< in: index page;
                                  NOT the root! */
{
  lock_mutex_enter();

  /* Move the locks on the supremum of the old page to the supremum
  of new_page */

  lock_rec_move(new_block, block, PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
  lock_rec_free_all_from_discard_page(block);

  lock_mutex_exit();
}

/** Updates the lock table when a page is split to the left. */
void lock_update_split_left(
    const buf_block_t *right_block, /*!< in: right page */
    const buf_block_t *left_block)  /*!< in: left page */
{
  ulint heap_no = lock_get_min_heap_no(right_block);

  lock_mutex_enter();

  /* Inherit the locks to the supremum of the left page from the
  successor of the infimum on the right page */

  lock_rec_inherit_to_gap(left_block, right_block, PAGE_HEAP_NO_SUPREMUM,
                          heap_no);

  lock_mutex_exit();
}

/** Updates the lock table when a page is merged to the left. */
void lock_update_merge_left(
    const buf_block_t *left_block,  /*!< in: left page to
                                    which merged */
    const rec_t *orig_pred,         /*!< in: original predecessor
                                    of supremum on the left page
                                    before merge */
    const buf_block_t *right_block) /*!< in: merged index page
                                    which will be discarded */
{
  const rec_t *left_next_rec;

  ut_ad(left_block->frame == page_align(orig_pred));

  lock_mutex_enter();

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

#ifdef UNIV_DEBUG
  /* there should exist no page lock on the right page,
  otherwise, it will be blocked from merge */
  space_id_t space = right_block->page.id.space();
  page_no_t page_no = right_block->page.id.page_no();
  lock_t *lock_test =
      lock_rec_get_first_on_page_addr(lock_sys->prdt_page_hash, space, page_no);
  ut_ad(!lock_test);
#endif /* UNIV_DEBUG */

  lock_rec_free_all_from_discard_page(right_block);

  lock_mutex_exit();
}

/** Resets the original locks on heir and replaces them with gap type locks
 inherited from rec. */
void lock_rec_reset_and_inherit_gap_locks(
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
  lock_mutex_enter();

  lock_rec_reset_and_release_wait(heir_block, heir_heap_no);

  lock_rec_inherit_to_gap(heir_block, block, heir_heap_no, heap_no);

  lock_mutex_exit();
}

/** Updates the lock table when a page is discarded. */
void lock_update_discard(
    const buf_block_t *heir_block, /*!< in: index page
                                   which will inherit the locks */
    ulint heir_heap_no,            /*!< in: heap_no of the record
                                   which will inherit the locks */
    const buf_block_t *block)      /*!< in: index page
                                   which will be discarded */
{
  const rec_t *rec;
  ulint heap_no;
  const page_t *page = block->frame;

  lock_mutex_enter();

  if (!lock_rec_get_first_on_page(lock_sys->rec_hash, block) &&
      (!lock_rec_get_first_on_page(lock_sys->prdt_page_hash, block)) &&
      (!lock_rec_get_first_on_page(lock_sys->prdt_hash, block))) {
    /* No locks exist on page, nothing to do */
    lock_mutex_exit();

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

  lock_mutex_exit();
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

/** Updates the lock table when a record is removed. */
void lock_update_delete(
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec)         /*!< in: the record to be removed */
{
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

  lock_mutex_enter();

  /* Let the next record inherit the locks from rec, in gap mode */

  lock_rec_inherit_to_gap(block, block, next_heap_no, heap_no);

  /* Reset the lock bits on rec and release waiting transactions */

  lock_rec_reset_and_release_wait(block, heap_no);

  lock_mutex_exit();
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

  lock_mutex_enter();

  lock_rec_move(block, block, PAGE_HEAP_NO_INFIMUM, heap_no);

  lock_mutex_exit();
}

/** Restores the state of explicit lock requests on a single record, where the
 state was stored on the infimum of the page. */
void lock_rec_restore_from_page_infimum(
    const buf_block_t *block,   /*!< in: buffer block containing rec */
    const rec_t *rec,           /*!< in: record whose lock state
                                is restored */
    const buf_block_t *donator) /*!< in: page (rec is not
                                necessarily on this page)
                                whose infimum stored the lock
                                state; lock bits are reset on
                                the infimum */
{
  ulint heap_no = page_rec_get_heap_no(rec);

  lock_mutex_enter();

  lock_rec_move(block, donator, heap_no, PAGE_HEAP_NO_INFIMUM);

  lock_mutex_exit();
}

/*========================= TABLE LOCKS ==============================*/

/** Functor for accessing the embedded node within a table lock. */
struct TableLockGetNode {
  ut_list_node<lock_t> &operator()(lock_t &elem) {
    return (elem.tab_lock.locks);
  }
};

/** Creates a table lock object and adds it as the last in the lock queue
 of the table. Does NOT check for deadlocks or lock compatibility.
 @return own: new lock object */
UNIV_INLINE
lock_t *lock_table_create(dict_table_t *table, /*!< in/out: database table
                                               in dictionary cache */
                          ulint type_mode, /*!< in: lock mode possibly ORed with
                                         LOCK_WAIT */
                          trx_t *trx)      /*!< in: trx */
{
  lock_t *lock;

  ut_ad(table && trx);
  ut_ad(lock_mutex_own());
  ut_ad(trx_mutex_own(trx));

  check_trx_state(trx);
  ++table->count_by_mode[type_mode & LOCK_MODE_MASK];
  /* For AUTOINC locking we reuse the lock instance only if
  there is no wait involved else we allocate the waiting lock
  from the transaction lock heap. */
  if (type_mode == LOCK_AUTO_INC) {
    lock = table->autoinc_lock;

    table->autoinc_trx = trx;

    ib_vector_push(trx->lock.autoinc_locks, &lock);

  } else if (trx->lock.table_cached < trx->lock.table_pool.size()) {
    lock = trx->lock.table_pool[trx->lock.table_cached++];
  } else {
    lock = static_cast<lock_t *>(
        mem_heap_alloc(trx->lock.lock_heap, sizeof(*lock)));
  }

  lock->type_mode = uint32_t(type_mode | LOCK_TABLE);
  lock->trx = trx;

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

  UT_LIST_ADD_LAST(trx->lock.trx_locks, lock);

  ut_list_append(table->locks, lock, TableLockGetNode());

  if (type_mode & LOCK_WAIT) {
    lock_set_lock_and_trx_wait(lock);
  }

  lock->trx->lock.table_locks.push_back(lock);

  MONITOR_INC(MONITOR_TABLELOCK_CREATED);
  MONITOR_INC(MONITOR_NUM_TABLELOCK);

  return (lock);
}

/** Pops autoinc lock requests from the transaction's autoinc_locks. We
 handle the case where there are gaps in the array and they need to
 be popped off the stack. */
UNIV_INLINE
void lock_table_pop_autoinc_locks(
    trx_t *trx) /*!< in/out: transaction that owns the AUTOINC locks */
{
  ut_ad(lock_mutex_own());
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

  } while (*(lock_t **)ib_vector_get_last(trx->lock.autoinc_locks) == NULL);
}

/** Removes an autoinc lock request from the transaction's autoinc_locks. */
UNIV_INLINE
void lock_table_remove_autoinc_lock(
    lock_t *lock, /*!< in: table lock */
    trx_t *trx)   /*!< in/out: transaction that owns the lock */
{
  /* We will access and modify trx->lock.autoinc_locks so we need trx->mutex */
  ut_ad(trx_mutex_own(trx));
  lock_t *autoinc_lock;
  lint i = ib_vector_size(trx->lock.autoinc_locks) - 1;

  ut_ad(lock_mutex_own());
  ut_ad(lock_get_mode(lock) == LOCK_AUTO_INC);
  ut_ad(lock_get_type_low(lock) & LOCK_TABLE);
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
    ut_a(autoinc_lock != NULL);

    /* Handle freeing the locks from within the stack. */

    while (--i >= 0) {
      autoinc_lock =
          *static_cast<lock_t **>(ib_vector_get(trx->lock.autoinc_locks, i));

      if (autoinc_lock == lock) {
        void *null_var = NULL;
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
UNIV_INLINE
void lock_table_remove_low(lock_t *lock) /*!< in/out: table lock */
{
  trx_t *trx;
  dict_table_t *table;

  ut_ad(lock_mutex_own());

  trx = lock->trx;
  /* We will modify trx->lock.trx_locks so we need trx->mutex */
  ut_ad(trx_mutex_own(trx));
  table = lock->tab_lock.table;
  const auto lock_mode = lock_get_mode(lock);
  /* Remove the table from the transaction's AUTOINC vector, if
  the lock that is being released is an AUTOINC lock. */
  if (lock_mode == LOCK_AUTO_INC) {
    /* The table's AUTOINC lock can get transferred to
    another transaction before we get here. */
    if (table->autoinc_trx == trx) {
      table->autoinc_trx = NULL;
    }

    /* The locks must be freed in the reverse order from
    the one in which they were acquired. This is to avoid
    traversing the AUTOINC lock vector unnecessarily.

    We only store locks that were granted in the
    trx->autoinc_locks vector (see lock_table_create()
    and lock_grant()). Therefore it can be empty and we
    need to check for that. */

    if (!lock_get_wait(lock) && !ib_vector_is_empty(trx->lock.autoinc_locks)) {
      lock_table_remove_autoinc_lock(lock, trx);
    }
  }
  ut_a(0 < table->count_by_mode[lock_mode]);
  --table->count_by_mode[lock_mode];

  UT_LIST_REMOVE(trx->lock.trx_locks, lock);
  ut_list_remove(table->locks, lock, TableLockGetNode());

  MONITOR_INC(MONITOR_TABLELOCK_REMOVED);
  MONITOR_DEC(MONITOR_NUM_TABLELOCK);
}

/** Enqueues a waiting request for a table lock which cannot be granted
 immediately. Checks for deadlocks.
 @return DB_LOCK_WAIT or DB_DEADLOCK */
static dberr_t lock_table_enqueue_waiting(
    ulint mode,          /*!< in: lock mode this transaction is
                         requesting */
    dict_table_t *table, /*!< in/out: table */
    que_thr_t *thr)      /*!< in: query thread */
{
  trx_t *trx;

  ut_ad(lock_mutex_own());
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
      ut_ad(0);
  }

  if (trx->in_innodb & TRX_FORCE_ROLLBACK_ASYNC) {
    return (DB_DEADLOCK);
  }

  /* Enqueue the lock request that will wait to be granted */
  lock_table_create(table, mode | LOCK_WAIT, trx);

  trx->lock.que_state = TRX_QUE_LOCK_WAIT;

  trx->lock.wait_started = ut_time();
  trx->lock.was_chosen_as_deadlock_victim = false;

  auto stopped = que_thr_stop(thr);
  ut_a(stopped);

  MONITOR_INC(MONITOR_TABLELOCK_WAIT);

  return (DB_LOCK_WAIT);
}

/** Checks if other transactions have an incompatible mode lock request in
 the lock queue.
 @return lock or NULL */
UNIV_INLINE
const lock_t *lock_table_other_has_incompatible(
    const trx_t *trx,          /*!< in: transaction, or NULL if all
                               transactions should be included */
    ulint wait,                /*!< in: LOCK_WAIT if also
                               waiting locks are taken into
                               account, or 0 if not */
    const dict_table_t *table, /*!< in: table */
    lock_mode mode)            /*!< in: lock mode */
{
  const lock_t *lock;

  ut_ad(lock_mutex_own());

  // According to lock_compatibility_matrix, an intention lock can wait only
  // for LOCK_S or LOCK_X. If there are no LOCK_S nor LOCK_X locks in the queue,
  // then we can avoid iterating through the list and return immediately.
  // This might help in OLTP scenarios, with no DDL queries,
  // as then there are almost no LOCK_S nor LOCK_X, but many DML queries still
  // need to get an intention lock to perform their action - while this never
  // causes them to wait for a "data lock", it might cause them to wait for
  // lock_sys->mutex if the operation takes Omega(n).

  if ((mode == LOCK_IS || mode == LOCK_IX) &&
      table->count_by_mode[LOCK_S] == 0 && table->count_by_mode[LOCK_X] == 0) {
    return NULL;
  }

  for (lock = UT_LIST_GET_LAST(table->locks); lock != NULL;
       lock = UT_LIST_GET_PREV(tab_lock.locks, lock)) {
    if (lock->trx != trx && !lock_mode_compatible(lock_get_mode(lock), mode) &&
        (wait || !lock_get_wait(lock))) {
      return (lock);
    }
  }

  return (NULL);
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
  In theory trx_t::table_locks can be modified in
  lock_trx_table_locks_remove which is called from:
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
      (this seems to be used during recovery, and recovery is single-threaded)
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
      trx->rsegs.m_redo.rseg == 0) {
    trx_set_rw_mode(trx);
  }

  lock_mutex_enter();

  /* We have to check if the new lock is compatible with any locks
  other transactions have in the table lock queue. */

  wait_for = lock_table_other_has_incompatible(trx, LOCK_WAIT, table, mode);

  trx_mutex_enter(trx);

  /* Another trx has a request on the table in an incompatible
  mode: this trx may have to wait */

  if (wait_for != NULL) {
    err = lock_table_enqueue_waiting(mode | flags, table, thr);
    if (err == DB_LOCK_WAIT) {
      lock_create_wait_for_edge(trx, wait_for->trx);
    }
  } else {
    lock_table_create(table, mode | flags, trx);

    ut_a(!flags || mode == LOCK_S || mode == LOCK_X);

    err = DB_SUCCESS;
  }

  lock_mutex_exit();
  trx_mutex_exit(trx);

  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

/** Creates a table IX lock object for a resurrected transaction. */
void lock_table_ix_resurrect(dict_table_t *table, /*!< in/out: table */
                             trx_t *trx)          /*!< in/out: transaction */
{
  ut_ad(trx->is_recovered);

  if (lock_table_has(trx, table, LOCK_IX)) {
    return;
  }

  lock_mutex_enter();

  /* We have to check if the new lock is compatible with any locks
  other transactions have in the table lock queue. */

  ut_ad(!lock_table_other_has_incompatible(trx, LOCK_WAIT, table, LOCK_IX));

  trx_mutex_enter(trx);
  lock_table_create(table, LOCK_IX, trx);
  lock_mutex_exit();
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
  const lock_t *lock;

  ut_ad(lock_mutex_own());
  ut_ad(lock_get_wait(wait_lock));

  table = wait_lock->tab_lock.table;

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

  for (lock = UT_LIST_GET_FIRST(table->locks); lock != wait_lock;
       lock = UT_LIST_GET_NEXT(tab_lock.locks, lock)) {
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
  ut_ad(lock_mutex_own());
  /* This is needed for lock_table_remove_low(), but it's easier to understand
  the code if we assert it here as well */
  ut_ad(trx_mutex_own(in_lock->trx));
  ut_a(lock_get_type_low(in_lock) == LOCK_TABLE);

  const auto mode = lock_get_mode(in_lock);
  const auto table = in_lock->tab_lock.table;

  lock_t *lock = UT_LIST_GET_NEXT(tab_lock.locks, in_lock);

  lock_table_remove_low(in_lock);

  // According to lock_compatibility_matrix, an intention lock can block only
  // LOCK_S or LOCK_X from being granted, and thus, releasing of an intention
  // lock can help in granting only LOCK_S or LOCK_X. If there are no LOCK_S nor
  // LOCK_X locks in the queue, then we can avoid iterating through the list and
  // return immediately. This might help in OLTP scenarios, with no DDL queries,
  // as then there are almost no LOCK_S nor LOCK_X, but many DML queries still
  // need to get an intention lock to perform their action - while this never
  // causes them to wait for a "data lock", it might cause them to wait for
  // lock_sys->mutex if the operation takes Omega(n) or even Omega(n^2)
  if ((mode == LOCK_IS || mode == LOCK_IX) &&
      table->count_by_mode[LOCK_S] == 0 && table->count_by_mode[LOCK_X] == 0) {
    return;
  }

  /* Check if waiting locks in the queue can now be granted: grant
  locks if there are no conflicting locks ahead. */

  for (/* No op */; lock != NULL;
       lock = UT_LIST_GET_NEXT(tab_lock.locks, lock)) {
    lock_grant_or_update_wait_for_edge_if_waiting(lock, in_lock->trx);
  }
}

/** Sets a lock on a table based on the given mode.
@param[in]	table	table to lock
@param[in,out]	trx	transaction
@param[in]	mode	LOCK_X or LOCK_S
@return error code or DB_SUCCESS. */
dberr_t lock_table_for_trx(dict_table_t *table, trx_t *trx,
                           enum lock_mode mode) {
  mem_heap_t *heap;
  que_thr_t *thr;
  dberr_t err;
  sel_node_t *node;
  heap = mem_heap_create(512);

  node = sel_node_create(heap);
  thr = pars_complete_graph_for_exec(node, trx, heap, NULL);
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

    auto was_lock_wait = row_mysql_handle_errors(&err, trx, thr, NULL);

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
@param[in]	first_lock	Lock to traverse from
@param[in]	lock		Lock that was unlocked
@param[in]	heap_no		Heap no within the page for the lock. */
static void lock_rec_unlock_grant(lock_t *first_lock, lock_t *lock,
                                  ulint heap_no) {
  ut_ad(lock_mutex_own());
  ut_ad(!lock_get_wait(lock));
  ut_ad(lock_get_type_low(lock) == LOCK_REC);
  ut_ad(!lock->is_predicate());
  ut_ad(lock_rec_get_nth_bit(lock, heap_no));
  lock_rec_reset_nth_bit(lock, heap_no);

  if (lock_use_fcfs(lock)) {
    const trx_t *trx = lock->trx;
    /* Check if we can now grant waiting lock requests */

    for (lock = first_lock; lock != nullptr;
         lock = lock_rec_get_next(heap_no, lock)) {
      lock_grant_or_update_wait_for_edge_if_waiting(lock, trx);
    }
  } else {
    lock_grant_cats(lock_sys->rec_hash, lock, heap_no);
  }
}

/** Removes a granted record lock of a transaction from the queue and grants
 locks to other transactions waiting in the queue if they now are entitled
 to a lock.
 This function is meant to be used only by row_unlock_for_mysql, and it assumes
 that the lock we are looking for has LOCK_REC_NOT_GAP flag.
 */
void lock_rec_unlock(
    trx_t *trx,               /*!< in/out: transaction that has
                              set a record lock */
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec,         /*!< in: record */
    lock_mode lock_mode)      /*!< in: LOCK_S or LOCK_X */
{
  ut_ad(!trx->lock.wait_lock);
  ut_ad(block->frame == page_align(rec));
  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
  ut_ad(lock_mode == LOCK_S || lock_mode == LOCK_X);

  ulint heap_no = page_rec_get_heap_no(rec);

  lock_mutex_enter();
  trx_mutex_enter(trx);

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
      lock_rec_unlock_grant(first_lock, lock, heap_no);

      lock_mutex_exit();
      trx_mutex_exit(trx);

      return;
    }
  }

  lock_mutex_exit();
  trx_mutex_exit(trx);

  {
    size_t stmt_len;

    auto stmt = innobase_get_stmt_unsafe(trx->mysql_thd, &stmt_len);

    ib::error err(ER_IB_MSG_1228);

    err << "Unlock row could not find a " << lock_mode
        << " mode lock on the record. Current statement: ";

    err.write(stmt, stmt_len);
  }
}

/** Remove GAP lock from a next key record lock
@param[in,out]	lock	lock object */
static void lock_remove_gap_lock(lock_t *lock) {
  /* Remove lock on supremum */
  lock_rec_reset_nth_bit(lock, PAGE_HEAP_NO_SUPREMUM);

  /* Remove GAP lock for other records */
  lock->remove_gap_lock();
}

/** Used to release a lock during PREPARE. The lock is only
released if rules permit it.
@param[in]   lock       the lock that we consider releasing
@param[in]   only_gap   true if we don't want to release records,
                        just the gaps between them */
static void lock_release_read_lock(lock_t *lock, bool only_gap) {
  if (!lock->is_record_lock() || lock->is_insert_intention() ||
      lock->is_predicate()) {
    /* DO NOTHING */
  } else if (lock->is_gap()) {
    /* Release any GAP only lock. */
    lock_rec_dequeue_from_page(lock, false);
  } else if (lock->is_record_not_gap() && only_gap) {
    /* Don't release any non-GAP lock if not asked.*/
  } else if (lock->mode() == LOCK_S && !only_gap) {
    /* Release Shared Next Key Lock(SH + GAP) if asked for */
    lock_rec_dequeue_from_page(lock, false);
  } else {
    /* Release GAP lock from Next Key lock */
    lock_remove_gap_lock(lock);

    /* Grant locks. Current CATS implementation does not grant locks for records
    for which the bit is already cleared in the bitmap, and lock_remove_gap_lock
    might have reset the PAGE_HEAP_NO_SUPREMUM-th bit. So, to ensure that trxs
    waiting for lock on supremum are properly woken up we need to use FCFS. */
    lock_rec_grant(lock, true);
  }
}

/** Release read locks of a transacion latching the whole lock-sys in
exclusive mode.
It is called during XA prepare to release locks early.
@param[in,out]	trx		transaction
@param[in]	only_gap	release only GAP locks */
static void lock_trx_release_read_locks_in_x_mode(trx_t *trx, bool only_gap) {
  ut_ad(!trx_mutex_own(trx));

  lock_mutex_enter();
  trx_mutex_enter(trx);

  lock_t *lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);

  while (lock != NULL) {
    /* Store the pointer to the next lock in the list, because in some cases
    we are going to remove `lock` from the list, which clears the pointer to
    next lock */
    lock_t *next_lock = UT_LIST_GET_NEXT(trx_locks, lock);

    lock_release_read_lock(lock, only_gap);

    lock = next_lock;
  }

  lock_mutex_exit();
  trx_mutex_exit(trx);
}

void lock_trx_release_read_locks(trx_t *trx, bool only_gap) {
  /* Avoid taking lock_sys if trx didn't acquire any lock.
  We do not hold trx->mutex nor lock_sys latch while checking the emptiness of
  trx->lock.trx_locks, but this is OK, because even if other threads are
  modifying this list in parallel, they do not change the emptiness of it:
  implicit-to-explicit conversion only occurs if the trx already has a table
  intention lock, B-tree modification related operations always first create
  a copy of old lock before removing old lock, and removal of wait lock can not
  happen since we are not waiting. */
  ut_ad(lock_current_thread_handles_trx(trx));
  ut_ad(trx->lock.wait_lock == NULL);
  if (UT_LIST_GET_LEN(trx->lock.trx_locks) == 0) {
    return;
  }

  lock_trx_release_read_locks_in_x_mode(trx, only_gap);
}

/** Releases transaction locks, and releases possible other transactions waiting
 because of these locks.
@param[in,out]  trx   transaction */
static void lock_release(trx_t *trx) {
  lock_t *lock;

  ut_ad(!lock_mutex_own());
  ut_ad(!trx_mutex_own(trx));
  ut_ad(!trx->is_dd_trx);

  /* Don't take lock_sys mutex if trx didn't acquire any lock.
  We want to check if trx->lock.trx_lock is empty without holding trx->mutex
  nor lock_sys->mutex.
  In order to access trx->lock.trx_locks safely we should hold at least
  trx->mutex. But:
  The transaction is already in TRX_STATE_COMMITTED_IN_MEMORY state and is no
  longer referenced, so we are not afraid of implicit-to-explicit conversions,
  nor a cancellation of a wait_lock (we are running, not waiting). Still, there
  might be some B-tree merge or split operations running in parallel which cause
  locks to be moved from one page to another, which at the low level means that
  a new lock is created (and added to trx->lock.trx_locks) and the old one is
  removed (also from trx->lock.trx_locks) in that specific order.
  Actually, there is no situation in our code, where some other thread can
  change the number of explicit locks from 0 to non-zero, or vice-versa.
  Even the implicit-to-explicit conversion presumes that our trx holds at least
  an explicit IX table lock (since it was allowed to modify the table).
  Thus, if the only thing we want to do is comparing with zero, then there is
  no real risk here. */
  if (UT_LIST_GET_LEN(trx->lock.trx_locks) == 0) {
    return;
  }

  lock_mutex_enter();
  trx_mutex_enter(trx);

  for (lock = UT_LIST_GET_LAST(trx->lock.trx_locks); lock != NULL;
       lock = UT_LIST_GET_LAST(trx->lock.trx_locks)) {
    if (lock_get_type_low(lock) == LOCK_REC) {
      lock_rec_dequeue_from_page(lock, false);
    } else {
      lock_table_dequeue(lock);
    }
  }

  lock_mutex_exit();
  trx_mutex_exit(trx);
}

/* True if a lock mode is S or X */
#define IS_LOCK_S_OR_X(lock) \
  (lock_get_mode(lock) == LOCK_S || lock_get_mode(lock) == LOCK_X)

/** Removes lock_to_remove from lock_to_remove->trx->lock.table_locks.
@param[in]  lock_to_remove  lock to remove */
static void lock_trx_table_locks_remove(const lock_t *lock_to_remove) {
  trx_t *trx = lock_to_remove->trx;

  ut_ad(lock_mutex_own());
  /* We will modify trx->lock.table_locks so we need trx->mutex */
  ut_ad(trx_mutex_own(trx));

  typedef lock_pool_t::reverse_iterator iterator;

  iterator end = trx->lock.table_locks.rend();

  iterator it = std::find(trx->lock.table_locks.rbegin(), end, lock_to_remove);

  /* Lock must exist in the vector. */
  ut_a(it != end);
  /* To keep it O(1) replace the removed position with lock from the back */
  *it = trx->lock.table_locks.back();
  trx->lock.table_locks.pop_back();
}

/** Removes locks of a transaction on a table to be dropped.
 If remove_also_table_sx_locks is true then table-level S and X locks are
 also removed in addition to other table-level and record-level locks.
 No lock that is going to be removed is allowed to be a wait lock. */
static void lock_remove_all_on_table_for_trx(
    dict_table_t *table,              /*!< in: table to be dropped */
    trx_t *trx,                       /*!< in: a transaction */
    ibool remove_also_table_sx_locks) /*!< in: also removes
                                   table S and X locks */
{
  lock_t *lock;
  lock_t *prev_lock;

  /* This is used when we drop a table and indeed have exclusive lock_sys
  access. */
  ut_ad(lock_mutex_own());
  /* We need trx->mutex to iterate over trx->lock.trx_lock and it is needed by
  lock_trx_table_locks_remove() and lock_table_remove_low() but we haven't
  acquired it yet. */
  ut_ad(!trx_mutex_own(trx));
  trx_mutex_enter(trx);

  for (lock = UT_LIST_GET_LAST(trx->lock.trx_locks); lock != NULL;
       lock = prev_lock) {
    prev_lock = UT_LIST_GET_PREV(trx_locks, lock);

    if (lock_get_type_low(lock) == LOCK_REC && lock->index->table == table) {
      ut_a(!lock_get_wait(lock));

      lock_rec_discard(lock);
    } else if (lock_get_type_low(lock) & LOCK_TABLE &&
               lock->tab_lock.table == table &&
               (remove_also_table_sx_locks || !IS_LOCK_S_OR_X(lock))) {
      ut_a(!lock_get_wait(lock));

      lock_trx_table_locks_remove(lock);
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
  ut_a(table != NULL);
  /* This is used in recovery where indeed we hold an exclusive lock_sys latch,
  which is needed as we are about to iterate over locks held by multiple
  transactions while they might be operating. */
  ut_ad(lock_mutex_own());

  ulint n_recovered_trx = 0;

  mutex_enter(&trx_sys->mutex);

  for (trx_t *trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list); trx != NULL;
       trx = UT_LIST_GET_NEXT(trx_list, trx)) {
    assert_trx_in_rw_list(trx);

    if (!trx->is_recovered) {
      continue;
    }
    /* We need trx->mutex to iterate over trx->lock.trx_lock and it is needed by
    lock_trx_table_locks_remove() and lock_table_remove_low() but we haven't
    acquired it yet. */
    ut_ad(!trx_mutex_own(trx));
    trx_mutex_enter(trx);
    /* Because we are holding the lock_sys->mutex,
    implicit locks cannot be converted to explicit ones
    while we are scanning the explicit locks. */

    lock_t *next_lock;

    for (lock_t *lock = UT_LIST_GET_FIRST(trx->lock.trx_locks); lock != NULL;
         lock = next_lock) {
      ut_a(lock->trx == trx);

      /* Recovered transactions can't wait on a lock. */

      ut_a(!lock_get_wait(lock));

      next_lock = UT_LIST_GET_NEXT(trx_locks, lock);

      switch (lock_get_type_low(lock)) {
        default:
          ut_error;
        case LOCK_TABLE:
          if (lock->tab_lock.table == table) {
            lock_trx_table_locks_remove(lock);
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
    dict_table_t *table,              /*!< in: table to be dropped
                                      or discarded */
    ibool remove_also_table_sx_locks) /*!< in: also removes
                                   table S and X locks */
{
  lock_t *lock;

  lock_mutex_enter();

  for (lock = UT_LIST_GET_FIRST(table->locks); lock != NULL;
       /* No op */) {
    lock_t *prev_lock;

    prev_lock = UT_LIST_GET_PREV(tab_lock.locks, lock);

    /* If we should remove all locks (remove_also_table_sx_locks
    is true), or if the lock is not table-level S or X lock,
    then check we are not going to remove a wait lock. */
    if (remove_also_table_sx_locks ||
        !(lock_get_type(lock) == LOCK_TABLE && IS_LOCK_S_OR_X(lock))) {
      ut_a(!lock_get_wait(lock));
    }

    lock_remove_all_on_table_for_trx(table, lock->trx,
                                     remove_also_table_sx_locks);

    if (prev_lock == NULL) {
      if (lock == UT_LIST_GET_FIRST(table->locks)) {
        /* lock was not removed, pick its successor */
        lock = UT_LIST_GET_NEXT(tab_lock.locks, lock);
      } else {
        /* lock was removed, pick the first one */
        lock = UT_LIST_GET_FIRST(table->locks);
      }
    } else if (UT_LIST_GET_NEXT(tab_lock.locks, prev_lock) != lock) {
      /* If lock was removed by
      lock_remove_all_on_table_for_trx() then pick the
      successor of prev_lock ... */
      lock = UT_LIST_GET_NEXT(tab_lock.locks, prev_lock);
    } else {
      /* ... otherwise pick the successor of lock. */
      lock = UT_LIST_GET_NEXT(tab_lock.locks, lock);
    }
  }

  /* Note: Recovered transactions don't have table level IX or IS locks
  but can have implicit record locks that have been converted to explicit
  record locks. Such record locks cannot be freed by traversing the
  transaction lock list in dict_table_t (as above). */

  if (!lock_sys->rollback_complete &&
      lock_remove_recovered_trx_record_locks(table) == 0) {
    lock_sys->rollback_complete = true;
  }

  lock_mutex_exit();
}

/*===================== VALIDATION AND DEBUGGING ====================*/

/** Prints info of a table lock. */
static void lock_table_print(FILE *file,         /*!< in: file where to print */
                             const lock_t *lock) /*!< in: table type lock */
{
  ut_ad(lock_mutex_own());
  ut_a(lock_get_type_low(lock) == LOCK_TABLE);

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
  space_id_t space;
  page_no_t page_no;
  mtr_t mtr;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(lock_mutex_own());
  ut_a(lock_get_type_low(lock) == LOCK_REC);

  space = lock->rec_lock.space;
  page_no = lock->rec_lock.page_no;

  fprintf(file,
          "RECORD LOCKS space id %lu page no %lu n bits %lu "
          "index %s of table ",
          (ulong)space, (ulong)page_no, (ulong)lock_rec_get_n_bits(lock),
          lock->index->name());
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

  block = buf_page_try_get(page_id_t(space, page_no), &mtr);

  for (ulint i = 0; i < lock_rec_get_n_bits(lock); ++i) {
    if (!lock_rec_get_nth_bit(lock, i)) {
      continue;
    }

    fprintf(file, "Record lock, heap no %lu", (ulong)i);

    if (block) {
      const rec_t *rec;

      rec = page_find_rec_with_heap_no(buf_block_get_frame(block), i);

      offsets =
          rec_get_offsets(rec, lock->index, offsets, ULINT_UNDEFINED, &heap);

      putc(' ', file);
      rec_print_new(file, rec, offsets);
    }

    putc('\n', file);
  }

  mtr_commit(&mtr);

  if (heap) {
    mem_heap_free(heap);
  }
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

  /* We need exclusive access to lock_sys to iterate over all buckets */
  ut_ad(lock_mutex_own());

  for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
    const lock_t *lock;

    for (lock =
             static_cast<const lock_t *>(HASH_GET_FIRST(lock_sys->rec_hash, i));
         lock != 0;
         lock = static_cast<const lock_t *>(HASH_GET_NEXT(hash, lock))) {
      n_locks++;
    }
  }

  return (n_locks);
}
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */

/** Prints info of locks for all transactions.
 @return false if not able to obtain lock mutex
 and exits without printing info */
bool lock_print_info_summary(
    FILE *file,   /*!< in: file where to print */
    ibool nowait) /*!< in: whether to wait for the lock mutex */
{
  /* if nowait is false, wait on the lock mutex,
  otherwise return immediately if fail to obtain the
  mutex. */
  if (!nowait) {
    lock_mutex_enter();
  } else if (lock_mutex_enter_nowait()) {
    fputs(
        "FAIL TO OBTAIN LOCK MUTEX,"
        " SKIP LOCK INFO PRINTING\n",
        file);
    return (false);
  }

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

  fprintf(file, "Trx id counter " TRX_ID_FMT "\n", trx_sys_get_max_trx_id());

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

  fprintf(file, "History list length %lu\n", (ulong)trx_sys->rseg_history_len);

#ifdef PRINT_NUM_OF_LOCK_STRUCTS
  fprintf(file, "Total number of lock structs in row lock hash table %lu\n",
          (ulong)lock_get_n_rec_locks());
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */
  return (true);
}

/** Functor to print not-started transaction from the mysql_trx_list. */

struct PrintNotStarted {
  PrintNotStarted(FILE *file) : m_file(file) {}

  void operator()(const trx_t *trx) {
    /* We require exclusive access to lock_sys */
    ut_ad(lock_mutex_own());
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
    lock_t *lock;
    ulint i = 0;
    /* trx->lock.trx_locks is protected by trx->mutex and lock_sys mutex, and we
    assume we have the exclusive latch on lock_sys here */
    ut_ad(lock_mutex_own());
    for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
         lock != NULL && i < m_index;
         lock = UT_LIST_GET_NEXT(trx_locks, lock), ++i) {
      /* No op */
    }

    return (lock);
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

/** This iterates over both the RW and RO trx_sys lists. We need to keep
track where the iterator was up to and we do that using an ordinal value. */

class TrxListIterator {
 public:
  TrxListIterator() : m_index() {
    /* We iterate over the RW trx list first. */

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
    ulint i;
    trx_t *trx;

    /* Make the transaction at the ordinal value of m_index
    the current transaction. ie. reposition/restore */

    for (i = 0, trx = UT_LIST_GET_FIRST(*m_trx_list);
         trx != NULL && (i < m_index);
         trx = UT_LIST_GET_NEXT(trx_list, trx), ++i) {
      check_trx_state(trx);
    }

    return (trx);
  }

  /** Ordinal value of the transaction in the current transaction list */
  ulint m_index;

  /** Current transaction list */
  trx_ut_list_t *m_trx_list;

  /** For iterating over a transaction's locks */
  TrxLockIterator m_lock_iter;
};

/** Prints transaction lock wait and MVCC state.
@param[in,out]	file	file where to print
@param[in]	trx	transaction */
void lock_trx_print_wait_and_mvcc_state(FILE *file, const trx_t *trx) {
  /* We require exclusive lock_sys access so that trx->lock.wait_lock is
  not being modified */
  ut_ad(lock_mutex_own());
  fprintf(file, "---");

  trx_print_latched(file, trx, 600);

  const ReadView *read_view = trx_get_read_view(trx);

  if (read_view != NULL) {
    read_view->print_limits(file);
  }

  if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
    fprintf(file,
            "------- TRX HAS BEEN WAITING %lu SEC"
            " FOR THIS LOCK TO BE GRANTED:\n",
            (ulong)difftime(ut_time(), trx->lock.wait_started));

    if (lock_get_type_low(trx->lock.wait_lock) == LOCK_REC) {
      lock_rec_print(file, trx->lock.wait_lock);
    } else {
      lock_table_print(file, trx->lock.wait_lock);
    }

    fprintf(file, "------------------\n");
  }
}

/** Prints info of locks for a transaction. This function will release the
 lock mutex and the trx_sys_t::mutex if the page was read from disk.
 @return true if page was read from the tablespace */
static bool lock_rec_fetch_page(const lock_t *lock) /*!< in: record lock */
{
  ut_ad(lock_get_type_low(lock) == LOCK_REC);

  space_id_t space_id = lock->rec_lock.space;
  fil_space_t *space;
  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space_id, &found);
  page_no_t page_no = lock->rec_lock.page_no;

  /* Check if the .ibd file exists. */
  if (found) {
    mtr_t mtr;

    lock_mutex_exit();

    mutex_exit(&trx_sys->mutex);

    DEBUG_SYNC_C("innodb_monitor_before_lock_page_read");

    /* Check if the space is exists or not. only
    when the space is valid, try to get the page. */
    space = fil_space_acquire(space_id);
    if (space) {
      mtr_start(&mtr);
      buf_page_get_gen(page_id_t(space_id, page_no), page_size, RW_NO_LATCH,
                       NULL, Page_fetch::POSSIBLY_FREED, __FILE__, __LINE__,
                       &mtr);
      mtr_commit(&mtr);
      fil_space_release(space);
    }

    lock_mutex_enter();

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
  ut_ad(lock_mutex_own());

  /* Iterate over the transaction's locks. */
  while ((lock = iter.current(trx)) != 0) {
    if (lock_get_type_low(lock) == LOCK_REC) {
      if (load_block) {
        /* Note: lock_rec_fetch_page() will
        release both the lock mutex and the
        trx_sys_t::mutex if it does a read
        from disk. */

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
                lock->rec_lock.space);
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

/** Prints info of locks for each transaction. This function assumes that the
 caller holds the lock mutex and more importantly it will release the lock
 mutex on behalf of the caller. (This should be fixed in the future). */
void lock_print_info_all_transactions(
    FILE *file) /*!< in/out: file where to print */
{
  /* We require exclusive access to lock_sys */
  ut_ad(lock_mutex_own());

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
  const trx_t *prev_trx = 0;

  /* Control whether a block should be fetched from the buffer pool. */
  bool load_block = true;
  bool monitor = srv_print_innodb_lock_monitor;

  while ((trx = trx_iter.current()) != 0) {
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
        /* Resync trx_iter, the trx_sys->mutex and
        the lock mutex were released. A page was
        successfully read in.  We need to print its
        contents on the next call to
        lock_trx_print_locks(). On the next call to
        lock_trx_print_locks() we should simply print
        the contents of the page just read in.*/
        load_block = false;

        continue;
      }
    }

    load_block = true;

    /* All record lock details were printed without fetching
    a page from disk, or we didn't need to print the detail. */
    trx_iter.next();
  }

  lock_mutex_exit();
  mutex_exit(&trx_sys->mutex);

  ut_ad(lock_validate());
}

#ifdef UNIV_DEBUG
/** Check if the lock exists in the trx_t::trx_lock_t::table_locks vector.
@param[in]    trx         the trx to validate
@param[in]    find_lock   lock to find
@return true if found */
static bool lock_trx_table_locks_find(const trx_t *trx,
                                      const lock_t *find_lock) {
  /* We will access trx->lock.table_locks so we need trx->mutex */
  trx_mutex_enter(trx);

  typedef lock_pool_t::const_reverse_iterator iterator;

  const iterator end = trx->lock.table_locks.rend();
  const iterator begin = trx->lock.table_locks.rbegin();
  const bool found = std::find(begin, end, find_lock) != end;

  trx_mutex_exit(trx);

  return (found);
}

/** Validates the lock queue on a table.
 @return true if ok */
static bool lock_table_queue_validate(
    const dict_table_t *table) /*!< in: table */
{
  const lock_t *lock;

  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  for (lock = UT_LIST_GET_FIRST(table->locks); lock != NULL;
       lock = UT_LIST_GET_NEXT(tab_lock.locks, lock)) {
    /* lock->trx->state cannot change from or to NOT_STARTED
    while we are holding the trx_sys->mutex. It may change
    from ACTIVE to PREPARED, but it may not change to
    COMMITTED, because we are holding the lock_sys->mutex. */
    ut_ad(trx_assert_started(lock->trx));

    if (!lock_get_wait(lock)) {
      ut_a(!lock_table_other_has_incompatible(lock->trx, 0, table,
                                              lock_get_mode(lock)));
    } else {
      ut_a(lock_table_has_to_wait_in_queue(lock));
    }

    ut_a(lock_trx_table_locks_find(lock->trx, lock));
  }

  return (true);
}

/** Validates the lock queue on a single record.
 @return true if ok */
static bool lock_rec_queue_validate(
    bool locked_lock_trx_sys,
    /*!< in: if the caller holds
    both the lock mutex and
    trx_sys_t->lock. */
    const buf_block_t *block,  /*!< in: buffer block containing rec */
    const rec_t *rec,          /*!< in: record to look at */
    const dict_index_t *index, /*!< in: index, or NULL if not known */
    const ulint *offsets)      /*!< in: rec_get_offsets(rec, index) */
{
  ut_a(rec);
  ut_a(block->frame == page_align(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));
  ut_ad(lock_mutex_own() == locked_lock_trx_sys);
  ut_ad(!index || index->is_clustered() || !dict_index_is_online_ddl(index));

  ulint heap_no = page_rec_get_heap_no(rec);
  RecID rec_id{block, heap_no};

  if (!locked_lock_trx_sys) {
    lock_mutex_enter();
    mutex_enter(&trx_sys->mutex);
  }

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

    if (!locked_lock_trx_sys) {
      lock_mutex_exit();
      mutex_exit(&trx_sys->mutex);
    }

    return (true);
  }

  if (index == NULL) {
    /* Nothing we can do */

  } else if (index->is_clustered()) {
    trx_id_t trx_id;

    /* Unlike the non-debug code, this invariant can only succeed
    if the check and assertion are covered by the lock mutex. */

    trx_id = lock_clust_rec_some_has_impl(rec, index, offsets);

    const trx_t *impl_trx = trx_rw_is_active_low(trx_id, NULL);
    if (impl_trx != nullptr) {
      ut_ad(lock_mutex_own());
      ut_ad(trx_sys_mutex_own());
      /* impl_trx cannot become TRX_STATE_COMMITTED_IN_MEMORY nor removed from
      rw_trx_set until we release trx_sys->mutex, which means that currently all
      other threads in the system consider this impl_trx active and thus should
      respect implicit locks held by impl_trx*/

      const lock_t *other_lock =
          lock_rec_other_has_expl_req(LOCK_S, block, true, heap_no, impl_trx);

      /* The impl_trx is holding an implicit lock on the
      given record 'rec'. So there cannot be another
      explicit granted lock.  Also, there can be another
      explicit waiting lock only if the impl_trx has an
      explicit granted lock. */

      if (other_lock != NULL) {
        ut_a(lock_get_wait(other_lock));
        ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block, heap_no,
                               impl_trx));
      }
    }
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

  if (!locked_lock_trx_sys) {
    lock_mutex_exit();

    mutex_exit(&trx_sys->mutex);
  }

  return (true);
}

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
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(!lock_mutex_own());

  lock_mutex_enter();
  mutex_enter(&trx_sys->mutex);
loop:
  lock = lock_rec_get_first_on_page_addr(
      lock_sys->rec_hash, block->page.id.space(), block->page.id.page_no());

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
        offsets =
            rec_get_offsets(rec, lock->index, offsets, ULINT_UNDEFINED, &heap);

        /* If this thread is holding the file space
        latch (fil_space_t::latch), the following
        check WILL break the latching order and may
        cause a deadlock of threads. */

        lock_rec_queue_validate(true, block, rec, lock->index, offsets);

        nth_bit = i + 1;

        goto loop;
      }
    }

  nth_bit = 0;
  nth_lock++;

  goto loop;

function_exit:
  lock_mutex_exit();
  mutex_exit(&trx_sys->mutex);

  if (heap != NULL) {
    mem_heap_free(heap);
  }
  return (true);
}

/** Validates the table locks.
 @return true if ok */
static bool lock_validate_table_locks(
    const trx_ut_list_t *trx_list) /*!< in: trx list */
{
  const trx_t *trx;

  /* We need exclusive access to lock_sys to iterate over trxs' locks */
  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  ut_ad(trx_list == &trx_sys->rw_trx_list);

  for (trx = UT_LIST_GET_FIRST(*trx_list); trx != NULL;
       trx = UT_LIST_GET_NEXT(trx_list, trx)) {
    const lock_t *lock;

    check_trx_state(trx);

    for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks); lock != NULL;
         lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
      if (lock_get_type_low(lock) & LOCK_TABLE) {
        lock_table_queue_validate(lock->tab_lock.table);
      }
    }
  }

  return (true);
}

/** Validate record locks up to a limit.
 @return lock at limit or NULL if no more locks in the hash bucket */
static MY_ATTRIBUTE((warn_unused_result)) const lock_t *lock_rec_validate(
    ulint start,     /*!< in: lock_sys->rec_hash
                     bucket */
    uint64_t *limit) /*!< in/out: upper limit of
                     (space, page_no) */
{
  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  for (const lock_t *lock = static_cast<const lock_t *>(
           HASH_GET_FIRST(lock_sys->rec_hash, start));
       lock != NULL;
       lock = static_cast<const lock_t *>(HASH_GET_NEXT(hash, lock))) {
    uint64_t current;

    ut_ad(!trx_is_ac_nl_ro(lock->trx));
    ut_ad(lock_get_type(lock) == LOCK_REC);

    current = ut_ull_create(lock->rec_lock.space, lock->rec_lock.page_no);

    if (current > *limit) {
      *limit = current + 1;
      return (lock);
    }
  }

  return (0);
}

/** Validate a record lock's block */
static void lock_rec_block_validate(space_id_t space_id, page_no_t page_no) {
  /* The lock and the block that it is referring to may be freed at
  this point. We pass Page_fetch::POSSIBLY_FREED to skip a debug check.
  If the lock exists in lock_rec_validate_page() we assert
  !block->page.file_page_was_freed. */

  buf_block_t *block;
  mtr_t mtr;

  /* Make sure that the tablespace is not deleted while we are
  trying to access the page. */
  if (fil_space_t *space = fil_space_acquire(space_id)) {
    mtr_start(&mtr);

    block = buf_page_get_gen(
        page_id_t(space_id, page_no), page_size_t(space->flags), RW_X_LATCH,
        NULL, Page_fetch::POSSIBLY_FREED, __FILE__, __LINE__, &mtr);

    buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

    ut_ad(lock_rec_validate_page(block));
    mtr_commit(&mtr);

    fil_space_release(space);
  }
}

/** Validates the lock system.
 @return true if ok */
static bool lock_validate() {
  typedef std::pair<space_id_t, page_no_t> page_addr_t;
  typedef std::set<page_addr_t, std::less<page_addr_t>,
                   ut_allocator<page_addr_t>>
      page_addr_set;

  page_addr_set pages;

  lock_mutex_enter();
  mutex_enter(&trx_sys->mutex);

  ut_a(lock_validate_table_locks(&trx_sys->rw_trx_list));

  /* Iterate over all the record locks and validate the locks. We
  don't want to hog the lock_sys_t::mutex and the trx_sys_t::mutex.
  Release both mutexes during the validation check. */

  for (ulint i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
    const lock_t *lock;
    uint64_t limit = 0;

    while ((lock = lock_rec_validate(i, &limit)) != nullptr) {
      page_no_t page_no;
      space_id_t space = lock->rec_lock.space;

      page_no = lock->rec_lock.page_no;

      pages.insert(std::make_pair(space, page_no));
    }
  }

  mutex_exit(&trx_sys->mutex);
  lock_mutex_exit();

  for (page_addr_set::const_iterator it = pages.begin(); it != pages.end();
       ++it) {
    lock_rec_block_validate((*it).first, (*it).second);
  }

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
    ibool *inherit)      /*!< out: set to true if the new
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

  dberr_t err;
  lock_t *lock;
  ibool inherit_in = *inherit;
  trx_t *trx = thr_get_trx(thr);
  const rec_t *next_rec = page_rec_get_next_const(rec);
  ulint heap_no = page_rec_get_heap_no(next_rec);

  lock_mutex_enter();
  /* Because this code is invoked for a running transaction by
  the thread that is serving the transaction, it is not necessary
  to hold trx->mutex here. */

  /* When inserting a record into an index, the table must be at
  least IX-locked. When we are building an index, we would pass
  BTR_NO_LOCKING_FLAG and skip the locking altogether. */
  ut_ad(lock_table_has(trx, index->table, LOCK_IX));

  lock = lock_rec_get_first(lock_sys->rec_hash, block, heap_no);

  if (lock == NULL) {
    /* We optimize CPU time usage in the simplest case */

    lock_mutex_exit();

    if (inherit_in && !index->is_clustered()) {
      /* Update the page max trx id field */
      page_update_max_trx_id(block, buf_block_get_page_zip(block), trx->id,
                             mtr);
    }

    *inherit = false;

    return (DB_SUCCESS);
  }

  /* Spatial index does not use GAP lock protection. It uses
  "predicate lock" to protect the "range" */
  if (dict_index_is_spatial(index)) {
    return (DB_SUCCESS);
  }

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

  const lock_t *wait_for =
      lock_rec_other_has_conflicting(type_mode, block, heap_no, trx);

  if (wait_for != NULL) {
    RecLock rec_lock(thr, index, block, heap_no, type_mode);

    trx_mutex_enter(trx);

    err = rec_lock.add_to_waitq(wait_for);

    trx_mutex_exit(trx);

  } else {
    err = DB_SUCCESS;
  }

  lock_mutex_exit();

  switch (err) {
    case DB_SUCCESS_LOCKED_REC:
      err = DB_SUCCESS;
      /* fall through */
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

#ifdef UNIV_DEBUG
  {
    mem_heap_t *heap = NULL;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    const ulint *offsets;
    rec_offs_init(offsets_);

    offsets =
        rec_get_offsets(next_rec, index, offsets_, ULINT_UNDEFINED, &heap);

    ut_ad(lock_rec_queue_validate(false, block, next_rec, index, offsets));

    if (heap != NULL) {
      mem_heap_free(heap);
    }
  }
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
#endif /* UNIV_DEBUG */

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

  lock_mutex_enter();
  /* This trx->mutex acquisition here is not really needed.
  Its purpose is to prevent a state transition between calls to trx_state_eq()
  and lock_rec_add_to_queue().
  But one can prove, that even if the state did change, it is not
  a big problem, because we still keep reference count from dropping
  to zero, so the trx object is still in use, and we hold the lock mutex
  so trx can not release its explicit lock (if it has any) so we will
  notice the explicit lock in lock_rec_has_expl.
  On the other hand if trx does not have explicit lock, then we would create one
  on its behalf, which is wasteful, but does not cause a problem, as once the
  reference count drops to zero the trx will notice and remove this new explicit
  lock.
  Also, even if some other trx had observed that trx is already removed from
  rw trxs list and thus ignored the implicit lock and decided to add its own
  lock, it will still have to wait for lock_mutex before adding her lock.
  However it does not cost us much to simply take the trx->mutex
  and avoid this whole shaky reasoning. */
  trx_mutex_enter(trx);

  ut_ad(!trx_state_eq(trx, TRX_STATE_NOT_STARTED));

  if (!trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY) &&
      !lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block, heap_no, trx)) {
    ulint type_mode;

    type_mode = (LOCK_REC | LOCK_X | LOCK_REC_NOT_GAP);

    lock_rec_add_to_queue(type_mode, block, heap_no, index, trx, true);
  }

  lock_mutex_exit();
  trx_mutex_exit(trx);

  trx_release_reference(trx);

  DEBUG_SYNC_C("after_lock_rec_convert_impl_to_expl_for_trx");
}

/** If a transaction has an implicit x-lock on a record, but no explicit x-lock
set on the record, sets one for it.
@param[in]	block		buffer block of rec
@param[in]	rec		user record on page
@param[in]	index		index of record
@param[in]	offsets		rec_get_offsets(rec, index) */
static void lock_rec_convert_impl_to_expl(const buf_block_t *block,
                                          const rec_t *rec, dict_index_t *index,
                                          const ulint *offsets) {
  trx_t *trx;

  ut_ad(!lock_mutex_own());
  ut_ad(page_rec_is_user_rec(rec));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));

  DEBUG_SYNC_C("lock_rec_convert_impl_to_expl");

  if (index->is_clustered()) {
    trx_id_t trx_id;

    trx_id = lock_clust_rec_some_has_impl(rec, index, offsets);

    trx = trx_rw_is_active(trx_id, NULL, true);
  } else {
    ut_ad(!dict_index_is_online_ddl(index));

    trx = lock_sec_rec_some_has_impl(rec, index, offsets);
    if (trx) {
      DEBUG_SYNC_C("lock_rec_convert_impl_to_expl_will_validate");
      ut_ad(!lock_rec_other_trx_holds_expl(LOCK_S | LOCK_REC_NOT_GAP, trx, rec,
                                           block));
    }
  }

  if (trx != 0) {
    ulint heap_no = page_rec_get_heap_no(rec);

    ut_ad(trx_is_referenced(trx));

    /* If the transaction is still active and has no
    explicit x-lock set on the record, set one for it.
    trx cannot be committed until the ref count is zero. */

    lock_rec_convert_impl_to_expl_for_trx(block, rec, index, offsets, trx,
                                          heap_no);
  }
}

void lock_rec_convert_active_impl_to_expl(const buf_block_t *block,
                                          const rec_t *rec, dict_index_t *index,
                                          const ulint *offsets, trx_t *trx,
                                          ulint heap_no) {
  trx_reference(trx, true);
  lock_rec_convert_impl_to_expl_for_trx(block, rec, index, offsets, trx,
                                        heap_no);
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

  lock_mutex_enter();

  ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

  err = lock_rec_lock(true, SELECT_ORDINARY, LOCK_X | LOCK_REC_NOT_GAP, block,
                      heap_no, index, thr);

  MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);

  lock_mutex_exit();

  ut_ad(lock_rec_queue_validate(false, block, rec, index, offsets));

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

  lock_mutex_enter();

  ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

  err = lock_rec_lock(true, SELECT_ORDINARY, LOCK_X | LOCK_REC_NOT_GAP, block,
                      heap_no, index, thr);

  MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);

  lock_mutex_exit();

#ifdef UNIV_DEBUG
  {
    mem_heap_t *heap = NULL;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    const ulint *offsets;
    rec_offs_init(offsets_);

    offsets = rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap);

    ut_ad(lock_rec_queue_validate(false, block, rec, index, offsets));

    if (heap != NULL) {
      mem_heap_free(heap);
    }
  }
#endif /* UNIV_DEBUG */

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

  /* Some transaction may have an implicit x-lock on the record only
  if the max trx id for the page >= min trx id for the trx list or a
  database recovery is running. */

  if ((page_get_max_trx_id(block->frame) >= trx_rw_min_trx_id() ||
       recv_recovery_is_on()) &&
      !page_rec_is_supremum(rec)) {
    lock_rec_convert_impl_to_expl(block, rec, index, offsets);
  }

  lock_mutex_enter();

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

  lock_mutex_exit();
  DEBUG_SYNC_C("lock_sec_rec_read_check_and_lock_has_locked");

  ut_ad(lock_rec_queue_validate(false, block, rec, index, offsets));
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
  lock_mutex_enter();

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

  lock_mutex_exit();

  ut_ad(lock_rec_queue_validate(false, block, rec, index, offsets));

  DEBUG_SYNC_C("after_lock_clust_rec_read_check_and_lock");
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
  mem_heap_t *tmp_heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  dberr_t err;
  rec_offs_init(offsets_);

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &tmp_heap);
  err = lock_clust_rec_read_check_and_lock(lock_duration_t::REGULAR, block, rec,
                                           index, offsets, SELECT_ORDINARY,
                                           mode, gap_mode, thr);
  if (tmp_heap) {
    mem_heap_free(tmp_heap);
  }

  if (err == DB_SUCCESS_LOCKED_REC) {
    err = DB_SUCCESS;
  }
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

/** Release the last lock from the transaction's autoinc locks.
@param[in]  trx   trx which vector of AUTOINC locks to modify */
UNIV_INLINE
void lock_release_autoinc_last_lock(trx_t *trx) {
  ulint last;
  lock_t *lock;

  /* We will access trx->lock.autoinc_locks which requires trx->mutex */
  ut_ad(trx_mutex_own(trx));
  ib_vector_t *autoinc_locks = trx->lock.autoinc_locks;

  ut_ad(lock_mutex_own());
  ut_a(!ib_vector_is_empty(autoinc_locks));

  /* The lock to be release must be the last lock acquired. */
  last = ib_vector_size(autoinc_locks) - 1;
  lock = *static_cast<lock_t **>(ib_vector_get(autoinc_locks, last));

  /* Should have only AUTOINC locks in the vector. */
  ut_a(lock_get_mode(lock) == LOCK_AUTO_INC);
  ut_a(lock_get_type(lock) == LOCK_TABLE);

  ut_a(lock->tab_lock.table != NULL);

  /* This will remove the lock from the trx autoinc_locks too. */
  lock_table_dequeue(lock);

  /* Remove from the table vector too. */
  lock_trx_table_locks_remove(lock);
}

/** Check if a transaction holds any autoinc locks.
 @return true if the transaction holds any AUTOINC locks. */
static bool lock_trx_holds_autoinc_locks(
    const trx_t *trx) /*!< in: transaction */
{
  /* We will access trx->lock.autoinc_locks which requires trx->mutex */
  ut_ad(trx_mutex_own(trx));
  ut_a(trx->lock.autoinc_locks != NULL);

  return (!ib_vector_is_empty(trx->lock.autoinc_locks));
}

/** Release all the transaction's autoinc locks. */
static void lock_release_autoinc_locks(trx_t *trx) /*!< in/out: transaction */
{
  ut_ad(lock_mutex_own());
  ut_ad(trx_mutex_own(trx));

  ut_a(trx->lock.autoinc_locks != NULL);

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
  return (reinterpret_cast<uint64_t>(lock));
}

/** Get the performance schema event (thread_id, event_id)
that created the lock.
@param[in]	lock		Lock
@param[out]	thread_id	Thread ID that created the lock
@param[out]	event_id	Event ID that created the lock
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
@param[in]	trx_lock	the trx lock
@return The first lock
*/
const lock_t *lock_get_first_trx_locks(const trx_lock_t *trx_lock) {
  /* trx->lock.trx_locks is protected by trx->mutex and lock_sys mutex, and we
  assume we have the exclusive latch on lock_sys here */
  ut_ad(lock_mutex_own());
  const lock_t *result = UT_LIST_GET_FIRST(trx_lock->trx_locks);
  return (result);
}

/** Get the next lock of a trx lock list.
@param[in]	lock	the current lock
@return The next lock
*/
const lock_t *lock_get_next_trx_locks(const lock_t *lock) {
  /* trx->lock.trx_locks is protected by trx->mutex and lock_sys mutex, and we
  assume we have the exclusive latch on lock_sys here */
  ut_ad(lock_mutex_own());
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
  /* We might need to modify lock_cached_lock_mode_names, so we need exclusive
  access. Thankfully lock_get_mode_str is used only while holding the
  lock_sys->mutex so we don't need dedicated mutex */
  ut_ad(lock_mutex_own());

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
  char *name_buffer = (char *)ut_malloc_nokey(name_string.length() + 1);
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
UNIV_INLINE
dict_table_t *lock_get_table(const lock_t *lock) /*!< in: lock */
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
@param[in]	lock	the lock
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

/** For a record lock, gets the tablespace number on which the lock is.
 @return tablespace number */
space_id_t lock_rec_get_space_id(const lock_t *lock) /*!< in: lock */
{
  ut_a(lock_get_type_low(lock) == LOCK_REC);

  return (lock->rec_lock.space);
}

/** For a record lock, gets the page number on which the lock is.
 @return page number */
page_no_t lock_rec_get_page_no(const lock_t *lock) /*!< in: lock */
{
  ut_a(lock_get_type_low(lock) == LOCK_REC);

  return (lock->rec_lock.page_no);
}

/** Cancels a waiting lock request and releases possible other transactions
waiting behind it.
@param[in,out]	lock		Waiting lock request
@param[in]	use_fcfs	true -> use first come first served strategy */
void lock_cancel_waiting_and_release(lock_t *lock, bool use_fcfs) {
  ut_ad(lock_mutex_own());
  /* We will access lock->trx->lock.autoinc_locks which requires trx->mutex */
  ut_ad(trx_mutex_own(lock->trx));

  if (lock_get_type_low(lock) == LOCK_REC) {
    lock_rec_dequeue_from_page(lock, use_fcfs);
  } else {
    ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

    if (lock->trx->lock.autoinc_locks != NULL) {
      /* Release the transaction's AUTOINC locks. */
      lock_release_autoinc_locks(lock->trx);
    }

    lock_table_dequeue(lock);
  }

  lock_reset_wait_and_release_thread_if_suspended(lock);
}

/** Unlocks AUTO_INC type locks that were possibly reserved by a trx. This
 function should be called at the the end of an SQL statement, by the
 connection thread that owns the transaction (trx->mysql_thd). */
void lock_unlock_table_autoinc(trx_t *trx) /*!< in/out: transaction */
{
  ut_ad(!lock_mutex_own());
  ut_ad(!trx_mutex_own(trx));
  ut_ad(!trx->lock.wait_lock);

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
  bool might_have_autoinc_locks = lock_trx_holds_autoinc_locks(trx);
  trx_mutex_exit(trx);

  if (might_have_autoinc_locks) {
    lock_mutex_enter();
    trx_mutex_enter(trx);
    lock_release_autoinc_locks(trx);
    lock_mutex_exit();
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

  if (trx_is_referenced(trx)) {
    while (trx_is_referenced(trx)) {
      trx_mutex_exit(trx);

      DEBUG_SYNC_C("waiting_trx_is_not_referenced");

      /** Doing an implicit to explicit conversion
      should not be expensive. */
      ut_delay(ut_rnd_interval(0, srv_spin_wait_delay));

      trx_mutex_enter(trx);
    }
  }

  ut_ad(!trx_is_referenced(trx));

  /* If the background thread trx_rollback_or_clean_recovered()
  is still active then there is a chance that the rollback
  thread may see this trx as COMMITTED_IN_MEMORY and goes ahead
  to clean it up calling trx_cleanup_at_db_startup(). This can
  happen in the case we are committing a trx here that is left
  in PREPARED state during the crash. Note that commit of the
  rollback of a PREPARED trx happens in the recovery thread
  while the rollback of other transactions happen in the
  background thread. To avoid this race we unconditionally unset
  the is_recovered flag. */

  trx->is_recovered = false;

  trx_mutex_exit(trx);

  lock_release(trx);

  /* We don't remove the locks one by one from the vector for
  efficiency reasons. We simply reset it because we would have
  released all the locks anyway.
  At this point there should be no one else interested in our trx's
  locks as we've released and removed all of them, and the trx is no longer
  referenced so nobody will attempt implicit to explicit conversion neither.
  Please note that we are either the thread which runs the transaction, or we
  are the thread of a high priority transaction which decided to kill trx, in
  which case it had to first make sure that it is no longer running in InnoDB.
  So the race between lock_table() accessing table_locks, and our clear() should
  not happen.
  All that being said, it does not cost us anything in terms of performance to
  protect these operations with trx->mutex, which makes some class of errors
  impossible even if the above reasoning was wrong. */
  trx_mutex_enter(trx);
  trx->lock.table_locks.clear();
  trx->lock.n_rec_locks.store(0);

  ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);
  ut_a(ib_vector_is_empty(trx->lock.autoinc_locks));
  ut_a(trx->lock.table_locks.empty());

  mem_heap_empty(trx->lock.lock_heap);
  trx_mutex_exit(trx);
}

/** Check whether the transaction has already been rolled back because it
 was selected as a deadlock victim, or if it has to wait then cancel
 the wait lock.
 @return DB_DEADLOCK, DB_LOCK_WAIT or DB_SUCCESS */
dberr_t lock_trx_handle_wait(trx_t *trx) /*!< in/out: trx lock state */
{
  dberr_t err;

  lock_mutex_enter();

  trx_mutex_enter(trx);

  if (trx->lock.was_chosen_as_deadlock_victim) {
    err = DB_DEADLOCK;
  } else if (trx->lock.wait_lock != NULL) {
    lock_cancel_waiting_and_release(trx->lock.wait_lock, false);
    err = DB_LOCK_WAIT;
  } else {
    /* The lock was probably granted before we got here. */
    err = DB_SUCCESS;
  }

  lock_mutex_exit();
  trx_mutex_exit(trx);

  return (err);
}

#ifdef UNIV_DEBUG
/** Do an exhaustive check for any locks (table or rec) against the table.
 @return lock if found */
static const lock_t *lock_table_locks_lookup(
    const dict_table_t *table,     /*!< in: check if there are
                                   any locks held on records in
                                   this table or on the table
                                   itself */
    const trx_ut_list_t *trx_list) /*!< in: trx list to check */
{
  const trx_t *trx;

  ut_a(table != NULL);
  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  for (trx = UT_LIST_GET_FIRST(*trx_list); trx != NULL;
       trx = UT_LIST_GET_NEXT(trx_list, trx)) {
    const lock_t *lock;

    check_trx_state(trx);

    for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks); lock != NULL;
         lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
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

  return (NULL);
}
#endif /* UNIV_DEBUG */

/** Check if there are any locks (table or rec) against table.
 @return true if table has either table or record locks. */
bool lock_table_has_locks(
    const dict_table_t *table) /*!< in: check if there are any locks
                               held on records in this table or on the
                               table itself */
{
  ibool has_locks;

  lock_mutex_enter();

  has_locks = UT_LIST_GET_LEN(table->locks) > 0 || table->n_rec_locks > 0;

#ifdef UNIV_DEBUG
  if (!has_locks) {
    mutex_enter(&trx_sys->mutex);

    ut_ad(!lock_table_locks_lookup(table, &trx_sys->rw_trx_list));

    mutex_exit(&trx_sys->mutex);
  }
#endif /* UNIV_DEBUG */

  lock_mutex_exit();

  return (has_locks);
}

/** Initialise the table lock list. */
void lock_table_lock_list_init(
    table_lock_list_t *lock_list) /*!< List to initialise */
{
  UT_LIST_INIT(*lock_list, &lock_table_t::locks);
}

/** Initialise the trx lock list. */
void lock_trx_lock_list_init(
    trx_lock_list_t *lock_list) /*!< List to initialise */
{
  UT_LIST_INIT(*lock_list, &lock_t::trx_locks);
}

/** Set the lock system timeout event. */
void lock_set_timeout_event() { os_event_set(lock_sys->timeout_event); }

#ifdef UNIV_DEBUG

bool lock_trx_has_rec_x_lock(que_thr_t *thr, const dict_table_t *table,
                             const buf_block_t *block, ulint heap_no) {
  ut_ad(heap_no > PAGE_HEAP_NO_SUPREMUM);

  const trx_t *trx = thr_get_trx(thr);
  lock_mutex_enter();
  ut_a(lock_table_has(trx, table, LOCK_IX) || table->is_temporary());
  ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block, heap_no, trx) ||
       table->is_temporary());
  lock_mutex_exit();
  return (true);
}
#endif /* UNIV_DEBUG */

/** rewind(3) the file used for storing the latest detected deadlock and
print a heading message to stderr if printing of all deadlocks to stderr
is enabled. */
void Deadlock_notifier::start_print() {
  /* I/O operations on lock_latest_err_file require exclusive latch on
  lock_sys */
  ut_ad(lock_mutex_own());

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
  ut_ad(lock_mutex_own());
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
  ut_ad(lock_mutex_own());

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
  ut_ad(lock_mutex_own());

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
  ut_ad(lock_mutex_own());
  ut::ostringstream buff;
  buff << "\n*** (" << (pos_on_cycle + 1) << ") " << title << ":\n";
  print(buff.str().c_str());
}

void Deadlock_notifier::notify(const ut::vector<const trx_t *> &trxs_on_cycle,
                               const trx_t *victim_trx) {
  ut_ad(lock_mutex_own());

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
@param trx		allocate cached record locks for this transaction */
void lock_trx_alloc_locks(trx_t *trx) {
  /* We will create trx->lock.table_pool and rec_pool which are protected by
  trx->mutex. In theory nobody else should use the trx object while it is being
  constructed, but how can we (the lock-sys) "know" about it and why risk? */
  trx_mutex_enter(trx);
  ulint sz = REC_LOCK_SIZE * REC_LOCK_CACHE;
  byte *ptr = reinterpret_cast<byte *>(ut_malloc_nokey(sz));

  /* We allocate one big chunk and then distribute it among
  the rest of the elements. The allocated chunk pointer is always
  at index 0. */

  for (ulint i = 0; i < REC_LOCK_CACHE; ++i, ptr += REC_LOCK_SIZE) {
    trx->lock.rec_pool.push_back(reinterpret_cast<ib_lock_t *>(ptr));
  }

  sz = TABLE_LOCK_SIZE * TABLE_LOCK_CACHE;
  ptr = reinterpret_cast<byte *>(ut_malloc_nokey(sz));

  for (ulint i = 0; i < TABLE_LOCK_CACHE; ++i, ptr += TABLE_LOCK_SIZE) {
    trx->lock.table_pool.push_back(reinterpret_cast<ib_lock_t *>(ptr));
  }
  trx_mutex_exit(trx);
}

void lock_notify_about_deadlock(const ut::vector<const trx_t *> &trxs_on_cycle,
                                const trx_t *victim_trx) {
  Deadlock_notifier::notify(trxs_on_cycle, victim_trx);
}