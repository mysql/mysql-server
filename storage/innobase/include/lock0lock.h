/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file include/lock0lock.h
 The transaction lock system

 Created 5/7/1996 Heikki Tuuri
 *******************************************************/

#ifndef lock0lock_h
#define lock0lock_h

#include "buf0types.h"
#include "dict0types.h"
#include "hash0hash.h"
#include "lock0types.h"
#include "mtr0types.h"
#include "que0types.h"
#include "rem0types.h"
#include "srv0srv.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0vec.h"
#ifndef UNIV_HOTBACKUP
#include "gis0rtree.h"
#endif /* UNIV_HOTBACKUP */
#include "lock0latches.h"
#include "lock0prdt.h"

/**
@page PAGE_INNODB_LOCK_SYS Innodb Lock-sys


@section sect_lock_sys_introduction Introduction

The Lock-sys orchestrates access to tables and rows. Each table, and each row,
can be thought of as a resource, and a transaction may request access right for
a resource. As two transactions operating on a single resource can lead to
problems if the two operations conflict with each other, each lock request also
specifies the way the transaction intends to use it, by providing a `mode`. For
example a LOCK_X mode, means that transaction needs exclusive access
(presumably, it will modify the resource), and LOCK_S mode means that a
transaction can share the resource with other transaction which also use LOCK_S
mode. There are many different possible modes beside these two, and the logic of
checking if given two modes are in conflict is a responsibility of the Lock-sys.
A lock request, is called "a lock" for short.
A lock can be WAITING or GRANTED.

So, a lock, conceptually is a tuple identifying:
- requesting transaction
- resource (a particular row, a particular table)
- mode (LOCK_X, LOCK_S,...)
- state (WAITING or GRANTED)

@remark
In current implementation the "resource" and "mode" are not cleanly separated as
for example LOCK_GAP and LOCK_REC_NOT_GAP are often called "modes" even though
their semantic is to specify which "sub-resource" (the gap before the row, or
the row itself) the transaction needs to access.

@remark
The Lock-sys identifies records by their page_no (the identifier of the page
which contains the record) and the heap_no (the position in page's internal
array of allocated records), as opposed to table, index and primary key. This
becomes important in case of B-tree merges, splits, or reallocation of variable-
length records, all of which need to notify the Lock-sys to reflect the change.

Conceptually, the Lock-sys maintains a separate queue for each resource, thus
one can analyze and reason about its operations in the scope of a single queue.

@remark
In practice, locks for gaps and rows are treated as belonging to the same queue.
Moreover, to save space locks of a transaction which refer to several rows on
the same page might be stored in a single data structure, and thus the physical
queue corresponds to a whole page, and not to a single row.
Also, each predicate lock (from GIS) is tied to a page, not a record.
Finally, the lock queue is implemented by reusing chain links in the hash table,
which means that pages with equal hash are held together in a single linked
list for their hash cell.
Therefore care must be taken to filter the subset of locks which refer to a
given resource when accessing these data structures.

The life cycle of a lock is usually as follows:

-# The transaction requests the lock, which can either be immediately GRANTED,
   or, in case of a conflict with an existing lock, goes to the WAITING state.
-# In case the lock is WAITING the thread (voluntarily) goes to sleep.
-# A WAITING lock either becomes GRANTED (once the conflicting transactions
   finished and it is our turn) or (in case of a rollback) it gets canceled.
-# Once the transaction is finishing (due to commit or rollback) it releases all
   of its locks.

@remark For performance reasons, in Read Committed and weaker Isolation Levels
there is also a Step in between 3 and 4 in which we release some of the read
locks on gaps, which is done to minimize risk of deadlocks during replication.

When a lock is released (due to cancellation in Step 3, or clean up in Step 4),
the Lock-sys inspects the corresponding lock queue to see if one or more of the
WAITING locks in it can now be granted. If so, some locks become GRANTED and the
Lock-sys signals their threads to wake up.


@section sect_lock_sys_scheduling The scheduling algorithm

We use a variant of the algorithm described in paper "Contention-Aware Lock
Scheduling for Transactional Databases" by Boyu Tian, Jiamin Huang, Barzan
Mozafari and Grant Schoenebeck.
The algorithm, "CATS" for short, analyzes the Wait-for graph, and assigns a
weight to each WAITING transaction, equal to the number of transactions which
it (transitively) blocks. The idea being that favoring heavy transactions will
help to make more progress by helping more transactions to become eventually
runnable.

The actual implementation of this theoretical idea is currently as follows.

-# Locks can be thought of being in 2 logical groups (Granted & Waiting)
   maintained in the same queue.

  -# Granted locks are added at the HEAD of the queue.
  -# Waiting locks are added at the TAIL of the queue.
  .
  The queue looks like:
  ```
                                           |
Grows <---- [HEAD] [G7 -- G3 -- G2 -- G1] -|- [W4 -- W5 -- W6] [TAIL] ---> Grows
                         Grant Group       |         Wait Group

        G - Granted W - waiting,
        suffix number is the chronological order of requests.
  ```
  @remark
    - In the Wait Group the locks are in chronological order. We will not assert
      this invariant as there is no significance of the order (and hence the
      position) as the locks are re-ordered based on CATS weight while making a
      choice for grant, and CATS weights change constantly to reflect current
      shape of the Wait-for graph.
    - In the Grant Group the locks are in reverse chronological order. We will
      assert this invariant. CATS algorithm doesn't need it, but deadlock
      detection does, as explained further below.
-# When a new lock request comes, we check for conflict with all (GRANTED and
   WAITING) locks already in the queue.
    -# If there is a conflicting lock already in the queue, then the new lock
       request is put into WAITING state and appended at the TAIL. The
       transaction which requested the conflicting lock found is said to be the
       Blocking Transaction for the incoming transaction. As each transaction
       can have at most one WAITING lock, it also can have at most one Blocking
       Transaction, and thus we store the information about Blocking Transaction
       (if any) in the transaction object itself (as opposed to: separately for
       each lock request).
    -# If there is no conflict, the request can be GRANTED, and lock is
       prepended at the HEAD.

-# When we release a lock, locks which conflict with it need to be checked again
if they can now be granted. Note that if there are multiple locks which could be
granted, the order in which we decide to grant has an impact on who will have to
wait: granting a lock to one transaction, can prevent another waiting
transaction from being granted if their request conflict with each other.
At the minimum, the Lock-sys must guarantee that a newly GRANTED lock,
does not conflict with any other GRANTED lock.
Therefore, we will specify the order in which the Lock-sys checks the WAITING
locks one by one, and assume that such check involves checking if there is any
conflict with already GRANTED locks - if so, the lock remains WAITING, we update
the Blocking Transaction of the lock to be the newly identified conflicting
transaction, and we check a next lock from the sorted list, otherwise, we grant
it (and thus it is checked against in subsequent checks).
The Lock-sys uses CATS weight for ordering: it favors transactions with highest
CATS weight.
Moreover, only the locks which point to the transaction currently releasing the
lock as their Blocking Transaction participate as candidates for granting a
lock.

@remark
For each WAITING lock the Blocking Transaction always points to a transaction
which has a conflicting lock request, so if the Blocking Transaction is not the
one which releases the lock right now, then we know that there is still at least
one conflicting transaction. However, there is a subtle issue here: when we
request a lock in point 2. we check for conflicts with both GRANTED and WAITING
locks, while in point 3. we only check for conflicts with GRANTED locks. So, the
Blocking Transaction might be a WAITING one identified in point 2., so we
might be tempted to ignore it in point 3. Such "bypassing of waiters" is
intentionally prevented to avoid starvation of a WAITING LOCK_X, by a steady
stream of LOCK_S requests. Respecting the rule that a Blocking Transaction has
to finish before a lock can be granted implies that at least one of WAITING
LOCK_Xs will be granted before a LOCK_S can be granted.

@remark
High Priority transactions in Wait Group are unconditionally kept ahead while
sorting the wait queue. The HP is a concept related to Group Replication, and
currently has nothing to do with CATS weight.

@subsection subsect_lock_sys_blocking How do we choose the Blocking Transaction?

It is done differently for new lock requests in point 2. and differently for
old lock requests in point 3.

For new lock requests, we simply scan the whole queue in its natural order,
and the first conflicting lock is chosen. In particular, a WAITING transaction
can be chosen, if it is conflicting, and there are no GRATNED conflicting locks.

For old lock requests we scan only the Grant Group, and we do so in the
chronological order, starting from the oldest lock requests [G1,G2,G3,G7] that
is from the middle of the queue towards HEAD. In particular we also check
against the locks which recently become GRANTED as they were processed before us
in the sorting order, and we do so in a chronological order as well.

@remark
The idea here is that if we chose G1 as the Blocking Transaction and if there
existed a dead lock with another conflicting transaction G3, the deadlock
detection would not be postponed indefinitely while new GRANTED locks are
added as they are going to be added to HEAD only.
In other words: each of the conflicting locks in the Grant Group will eventually
be set as the Blocking Transaction at some point in time, and thus it will
become visible for the deadlock detection.
If, by contrast, we were always picking the first one in the natural order, it
might happen that we never get to assign G3 as the Blocking Transaction
because new conflicting locks appear in front of the queue (and are released).
That might lead to the deadlock with G3 never being noticed.

*/

// Forward declaration
class ReadView;

extern bool innobase_deadlock_detect;

/** Gets the size of a lock struct.
 @return size in bytes */
ulint lock_get_size(void);
/** Creates the lock system at database start. */
void lock_sys_create(
    ulint n_cells); /*!< in: number of slots in lock hash table */

/** Resize the lock hash tables.
@param[in]      n_cells number of slots in lock hash table */
void lock_sys_resize(ulint n_cells);

/** Closes the lock system at database shutdown. */
void lock_sys_close(void);
/** Gets the heap_no of the smallest user record on a page.
 @return heap_no of smallest user record, or PAGE_HEAP_NO_SUPREMUM */
static inline ulint lock_get_min_heap_no(
    const buf_block_t *block); /*!< in: buffer block */
/** Updates the lock table when we have reorganized a page. NOTE: we copy
 also the locks set on the infimum of the page; the infimum may carry
 locks if an update of a record is occurring on the page, and its locks
 were temporarily stored on the infimum. */
void lock_move_reorganize_page(
    const buf_block_t *block,   /*!< in: old index page, now
                                reorganized */
    const buf_block_t *oblock); /*!< in: copy of the old, not
                                reorganized page */

/** Moves the explicit locks on user records to another page if a record
list end is moved to another page.
@param[in] new_block Index page to move to
@param[in] block Index page
@param[in,out] rec Record on page: this is the first record moved */
void lock_move_rec_list_end(const buf_block_t *new_block,
                            const buf_block_t *block, const rec_t *rec);

/** Moves the explicit locks on user records to another page if a record
 list start is moved to another page.
@param[in] new_block Index page to move to
@param[in] block Index page
@param[in,out] rec Record on page: this is the first record not copied
@param[in] old_end Old previous-to-last record on new_page before the records
were copied */
void lock_move_rec_list_start(const buf_block_t *new_block,
                              const buf_block_t *block, const rec_t *rec,
                              const rec_t *old_end);

/** Updates the lock table when a page is split to the right.
@param[in] right_block Right page
@param[in] left_block Left page */
void lock_update_split_right(const buf_block_t *right_block,
                             const buf_block_t *left_block);

/** Updates the lock table when a page is merged to the right.
@param[in] right_block Right page to which merged
@param[in] orig_succ Original successor of infimum on the right page before
merge
@param[in] left_block Merged index page which will be discarded */
void lock_update_merge_right(const buf_block_t *right_block,
                             const rec_t *orig_succ,
                             const buf_block_t *left_block);

/** Updates the lock table when the root page is copied to another in
 btr_root_raise_and_insert. Note that we leave lock structs on the
 root page, even though they do not make sense on other than leaf
 pages: the reason is that in a pessimistic update the infimum record
 of the root page will act as a dummy carrier of the locks of the record
 to be updated. */
void lock_update_root_raise(
    const buf_block_t *block, /*!< in: index page to which copied */
    const buf_block_t *root); /*!< in: root page */

/** Updates the lock table when a page is copied to another and the original
 page is removed from the chain of leaf pages, except if page is the root!
@param[in] new_block Index page to which copied
@param[in] block Index page; not the root! */
void lock_update_copy_and_discard(const buf_block_t *new_block,
                                  const buf_block_t *block);

/** Requests the Lock System to update record locks regarding the gap between
the last record of the left_page and the first record of the right_page when the
caller is about to prepended a new record as the first record on the right page,
even though it should "naturally" be inserted as the last record of the
left_page according to the information in the higher levels of the index.

That is, we assume that the lowest common ancestor of the left_page and the
right_page routes the key of the new record to the left_page, but a heuristic
which tries to avoid overflowing the left_page has chosen to prepend the new
record to the right_page instead. Said ancestor performs this routing by
comparing the key of the record to a "split point" - the key associated with the
right_page's subtree, such that all records larger than that split point are to
be found in the right_page (or some even further page). Ideally this should be
the minimum key in this whole subtree, however due to the way we optimize the
DELETE and INSERT operations, we often do not update this information, so that
such "split point" can actually be smaller than the real minimum. Still, even if
not up-to-date, its value is always correct, in that it really separates the
subtrees (keys smaller than "split point" are not in left_page and larger are
not in right_page).

The reason this is important to Lock System, is that the gap between the last
record on the left_page and the first record on the right_page is represented as
two gaps:
1. The gap between the last record on the left_page and the "split point",
represented as the gap before the supremum pseudo-record of the left_page.
2. The gap between the "split point" and the first record of the right_page,
represented as the gap before the first user record of the right_page.

Thus, inserting the new record, and subsequently adjusting "split points" in its
ancestors to values smaller or equal to the new records' key, will mean that gap
will be sliced at a different place ("moved to the left"): fragment of the 1st
gap will now become treated as 2nd. Therefore, Lock System must copy any GRANTED
locks from 1st gap to the 2nd gap. Any WAITING locks must be of INSERT_INTENTION
type (as no other GAP locks ever wait for anything) and can stay at 1st gap, as
their only purpose is to notify the requester they can retry insertion, and
there's no correctness requirement to avoid waking them up too soon.

@param[in] right_block Right page
@param[in] left_block Left page */
void lock_update_split_point(const buf_block_t *right_block,
                             const buf_block_t *left_block);

/** Updates the lock table when a page is split to the left.
@param[in] right_block Right page
@param[in] left_block Left page */
void lock_update_split_left(const buf_block_t *right_block,
                            const buf_block_t *left_block);

/** Updates the lock table when a page is merged to the left.
@param[in] left_block Left page to which merged
@param[in] orig_pred Original predecessor of supremum on the left page before
merge
@param[in] right_block Merged index page which will be discarded */
void lock_update_merge_left(const buf_block_t *left_block,
                            const rec_t *orig_pred,
                            const buf_block_t *right_block);

/** Resets the original locks on heir and replaces them with gap type locks
 inherited from rec.
@param[in] heir_block Block containing the record which inherits
@param[in] block Block containing the record from which inherited; does not
reset the locks on this record
@param[in] heir_heap_no Heap_no of the inheriting record
@param[in] heap_no Heap_no of the donating record */
void lock_rec_reset_and_inherit_gap_locks(const buf_block_t *heir_block,
                                          const buf_block_t *block,
                                          ulint heir_heap_no, ulint heap_no);

/** Updates the lock table when a page is discarded.
@param[in] heir_block Index page which will inherit the locks
@param[in] heir_heap_no Heap_no of the record which will inherit the locks
@param[in] block Index page which will be discarded */
void lock_update_discard(const buf_block_t *heir_block, ulint heir_heap_no,
                         const buf_block_t *block);

/** Updates the lock table when a new user record is inserted.
@param[in] block Buffer block containing rec
@param[in] rec The inserted record */
void lock_update_insert(const buf_block_t *block, const rec_t *rec);

/** Updates the lock table when a record is removed.
@param[in] block Buffer block containing rec
@param[in] rec The record to be removed */
void lock_update_delete(const buf_block_t *block, const rec_t *rec);

/** Stores on the page infimum record the explicit locks of another record.
 This function is used to store the lock state of a record when it is
 updated and the size of the record changes in the update. The record
 is in such an update moved, perhaps to another page. The infimum record
 acts as a dummy carrier record, taking care of lock releases while the
 actual record is being moved. */
void lock_rec_store_on_page_infimum(
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec);        /*!< in: record whose lock state
                              is stored on the infimum
                              record of the same page; lock
                              bits are reset on the
                              record */

/** Restores the state of explicit lock requests on a single record, where the
 state was stored on the infimum of the page.
@param[in] block Buffer block containing rec
@param[in] rec Record whose lock state is restored
@param[in] donator Page (rec is not necessarily on this page) whose infimum
stored the lock state; lock bits are reset on the infimum */
void lock_rec_restore_from_page_infimum(const buf_block_t *block,
                                        const rec_t *rec,
                                        const buf_block_t *donator);

/** Determines if there are explicit record locks on a page.
@param[in]    page_id     space id and page number
@return true iff an explicit record lock on the page exists */
[[nodiscard]] bool lock_rec_expl_exist_on_page(const page_id_t &page_id);
/** Checks if locks of other transactions prevent an immediate insert of
 a record. If they do, first tests if the query thread should anyway
 be suspended for some reason; if not, then puts the transaction and
 the query thread to the lock wait state and inserts a waiting request
 for a gap x-lock to the lock queue.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
[[nodiscard]] dberr_t lock_rec_insert_check_and_lock(
    ulint flags,         /*!< in: if BTR_NO_LOCKING_FLAG bit is
                         set, does nothing */
    const rec_t *rec,    /*!< in: record after which to insert */
    buf_block_t *block,  /*!< in/out: buffer block of rec */
    dict_index_t *index, /*!< in: index */
    que_thr_t *thr,      /*!< in: query thread */
    mtr_t *mtr,          /*!< in/out: mini-transaction */
    bool *inherit);      /*!< out: set to true if the new
                         inserted record maybe should inherit
                         LOCK_GAP type locks from the successor
                         record */

/** Checks if locks of other transactions prevent an immediate modify (update,
 delete mark, or delete unmark) of a clustered index record. If they do,
 first tests if the query thread should anyway be suspended for some
 reason; if not, then puts the transaction and the query thread to the
 lock wait state and inserts a waiting request for a record x-lock to the
 lock queue.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
[[nodiscard]] dberr_t lock_clust_rec_modify_check_and_lock(
    ulint flags,              /*!< in: if BTR_NO_LOCKING_FLAG
                              bit is set, does nothing */
    const buf_block_t *block, /*!< in: buffer block of rec */
    const rec_t *rec,         /*!< in: record which should be
                              modified */
    dict_index_t *index,      /*!< in: clustered index */
    const ulint *offsets,     /*!< in: rec_get_offsets(rec, index) */
    que_thr_t *thr);          /*!< in: query thread */
/** Checks if locks of other transactions prevent an immediate modify
 (delete mark or delete unmark) of a secondary index record.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
[[nodiscard]] dberr_t lock_sec_rec_modify_check_and_lock(
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
    mtr_t *mtr);         /*!< in/out: mini-transaction */

/** Called to inform lock-sys that a statement processing for a trx has just
finished.
@param[in]  trx   transaction which has finished processing a statement */
void lock_on_statement_end(trx_t *trx);

/** Used to specify the intended duration of a record lock. */
enum class lock_duration_t {
  /** Keep the lock according to the rules of particular isolation level, in
  particular in case of READ COMMITTED or less restricive modes, do not inherit
  the lock if the record is purged. */
  REGULAR = 0,
  /** Keep the lock around for at least the duration of the current statement,
  in particular make sure it is inherited as gap lock if the record is purged.*/
  AT_LEAST_STATEMENT = 1,
};

/** Like lock_clust_rec_read_check_and_lock(), but reads a
secondary index record.
@param[in]      duration        If equal to AT_LEAST_STATEMENT, then makes sure
                                that the lock will be kept around and inherited
                                for at least the duration of current statement.
                                If equal to REGULAR the life-cycle of the lock
                                will depend on isolation level rules.
@param[in]      block           buffer block of rec
@param[in]      rec             user record or page supremum record which should
                                be read or passed over by a read cursor
@param[in]      index           secondary index
@param[in]      offsets         rec_get_offsets(rec, index)
@param[in]      sel_mode        select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOKCED, or SELECT_NO_WAIT
@param[in]      mode            mode of the lock which the read cursor should
                                set on records: LOCK_S or LOCK_X; the latter is
                                possible in SELECT FOR UPDATE
@param[in]      gap_mode        LOCK_ORDINARY, LOCK_GAP, or LOCK_REC_NOT_GAP
@param[in,out]  thr             query thread
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
DB_SKIP_LOCKED, or DB_LOCK_NOWAIT */
dberr_t lock_sec_rec_read_check_and_lock(lock_duration_t duration,
                                         const buf_block_t *block,
                                         const rec_t *rec, dict_index_t *index,
                                         const ulint *offsets,
                                         select_mode sel_mode, lock_mode mode,
                                         ulint gap_mode, que_thr_t *thr);

/** Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record.
@param[in]      duration        If equal to AT_LEAST_STATEMENT, then makes sure
                                that the lock will be kept around and inherited
                                for at least the duration of current statement.
                                If equal to REGULAR the life-cycle of the lock
                                will depend on isolation level rules.
@param[in]      block           buffer block of rec
@param[in]      rec             user record or page supremum record which should
                                be read or passed over by a read cursor
@param[in]      index           secondary index
@param[in]      offsets         rec_get_offsets(rec, index)
@param[in]      sel_mode        select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOKCED, or SELECT_NO_WAIT
@param[in]      mode            mode of the lock which the read cursor should
                                set on records: LOCK_S or LOCK_X; the latter is
                                possible in SELECT FOR UPDATE
@param[in]      gap_mode        LOCK_ORDINARY, LOCK_GAP, or LOCK_REC_NOT_GAP
@param[in,out]  thr             query thread
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
DB_SKIP_LOCKED, or DB_LOCK_NOWAIT */
dberr_t lock_clust_rec_read_check_and_lock(
    lock_duration_t duration, const buf_block_t *block, const rec_t *rec,
    dict_index_t *index, const ulint *offsets, select_mode sel_mode,
    lock_mode mode, ulint gap_mode, que_thr_t *thr);

/** Checks if locks of other transactions prevent an immediate read, or passing
 over by a read cursor, of a clustered index record. If they do, first tests
 if the query thread should anyway be suspended for some reason; if not, then
 puts the transaction and the query thread to the lock wait state and inserts a
 waiting request for a record lock to the lock queue. Sets the requested mode
 lock on the record. This is an alternative version of
 lock_clust_rec_read_check_and_lock() that does not require the parameter
 "offsets".
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
[[nodiscard]] dberr_t lock_clust_rec_read_check_and_lock_alt(
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
    que_thr_t *thr);          /*!< in: query thread */
/** Checks that a record is seen in a consistent read.
 @return true if sees, or false if an earlier version of the record
 should be retrieved */
bool lock_clust_rec_cons_read_sees(
    const rec_t *rec,     /*!< in: user record which should be read or
                          passed over by a read cursor */
    dict_index_t *index,  /*!< in: clustered index */
    const ulint *offsets, /*!< in: rec_get_offsets(rec, index) */
    ReadView *view);      /*!< in: consistent read view */
/** Checks that a non-clustered index record is seen in a consistent read.

 NOTE that a non-clustered index page contains so little information on
 its modifications that also in the case false, the present version of
 rec may be the right, but we must check this from the clustered index
 record.

 @return true if certainly sees, or false if an earlier version of the
 clustered index record might be needed */
[[nodiscard]] bool lock_sec_rec_cons_read_sees(
    const rec_t *rec,          /*!< in: user record which
                               should be read or passed over
                               by a read cursor */
    const dict_index_t *index, /*!< in: index */
    const ReadView *view);     /*!< in: consistent read view */
/** Locks the specified database table in the mode given. If the lock cannot
 be granted immediately, the query thread is put to wait.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
[[nodiscard]] dberr_t lock_table(
    ulint flags,         /*!< in: if BTR_NO_LOCKING_FLAG bit is set,
            does nothing */
    dict_table_t *table, /*!< in/out: database table
                         in dictionary cache */
    lock_mode mode,      /*!< in: lock mode */
    que_thr_t *thr);     /*!< in: query thread */

/** Creates a table IX lock object for a resurrected transaction.
@param[in,out] table Table
@param[in,out] trx Transaction */
void lock_table_ix_resurrect(dict_table_t *table, trx_t *trx);

/** Sets a lock on a table based on the given mode.
@param[in]      table   table to lock
@param[in,out]  trx     transaction
@param[in]      mode    LOCK_X or LOCK_S
@return error code or DB_SUCCESS. */
[[nodiscard]] dberr_t lock_table_for_trx(dict_table_t *table, trx_t *trx,
                                         enum lock_mode mode)
    MY_ATTRIBUTE((nonnull));

/** Removes a granted record lock of a transaction from the queue and grants
 locks to other transactions waiting in the queue if they now are entitled
 to a lock. */
void lock_rec_unlock(
    trx_t *trx,               /*!< in/out: transaction that has
                              set a record lock */
    const buf_block_t *block, /*!< in: buffer block containing rec */
    const rec_t *rec,         /*!< in: record */
    lock_mode lock_mode);     /*!< in: LOCK_S or LOCK_X */
/** Releases a transaction's locks, and releases possible other transactions
 waiting because of these locks. Change the state of the transaction to
 TRX_STATE_COMMITTED_IN_MEMORY. */
void lock_trx_release_locks(trx_t *trx); /*!< in/out: transaction */

/** Release read locks of a transaction. It is called during XA
prepare to release locks early.
@param[in,out]  trx             transaction
@param[in]      only_gap        release only GAP locks */
void lock_trx_release_read_locks(trx_t *trx, bool only_gap);

/** Iterate over the granted locks which conflict with trx->lock.wait_lock and
prepare the hit list for ASYNC Rollback.

If the transaction is waiting for some other lock then wake up
with deadlock error.  Currently we don't mark following transactions
for ASYNC Rollback.

1. Read only transactions
2. Background transactions
3. Other High priority transactions
@param[in]      trx       High Priority transaction
@param[in,out]  hit_list  List of transactions which need to be rolled back */
void lock_make_trx_hit_list(trx_t *trx, hit_list_t &hit_list);

/** Removes locks on a table to be dropped.
 If remove_also_table_sx_locks is true then table-level S and X locks are
 also removed in addition to other table-level and record-level locks.
 No lock, that is going to be removed, is allowed to be a wait lock. */
void lock_remove_all_on_table(
    dict_table_t *table,              /*!< in: table to be dropped
                                      or discarded */
    bool remove_also_table_sx_locks); /*!< in: also removes
                                   table S and X locks */

/** Calculates the hash value of a page file address: used in inserting or
searching for a lock in the hash table or getting global shard index.
@param  page_id    specifies the page
@return hash value */
static inline uint64_t lock_rec_hash_value(const page_id_t &page_id);

/** Get the lock hash table */
static inline hash_table_t *lock_hash_get(ulint mode); /*!< in: lock mode */

/** Looks for a set bit in a record lock bitmap.
Returns ULINT_UNDEFINED, if none found.
@param[in]  lock    A record lock
@return bit index == heap number of the record, or ULINT_UNDEFINED if none
found */
ulint lock_rec_find_set_bit(const lock_t *lock);

/** Looks for the next set bit in the record lock bitmap.
@param[in] lock         record lock with at least one bit set
@param[in] heap_no      current set bit
@return The next bit index  == heap number following heap_no, or ULINT_UNDEFINED
if none found */
ulint lock_rec_find_next_set_bit(const lock_t *lock, ulint heap_no);

/** Checks if a lock request lock1 has to wait for request lock2.
@param[in]  lock1   A waiting lock
@param[in]  lock2   Another lock;
                    NOTE that it is assumed that this has a lock bit set on the
                    same record as in lock1 if the locks are record lock
@return true if lock1 has to wait for lock2 to be removed */
bool lock_has_to_wait(const lock_t *lock1, const lock_t *lock2);

namespace locksys {
/** An object which can be passed to consecutive calls to
rec_lock_has_to_wait(trx, mode, lock, is_supremum, trx_locks_cache) for the same
trx and heap_no (which is implicitly the bit common to all lock objects passed)
which can be used by this function to cache some partial results. */
class Trx_locks_cache {
 private:
  bool m_computed{false};
  bool m_has_s_lock_on_record{false};
#ifdef UNIV_DEBUG
  const trx_t *m_cached_trx{};
  page_id_t m_cached_page_id{0, 0};
  size_t m_cached_heap_no{};
#endif /* UNIV_DEBUG*/
 public:
  /* Checks if trx has a granted lock which is blocking the waiting_lock.
  @param[in]  trx           The trx object for which we want to know if one of
                            its granted locks is one of the locks directly
                            blocking the waiting_lock.
                            It must not change between invocations of this
                            method.
  @param[in]  waiting_lock  A waiting record lock. Multiple calls to this method
                            must query the same heap_no and page_id. Currently
                            only X and X|REC_NOT_GAP are supported.
  @return true iff the trx holds a granted record lock which is one of the
  reasons waiting_lock has to wait.
  */
  bool has_granted_blocker(const trx_t *trx, const lock_t *waiting_lock);
};

/** Checks if a lock request lock1 has to wait for request lock2. It returns the
same result as @see lock_has_to_wait(lock1, lock2), but in case these are record
locks, it might use lock1_cache object to speed up the computation.
If the same lock1_cache is passed to multiple calls of this method, then lock1
also needs to be the same.
@param[in]  lock1         A waiting lock
@param[in]  lock2         Another lock;
                          NOTE that it is assumed that this has a lock bit set
                          on the same record as in lock1 if the locks are record
                          locks.
@param[in]  lock1_cache   An object which can be passed to consecutive calls to
                          this function for the same lock1 which can be used by
                          this function to cache some partial results.
@return true if lock1 has to wait for lock2 to be removed */
bool has_to_wait(const lock_t *lock1, const lock_t *lock2,
                 Trx_locks_cache &lock1_cache);
}  // namespace locksys

/** Reports that a transaction id is insensible, i.e., in the future.
@param[in] trx_id Trx id
@param[in] rec User record
@param[in] index Index
@param[in] offsets Rec_get_offsets(rec, index)
@param[in] next_trx_id value received from trx_sys_get_next_trx_id_or_no() */
void lock_report_trx_id_insanity(trx_id_t trx_id, const rec_t *rec,
                                 const dict_index_t *index,
                                 const ulint *offsets, trx_id_t next_trx_id);

/** Prints info of locks for all transactions.
@param[in]  file   file where to print */
void lock_print_info_summary(FILE *file);

/** Prints transaction lock wait and MVCC state.
@param[in,out]  file    file where to print
@param[in]      trx     transaction */
void lock_trx_print_wait_and_mvcc_state(FILE *file, const trx_t *trx);

/** Prints info of locks for each transaction. This function assumes that the
caller holds the exclusive global latch and more importantly it may release and
reacquire it on behalf of the caller. (This should be fixed in the future).
@param[in,out] file  the file where to print */
void lock_print_info_all_transactions(FILE *file);

/** Return approximate number or record locks (bits set in the bitmap) for
 this transaction. Since delete-marked records may be removed, the
 record count will not be precise.
 The caller must be holding exclusive global lock_sys latch.
 @param[in] trx_lock  transaction locks
 */
[[nodiscard]] ulint lock_number_of_rows_locked(const trx_lock_t *trx_lock);

/** Return the number of table locks for a transaction.
 The caller must be holding trx->mutex.
@param[in]  trx   the transaction for which we want the number of table locks */
[[nodiscard]] ulint lock_number_of_tables_locked(const trx_t *trx);

/** Gets the type of a lock. Non-inline version for using outside of the
 lock module.
 @return LOCK_TABLE or LOCK_REC */
uint32_t lock_get_type(const lock_t *lock); /*!< in: lock */

/** Gets the id of the transaction owning a lock.
@param[in]  lock  A lock of the transaction we are interested in
@return the transaction's id */
trx_id_t lock_get_trx_id(const lock_t *lock);

/** Gets the immutable id of the transaction owning a lock
@param[in]  lock   A lock of the transaction we are interested in
@return the transaction's immutable id */
uint64_t lock_get_trx_immutable_id(const lock_t *lock);

/** Gets the immutable id of this lock.
@param[in]  lock   The lock we are interested in
@return The lock's immutable id */
uint64_t lock_get_immutable_id(const lock_t *lock);

/** Get the performance schema event (thread_id, event_id)
that created the lock.
@param[in]      lock            Lock
@param[out]     thread_id       Thread ID that created the lock
@param[out]     event_id        Event ID that created the lock
*/
void lock_get_psi_event(const lock_t *lock, ulonglong *thread_id,
                        ulonglong *event_id);

/** Get the first lock of a trx lock list.
@param[in]      trx_lock        the trx lock
@return The first lock
*/
const lock_t *lock_get_first_trx_locks(const trx_lock_t *trx_lock);

/** Get the next lock of a trx lock list.
@param[in]      lock    the current lock
@return The next lock
*/
const lock_t *lock_get_next_trx_locks(const lock_t *lock);

/** Gets the mode of a lock in a human readable string.
 The string should not be free()'d or modified.
 @return lock mode */
const char *lock_get_mode_str(const lock_t *lock); /*!< in: lock */

/** Gets the type of a lock in a human readable string.
 The string should not be free()'d or modified.
 @return lock type */
const char *lock_get_type_str(const lock_t *lock); /*!< in: lock */

/** Gets the id of the table on which the lock is.
 @return id of the table */
table_id_t lock_get_table_id(const lock_t *lock); /*!< in: lock */

/** Determine which table a lock is associated with.
@param[in]      lock    the lock
@return name of the table */
const table_name_t &lock_get_table_name(const lock_t *lock);

/** For a record lock, gets the index on which the lock is.
 @return index */
const dict_index_t *lock_rec_get_index(const lock_t *lock); /*!< in: lock */

/** For a record lock, gets the name of the index on which the lock is.
 The string should not be free()'d or modified.
 @return name of the index */
const char *lock_rec_get_index_name(const lock_t *lock); /*!< in: lock */

/** For a record lock, gets the tablespace number and page number on which the
lock is.
 @return tablespace number */
page_id_t lock_rec_get_page_id(const lock_t *lock); /*!< in: lock */

/** Check if there are any locks (table or rec) against table.
Returned value might be obsolete.
@param[in]  table   the table
@return true if there were any locks held on records in this table or on the
table itself at some point in time during the call */
bool lock_table_has_locks(const dict_table_t *table);

/** A thread which wakes up threads whose lock wait may have lasted too long. */
void lock_wait_timeout_thread();

/** Notifies the thread which analyzes wait-for-graph that there was
 at least one new edge added or modified ( trx->blocking_trx has changed ),
 so that the thread will know it has to analyze it. */
void lock_wait_request_check_for_cycles();

/** Puts a user OS thread to wait for a lock to be released. If an error
 occurs during the wait trx->error_state associated with thr is != DB_SUCCESS
 when we return. DB_INTERRUPTED, DB_LOCK_WAIT_TIMEOUT and DB_DEADLOCK
 are possible errors. DB_DEADLOCK is returned if selective deadlock
 resolution chose this transaction as a victim. */
void lock_wait_suspend_thread(que_thr_t *thr); /*!< in: query thread associated
                                               with the user OS thread */
/** Unlocks AUTO_INC type locks that were possibly reserved by a trx. This
 function should be called at the the end of an SQL statement, by the
 connection thread that owns the transaction (trx->mysql_thd). */
void lock_unlock_table_autoinc(trx_t *trx); /*!< in/out: transaction */

/** Cancels the waiting lock request of the trx, if any.
If the transaction has already committed (trx->version has changed) or is no
longer waiting for a lock (trx->lock.blocking_trx is nullptr) this function
will not cancel the waiting lock.

@note There is a possibility of ABA in which a waiting lock request was already
granted or canceled and then the trx requested another lock and started waiting
for it - in such case this function might either cancel or not the second
request depending on timing. Currently all usages of this function ensure that
this is impossible:
- innodb_kill_connection ensures trx_is_interrupted(trx), thus upon first wake
up it will realize it has to report an error and rollback
- HP transaction marks the trx->in_innodb & TRX_FORCE_ROLLBACK flag which is
checked when the trx attempts RecLock::add_to_waitq and reports DB_DEADLOCK

@param[in]      trx_version   The trx we want to wake up and its expected
                              version
@return true iff the function did release a waiting lock
*/
bool lock_cancel_if_waiting_and_release(TrxVersion trx_version);

/** Set the lock system timeout event. */
void lock_set_timeout_event();

/** Checks that a transaction id is sensible, i.e., not in the future.
Emits an error otherwise.
@param[in]  trx_id   The trx id to check, found in user record or secondary
                     index page header
@param[in]  rec      The user record which contained the trx_id in its header
                     or in header of its page
@param[in]  index    The index which contained the rec
@param[in]  offsets  The result of rec_get_offsets(rec, index)
@return true iff ok */
bool lock_check_trx_id_sanity(trx_id_t trx_id, const rec_t *rec,
                              const dict_index_t *index, const ulint *offsets);

#ifdef UNIV_DEBUG
/** Check if the transaction holds an exclusive lock on a record.
@param[in]  thr     query thread of the transaction
@param[in]  table   table to check
@param[in]  block   buffer block of the record
@param[in]  heap_no record heap number
@return whether the locks are held */
[[nodiscard]] bool lock_trx_has_rec_x_lock(que_thr_t *thr,
                                           const dict_table_t *table,
                                           const buf_block_t *block,
                                           ulint heap_no);

/** Validates the lock system.
 @return true if ok */
bool lock_validate();
#endif /* UNIV_DEBUG */

/**
Allocate cached locks for the transaction.
@param trx              allocate cached record locks for this transaction */
void lock_trx_alloc_locks(trx_t *trx);

/** Lock modes and types */
/** @{ */
/** mask used to extract mode from the  type_mode field in a lock */
constexpr uint32_t LOCK_MODE_MASK = 0xF;
/** Lock types */
/** table lock */
constexpr uint32_t LOCK_TABLE = 16;
/** record lock */
constexpr uint32_t LOCK_REC = 32;
/** mask used to extract lock type from the type_mode field in a lock */
constexpr uint32_t LOCK_TYPE_MASK = 0xF0UL;
static_assert((LOCK_MODE_MASK & LOCK_TYPE_MASK) == 0,
              "LOCK_MODE_MASK & LOCK_TYPE_MASK");

/** Waiting lock flag; when set, it  means that the lock has not yet been
 granted, it is just waiting for its  turn in the wait queue */
constexpr uint32_t LOCK_WAIT = 256;
/* Precise modes */
/** this flag denotes an ordinary next-key lock in contrast to LOCK_GAP or
 LOCK_REC_NOT_GAP */
constexpr uint32_t LOCK_ORDINARY = 0;
/** when this bit is set, it means that the lock holds only on the gap before
  the record; for instance, an x-lock on the gap does not give permission to
  modify the record on which the bit is set; locks of this type are created
  when records are removed from the index chain of records */
constexpr uint32_t LOCK_GAP = 512;
/** this bit means that the lock is only on the index record and does NOT
   block inserts to the gap before the index record; this is used in the case
   when we retrieve a record with a unique key, and is also used in locking
   plain SELECTs (not part of UPDATE or DELETE) when the user has set the READ
   COMMITTED isolation level */
constexpr uint32_t LOCK_REC_NOT_GAP = 1024;
/** this bit is set when we place a waiting gap type record lock request in
   order to let an insert of an index record to wait until there are no
   conflicting locks by other transactions on the gap; note that this flag
   remains set when the waiting lock is granted, or if the lock is inherited to
   a neighboring record */
constexpr uint32_t LOCK_INSERT_INTENTION = 2048;
/** Predicate lock */
constexpr uint32_t LOCK_PREDICATE = 8192;
/** Page lock */
constexpr uint32_t LOCK_PRDT_PAGE = 16384;

static_assert(
    ((LOCK_WAIT | LOCK_GAP | LOCK_REC_NOT_GAP | LOCK_INSERT_INTENTION |
      LOCK_PREDICATE | LOCK_PRDT_PAGE) &
     LOCK_MODE_MASK) == 0,
    "(LOCK_WAIT | LOCK_GAP | LOCK_REC_NOT_GAP | LOCK_INSERT_INTENTION | "
    "LOCK_PREDICATE | LOCK_PRDT_PAGE) & LOCK_TYPE_MASK");
/** @} */

/** Lock operation struct */
struct lock_op_t {
  dict_table_t *table; /*!< table to be locked */
  lock_mode mode;      /*!< lock mode */
};

typedef ib_mutex_t Lock_mutex;

/** The lock system struct */
struct lock_sys_t {
  /** The latches protecting queues of record and table locks */
  locksys::Latches latches;

  /** The hash table of the record (LOCK_REC) locks, except for predicate
  (LOCK_PREDICATE) and predicate page (LOCK_PRDT_PAGE) locks */
  hash_table_t *rec_hash;

  /** The hash table of predicate (LOCK_PREDICATE) locks */
  hash_table_t *prdt_hash;

  /** The hash table of the predicate page (LOCK_PRD_PAGE) locks */
  hash_table_t *prdt_page_hash;

  /** Padding to avoid false sharing of wait_mutex field */
  char pad2[ut::INNODB_CACHE_LINE_SIZE];

  /** The mutex protecting the next two fields */
  Lock_mutex wait_mutex;

  /** Array of user threads suspended while waiting for locks within InnoDB.
  Protected by the lock_sys->wait_mutex. */
  srv_slot_t *waiting_threads;

  /** The highest slot ever used in the waiting_threads array.
  Protected by lock_sys->wait_mutex. */
  srv_slot_t *last_slot;

  /** true if rollback of all recovered transactions is complete.
  Protected by exclusive global lock_sys latch. */
  bool rollback_complete;

  /** Max lock wait time observed, for innodb_row_lock_time_max reporting. */
  std::chrono::steady_clock::duration n_lock_max_wait_time;

  /** Set to the event that is created in the lock wait monitor thread. A value
  of 0 means the thread is not active */
  os_event_t timeout_event;

#ifdef UNIV_DEBUG
  /** Lock timestamp counter, used to assign lock->m_seq on creation. */
  std::atomic<uint64_t> m_seq;
#endif /* UNIV_DEBUG */
};

/** If a transaction has an implicit x-lock on a record, but no explicit x-lock
set on the record, sets one for it.
@param[in]  block     buffer block of rec
@param[in]  rec       user record on page
@param[in]  index     index of record
@param[in]  offsets   rec_get_offsets(rec, index) */
void lock_rec_convert_impl_to_expl(const buf_block_t *block, const rec_t *rec,
                                   dict_index_t *index, const ulint *offsets);

/** Removes a record lock request, waiting or granted, from the queue. */
void lock_rec_discard(lock_t *in_lock); /*!< in: record lock object: all
                                        record locks which are contained
                                        in this lock object are removed */

/** Moves the explicit locks on user records to another page if a record
 list start is moved to another page.
@param[in] new_block Index page to move to
@param[in] block Index page
@param[in] rec_move Recording records moved
@param[in] num_move Num of rec to move */
void lock_rtr_move_rec_list(const buf_block_t *new_block,
                            const buf_block_t *block, rtr_rec_move_t *rec_move,
                            ulint num_move);

/** Removes record lock objects set on an index page which is discarded. This
 function does not move locks, or check for waiting locks, therefore the
 lock bitmaps must already be reset when this function is called. */
void lock_rec_free_all_from_discard_page(
    const buf_block_t *block); /*!< in: page to be discarded */

/** Reset the nth bit of a record lock.
@param[in,out]  lock record lock
@param[in] i    index of the bit that will be reset
@param[in] type whether the lock is in wait mode */
void lock_rec_trx_wait(lock_t *lock, ulint i, ulint type);

/** The lock system */
extern lock_sys_t *lock_sys;

#ifdef UNIV_DEBUG
/** Test if lock_sys->wait_mutex is owned. */
static inline bool lock_wait_mutex_own() {
  return lock_sys->wait_mutex.is_owned();
}
#endif

/** Acquire the lock_sys->wait_mutex. */
static inline void lock_wait_mutex_enter() {
  mutex_enter(&lock_sys->wait_mutex);
}
/** Release the lock_sys->wait_mutex. */
static inline void lock_wait_mutex_exit() { lock_sys->wait_mutex.exit(); }

#include "lock0lock.ic"

namespace locksys {

/* OWNERSHIP TESTS */
#ifdef UNIV_DEBUG

/**
Tests if lock_sys latch is exclusively owned by the current thread.
@return true iff the current thread owns exclusive global lock_sys latch
*/
bool owns_exclusive_global_latch();

/**
Tests if lock_sys latch is owned in shared mode by the current thread.
@return true iff the current thread owns shared global lock_sys latch
*/
bool owns_shared_global_latch();

/**
Tests if given page shard can be safely accessed by the current thread.
@param  page_id    specifies the page
@return true iff the current thread owns exclusive global lock_sys latch or both
a shared global lock_sys latch and mutex protecting the page shard
*/
bool owns_page_shard(const page_id_t &page_id);

/**
Test if given table shard can be safely accessed by the current thread.
@param  table   the table
@return true iff the current thread owns exclusive global lock_sys latch or
both a shared global lock_sys latch and mutex protecting the table shard
*/
bool owns_table_shard(const dict_table_t &table);

/** Checks if shard which contains lock is latched (or that an exclusive latch
on whole lock_sys is held) by current thread
@param[in]  lock   lock which belongs to a shard we want to check
@return true iff the current thread owns exclusive global lock_sys latch or
both a shared global lock_sys latch and mutex protecting the shard
containing the specified lock */
bool owns_lock_shard(const lock_t *lock);

#endif /* UNIV_DEBUG */

}  // namespace locksys

#include "lock0guards.h"

#endif
