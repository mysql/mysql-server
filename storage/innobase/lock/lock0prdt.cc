/*****************************************************************************

Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

/** @file lock/lock0prdt.cc
 The transaction lock system

 Created 9/7/2013 Jimmy Yang
 *******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include <set>

#include "btr0btr.h"
#include "dict0boot.h"
#include "dict0mem.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "lock0prdt.h"
#include "lock0priv.h"
#include "srv0mon.h"
#include "trx0purge.h"
#include "trx0sys.h"
#include "usr0sess.h"
#include "ut0vec.h"

/** Get a minimum bounding box from a Predicate
 @return        the minimum bounding box */
static inline rtr_mbr_t *prdt_get_mbr_from_prdt(
    const lock_prdt_t *prdt) /*!< in: the lock predicate */
{
  rtr_mbr_t *mbr_loc = reinterpret_cast<rtr_mbr_t *>(prdt->data);

  return (mbr_loc);
}

/** Get a predicate from a lock
 @return        the predicate */
lock_prdt_t *lock_get_prdt_from_lock(const lock_t *lock) /*!< in: the lock */
{
  lock_prdt_t *prdt =
      reinterpret_cast<lock_prdt_t *>(&((reinterpret_cast<byte *>(
          const_cast<lock_t *>(&lock[1])))[UNIV_WORD_SIZE]));

  return (prdt);
}

/** Get a minimum bounding box directly from a lock
 @return        the minimum bounding box*/
static inline rtr_mbr_t *lock_prdt_get_mbr_from_lock(
    const lock_t *lock) /*!< in: the lock */
{
  ut_ad(lock->type_mode & LOCK_PREDICATE);

  lock_prdt_t *prdt = lock_get_prdt_from_lock(lock);

  rtr_mbr_t *mbr_loc = prdt_get_mbr_from_prdt(prdt);

  return (mbr_loc);
}

/** Append a predicate to the lock
@param[in] lock Lock
@param[in] prdt Predicate */
void lock_prdt_set_prdt(lock_t *lock, const lock_prdt_t *prdt) {
  ut_ad(lock->type_mode & LOCK_PREDICATE);

  memcpy(&(((byte *)&lock[1])[UNIV_WORD_SIZE]), prdt, sizeof *prdt);
}

/** Check whether two predicate locks are compatible with each other
@param[in]      prdt1   first predicate lock
@param[in]      prdt2   second predicate lock
@param[in]      op      predicate comparison operator
@param[in]      srs      Spatial reference system of R-tree
@return true if consistent */
static bool lock_prdt_consistent(lock_prdt_t *prdt1, lock_prdt_t *prdt2,
                                 ulint op,
                                 const dd::Spatial_reference_system *srs) {
  bool ret = false;
  rtr_mbr_t *mbr1 = prdt_get_mbr_from_prdt(prdt1);
  rtr_mbr_t *mbr2 = prdt_get_mbr_from_prdt(prdt2);
  ulint action;

  if (op) {
    action = op;
  } else {
    if (prdt2->op != 0 && (prdt1->op != prdt2->op)) {
      return (false);
    }

    action = prdt1->op;
  }

  switch (action) {
    case PAGE_CUR_CONTAIN:
      ret = mbr_contain_cmp(srs, mbr1, mbr2);
      break;
    case PAGE_CUR_DISJOINT:
      ret = mbr_disjoint_cmp(srs, mbr1, mbr2);
      break;
    case PAGE_CUR_MBR_EQUAL:
      ret = mbr_equal_cmp(srs, mbr1, mbr2);
      break;
    case PAGE_CUR_INTERSECT:
      ret = mbr_intersect_cmp(srs, mbr1, mbr2);
      break;
    case PAGE_CUR_WITHIN:
      ret = mbr_within_cmp(srs, mbr1, mbr2);
      break;
    default:
      ib::error(ER_IB_MSG_645) << "invalid operator " << action;
      ut_error;
  }

  return (ret);
}

/** Checks if a predicate lock request for a new lock has to wait for
 another lock.
 @return        true if new lock has to wait for lock2 to be released */
bool lock_prdt_has_to_wait(
    const trx_t *trx,    /*!< in: trx of new lock */
    ulint type_mode,     /*!< in: precise mode of the new lock
                       to set: LOCK_S or LOCK_X, possibly
                       ORed to LOCK_PREDICATE or LOCK_PRDT_PAGE,
                       LOCK_INSERT_INTENTION */
    lock_prdt_t *prdt,   /*!< in: lock predicate to check */
    const lock_t *lock2) /*!< in: another record lock; NOTE that
                         it is assumed that this has a lock bit
                         set on the same record as in the new
                         lock we are setting */
{
  ut_ad(trx && lock2);
  ut_ad((lock2->type_mode & LOCK_PREDICATE && type_mode & LOCK_PREDICATE) ||
        (lock2->type_mode & LOCK_PRDT_PAGE && type_mode & LOCK_PRDT_PAGE));

  ut_ad(type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));

  if (trx != lock2->trx &&
      !lock_mode_compatible(static_cast<lock_mode>(LOCK_MODE_MASK & type_mode),
                            lock_get_mode(lock2))) {
    const bool is_hp = trx_is_high_priority(trx);
    /* If our trx is High Priority and the existing lock is WAITING and not
        high priority, then we can ignore it. */
    if (is_hp && lock2->is_waiting() && !trx_is_high_priority(lock2->trx)) {
      return (false);
    }

    /* If it is a page lock, then return true (conflict) */
    if (type_mode & LOCK_PRDT_PAGE) {
      ut_ad(lock2->type_mode & LOCK_PRDT_PAGE);

      return (true);
    }

    ut_ad(lock2->type_mode & LOCK_PREDICATE);

    if (!(type_mode & LOCK_INSERT_INTENTION)) {
      /* PREDICATE locks without LOCK_INSERT_INTENTION flag
      do not need to wait for anything. This is because
      different users can have conflicting lock types
      on predicates. */

      return false;
    }

    if (lock2->type_mode & LOCK_INSERT_INTENTION) {
      /* No lock request needs to wait for an insert
      intention lock to be removed. This makes it similar
      to GAP lock, that allows conflicting insert intention
      locks */
      return false;
    }
    lock_prdt_t *cur_prdt = lock_get_prdt_from_lock(lock2);

    if (!lock_prdt_consistent(cur_prdt, prdt, 0, lock2->index->rtr_srs.get())) {
      return (false);
    }

    return true;
  }

  return false;
}

/** Checks if a transaction has a GRANTED stronger or equal predicate lock
 on the page
 @return        lock or NULL */
static inline lock_t *lock_prdt_has_lock(
    ulint precise_mode,       /*!< in: LOCK_S or LOCK_X */
    ulint type_mode,          /*!< in: LOCK_PREDICATE etc. */
    const buf_block_t *block, /*!< in: buffer block
                              containing the record */
    lock_prdt_t *prdt,        /*!< in: The predicate to be
                              attached to the new lock */
    const trx_t *trx)         /*!< in: transaction */
{
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S ||
        (precise_mode & LOCK_MODE_MASK) == LOCK_X);
  ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));
  return lock_hash_get(type_mode).find_on_record(
      RecID{block->get_page_id(), PRDT_HEAPNO}, [&](lock_t *lock) {
        ut_ad(lock->type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));

        if (lock->trx == trx && !(lock->type_mode & LOCK_INSERT_INTENTION) &&
            !lock_get_wait(lock) &&
            lock_mode_stronger_or_eq(
                lock_get_mode(lock),
                static_cast<lock_mode>(precise_mode & LOCK_MODE_MASK))) {
          if (lock->type_mode & LOCK_PRDT_PAGE) {
            return true;
          }

          ut_ad(lock->type_mode & LOCK_PREDICATE);
          lock_prdt_t *cur_prdt = lock_get_prdt_from_lock(lock);

          /* if the lock predicate operator is the same as the one to look, and
          prdicate test is successful, then we find a lock */
          if (cur_prdt->op == prdt->op &&
              lock_prdt_consistent(cur_prdt, prdt, 0,
                                   lock->index->rtr_srs.get())) {
            return true;
          }
        }
        return false;
      });
}

/** Checks if some other transaction has a conflicting predicate
 lock request in the queue, so that we have to wait.
 @return        lock or NULL */
static const lock_t *lock_prdt_other_has_conflicting(
    ulint mode,               /*!< in: LOCK_S or LOCK_X,
                              possibly ORed to LOCK_PREDICATE or
                              LOCK_PRDT_PAGE, LOCK_INSERT_INTENTION */
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    lock_prdt_t *prdt,        /*!< in: Predicates (currently)
                             the Minimum Bounding Rectangle)
                             the new lock will be on */
    const trx_t *trx)         /*!< in: our transaction */
{
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  return lock_hash_get(mode).find_on_record(
      RecID{block, PRDT_HEAPNO}, [&](lock_t *lock) {
        return lock_prdt_has_to_wait(trx, mode, prdt, lock);
      });
}

/** Reset the Minimum Bounding Rectangle (to a large area) */
static void lock_prdt_enlarge_mbr(
    const lock_t *lock, /*!< in/out: lock to modify */
    rtr_mbr_t *mbr)     /*!< in: Minimum Bounding Rectangle */
{
  rtr_mbr_t *cur_mbr = lock_prdt_get_mbr_from_lock(lock);

  if (cur_mbr->xmin > mbr->xmin) {
    cur_mbr->xmin = mbr->xmin;
  }

  if (cur_mbr->ymin > mbr->ymin) {
    cur_mbr->ymin = mbr->ymin;
  }

  if (cur_mbr->xmax < mbr->xmax) {
    cur_mbr->xmax = mbr->xmax;
  }

  if (cur_mbr->ymax < mbr->ymax) {
    cur_mbr->ymax = mbr->ymax;
  }
}

/** Reset the predicates to a "covering" (larger) predicates */
static void lock_prdt_enlarge_prdt(lock_t *lock, /*!< in/out: lock to modify */
                                   lock_prdt_t *prdt) /*!< in: predicate */
{
  rtr_mbr_t *mbr = prdt_get_mbr_from_prdt(prdt);

  lock_prdt_enlarge_mbr(lock, mbr);
}

/** Check two predicates' MBRs are the same
 @return        true if they are the same */
static bool lock_prdt_is_same(
    lock_prdt_t *prdt1,                      /*!< in: MBR with the lock */
    lock_prdt_t *prdt2,                      /*!< in: MBR with the lock */
    const dd::Spatial_reference_system *srs) /*!< in: SRS of R-tree */
{
  rtr_mbr_t *mbr1 = prdt_get_mbr_from_prdt(prdt1);
  rtr_mbr_t *mbr2 = prdt_get_mbr_from_prdt(prdt2);

  if (prdt1->op == prdt2->op && mbr_equal_cmp(srs, mbr1, mbr2)) {
    return (true);
  }

  return (false);
}

/** Looks for a similar predicate lock struct by the same trx on the same page.
 This can be used to save space when a new record lock should be set on a page:
 no new struct is needed, if a suitable old one is found.
 @return        lock or NULL */
static lock_t *lock_prdt_find_on_page(
    ulint type_mode,          /*!< in: lock type_mode field */
    const buf_block_t *block, /*!< in: buffer block */
    lock_prdt_t *prdt,        /*!< in: MBR with the lock */
    const trx_t *trx)         /*!< in: transaction */
{
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  return lock_hash_get(type_mode).find_on_block(block, [&](lock_t *lock) {
    if (lock->trx == trx && lock->type_mode == type_mode) {
      if (lock->type_mode & LOCK_PRDT_PAGE) {
        return true;
      }

      ut_ad(lock->type_mode & LOCK_PREDICATE);

      return lock_prdt_is_same(lock_get_prdt_from_lock(lock), prdt,
                               lock->index->rtr_srs.get());
    }
    return false;
  });
}

/** Adds a predicate lock request in the predicate lock queue.
 @return        lock where the bit was set */
static lock_t *lock_prdt_add_to_queue(
    ulint type_mode,          /*!< in: lock mode, wait, predicate
                            etc. flags; type is ignored
                            and replaced by LOCK_REC */
    const buf_block_t *block, /*!< in: buffer block containing
                              the record */
    dict_index_t *index,      /*!< in: index of record */
    trx_t *trx,               /*!< in/out: transaction */
    lock_prdt_t *prdt)        /*!< in: Minimum Bounding Rectangle
                              the new lock will be on */
{
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  ut_ad(!index->is_clustered() && !dict_index_is_online_ddl(index));
  ut_ad(type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));
  ut_ad(!trx_mutex_own(trx));

#ifdef UNIV_DEBUG
  switch (type_mode & LOCK_MODE_MASK) {
    case LOCK_X:
    case LOCK_S:
      break;
    default:
      ut_error;
  }
#endif /* UNIV_DEBUG */

  type_mode |= LOCK_REC;

  if (!(type_mode & LOCK_WAIT)) {
    /* Look for a similar record lock on the same page:
    if one is found we can just set the bit */

    lock_t *lock = lock_prdt_find_on_page(type_mode, block, prdt, trx);

    if (lock != nullptr) {
      if (lock->type_mode & LOCK_PREDICATE) {
        lock_prdt_enlarge_prdt(lock, prdt);
      }

      return (lock);
    }
  }

  RecLock rec_lock(index, block, PRDT_HEAPNO, type_mode);

  trx_mutex_enter(trx);
  auto *created_lock = (rec_lock.create(trx, prdt));
  trx_mutex_exit(trx);

  return (created_lock);
}

/** Checks if locks of other transactions prevent an immediate insert of
 a predicate record.
 @return        DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK
 */
dberr_t lock_prdt_insert_check_and_lock(
    ulint flags,         /*!< in: if BTR_NO_LOCKING_FLAG bit is
                         set, does nothing */
    const rec_t *rec,    /*!< in: record after which to insert */
    buf_block_t *block,  /*!< in/out: buffer block of rec */
    dict_index_t *index, /*!< in: index */
    que_thr_t *thr,      /*!< in: query thread */
    mtr_t *mtr,          /*!< in/out: mini-transaction */
    lock_prdt_t *prdt)   /*!< in: Predicates with Minimum Bound
                         Rectangle */
{
  ut_ad(block->frame == page_align(rec));

  if (flags & BTR_NO_LOCKING_FLAG) {
    return (DB_SUCCESS);
  }

  ut_ad(!index->table->is_temporary());
  ut_ad(!index->is_clustered());

  trx_t *trx = thr_get_trx(thr);

  dberr_t err = DB_SUCCESS;
  {
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

    /* Because this code is invoked for a running transaction by
    the thread that is serving the transaction, it is not necessary
    to hold trx->mutex here. */

    ut_ad(lock_table_has(trx, index->table, LOCK_IX));

    /* Only need to check locks on prdt_hash */

    /* If another transaction has an explicit lock request which locks
    the predicate, waiting or granted, the insert has to wait.

    Similar to GAP lock, we do not consider lock from inserts conflicts
    with each other */

    const ulint mode = LOCK_X | LOCK_PREDICATE | LOCK_INSERT_INTENTION;

    const lock_t *wait_for =
        lock_prdt_other_has_conflicting(mode, block, prdt, trx);

    if (wait_for != nullptr) {
      rtr_mbr_t *mbr = prdt_get_mbr_from_prdt(prdt);

      trx_mutex_enter(trx);

      /* Allocate MBR on the lock heap */
      lock_init_prdt_from_mbr(prdt, mbr, 0, trx->lock.lock_heap);

      RecLock rec_lock(thr, index, block, PRDT_HEAPNO, mode);

      /* Note that we may get DB_SUCCESS also here! */

      err = rec_lock.add_to_waitq(wait_for, prdt);

      trx_mutex_exit(trx);
    }

  }  // release block latch

  switch (err) {
    case DB_SUCCESS_LOCKED_REC:
      err = DB_SUCCESS;
      [[fallthrough]];
    case DB_SUCCESS:
      /* Update the page max trx id field */
      page_update_max_trx_id(block, buf_block_get_page_zip(block), trx->id,
                             mtr);
    default:
      /* We only care about the two return values. */
      break;
  }
  ut_ad(err == DB_SUCCESS || err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

void lock_prdt_update_parent(buf_block_t *left_block, buf_block_t *right_block,
                             lock_prdt_t *left_prdt, lock_prdt_t *right_prdt,
                             const page_id_t &page_id) {
  /* We will operate on three blocks (left, right, parent). Latching their
  shards without deadlock is easiest using exclusive global latch. */
  locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};

  /* Get all locks in parent */
  lock_sys->prdt_hash.find_on_page(page_id, [&](lock_t *lock) {
    lock_prdt_t *lock_prdt;
    ulint op = PAGE_CUR_DISJOINT;

    ut_ad(lock->type_mode & LOCK_PREDICATE);

    if ((lock->type_mode & LOCK_MODE_MASK) == LOCK_X) {
      return false;
    }

    lock_prdt = lock_get_prdt_from_lock(lock);

    /* Check each lock in parent to see if it intersects with
    left or right child */
    if (!lock_prdt_consistent(lock_prdt, left_prdt, op,
                              lock->index->rtr_srs.get()) &&
        !lock_prdt_find_on_page(lock->type_mode, left_block, lock_prdt,
                                lock->trx)) {
      lock_prdt_add_to_queue(lock->type_mode, left_block, lock->index,
                             lock->trx, lock_prdt);
    }

    if (!lock_prdt_consistent(lock_prdt, right_prdt, op,
                              lock->index->rtr_srs.get()) &&
        !lock_prdt_find_on_page(lock->type_mode, right_block, lock_prdt,
                                lock->trx)) {
      lock_prdt_add_to_queue(lock->type_mode, right_block, lock->index,
                             lock->trx, lock_prdt);
    }
    return false;
  });
}

/** Update predicate lock when page splits
@param[in,out]  block       page to be split
@param[in,out]  new_block   the new half page
@param[in]      prdt        MBR on the old page
@param[in]      new_prdt    MBR on the new page
@param[in]      type_mode   LOCK_PREDICATE or LOCK_PRDT_PAGE
*/
static void lock_prdt_update_split_low(buf_block_t *block,
                                       buf_block_t *new_block,
                                       lock_prdt_t *prdt, lock_prdt_t *new_prdt,
                                       ulint type_mode) {
  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *block, *new_block};
  lock_hash_get(type_mode).find_on_block(block, [&](lock_t *lock) {
    /* First dealing with Page Lock */
    if (lock->type_mode & LOCK_PRDT_PAGE) {
      /* Duplicate the lock to new page */

      lock_prdt_add_to_queue(lock->type_mode, new_block, lock->index, lock->trx,
                             nullptr);

      return false;
    }

    /* Now dealing with Predicate Lock */
    lock_prdt_t *lock_prdt;
    ulint op = PAGE_CUR_DISJOINT;

    ut_ad(lock->type_mode & LOCK_PREDICATE);

    /* No need to duplicate waiting X locks */
    if ((lock->type_mode & LOCK_MODE_MASK) == LOCK_X) {
      return false;
    }

    lock_prdt = lock_get_prdt_from_lock(lock);

    if (lock_prdt_consistent(lock_prdt, prdt, op, lock->index->rtr_srs.get())) {
      if (!lock_prdt_consistent(lock_prdt, new_prdt, op,
                                lock->index->rtr_srs.get())) {
        /* Move the lock to new page */

        lock_prdt_add_to_queue(lock->type_mode, new_block, lock->index,
                               lock->trx, lock_prdt);
      }
    } else if (!lock_prdt_consistent(lock_prdt, new_prdt, op,
                                     lock->index->rtr_srs.get())) {
      /* Duplicate the lock to new page */

      lock_prdt_add_to_queue(lock->type_mode, new_block, lock->index, lock->trx,
                             lock_prdt);
    }
    return false;
  });
}

void lock_prdt_update_split(buf_block_t *block, buf_block_t *new_block,
                            lock_prdt_t *prdt, lock_prdt_t *new_prdt)

{
  lock_prdt_update_split_low(block, new_block, prdt, new_prdt, LOCK_PREDICATE);

  lock_prdt_update_split_low(block, new_block, nullptr, nullptr,
                             LOCK_PRDT_PAGE);
}

/** Initiate a Predicate Lock from a MBR */
void lock_init_prdt_from_mbr(
    lock_prdt_t *prdt, /*!< in/out: predicate to initialized */
    rtr_mbr_t *mbr,    /*!< in: Minimum Bounding Rectangle */
    ulint mode,        /*!< in: Search mode */
    mem_heap_t *heap)  /*!< in: heap for allocating memory */
{
  memset(prdt, 0, sizeof(*prdt));

  if (heap != nullptr) {
    prdt->data = mem_heap_alloc(heap, sizeof(*mbr));
    ut_memcpy(prdt->data, mbr, sizeof(*mbr));
  } else {
    prdt->data = static_cast<void *>(mbr);
  }

  prdt->op = static_cast<uint16>(mode);
}

void lock_prdt_lock(buf_block_t *block, lock_prdt_t *prdt, dict_index_t *index,
                    que_thr_t *thr) {
  trx_t *trx = thr_get_trx(thr);

  if (trx->read_only || index->table->is_temporary()) {
    return;
  }

  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(index));

  /* Another transaction cannot have an implicit lock on the record,
  because when we come here, we already have modified the clustered
  index record, and this would not have been possible if another active
  transaction had modified this secondary index record. */

  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, block->get_page_id()};

  const ulint prdt_mode = LOCK_S | LOCK_PREDICATE;

  lock_t *lock = nullptr;
  lock_t *other_lock =
      lock_sys->prdt_hash.find_on_block(block, [&](lock_t *seen) {
        if (lock != nullptr) {
          return true;
        }
        lock = seen;
        return false;
      });

  if (lock == nullptr) {
    RecLock rec_lock(index, block, PRDT_HEAPNO, prdt_mode);

    trx_mutex_enter(trx);
    lock = rec_lock.create(trx, prdt);
    trx_mutex_exit(trx);

  } else {
    if (other_lock || lock->trx != trx ||
        lock->type_mode != (LOCK_REC | prdt_mode) ||
        lock_rec_get_n_bits(lock) == 0 ||
        ((!lock_prdt_consistent(lock_get_prdt_from_lock(lock), prdt, 0,
                                lock->index->rtr_srs.get())))) {
      if (!lock_prdt_has_lock(LOCK_S, LOCK_PREDICATE, block, prdt, trx)) {
        lock_prdt_add_to_queue(prdt_mode, block, index, trx, prdt);
      }

    } else {
      if (!lock_rec_get_nth_bit(lock, PRDT_HEAPNO)) {
        lock_rec_set_nth_bit(lock, PRDT_HEAPNO);
        lock_prdt_set_prdt(lock, prdt);
      }
    }
  }
}

dberr_t lock_place_prdt_page_lock(const page_id_t &page_id, dict_index_t *index,
                                  que_thr_t *thr) {
  ut_ad(thr != nullptr);
  ut_ad(!srv_read_only_mode);

  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(index));

  /* Another transaction cannot have an implicit lock on the record,
  because when we come here, we already have modified the clustered
  index record, and this would not have been possible if another active
  transaction had modified this secondary index record. */

  RecID rec_id(page_id, PRDT_HEAPNO);
  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, page_id};

  const ulint mode = LOCK_S | LOCK_PRDT_PAGE;
  trx_t *trx = thr_get_trx(thr);

  const lock_t *lock = lock_sys->prdt_page_hash.find_on_page(
      page_id, [&](lock_t *lock) { return lock->trx == trx; });

  if (lock == nullptr) {
    RecLock rec_lock(index, rec_id, mode);

    trx_mutex_enter(trx);
    rec_lock.create(trx);
    trx_mutex_exit(trx);

#ifdef PRDT_DIAG
    printf("GIS_DIAGNOSTIC: page lock %d\n", (int)page_no);
#endif /* PRDT_DIAG */
  } else {
    /* LOCK_PRDT_PAGE do not have a predicate, but have 1 byte (zeroed) bitmap,
    and they always use S mode. They purpose is not so much to conflict with
    each other (they are all S), rather indicate the page is still needed. */
    ut_ad(lock->type_mode == (mode | LOCK_REC));
    ut_ad(lock_rec_get_n_bits(lock) != 0);
  }

  return (DB_SUCCESS);
}

bool lock_other_has_prdt_page_lock(const trx_t *trx, const page_id_t &page_id) {
  locksys::Shard_latch_guard guard{UT_LOCATION_HERE, page_id};
  /* Make sure that the only locks on this page (if any) are ours. */
  return lock_sys->prdt_page_hash.find_on_page(
      page_id, [&](lock_t *lock) { return lock->trx != trx; });
}

/** Moves the locks of a page to another page and resets the lock bits of
 the donating records. */
void lock_prdt_rec_move(
    const buf_block_t *receiver, /*!< in: buffer block containing
                                 the receiving record */
    const buf_block_t *donator)  /*!< in: buffer block containing
                                 the donating record */
{
  locksys::Shard_latches_guard guard{UT_LOCATION_HERE, *receiver, *donator};
  lock_sys->prdt_hash.find_on_record(
      RecID{donator, PRDT_HEAPNO}, [&](lock_t *lock) {
        const ulint type_mode = lock->type_mode;
        lock_prdt_t *lock_prdt = lock_get_prdt_from_lock(lock);

        lock_rec_clear_request_no_wakeup(lock, PRDT_HEAPNO);

        lock_prdt_add_to_queue(type_mode, receiver, lock->index, lock->trx,
                               lock_prdt);
        return false;
      });
}

/** Removes predicate lock objects set on an index page which is discarded.
@param[in]      block           page to be discarded
@param[in]      lock_hash       lock hash */
void lock_prdt_page_free_from_discard(const buf_block_t *block,
                                      Locks_hashtable &lock_hash) {
  ut_ad(locksys::owns_page_shard(block->get_page_id()));
  lock_hash.find_on_block(block, [&](lock_t *lock) {
    trx_t *trx = lock->trx;
    trx_mutex_enter(trx);
    lock_rec_discard(lock);
    trx_mutex_exit(trx);
    return false;
  });
}
