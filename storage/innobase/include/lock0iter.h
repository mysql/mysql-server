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

/** @file include/lock0iter.h
 Lock queue iterator type and function prototypes.

 Created July 16, 2007 Vasil Dimov
 *******************************************************/

#ifndef lock0iter_h
#define lock0iter_h

#include "dict0types.h"
#include "lock0types.h"
#include "univ.i"
namespace locksys {
/** Calls visitor for each lock_t object which is a reason that wait_lock has to
wait. It is assumed that the wait_lock is waiting, and the caller has latched
the shard which contains the wait_lock
@param[in]  wait_lock   the waiting lock
@param[in]  visitor     a function to be called for each lock, s.t.
                        locksys::has_to_wait(wait_lock, lock) is true.
                        To stop iteration the visitor can return true, in which
                        case the lock for which it happened will be returned.
@return the first lock for which visitor returned true (in which case the search
ends) or nullptr if visitor never returned true (so all waiters were visited).*/
// TODO: this should use ut0function_reference.h or std::function_ref
const lock_t *find_blockers(const lock_t &wait_lock,
                            std::function<bool(const lock_t &)> visitor);
}  // namespace locksys
/** Iterates over all locks in the lock sys in a manner which guarantees that
all locks from the same lock queue are processed in a single critical section.*/
class All_locks_iterator {
 public:
  /** Processes a batch of one or more non-empty lock queues, calling the
  provided function f for each lock in the queue, making sure that the queue is
  not being modified during processing it.
  Please note, that this means that the locks from a single lock queue visited
  by f() present a consistent snapshot of this queue, however locks which reside
  in different queues, may be inconsistent with each other, as they are observed
  at different "times".
  Also, this iterator does not guarantee reporting all locks in case the
  lock-sys is being resized in parallel by lock_sys_resize() - resizing causes
  the iterator to stop processing to avoid double-reporting.
  @return true iff the iterator is done, and calling it again will not provide
  any further results */
  bool iterate_over_next_batch(
      const std::function<void(const lock_t &lock)> &f);

 private:
  /** This iterator moves through the following stages, where the move to next
  stage occurs when all locks from previous stage were reported. */
  enum class stage_t {
    /** iterator was just created (which does not cost much) */
    NOT_STARTED,
    /** iterating over LOCK_TABLE locks for tables from m_table_ids */
    TABLE_LOCKS,
    /** iterating over LOCK_PRDT_PAGE in lock_sys->prdt_page_hash */
    PRDT_PAGE_LOCKS,
    /** iterating over LOCK_PREDICATE locks in lock_sys->prdt_hash */
    PRDT_LOCKS,
    /** iterating over other (non-predicate) LOCK_RECORD locks in
    lock_sys->rec_hash */
    REC_LOCKS,
    /** finished iterating, nothing more to see */
    DONE,
  };

  /** The current stage this iterator is in. */
  stage_t m_stage{stage_t::NOT_STARTED};

  /** List of ids of all tables found in dict sys which are candidates for
  inspection in TABLE_LOCKS stage */
  std::vector<table_id_t> m_table_ids;

  /** Tracks progress within a single stage: index of table in m_table_ids for
  the TABLE_LOCKS stage, and cell of the hash_table for record locks.
  It is reset to 0 at the beginning of each stage. */
  size_t m_bucket_id{0};

  /** The value of lock_sys->n_resizes is stored in this field at the begging
  of stages involving iterating over lock sys hash tables so that we can spot
  if the hash table got resized during our iteration and invalidate the iterator
  */
  uint32_t m_lock_sys_n_resizes_at_the_beginning{0};

 private:
  /** Helper function for TABLE_LOCKS stage.
  Calls f for all locks associated with m_table_ids[m_bucket_id].
  @param[in]  f           function to apply to each lock
  @return true iff it succeeded */
  template <typename F>
  bool iterate_over_current_table(F &&f);

  /** Helper function for PRDT_PAGE_LOCKS, PRDT_LOCKS and REC_LOCKS stages.
  Calls f for all locks associated with hash_table m_bucket_id-th cell.
  @param[in]  hash_table  hash_table to inspect
  @param[in]  f           function to apply to each lock
  @return true iff it succeeded */
  template <typename F>
  bool iterate_over_current_cell(struct Locks_hashtable &hash_table, F &&f);
};

#endif /* lock0iter_h */
