/*****************************************************************************

Copyright (c) 2014, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
 @return	the minimum bounding box */
UNIV_INLINE
rtr_mbr_t *prdt_get_mbr_from_prdt(
    const lock_prdt_t *prdt) /*!< in: the lock predicate */
{
  rtr_mbr_t *mbr_loc = reinterpret_cast<rtr_mbr_t *>(prdt->data);

  return (mbr_loc);
}

/** Get a predicate from a lock
 @return	the predicate */
lock_prdt_t *lock_get_prdt_from_lock(const lock_t *lock) /*!< in: the lock */
{
  lock_prdt_t *prdt =
      reinterpret_cast<lock_prdt_t *>(&((reinterpret_cast<byte *>(
          const_cast<lock_t *>(&lock[1])))[UNIV_WORD_SIZE]));

  return (prdt);
}

/** Get a minimum bounding box directly from a lock
 @return	the minimum bounding box*/
UNIV_INLINE
rtr_mbr_t *lock_prdt_get_mbr_from_lock(const lock_t *lock) /*!< in: the lock */
{
  ut_ad(lock->type_mode & LOCK_PREDICATE);

  lock_prdt_t *prdt = lock_get_prdt_from_lock(lock);

  rtr_mbr_t *mbr_loc = prdt_get_mbr_from_prdt(prdt);

  return (mbr_loc);
}

/** Append a predicate to the lock */
void lock_prdt_set_prdt(lock_t *lock,            /*!< in: lock */
                        const lock_prdt_t *prdt) /*!< in: Predicate */
{
  ut_ad(lock->type_mode & LOCK_PREDICATE);

  memcpy(&(((byte *)&lock[1])[UNIV_WORD_SIZE]), prdt, sizeof *prdt);
}

/** Check whether two predicate locks are compatible with each other
@param[in]	prdt1	first predicate lock
@param[in]	prdt2	second predicate lock
@param[in]	op	predicate comparison operator
@param[in]	srs      Spatial reference system of R-tree
@return	true if consistent */
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
      ret = mbr_disjoint_cmp(mbr1, mbr2);
      break;
    case PAGE_CUR_MBR_EQUAL:
      ret = mbr_equal_cmp(srs, mbr1, mbr2);
      break;
    case PAGE_CUR_INTERSECT:
      ret = mbr_intersect_cmp(mbr1, mbr2);
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
 @return	true if new lock has to wait for lock2 to be released */
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
  lock_prdt_t *cur_prdt = lock_get_prdt_from_lock(lock2);

  ut_ad(trx && lock2);
  ut_ad((lock2->type_mode & LOCK_PREDICATE && type_mode & LOCK_PREDICATE) ||
        (lock2->type_mode & LOCK_PRDT_PAGE && type_mode & LOCK_PRDT_PAGE));

  ut_ad(type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));

  if (trx != lock2->trx &&
      !lock_mode_compatible(static_cast<lock_mode>(LOCK_MODE_MASK & type_mode),
                            lock_get_mode(lock2))) {
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

      return (FALSE);
    }

    if (lock2->type_mode & LOCK_INSERT_INTENTION) {
      /* No lock request needs to wait for an insert
      intention lock to be removed. This makes it similar
      to GAP lock, that allows conflicting insert intention
      locks */
      return (FALSE);
    }

    if (!lock_prdt_consistent(cur_prdt, prdt, 0, lock2->index->rtr_srs.get())) {
      return (false);
    }

    return (TRUE);
  }

  return (FALSE);
}

/** Checks if a transaction has a GRANTED stronger or equal predicate lock
 on the page
 @return	lock or NULL */
UNIV_INLINE
lock_t *lock_prdt_has_lock(ulint precise_mode, /*!< in: LOCK_S or LOCK_X */
                           ulint type_mode,    /*!< in: LOCK_PREDICATE etc. */
                           const buf_block_t *block, /*!< in: buffer block
                                                     containing the record */
                           lock_prdt_t *prdt, /*!< in: The predicate to be
                                              attached to the new lock */
                           const trx_t *trx)  /*!< in: transaction */
{
  lock_t *lock;

  ut_ad(lock_mutex_own());
  ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S ||
        (precise_mode & LOCK_MODE_MASK) == LOCK_X);
  ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));

  for (lock = lock_rec_get_first(lock_hash_get(type_mode), block, PRDT_HEAPNO);
       lock != NULL; lock = lock_rec_get_next(PRDT_HEAPNO, lock)) {
    ut_ad(lock->type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));

    if (lock->trx == trx && !(lock->type_mode & LOCK_INSERT_INTENTION) &&
        !lock_get_wait(lock) &&
        lock_mode_stronger_or_eq(
            lock_get_mode(lock),
            static_cast<lock_mode>(precise_mode & LOCK_MODE_MASK))) {
      if (lock->type_mode & LOCK_PRDT_PAGE) {
        return (lock);
      }

      ut_ad(lock->type_mode & LOCK_PREDICATE);
      lock_prdt_t *cur_prdt = lock_get_prdt_from_lock(lock);

      /* if the lock predicate operator is the same
      as the one to look, and prdicate test is successful,
      then we find a lock */
      if (cur_prdt->op == prdt->op &&
          lock_prdt_consistent(cur_prdt, prdt, 0, lock->index->rtr_srs.get())) {
        return (lock);
      }
    }
  }

  return (NULL);
}

/** Checks if some other transaction has a conflicting predicate
 lock request in the queue, so that we have to wait.
 @return	lock or NULL */
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
  ut_ad(lock_mutex_own());

  for (const lock_t *lock =
           lock_rec_get_first(lock_hash_get(mode), block, PRDT_HEAPNO);
       lock != NULL; lock = lock_rec_get_next_const(PRDT_HEAPNO, lock)) {
    if (lock->trx == trx) {
      continue;
    }

    if (lock_prdt_has_to_wait(trx, mode, prdt, lock)) {
      return (lock);
    }
  }

  return (NULL);
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
 @return	true if they are the same */
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
 @return	lock or NULL */
static lock_t *lock_prdt_find_on_page(
    ulint type_mode,          /*!< in: lock type_mode field */
    const buf_block_t *block, /*!< in: buffer block */
    lock_prdt_t *prdt,        /*!< in: MBR with the lock */
    const trx_t *trx)         /*!< in: transaction */
{
  lock_t *lock;

  ut_ad(lock_mutex_own());

  for (lock = lock_rec_get_first_on_page(lock_hash_get(type_mode), block);
       lock != NULL; lock = lock_rec_get_next_on_page(lock)) {
    if (lock->trx == trx && lock->type_mode == type_mode) {
      if (lock->type_mode & LOCK_PRDT_PAGE) {
        return (lock);
      }

      ut_ad(lock->type_mode & LOCK_PREDICATE);

      if (lock_prdt_is_same(lock_get_prdt_from_lock(lock), prdt,
                            lock->index->rtr_srs.get())) {
        return (lock);
      }
    }
  }

  return (NULL);
}

/** Adds a predicate lock request in the predicate lock queue.
 @return	lock where the bit was set */
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
  ut_ad(lock_mutex_own());
  ut_ad(trx->owns_mutex == trx_mutex_own(trx));
  ut_ad(!index->is_clustered() && !dict_index_is_online_ddl(index));
  ut_ad(type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));

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

  /* Look for a waiting lock request on the same record or on a gap */

  lock_t *lock;

  for (lock = lock_rec_get_first_on_page(lock_hash_get(type_mode), block);
       lock != NULL; lock = lock_rec_get_next_on_page(lock)) {
    if (lock_get_wait(lock) && lock_rec_get_nth_bit(lock, PRDT_HEAPNO) &&
        lock->type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE)) {
      break;
    }
  }

  if (lock == NULL && !(type_mode & LOCK_WAIT)) {
    /* Look for a similar record lock on the same page:
    if one is found and there are no waiting lock requests,
    we can just set the bit */

    lock = lock_prdt_find_on_page(type_mode, block, prdt, trx);

    if (lock != NULL) {
      if (lock->type_mode & LOCK_PREDICATE) {
        lock_prdt_enlarge_prdt(lock, prdt);
      }

      return (lock);
    }
  }

  RecLock rec_lock(index, block, PRDT_HEAPNO, type_mode);

  return (rec_lock.create(trx, true, prdt));
}

/** Checks if locks of other transactions prevent an immediate insert of
 a predicate record.
 @return	DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK
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

  lock_mutex_enter();

  /* Because this code is invoked for a running transaction by
  the thread that is serving the transaction, it is not necessary
  to hold trx->mutex here. */

  ut_ad(lock_table_has(trx, index->table, LOCK_IX));

  lock_t *lock;

  /* Only need to check locks on prdt_hash */
  lock = lock_rec_get_first(lock_sys->prdt_hash, block, PRDT_HEAPNO);

  if (lock == NULL) {
    lock_mutex_exit();

    /* Update the page max trx id field */
    page_update_max_trx_id(block, buf_block_get_page_zip(block), trx->id, mtr);

    return (DB_SUCCESS);
  }

  ut_ad(lock->type_mode & LOCK_PREDICATE);

  dberr_t err;

  /* If another transaction has an explicit lock request which locks
  the predicate, waiting or granted, on the successor, the insert
  has to wait.

  Similar to GAP lock, we do not consider lock from inserts conflicts
  with each other */

  const ulint mode = LOCK_X | LOCK_PREDICATE | LOCK_INSERT_INTENTION;

  const lock_t *wait_for =
      lock_prdt_other_has_conflicting(mode, block, prdt, trx);

  if (wait_for != NULL) {
    rtr_mbr_t *mbr = prdt_get_mbr_from_prdt(prdt);

    /* Allocate MBR on the lock heap */
    lock_init_prdt_from_mbr(prdt, mbr, 0, trx->lock.lock_heap);

    RecLock rec_lock(thr, index, block, PRDT_HEAPNO, mode);

    /* Note that we may get DB_SUCCESS also here! */

    trx_mutex_enter(trx);

    trx->owns_mutex = true;

    err = rec_lock.add_to_waitq(wait_for, prdt);

    trx->owns_mutex = false;

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

/** Check whether any predicate lock in parent needs to propagate to
 child page after split. */
void lock_prdt_update_parent(
    buf_block_t *left_block,  /*!< in/out: page to be split */
    buf_block_t *right_block, /*!< in/out: the new half page */
    lock_prdt_t *left_prdt,   /*!< in: MBR on the old page */
    lock_prdt_t *right_prdt,  /*!< in: MBR on the new page */
    lock_prdt_t *parent_prdt, /*!< in: original parent MBR */
    space_id_t space,         /*!< in: parent space id */
    page_no_t page_no)        /*!< in: parent page number */
{
  lock_t *lock;

  lock_mutex_enter();

  /* Get all locks in parent */
  for (lock =
           lock_rec_get_first_on_page_addr(lock_sys->prdt_hash, space, page_no);
       lock; lock = lock_rec_get_next_on_page(lock)) {
    lock_prdt_t *lock_prdt;
    ulint op = PAGE_CUR_DISJOINT;

    ut_ad(lock);

    if (!(lock->type_mode & LOCK_PREDICATE) ||
        (lock->type_mode & LOCK_MODE_MASK) == LOCK_X) {
      continue;
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
  }

  lock_mutex_exit();
}

/** Update predicate lock when page splits */
static void lock_prdt_update_split_low(
    buf_block_t *block,     /*!< in/out: page to be split */
    buf_block_t *new_block, /*!< in/out: the new half page */
    lock_prdt_t *prdt,      /*!< in: MBR on the old page */
    lock_prdt_t *new_prdt,  /*!< in: MBR on the new page */
    space_id_t space,       /*!< in: space id */
    page_no_t page_no,      /*!< in: page number */
    ulint type_mode)        /*!< in: LOCK_PREDICATE or
                            LOCK_PRDT_PAGE */
{
  lock_t *lock;

  lock_mutex_enter();

  for (lock = lock_rec_get_first_on_page_addr(lock_hash_get(type_mode), space,
                                              page_no);
       lock; lock = lock_rec_get_next_on_page(lock)) {
    ut_ad(lock);

    /* First dealing with Page Lock */
    if (lock->type_mode & LOCK_PRDT_PAGE) {
      /* Duplicate the lock to new page */
      trx_mutex_enter(lock->trx);

      lock->trx->owns_mutex = true;

      lock_prdt_add_to_queue(lock->type_mode, new_block, lock->index, lock->trx,
                             NULL);

      lock->trx->owns_mutex = false;

      trx_mutex_exit(lock->trx);

      continue;
    }

    /* Now dealing with Predicate Lock */
    lock_prdt_t *lock_prdt;
    ulint op = PAGE_CUR_DISJOINT;

    ut_ad(lock->type_mode & LOCK_PREDICATE);

    /* No need to duplicate waiting X locks */
    if ((lock->type_mode & LOCK_MODE_MASK) == LOCK_X) {
      continue;
    }

    lock_prdt = lock_get_prdt_from_lock(lock);

    if (lock_prdt_consistent(lock_prdt, prdt, op, lock->index->rtr_srs.get())) {
      if (!lock_prdt_consistent(lock_prdt, new_prdt, op,
                                lock->index->rtr_srs.get())) {
        /* Move the lock to new page */
        trx_mutex_enter(lock->trx);

        lock->trx->owns_mutex = true;

        lock_prdt_add_to_queue(lock->type_mode, new_block, lock->index,
                               lock->trx, lock_prdt);

        lock->trx->owns_mutex = false;

        trx_mutex_exit(lock->trx);
      }
    } else if (!lock_prdt_consistent(lock_prdt, new_prdt, op,
                                     lock->index->rtr_srs.get())) {
      /* Duplicate the lock to new page */
      trx_mutex_enter(lock->trx);

      lock->trx->owns_mutex = true;

      lock_prdt_add_to_queue(lock->type_mode, new_block, lock->index, lock->trx,
                             lock_prdt);

      lock->trx->owns_mutex = false;

      trx_mutex_exit(lock->trx);
    }
  }

  lock_mutex_exit();
}

/** Update predicate lock when page splits */
void lock_prdt_update_split(
    buf_block_t *block,     /*!< in/out: page to be split */
    buf_block_t *new_block, /*!< in/out: the new half page */
    lock_prdt_t *prdt,      /*!< in: MBR on the old page */
    lock_prdt_t *new_prdt,  /*!< in: MBR on the new page */
    space_id_t space,       /*!< in: space id */
    page_no_t page_no)      /*!< in: page number */
{
  lock_prdt_update_split_low(block, new_block, prdt, new_prdt, space, page_no,
                             LOCK_PREDICATE);

  lock_prdt_update_split_low(block, new_block, NULL, NULL, space, page_no,
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

  if (heap != NULL) {
    prdt->data = mem_heap_alloc(heap, sizeof(*mbr));
    ut_memcpy(prdt->data, mbr, sizeof(*mbr));
  } else {
    prdt->data = static_cast<void *>(mbr);
  }

  prdt->op = static_cast<uint16>(mode);
}

/** Acquire a predicate lock on a block
 @return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, or DB_DEADLOCK
 */
dberr_t lock_prdt_lock(buf_block_t *block,  /*!< in/out: buffer block of rec */
                       lock_prdt_t *prdt,   /*!< in: Predicate for the lock */
                       dict_index_t *index, /*!< in: secondary index */
                       lock_mode mode,      /*!< in: mode of the lock which
                                            the read cursor should set on
                                            records: LOCK_S or LOCK_X; the
                                            latter is possible in
                                            SELECT FOR UPDATE */
                       ulint type_mode,
                       /*!< in: LOCK_PREDICATE or LOCK_PRDT_PAGE */
                       que_thr_t *thr, /*!< in: query thread
                                       (can be NULL if BTR_NO_LOCKING_FLAG) */
                       mtr_t *mtr)     /*!< in/out: mini-transaction */
{
  trx_t *trx = thr_get_trx(thr);
  dberr_t err = DB_SUCCESS;
  lock_rec_req_status status = LOCK_REC_SUCCESS;

  if (trx->read_only || index->table->is_temporary()) {
    return (DB_SUCCESS);
  }

  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(index));
  ut_ad(type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));

  hash_table_t *hash = type_mode == LOCK_PREDICATE ? lock_sys->prdt_hash
                                                   : lock_sys->prdt_page_hash;

  /* Another transaction cannot have an implicit lock on the record,
  because when we come here, we already have modified the clustered
  index record, and this would not have been possible if another active
  transaction had modified this secondary index record. */

  lock_mutex_enter();

  const ulint prdt_mode = mode | type_mode;
  lock_t *lock = lock_rec_get_first_on_page(hash, block);

  if (lock == NULL) {
    RecLock rec_lock(index, block, PRDT_HEAPNO, prdt_mode);

    lock = rec_lock.create(trx, true);

    status = LOCK_REC_SUCCESS_CREATED;

  } else {
    trx_mutex_enter(trx);

    trx->owns_mutex = true;

    if (lock_rec_get_next_on_page(lock) || lock->trx != trx ||
        lock->type_mode != (LOCK_REC | prdt_mode) ||
        lock_rec_get_n_bits(lock) == 0 ||
        ((type_mode & LOCK_PREDICATE) &&
         (!lock_prdt_consistent(lock_get_prdt_from_lock(lock), prdt, 0,
                                lock->index->rtr_srs.get())))) {
      lock = lock_prdt_has_lock(mode, type_mode, block, prdt, trx);

      if (lock == NULL) {
        const lock_t *wait_for;

        wait_for = lock_prdt_other_has_conflicting(prdt_mode, block, prdt, trx);

        if (wait_for != NULL) {
          RecLock rec_lock(thr, index, block, PRDT_HEAPNO, prdt_mode, prdt);

          err = rec_lock.add_to_waitq(wait_for);

        } else {
          lock_prdt_add_to_queue(prdt_mode, block, index, trx, prdt);

          status = LOCK_REC_SUCCESS;
        }
      }

      trx->owns_mutex = false;

      trx_mutex_exit(trx);

    } else {
      trx->owns_mutex = false;

      trx_mutex_exit(trx);

      if (!lock_rec_get_nth_bit(lock, PRDT_HEAPNO)) {
        lock_rec_set_nth_bit(lock, PRDT_HEAPNO);
        status = LOCK_REC_SUCCESS_CREATED;
      }
    }
  }

  lock_mutex_exit();

  if (status == LOCK_REC_SUCCESS_CREATED && type_mode == LOCK_PREDICATE) {
    /* Append the predicate in the lock record */
    lock_prdt_set_prdt(lock, prdt);
  }
  ut_ad(err == DB_SUCCESS || err == DB_SUCCESS_LOCKED_REC ||
        err == DB_LOCK_WAIT || err == DB_DEADLOCK);
  return (err);
}

/** Acquire a "Page" lock on a block
 @return	DB_SUCCESS
 */
dberr_t lock_place_prdt_page_lock(
    space_id_t space,    /*!< in: space for the page to lock */
    page_no_t page_no,   /*!< in: page number */
    dict_index_t *index, /*!< in: secondary index */
    que_thr_t *thr)      /*!< in: query thread */
{
  ut_ad(thr != NULL);
  ut_ad(!srv_read_only_mode);

  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(index));

  /* Another transaction cannot have an implicit lock on the record,
  because when we come here, we already have modified the clustered
  index record, and this would not have been possible if another active
  transaction had modified this secondary index record. */

  lock_mutex_enter();

  const lock_t *lock =
      lock_rec_get_first_on_page_addr(lock_sys->prdt_page_hash, space, page_no);

  const ulint mode = LOCK_S | LOCK_PRDT_PAGE;
  trx_t *trx = thr_get_trx(thr);

  if (lock != NULL) {
    trx_mutex_enter(trx);

    /* Find a matching record lock owned by this transaction. */

    while (lock != NULL && lock->trx != trx) {
      lock = lock_rec_get_next_on_page_const(lock);
    }

    ut_ad(lock == NULL || lock->type_mode == (mode | LOCK_REC));
    ut_ad(lock == NULL || lock_rec_get_n_bits(lock) != 0);

    trx_mutex_exit(trx);
  }

  if (lock == NULL) {
    RecID rec_id(space, page_no, PRDT_HEAPNO);
    RecLock rec_lock(index, rec_id, mode);

    rec_lock.create(trx, true);

#ifdef PRDT_DIAG
    printf("GIS_DIAGNOSTIC: page lock %d\n", (int)page_no);
#endif /* PRDT_DIAG */
  }

  lock_mutex_exit();

  return (DB_SUCCESS);
}

/** Check whether there are R-tree Page lock on a page
@param[in]	trx	trx to test the lock
@param[in]	space	space id for the page
@param[in]	page_no	page number
@retval	true	if there is no lock
@retval	false	if some other trx holds a page lock */
bool lock_test_prdt_page_lock(const trx_t *trx, space_id_t space,
                              page_no_t page_no) {
  lock_t *lock;

  lock_mutex_enter();

  lock =
      lock_rec_get_first_on_page_addr(lock_sys->prdt_page_hash, space, page_no);

  lock_mutex_exit();

  return (lock == NULL || trx == lock->trx);
}

/** Moves the locks of a page to another page and resets the lock bits of
 the donating records. */
void lock_prdt_rec_move(
    const buf_block_t *receiver, /*!< in: buffer block containing
                                 the receiving record */
    const buf_block_t *donator)  /*!< in: buffer block containing
                                 the donating record */
{
  lock_t *lock;

  if (!lock_sys->prdt_hash) {
    return;
  }

  lock_mutex_enter();

  for (lock = lock_rec_get_first(lock_sys->prdt_hash, donator, PRDT_HEAPNO);
       lock != NULL; lock = lock_rec_get_next(PRDT_HEAPNO, lock)) {
    const ulint type_mode = lock->type_mode;
    lock_prdt_t *lock_prdt = lock_get_prdt_from_lock(lock);

    lock_rec_trx_wait(lock, PRDT_HEAPNO, type_mode);

    lock_prdt_add_to_queue(type_mode, receiver, lock->index, lock->trx,
                           lock_prdt);
  }

  lock_mutex_exit();
}

/** Removes predicate lock objects set on an index page which is discarded.
@param[in]	block		page to be discarded
@param[in]	lock_hash	lock hash */
void lock_prdt_page_free_from_discard(const buf_block_t *block,
                                      hash_table_t *lock_hash) {
  lock_t *lock;
  lock_t *next_lock;
  space_id_t space;
  page_no_t page_no;

  ut_ad(lock_mutex_own());

  space = block->page.id.space();
  page_no = block->page.id.page_no();

  lock = lock_rec_get_first_on_page_addr(lock_hash, space, page_no);

  while (lock != NULL) {
    next_lock = lock_rec_get_next_on_page(lock);

    lock_rec_discard(lock);

    lock = next_lock;
  }
}
