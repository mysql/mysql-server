/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file include/lock0lock.h
The transaction lock system

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#ifndef lock0lock_h
#define lock0lock_h

#include "univ.i"
#include "buf0types.h"
#include "trx0types.h"
#include "mtr0types.h"
#include "rem0types.h"
#include "dict0types.h"
#include "que0types.h"
#include "lock0types.h"
#include "hash0hash.h"
#include "srv0srv.h"
#include "ut0vec.h"
#include "gis0rtree.h"
#include "lock0prdt.h"

// Forward declaration
class ReadView;

extern my_bool	innobase_deadlock_detect;

/*********************************************************************//**
Gets the size of a lock struct.
@return size in bytes */
ulint
lock_get_size(void);
/*===============*/
/*********************************************************************//**
Creates the lock system at database start. */
void
lock_sys_create(
/*============*/
	ulint	n_cells);	/*!< in: number of slots in lock hash table */
/** Resize the lock hash table.
@param[in]	n_cells	number of slots in lock hash table */
void
lock_sys_resize(
	ulint	n_cells);

/*********************************************************************//**
Closes the lock system at database shutdown. */
void
lock_sys_close(void);
/*================*/
/*********************************************************************//**
Gets the heap_no of the smallest user record on a page.
@return heap_no of smallest user record, or PAGE_HEAP_NO_SUPREMUM */
UNIV_INLINE
ulint
lock_get_min_heap_no(
/*=================*/
	const buf_block_t*	block);	/*!< in: buffer block */
/*************************************************************//**
Updates the lock table when we have reorganized a page. NOTE: we copy
also the locks set on the infimum of the page; the infimum may carry
locks if an update of a record is occurring on the page, and its locks
were temporarily stored on the infimum. */
void
lock_move_reorganize_page(
/*======================*/
	const buf_block_t*	block,	/*!< in: old index page, now
					reorganized */
	const buf_block_t*	oblock);/*!< in: copy of the old, not
					reorganized page */
/*************************************************************//**
Moves the explicit locks on user records to another page if a record
list end is moved to another page. */
void
lock_move_rec_list_end(
/*===================*/
	const buf_block_t*	new_block,	/*!< in: index page to move to */
	const buf_block_t*	block,		/*!< in: index page */
	const rec_t*		rec);		/*!< in: record on page: this
						is the first record moved */
/*************************************************************//**
Moves the explicit locks on user records to another page if a record
list start is moved to another page. */
void
lock_move_rec_list_start(
/*=====================*/
	const buf_block_t*	new_block,	/*!< in: index page to move to */
	const buf_block_t*	block,		/*!< in: index page */
	const rec_t*		rec,		/*!< in: record on page:
						this is the first
						record NOT copied */
	const rec_t*		old_end);	/*!< in: old
						previous-to-last
						record on new_page
						before the records
						were copied */
/*************************************************************//**
Updates the lock table when a page is split to the right. */
void
lock_update_split_right(
/*====================*/
	const buf_block_t*	right_block,	/*!< in: right page */
	const buf_block_t*	left_block);	/*!< in: left page */
/*************************************************************//**
Updates the lock table when a page is merged to the right. */
void
lock_update_merge_right(
/*====================*/
	const buf_block_t*	right_block,	/*!< in: right page to
						which merged */
	const rec_t*		orig_succ,	/*!< in: original
						successor of infimum
						on the right page
						before merge */
	const buf_block_t*	left_block);	/*!< in: merged index
						page which will be
						discarded */
/*************************************************************//**
Updates the lock table when the root page is copied to another in
btr_root_raise_and_insert. Note that we leave lock structs on the
root page, even though they do not make sense on other than leaf
pages: the reason is that in a pessimistic update the infimum record
of the root page will act as a dummy carrier of the locks of the record
to be updated. */
void
lock_update_root_raise(
/*===================*/
	const buf_block_t*	block,	/*!< in: index page to which copied */
	const buf_block_t*	root);	/*!< in: root page */
/*************************************************************//**
Updates the lock table when a page is copied to another and the original page
is removed from the chain of leaf pages, except if page is the root! */
void
lock_update_copy_and_discard(
/*=========================*/
	const buf_block_t*	new_block,	/*!< in: index page to
						which copied */
	const buf_block_t*	block);		/*!< in: index page;
						NOT the root! */
/*************************************************************//**
Updates the lock table when a page is split to the left. */
void
lock_update_split_left(
/*===================*/
	const buf_block_t*	right_block,	/*!< in: right page */
	const buf_block_t*	left_block);	/*!< in: left page */
/*************************************************************//**
Updates the lock table when a page is merged to the left. */
void
lock_update_merge_left(
/*===================*/
	const buf_block_t*	left_block,	/*!< in: left page to
						which merged */
	const rec_t*		orig_pred,	/*!< in: original predecessor
						of supremum on the left page
						before merge */
	const buf_block_t*	right_block);	/*!< in: merged index page
						which will be discarded */
/*************************************************************//**
Resets the original locks on heir and replaces them with gap type locks
inherited from rec. */
void
lock_rec_reset_and_inherit_gap_locks(
/*=================================*/
	const buf_block_t*	heir_block,	/*!< in: block containing the
						record which inherits */
	const buf_block_t*	block,		/*!< in: block containing the
						record from which inherited;
						does NOT reset the locks on
						this record */
	ulint			heir_heap_no,	/*!< in: heap_no of the
						inheriting record */
	ulint			heap_no);	/*!< in: heap_no of the
						donating record */
/*************************************************************//**
Updates the lock table when a page is discarded. */
void
lock_update_discard(
/*================*/
	const buf_block_t*	heir_block,	/*!< in: index page
						which will inherit the locks */
	ulint			heir_heap_no,	/*!< in: heap_no of the record
						which will inherit the locks */
	const buf_block_t*	block);		/*!< in: index page
						which will be discarded */
/*************************************************************//**
Updates the lock table when a new user record is inserted. */
void
lock_update_insert(
/*===============*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec);	/*!< in: the inserted record */
/*************************************************************//**
Updates the lock table when a record is removed. */
void
lock_update_delete(
/*===============*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec);	/*!< in: the record to be removed */
/*********************************************************************//**
Stores on the page infimum record the explicit locks of another record.
This function is used to store the lock state of a record when it is
updated and the size of the record changes in the update. The record
is in such an update moved, perhaps to another page. The infimum record
acts as a dummy carrier record, taking care of lock releases while the
actual record is being moved. */
void
lock_rec_store_on_page_infimum(
/*===========================*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec);	/*!< in: record whose lock state
					is stored on the infimum
					record of the same page; lock
					bits are reset on the
					record */
/*********************************************************************//**
Restores the state of explicit lock requests on a single record, where the
state was stored on the infimum of the page. */
void
lock_rec_restore_from_page_infimum(
/*===============================*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record whose lock state
					is restored */
	const buf_block_t*	donator);/*!< in: page (rec is not
					necessarily on this page)
					whose infimum stored the lock
					state; lock bits are reset on
					the infimum */
/*********************************************************************//**
Determines if there are explicit record locks on a page.
@return an explicit record lock on the page, or NULL if there are none */
lock_t*
lock_rec_expl_exist_on_page(
/*========================*/
	ulint	space,	/*!< in: space id */
	ulint	page_no)/*!< in: page number */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Checks if locks of other transactions prevent an immediate insert of
a record. If they do, first tests if the query thread should anyway
be suspended for some reason; if not, then puts the transaction and
the query thread to the lock wait state and inserts a waiting request
for a gap x-lock to the lock queue.
@return DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
dberr_t
lock_rec_insert_check_and_lock(
/*===========================*/
	ulint		flags,	/*!< in: if BTR_NO_LOCKING_FLAG bit is
				set, does nothing */
	const rec_t*	rec,	/*!< in: record after which to insert */
	buf_block_t*	block,	/*!< in/out: buffer block of rec */
	dict_index_t*	index,	/*!< in: index */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	ibool*		inherit)/*!< out: set to TRUE if the new
				inserted record maybe should inherit
				LOCK_GAP type locks from the successor
				record */
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Enqueues a waiting request for a lock which cannot be granted immediately.
Checks for deadlocks.
@return DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED, or
DB_SUCCESS_LOCKED_REC; DB_SUCCESS_LOCKED_REC means that
there was a deadlock, but another transaction was chosen as a victim,
and we got the lock immediately: no need to wait then */
dberr_t
lock_rec_enqueue_waiting(
/*=====================*/
	ulint			type_mode,/*!< in: lock mode this
					transaction is requesting:
					LOCK_S or LOCK_X, possibly
					ORed with LOCK_GAP or
					LOCK_REC_NOT_GAP, ORed with
					LOCK_INSERT_INTENTION if this
					waiting lock request is set
					when performing an insert of
					an index record */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	dict_index_t*		index,	/*!< in: index of record */
	que_thr_t*		thr,	/*!< in: query thread */
	lock_prdt_t*		prdt);	/*!< in: Minimum Bounding Box */

/*************************************************************//**
Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed. */
void
lock_rec_dequeue_from_page(
/*=======================*/
        lock_t*         in_lock);        /*!< in: record lock object: all
                                        record locks which are contained in
                                        this lock object are removed;
                                        transactions waiting behind will
                                        get their lock requests granted,
                                        if they are now qualified to it */

/*************************************************************//**
Moves the locks of a record to another record and resets the lock bits of
the donating record. */
UNIV_INLINE
void
lock_rec_move(
/*==========*/
        const buf_block_t*      receiver,       /*!< in: buffer block containing
                                                the receiving record */
        const buf_block_t*      donator,        /*!< in: buffer block containing
                                                the donating record */
        ulint                   receiver_heap_no,/*!< in: heap_no of the record
                                                which gets the locks; there
                                                must be no lock requests
                                                on it! */
        ulint                   donator_heap_no);/*!< in: heap_no of the record
                                                which gives the locks */

/*************************************************************//**
Moves the locks of a record to another record and resets the lock bits of
the donating record. */
void
lock_rec_move_low(
/*==============*/
	hash_table_t*		lock_hash,	/*!< in: hash  table to use */
        const buf_block_t*      receiver,       /*!< in: buffer block containing
                                                the receiving record */
        const buf_block_t*      donator,        /*!< in: buffer block containing
                                                the donating record */
        ulint                   receiver_heap_no,/*!< in: heap_no of the record
                                                which gets the locks; there
                                                must be no lock requests
                                                on it! */
        ulint                   donator_heap_no);/*!< in: heap_no of the record
                                                which gives the locks */
/*********************************************************************//**
Checks if locks of other transactions prevent an immediate modify (update,
delete mark, or delete unmark) of a clustered index record. If they do,
first tests if the query thread should anyway be suspended for some
reason; if not, then puts the transaction and the query thread to the
lock wait state and inserts a waiting request for a record x-lock to the
lock queue.
@return DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
dberr_t
lock_clust_rec_modify_check_and_lock(
/*=================================*/
	ulint			flags,	/*!< in: if BTR_NO_LOCKING_FLAG
					bit is set, does nothing */
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: record which should be
					modified */
	dict_index_t*		index,	/*!< in: clustered index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	que_thr_t*		thr)	/*!< in: query thread */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Checks if locks of other transactions prevent an immediate modify
(delete mark or delete unmark) of a secondary index record.
@return DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
dberr_t
lock_sec_rec_modify_check_and_lock(
/*===============================*/
	ulint		flags,	/*!< in: if BTR_NO_LOCKING_FLAG
				bit is set, does nothing */
	buf_block_t*	block,	/*!< in/out: buffer block of rec */
	const rec_t*	rec,	/*!< in: record which should be
				modified; NOTE: as this is a secondary
				index, we always have to modify the
				clustered index record first: see the
				comment below */
	dict_index_t*	index,	/*!< in: secondary index */
	que_thr_t*	thr,	/*!< in: query thread
				(can be NULL if BTR_NO_LOCKING_FLAG) */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Like lock_clust_rec_read_check_and_lock(), but reads a
secondary index record.
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
dberr_t
lock_sec_rec_read_check_and_lock(
/*=============================*/
	ulint			flags,	/*!< in: if BTR_NO_LOCKING_FLAG
					bit is set, does nothing */
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: user record or page
					supremum record which should
					be read or passed over by a
					read cursor */
	dict_index_t*		index,	/*!< in: secondary index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	lock_mode		mode,	/*!< in: mode of the lock which
					the read cursor should set on
					records: LOCK_S or LOCK_X; the
					latter is possible in
					SELECT FOR UPDATE */
	ulint			gap_mode,/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP */
	que_thr_t*		thr);	/*!< in: query thread */
/*********************************************************************//**
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record.
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
dberr_t
lock_clust_rec_read_check_and_lock(
/*===============================*/
	ulint			flags,	/*!< in: if BTR_NO_LOCKING_FLAG
					bit is set, does nothing */
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: user record or page
					supremum record which should
					be read or passed over by a
					read cursor */
	dict_index_t*		index,	/*!< in: clustered index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	lock_mode		mode,	/*!< in: mode of the lock which
					the read cursor should set on
					records: LOCK_S or LOCK_X; the
					latter is possible in
					SELECT FOR UPDATE */
	ulint			gap_mode,/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP */
	que_thr_t*		thr);	/*!< in: query thread */
/*********************************************************************//**
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record. This is an alternative version of
lock_clust_rec_read_check_and_lock() that does not require the parameter
"offsets".
@return DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
dberr_t
lock_clust_rec_read_check_and_lock_alt(
/*===================================*/
	ulint			flags,	/*!< in: if BTR_NO_LOCKING_FLAG
					bit is set, does nothing */
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: user record or page
					supremum record which should
					be read or passed over by a
					read cursor */
	dict_index_t*		index,	/*!< in: clustered index */
	lock_mode		mode,	/*!< in: mode of the lock which
					the read cursor should set on
					records: LOCK_S or LOCK_X; the
					latter is possible in
					SELECT FOR UPDATE */
	ulint			gap_mode,/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP */
	que_thr_t*		thr)	/*!< in: query thread */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Checks that a record is seen in a consistent read.
@return true if sees, or false if an earlier version of the record
should be retrieved */
bool
lock_clust_rec_cons_read_sees(
/*==========================*/
	const rec_t*	rec,	/*!< in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/*!< in: clustered index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	ReadView*	view);	/*!< in: consistent read view */
/*********************************************************************//**
Checks that a non-clustered index record is seen in a consistent read.

NOTE that a non-clustered index page contains so little information on
its modifications that also in the case false, the present version of
rec may be the right, but we must check this from the clustered index
record.

@return true if certainly sees, or false if an earlier version of the
clustered index record might be needed */
bool
lock_sec_rec_cons_read_sees(
/*========================*/
	const rec_t*		rec,	/*!< in: user record which
					should be read or passed over
					by a read cursor */
	const dict_index_t*     index,  /*!< in: index */
	const ReadView*	view)	/*!< in: consistent read view */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Locks the specified database table in the mode given. If the lock cannot
be granted immediately, the query thread is put to wait.
@return DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
dberr_t
lock_table(
/*=======*/
	ulint		flags,	/*!< in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	dict_table_t*	table,	/*!< in/out: database table
				in dictionary cache */
	lock_mode	mode,	/*!< in: lock mode */
	que_thr_t*	thr)	/*!< in: query thread */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Creates a table IX lock object for a resurrected transaction. */
void
lock_table_ix_resurrect(
/*====================*/
	dict_table_t*	table,	/*!< in/out: table */
	trx_t*		trx);	/*!< in/out: transaction */

/** Sets a lock on a table based on the given mode.
@param[in]	table	table to lock
@param[in,out]	trx	transaction
@param[in]	mode	LOCK_X or LOCK_S
@return error code or DB_SUCCESS. */
dberr_t
lock_table_for_trx(
	dict_table_t*	table,
	trx_t*		trx,
	enum lock_mode	mode)
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/*************************************************************//**
Removes a granted record lock of a transaction from the queue and grants
locks to other transactions waiting in the queue if they now are entitled
to a lock. */
void
lock_rec_unlock(
/*============*/
	trx_t*			trx,	/*!< in/out: transaction that has
					set a record lock */
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record */
	lock_mode		lock_mode);/*!< in: LOCK_S or LOCK_X */
/*********************************************************************//**
Releases a transaction's locks, and releases possible other transactions
waiting because of these locks. Change the state of the transaction to
TRX_STATE_COMMITTED_IN_MEMORY. */
void
lock_trx_release_locks(
/*===================*/
	trx_t*	trx);	/*!< in/out: transaction */

/** Release read locks of a transacion. It is called during XA
prepare to release locks early.
@param[in,out]	trx		transaction
@param[in]	only_gap	release only GAP locks */
void
lock_trx_release_read_locks(
	trx_t*	trx,
	bool	only_gap);

/*********************************************************************//**
Removes locks on a table to be dropped or truncated.
If remove_also_table_sx_locks is TRUE then table-level S and X locks are
also removed in addition to other table-level and record-level locks.
No lock, that is going to be removed, is allowed to be a wait lock. */
void
lock_remove_all_on_table(
/*=====================*/
	dict_table_t*	table,			/*!< in: table to be dropped
						or truncated */
	ibool		remove_also_table_sx_locks);/*!< in: also removes
						table S and X locks */

/*********************************************************************//**
Calculates the fold value of a page file address: used in inserting or
searching for a lock in the hash table.
@return folded value */
UNIV_INLINE
ulint
lock_rec_fold(
/*==========*/
	ulint	space,	/*!< in: space */
	ulint	page_no)/*!< in: page number */
	MY_ATTRIBUTE((const));
/*********************************************************************//**
Calculates the hash value of a page file address: used in inserting or
searching for a lock in the hash table.
@return hashed value */
UNIV_INLINE
ulint
lock_rec_hash(
/*==========*/
	ulint	space,	/*!< in: space */
	ulint	page_no);/*!< in: page number */

/*************************************************************//**
Get the lock hash table */
UNIV_INLINE
hash_table_t*
lock_hash_get(
/*==========*/
	ulint	mode);	/*!< in: lock mode */

/**********************************************************************//**
Looks for a set bit in a record lock bitmap. Returns ULINT_UNDEFINED,
if none found.
@return bit index == heap number of the record, or ULINT_UNDEFINED if
none found */
ulint
lock_rec_find_set_bit(
/*==================*/
	const lock_t*	lock);	/*!< in: record lock with at least one
				bit set */

/*********************************************************************//**
Gets the source table of an ALTER TABLE transaction.  The table must be
covered by an IX or IS table lock.
@return the source table of transaction, if it is covered by an IX or
IS table lock; dest if there is no source table, and NULL if the
transaction is locking more than two tables or an inconsistency is
found */
dict_table_t*
lock_get_src_table(
/*===============*/
	trx_t*		trx,	/*!< in: transaction */
	dict_table_t*	dest,	/*!< in: destination of ALTER TABLE */
	lock_mode*	mode);	/*!< out: lock mode of the source table */
/*********************************************************************//**
Determine if the given table is exclusively "owned" by the given
transaction, i.e., transaction holds LOCK_IX and possibly LOCK_AUTO_INC
on the table.
@return TRUE if table is only locked by trx, with LOCK_IX, and
possibly LOCK_AUTO_INC */
ibool
lock_is_table_exclusive(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	const trx_t*		trx);	/*!< in: transaction */
/*********************************************************************//**
Checks if a lock request lock1 has to wait for request lock2.
@return TRUE if lock1 has to wait for lock2 to be removed */
ibool
lock_has_to_wait(
/*=============*/
	const lock_t*	lock1,	/*!< in: waiting lock */
	const lock_t*	lock2);	/*!< in: another lock; NOTE that it is
				assumed that this has a lock bit set
				on the same record as in lock1 if the
				locks are record locks */
/*********************************************************************//**
Reports that a transaction id is insensible, i.e., in the future. */
void
lock_report_trx_id_insanity(
/*========================*/
	trx_id_t	trx_id,		/*!< in: trx id */
	const rec_t*	rec,		/*!< in: user record */
	dict_index_t*	index,		/*!< in: index */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index) */
	trx_id_t	max_trx_id);	/*!< in: trx_sys_get_max_trx_id() */
/*********************************************************************//**
Prints info of a table lock. */
void
lock_table_print(
/*=============*/
	FILE*		file,	/*!< in: file where to print */
	const lock_t*	lock);	/*!< in: table type lock */
/*********************************************************************//**
Prints info of a record lock. */
void
lock_rec_print(
/*===========*/
	FILE*		file,	/*!< in: file where to print */
	const lock_t*	lock);	/*!< in: record type lock */
/*********************************************************************//**
Prints info of locks for all transactions.
@return FALSE if not able to obtain lock mutex and exits without
printing info */
ibool
lock_print_info_summary(
/*====================*/
	FILE*	file,	/*!< in: file where to print */
	ibool   nowait)	/*!< in: whether to wait for the lock mutex */
	MY_ATTRIBUTE((warn_unused_result));

/** Prints transaction lock wait and MVCC state.
@param[in,out]	file	file where to print
@param[in]	trx	transaction */
void
lock_trx_print_wait_and_mvcc_state(
	FILE*		file,
	const trx_t*	trx);

/*********************************************************************//**
Prints info of locks for each transaction. This function assumes that the
caller holds the lock mutex and more importantly it will release the lock
mutex on behalf of the caller. (This should be fixed in the future). */
void
lock_print_info_all_transactions(
/*=============================*/
	FILE*	file);	/*!< in: file where to print */
/*********************************************************************//**
Return approximate number or record locks (bits set in the bitmap) for
this transaction. Since delete-marked records may be removed, the
record count will not be precise.
The caller must be holding lock_sys->mutex. */
ulint
lock_number_of_rows_locked(
/*=======================*/
	const trx_lock_t*	trx_lock)	/*!< in: transaction locks */
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Return the number of table locks for a transaction.
The caller must be holding lock_sys->mutex. */
ulint
lock_number_of_tables_locked(
/*=========================*/
	const trx_lock_t*	trx_lock)	/*!< in: transaction locks */
	MY_ATTRIBUTE((warn_unused_result));

/*******************************************************************//**
Gets the type of a lock. Non-inline version for using outside of the
lock module.
@return LOCK_TABLE or LOCK_REC */
ulint
lock_get_type(
/*==========*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
Gets the id of the transaction owning a lock.
@return transaction id */
trx_id_t
lock_get_trx_id(
/*============*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
Gets the mode of a lock in a human readable string.
The string should not be free()'d or modified.
@return lock mode */
const char*
lock_get_mode_str(
/*==============*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
Gets the type of a lock in a human readable string.
The string should not be free()'d or modified.
@return lock type */
const char*
lock_get_type_str(
/*==============*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
Gets the id of the table on which the lock is.
@return id of the table */
table_id_t
lock_get_table_id(
/*==============*/
	const lock_t*	lock);	/*!< in: lock */

/** Determine which table a lock is associated with.
@param[in]	lock	the lock
@return name of the table */
const table_name_t&
lock_get_table_name(
	const lock_t*	lock);

/*******************************************************************//**
For a record lock, gets the index on which the lock is.
@return index */
const dict_index_t*
lock_rec_get_index(
/*===============*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
For a record lock, gets the name of the index on which the lock is.
The string should not be free()'d or modified.
@return name of the index */
const char*
lock_rec_get_index_name(
/*====================*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
For a record lock, gets the tablespace number on which the lock is.
@return tablespace number */
ulint
lock_rec_get_space_id(
/*==================*/
	const lock_t*	lock);	/*!< in: lock */

/*******************************************************************//**
For a record lock, gets the page number on which the lock is.
@return page number */
ulint
lock_rec_get_page_no(
/*=================*/
	const lock_t*	lock);	/*!< in: lock */
/*******************************************************************//**
Check if there are any locks (table or rec) against table.
@return TRUE if locks exist */
bool
lock_table_has_locks(
/*=================*/
	const dict_table_t*	table);	/*!< in: check if there are any locks
					held on records in this table or on the
					table itself */

/*********************************************************************//**
A thread which wakes up threads whose lock wait may have lasted too long.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(lock_wait_timeout_thread)(
/*=====================================*/
	void*	arg);	/*!< in: a dummy parameter required by
			os_thread_create */

/********************************************************************//**
Releases a user OS thread waiting for a lock to be released, if the
thread is already suspended. */
void
lock_wait_release_thread_if_suspended(
/*==================================*/
	que_thr_t*	thr);	/*!< in: query thread associated with the
				user OS thread	 */

/***************************************************************//**
Puts a user OS thread to wait for a lock to be released. If an error
occurs during the wait trx->error_state associated with thr is
!= DB_SUCCESS when we return. DB_LOCK_WAIT_TIMEOUT and DB_DEADLOCK
are possible errors. DB_DEADLOCK is returned if selective deadlock
resolution chose this transaction as a victim. */
void
lock_wait_suspend_thread(
/*=====================*/
	que_thr_t*	thr);	/*!< in: query thread associated with the
				user OS thread */
/*********************************************************************//**
Unlocks AUTO_INC type locks that were possibly reserved by a trx. This
function should be called at the the end of an SQL statement, by the
connection thread that owns the transaction (trx->mysql_thd). */
void
lock_unlock_table_autoinc(
/*======================*/
	trx_t*	trx);			/*!< in/out: transaction */
/*********************************************************************//**
Check whether the transaction has already been rolled back because it
was selected as a deadlock victim, or if it has to wait then cancel
the wait lock.
@return DB_DEADLOCK, DB_LOCK_WAIT or DB_SUCCESS */
dberr_t
lock_trx_handle_wait(
/*=================*/
	trx_t*	trx);	/*!< in/out: trx lock state */
/*********************************************************************//**
Get the number of locks on a table.
@return number of locks */
ulint
lock_table_get_n_locks(
/*===================*/
	const dict_table_t*	table);	/*!< in: table */
/*******************************************************************//**
Initialise the trx lock list. */
void
lock_trx_lock_list_init(
/*====================*/
	trx_lock_list_t*	lock_list);	/*!< List to initialise */

/*******************************************************************//**
Set the lock system timeout event. */
void
lock_set_timeout_event();
/*====================*/
#ifdef UNIV_DEBUG
/*********************************************************************//**
Checks that a transaction id is sensible, i.e., not in the future.
@return true if ok */
bool
lock_check_trx_id_sanity(
/*=====================*/
	trx_id_t	trx_id,		/*!< in: trx id */
	const rec_t*	rec,		/*!< in: user record */
	dict_index_t*	index,		/*!< in: index */
	const ulint*	offsets)	/*!< in: rec_get_offsets(rec, index) */
	MY_ATTRIBUTE((warn_unused_result));
/*******************************************************************//**
Check if the transaction holds any locks on the sys tables
or its records.
@return the strongest lock found on any sys table or 0 for none */
const lock_t*
lock_trx_has_sys_table_locks(
/*=========================*/
	const trx_t*	trx)	/*!< in: transaction to check */
	MY_ATTRIBUTE((warn_unused_result));

/*******************************************************************//**
Check if the transaction holds an exclusive lock on a record.
@return whether the locks are held */
bool
lock_trx_has_rec_x_lock(
/*====================*/
	const trx_t*		trx,	/*!< in: transaction to check */
	const dict_table_t*	table,	/*!< in: table to check */
	const buf_block_t*	block,	/*!< in: buffer block of the record */
	ulint			heap_no)/*!< in: record heap number */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */

/**
Allocate cached locks for the transaction.
@param trx		allocate cached record locks for this transaction */
void
lock_trx_alloc_locks(trx_t* trx);

/** Lock modes and types */
/* @{ */
#define LOCK_MODE_MASK	0xFUL	/*!< mask used to extract mode from the
				type_mode field in a lock */
/** Lock types */
/* @{ */
#define LOCK_TABLE	16	/*!< table lock */
#define	LOCK_REC	32	/*!< record lock */
#define LOCK_TYPE_MASK	0xF0UL	/*!< mask used to extract lock type from the
				type_mode field in a lock */
#if LOCK_MODE_MASK & LOCK_TYPE_MASK
# error "LOCK_MODE_MASK & LOCK_TYPE_MASK"
#endif

#define LOCK_WAIT	256	/*!< Waiting lock flag; when set, it
				means that the lock has not yet been
				granted, it is just waiting for its
				turn in the wait queue */
/* Precise modes */
#define LOCK_ORDINARY	0	/*!< this flag denotes an ordinary
				next-key lock in contrast to LOCK_GAP
				or LOCK_REC_NOT_GAP */
#define LOCK_GAP	512	/*!< when this bit is set, it means that the
				lock holds only on the gap before the record;
				for instance, an x-lock on the gap does not
				give permission to modify the record on which
				the bit is set; locks of this type are created
				when records are removed from the index chain
				of records */
#define LOCK_REC_NOT_GAP 1024	/*!< this bit means that the lock is only on
				the index record and does NOT block inserts
				to the gap before the index record; this is
				used in the case when we retrieve a record
				with a unique key, and is also used in
				locking plain SELECTs (not part of UPDATE
				or DELETE) when the user has set the READ
				COMMITTED isolation level */
#define LOCK_INSERT_INTENTION 2048 /*!< this bit is set when we place a waiting
				gap type record lock request in order to let
				an insert of an index record to wait until
				there are no conflicting locks by other
				transactions on the gap; note that this flag
				remains set when the waiting lock is granted,
				or if the lock is inherited to a neighboring
				record */
#define LOCK_PREDICATE	8192	/*!< Predicate lock */
#define LOCK_PRDT_PAGE	16384	/*!< Page lock */


#if (LOCK_WAIT|LOCK_GAP|LOCK_REC_NOT_GAP|LOCK_INSERT_INTENTION|LOCK_PREDICATE|LOCK_PRDT_PAGE)&LOCK_MODE_MASK
# error
#endif
#if (LOCK_WAIT|LOCK_GAP|LOCK_REC_NOT_GAP|LOCK_INSERT_INTENTION|LOCK_PREDICATE|LOCK_PRDT_PAGE)&LOCK_TYPE_MASK
# error
#endif
/* @} */

/** Lock operation struct */
struct lock_op_t{
	dict_table_t*	table;	/*!< table to be locked */
	lock_mode	mode;	/*!< lock mode */
};

typedef ib_mutex_t LockMutex;

/** The lock system struct */
struct lock_sys_t{
	char		pad1[CACHE_LINE_SIZE];	/*!< padding to prevent other
						memory update hotspots from
						residing on the same memory
						cache line */
	LockMutex	mutex;			/*!< Mutex protecting the
						locks */
	hash_table_t*	rec_hash;		/*!< hash table of the record
						locks */
	hash_table_t*	prdt_hash;		/*!< hash table of the predicate
						lock */
	hash_table_t*	prdt_page_hash;		/*!< hash table of the page
						lock */

	char		pad2[CACHE_LINE_SIZE];	/*!< Padding */
	LockMutex	wait_mutex;		/*!< Mutex protecting the
						next two fields */
	srv_slot_t*	waiting_threads;	/*!< Array  of user threads
						suspended while waiting for
						locks within InnoDB, protected
						by the lock_sys->wait_mutex */
	srv_slot_t*	last_slot;		/*!< highest slot ever used
						in the waiting_threads array,
						protected by
						lock_sys->wait_mutex */
	ibool		rollback_complete;
						/*!< TRUE if rollback of all
						recovered transactions is
						complete. Protected by
						lock_sys->mutex */

	ulint		n_lock_max_wait_time;	/*!< Max wait time */

	os_event_t	timeout_event;		/*!< Set to the event that is
						created in the lock wait monitor
						thread. A value of 0 means the
						thread is not active */

	bool		timeout_thread_active;	/*!< True if the timeout thread
						is running */
};

/*************************************************************//**
Removes a record lock request, waiting or granted, from the queue. */
void
lock_rec_discard(
/*=============*/
	lock_t*		in_lock);	/*!< in: record lock object: all
					record locks which are contained
					in this lock object are removed */

/*************************************************************//**
Moves the explicit locks on user records to another page if a record
list start is moved to another page. */
void
lock_rtr_move_rec_list(
/*===================*/
	const buf_block_t*	new_block,	/*!< in: index page to
						move to */
	const buf_block_t*	block,		/*!< in: index page */
	rtr_rec_move_t*		rec_move,	/*!< in: recording records
						moved */
	ulint			num_move);	/*!< in: num of rec to move */

/*************************************************************//**
Removes record lock objects set on an index page which is discarded. This
function does not move locks, or check for waiting locks, therefore the
lock bitmaps must already be reset when this function is called. */
void
lock_rec_free_all_from_discard_page(
/*================================*/
	const buf_block_t*	block);		/*!< in: page to be discarded */

/** Reset the nth bit of a record lock.
@param[in,out]	lock record lock
@param[in] i	index of the bit that will be reset
@param[in] type	whether the lock is in wait mode  */
void
lock_rec_trx_wait(
	lock_t*		lock,
	ulint		i,
	ulint		type);

/** The lock system */
extern lock_sys_t*	lock_sys;

/** Test if lock_sys->mutex can be acquired without waiting. */
#define lock_mutex_enter_nowait() 		\
	(lock_sys->mutex.trylock(__FILE__, __LINE__))

/** Test if lock_sys->mutex is owned. */
#define lock_mutex_own() (lock_sys->mutex.is_owned())

/** Acquire the lock_sys->mutex. */
#define lock_mutex_enter() do {			\
	mutex_enter(&lock_sys->mutex);		\
} while (0)

/** Release the lock_sys->mutex. */
#define lock_mutex_exit() do {			\
	lock_sys->mutex.exit();			\
} while (0)

/** Test if lock_sys->wait_mutex is owned. */
#define lock_wait_mutex_own() (lock_sys->wait_mutex.is_owned())

/** Acquire the lock_sys->wait_mutex. */
#define lock_wait_mutex_enter() do {		\
	mutex_enter(&lock_sys->wait_mutex);	\
} while (0)

/** Release the lock_sys->wait_mutex. */
#define lock_wait_mutex_exit() do {		\
	lock_sys->wait_mutex.exit();		\
} while (0)

#ifndef UNIV_NONINL
#include "lock0lock.ic"
#endif

#endif
