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

/** @file include/lock0prdt.h
 The predicate lock system

 Created 9/7/2013 Jimmy Yang
 *******************************************************/
#ifndef lock0prdt_h
#define lock0prdt_h

#include "lock0lock.h"
#include "univ.i"

/* Predicate lock data */
typedef struct lock_prdt {
  void *data; /* Predicate data */
  uint16 op;  /* Predicate operator */
} lock_prdt_t;

/** Acquire a predicate lock on a block
 @return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_prdt_lock(buf_block_t *block,  /*!< in/out: buffer block of rec */
                       lock_prdt_t *prdt,   /*!< in: Predicate for the lock */
                       dict_index_t *index, /*!< in: secondary index */
                       enum lock_mode mode, /*!< in: mode of the lock which
                                            the read cursor should set on
                                            records: LOCK_S or LOCK_X; the
                                            latter is possible in
                                            SELECT FOR UPDATE */
                       ulint type_mode,
                       /*!< in: LOCK_PREDICATE or LOCK_PRDT_PAGE */
                       que_thr_t *thr); /*!< in: query thread
                                       (can be NULL if BTR_NO_LOCKING_FLAG) */

/** Acquire a "Page" lock on a block
@param[in]  page_id   id of the page to lock
@param[in]  index     secondary index
@param[in]  thr       query thread
@return DB_SUCCESS */
dberr_t lock_place_prdt_page_lock(const page_id_t &page_id, dict_index_t *index,
                                  que_thr_t *thr);

/** Initiate a Predicate lock from a MBR */
void lock_init_prdt_from_mbr(
    lock_prdt_t *prdt, /*!< in/out: predicate to initialized */
    rtr_mbr_t *mbr,    /*!< in: Minimum Bounding Rectangle */
    ulint mode,        /*!< in: Search mode */
    mem_heap_t *heap); /*!< in: heap for allocating memory */

/** Get predicate lock's minimum bounding box
 @return the minimum bounding box*/
lock_prdt_t *lock_get_prdt_from_lock(const lock_t *lock); /*!< in: the lock */

/** Checks if a predicate lock request for a new lock has to wait for
 request lock2.
 @return true if new lock has to wait for lock2 to be removed */
bool lock_prdt_has_to_wait(
    const trx_t *trx,     /*!< in: trx of new lock */
    ulint type_mode,      /*!< in: precise mode of the new lock
                        to set: LOCK_S or LOCK_X, possibly
                        ORed to LOCK_PREDICATE or LOCK_PRDT_PAGE,
                        LOCK_INSERT_INTENTION */
    lock_prdt_t *prdt,    /*!< in: lock predicate to check */
    const lock_t *lock2); /*!< in: another record lock; NOTE that
                          it is assumed that this has a lock bit
                          set on the same record as in the new
                          lock we are setting */

/** Update predicate lock when page splits

@param[in,out]  block       page to be split
@param[in,out]  new_block   the new half page
@param[in]      prdt        MBR on the old page
@param[in]      new_prdt    MBR on the new page
*/
void lock_prdt_update_split(buf_block_t *block, buf_block_t *new_block,
                            lock_prdt_t *prdt, lock_prdt_t *new_prdt);

/** Adjust locks from an ancestor page of Rtree on the appropriate level .
Check whether any predicate lock in parent needs to propagate to child page
after split.

@param[in,out]  left_block    page to be split
@param[in,out]  right_block   the new half page
@param[in]      left_prdt     MBR on the old page
@param[in]      right_prdt    MBR on the new page
@param[in]      page_id       the parent's page id
*/
void lock_prdt_update_parent(buf_block_t *left_block, buf_block_t *right_block,
                             lock_prdt_t *left_prdt, lock_prdt_t *right_prdt,
                             const page_id_t &page_id);

/** Checks if locks of other transactions prevent an immediate insert of
 a predicate record.
 @return DB_SUCCESS, DB_LOCK_WAIT, or DB_DEADLOCK */
dberr_t lock_prdt_insert_check_and_lock(
    ulint flags,         /*!< in: if BTR_NO_LOCKING_FLAG bit is
                         set, does nothing */
    const rec_t *rec,    /*!< in: record after which to insert */
    buf_block_t *block,  /*!< in/out: buffer block of rec */
    dict_index_t *index, /*!< in: index */
    que_thr_t *thr,      /*!< in: query thread */
    mtr_t *mtr,          /*!< in/out: mini-transaction */
    lock_prdt_t *prdt);  /*!< in: Minimum Bound Rectangle */

/** Append a predicate to the lock
@param[in] lock Lock
@param[in] prdt Predicate */
void lock_prdt_set_prdt(lock_t *lock, const lock_prdt_t *prdt);

#if 0

/*********************************************************************//**
Get predicate lock's minimum bounding box
@return the minimum bounding box*/
static inline
rtr_mbr_t*
prdt_get_mbr_from_prdt(
        const lock_prdt_t*      prdt);  /*!< in: the lock predicate */

#endif
/** Moves the locks of a record to another record and resets the lock bits of
 the donating record. */
void lock_prdt_rec_move(
    const buf_block_t *receiver, /*!< in: buffer block containing
                                 the receiving record */
    const buf_block_t *donator); /*!< in: buffer block containing
                                 the donating record */

/** Check whether there are no R-tree Page locks on a page by other transactions
@param[in]      trx     trx to test the lock
@param[in]      page_id id of the page
@retval true    if there is no lock
@retval false   if some transaction other than trx holds a page lock */
bool lock_test_prdt_page_lock(const trx_t *trx, const page_id_t &page_id);

/** Removes predicate lock objects set on an index page which is discarded.
@param[in]      block           page to be discarded
@param[in]      lock_hash       lock hash */
void lock_prdt_page_free_from_discard(const buf_block_t *block,
                                      hash_table_t *lock_hash);

#endif
