/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file lock/lock0lock.cc
The transaction lock system

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include "lock0lock.h"
#include "lock0priv.h"

#ifdef UNIV_NONINL
#include "lock0lock.ic"
#include "lock0priv.ic"
#endif

#include "ha_prototypes.h"
#include "usr0sess.h"
#include "trx0purge.h"
#include "dict0mem.h"
#include "dict0boot.h"
#include "trx0sys.h"
#include "pars0pars.h" /* pars_complete_graph_for_exec() */
#include "que0que.h" /* que_node_get_parent() */
#include "row0mysql.h" /* row_mysql_handle_errors() */
#include "row0sel.h" /* sel_node_create(), sel_node_t */
#include "row0types.h" /* sel_node_t */
#include "srv0mon.h"
#include "ut0vec.h"
#include "btr0btr.h"
#include "dict0boot.h"

/* Restricts the length of search we will do in the waits-for
graph of transactions */
#define LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK 1000000

/* Restricts the search depth we will do in the waits-for graph of
transactions */
#define LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK 200

/* When releasing transaction locks, this specifies how often we release
the lock mutex for a moment to give also others access to it */

#define LOCK_RELEASE_INTERVAL		1000

/* Safety margin when creating a new record lock: this many extra records
can be inserted to the page without need to create a lock with a bigger
bitmap */

#define LOCK_PAGE_BITMAP_MARGIN		64

/* An explicit record lock affects both the record and the gap before it.
An implicit x-lock does not affect the gap, it only locks the index
record from read or update.

If a transaction has modified or inserted an index record, then
it owns an implicit x-lock on the record. On a secondary index record,
a transaction has an implicit x-lock also if it has modified the
clustered index record, the max trx id of the page where the secondary
index record resides is >= trx id of the transaction (or database recovery
is running), and there are no explicit non-gap lock requests on the
secondary index record.

This complicated definition for a secondary index comes from the
implementation: we want to be able to determine if a secondary index
record has an implicit x-lock, just by looking at the present clustered
index record, not at the historical versions of the record. The
complicated definition can be explained to the user so that there is
nondeterminism in the access path when a query is answered: we may,
or may not, access the clustered index record and thus may, or may not,
bump into an x-lock set there.

Different transaction can have conflicting locks set on the gap at the
same time. The locks on the gap are purely inhibitive: an insert cannot
be made, or a select cursor may have to wait if a different transaction
has a conflicting lock on the gap. An x-lock on the gap does not give
the right to insert into the gap.

An explicit lock can be placed on a user record or the supremum record of
a page. The locks on the supremum record are always thought to be of the gap
type, though the gap bit is not set. When we perform an update of a record
where the size of the record changes, we may temporarily store its explicit
locks on the infimum record of the page, though the infimum otherwise never
carries locks.

A waiting record lock can also be of the gap type. A waiting lock request
can be granted when there is no conflicting mode lock request by another
transaction ahead of it in the explicit lock queue.

In version 4.0.5 we added yet another explicit lock type: LOCK_REC_NOT_GAP.
It only locks the record it is placed on, not the gap before the record.
This lock type is necessary to emulate an Oracle-like READ COMMITTED isolation
level.

-------------------------------------------------------------------------
RULE 1: If there is an implicit x-lock on a record, and there are non-gap
-------
lock requests waiting in the queue, then the transaction holding the implicit
x-lock also has an explicit non-gap record x-lock. Therefore, as locks are
released, we can grant locks to waiting lock requests purely by looking at
the explicit lock requests in the queue.

RULE 3: Different transactions cannot have conflicting granted non-gap locks
-------
on a record at the same time. However, they can have conflicting granted gap
locks.
RULE 4: If a there is a waiting lock request in a queue, no lock request,
-------
gap or not, can be inserted ahead of it in the queue. In record deletes
and page splits new gap type locks can be created by the database manager
for a transaction, and without rule 4, the waits-for graph of transactions
might become cyclic without the database noticing it, as the deadlock check
is only performed when a transaction itself requests a lock!
-------------------------------------------------------------------------

An insert is allowed to a gap if there are no explicit lock requests by
other transactions on the next record. It does not matter if these lock
requests are granted or waiting, gap bit set or not, with the exception
that a gap type request set by another transaction to wait for
its turn to do an insert is ignored. On the other hand, an
implicit x-lock by another transaction does not prevent an insert, which
allows for more concurrency when using an Oracle-style sequence number
generator for the primary key with many transactions doing inserts
concurrently.

A modify of a record is allowed if the transaction has an x-lock on the
record, or if other transactions do not have any non-gap lock requests on the
record.

A read of a single user record with a cursor is allowed if the transaction
has a non-gap explicit, or an implicit lock on the record, or if the other
transactions have no x-lock requests on the record. At a page supremum a
read is always allowed.

In summary, an implicit lock is seen as a granted x-lock only on the
record, not on the gap. An explicit lock with no gap bit set is a lock
both on the record and the gap. If the gap bit is set, the lock is only
on the gap. Different transaction cannot own conflicting locks on the
record at the same time, but they may own conflicting locks on the gap.
Granted locks on a record give an access right to the record, but gap type
locks just inhibit operations.

NOTE: Finding out if some transaction has an implicit x-lock on a secondary
index record can be cumbersome. We may have to look at previous versions of
the corresponding clustered index record to find out if a delete marked
secondary index record was delete marked by an active transaction, not by
a committed one.

FACT A: If a transaction has inserted a row, it can delete it any time
without need to wait for locks.

PROOF: The transaction has an implicit x-lock on every index record inserted
for the row, and can thus modify each record without the need to wait. Q.E.D.

FACT B: If a transaction has read some result set with a cursor, it can read
it again, and retrieves the same result set, if it has not modified the
result set in the meantime. Hence, there is no phantom problem. If the
biggest record, in the alphabetical order, touched by the cursor is removed,
a lock wait may occur, otherwise not.

PROOF: When a read cursor proceeds, it sets an s-lock on each user record
it passes, and a gap type s-lock on each page supremum. The cursor must
wait until it has these locks granted. Then no other transaction can
have a granted x-lock on any of the user records, and therefore cannot
modify the user records. Neither can any other transaction insert into
the gaps which were passed over by the cursor. Page splits and merges,
and removal of obsolete versions of records do not affect this, because
when a user record or a page supremum is removed, the next record inherits
its locks as gap type locks, and therefore blocks inserts to the same gap.
Also, if a page supremum is inserted, it inherits its locks from the successor
record. When the cursor is positioned again at the start of the result set,
the records it will touch on its course are either records it touched
during the last pass or new inserted page supremums. It can immediately
access all these records, and when it arrives at the biggest record, it
notices that the result set is complete. If the biggest record was removed,
lock wait can occur because the next record only inherits a gap type lock,
and a wait may be needed. Q.E.D. */

/* If an index record should be changed or a new inserted, we must check
the lock on the record or the next. When a read cursor starts reading,
we will set a record level s-lock on each record it passes, except on the
initial record on which the cursor is positioned before we start to fetch
records. Our index tree search has the convention that the B-tree
cursor is positioned BEFORE the first possibly matching record in
the search. Optimizations are possible here: if the record is searched
on an equality condition to a unique key, we could actually set a special
lock on the record, a lock which would not prevent any insert before
this record. In the next key locking an x-lock set on a record also
prevents inserts just before that record.
	There are special infimum and supremum records on each page.
A supremum record can be locked by a read cursor. This records cannot be
updated but the lock prevents insert of a user record to the end of
the page.
	Next key locks will prevent the phantom problem where new rows
could appear to SELECT result sets after the select operation has been
performed. Prevention of phantoms ensures the serilizability of
transactions.
	What should we check if an insert of a new record is wanted?
Only the lock on the next record on the same page, because also the
supremum record can carry a lock. An s-lock prevents insertion, but
what about an x-lock? If it was set by a searched update, then there
is implicitly an s-lock, too, and the insert should be prevented.
What if our transaction owns an x-lock to the next record, but there is
a waiting s-lock request on the next record? If this s-lock was placed
by a read cursor moving in the ascending order in the index, we cannot
do the insert immediately, because when we finally commit our transaction,
the read cursor should see also the new inserted record. So we should
move the read cursor backward from the next record for it to pass over
the new inserted record. This move backward may be too cumbersome to
implement. If we in this situation just enqueue a second x-lock request
for our transaction on the next record, then the deadlock mechanism
notices a deadlock between our transaction and the s-lock request
transaction. This seems to be an ok solution.
	We could have the convention that granted explicit record locks,
lock the corresponding records from changing, and also lock the gaps
before them from inserting. A waiting explicit lock request locks the gap
before from inserting. Implicit record x-locks, which we derive from the
transaction id in the clustered index record, only lock the record itself
from modification, not the gap before it from inserting.
	How should we store update locks? If the search is done by a unique
key, we could just modify the record trx id. Otherwise, we could put a record
x-lock on the record. If the update changes ordering fields of the
clustered index record, the inserted new record needs no record lock in
lock table, the trx id is enough. The same holds for a secondary index
record. Searched delete is similar to update.

PROBLEM:
What about waiting lock requests? If a transaction is waiting to make an
update to a record which another modified, how does the other transaction
know to send the end-lock-wait signal to the waiting transaction? If we have
the convention that a transaction may wait for just one lock at a time, how
do we preserve it if lock wait ends?

PROBLEM:
Checking the trx id label of a secondary index record. In the case of a
modification, not an insert, is this necessary? A secondary index record
is modified only by setting or resetting its deleted flag. A secondary index
record contains fields to uniquely determine the corresponding clustered
index record. A secondary index record is therefore only modified if we
also modify the clustered index record, and the trx id checking is done
on the clustered index record, before we come to modify the secondary index
record. So, in the case of delete marking or unmarking a secondary index
record, we do not have to care about trx ids, only the locks in the lock
table must be checked. In the case of a select from a secondary index, the
trx id is relevant, and in this case we may have to search the clustered
index record.

PROBLEM: How to update record locks when page is split or merged, or
--------------------------------------------------------------------
a record is deleted or updated?
If the size of fields in a record changes, we perform the update by
a delete followed by an insert. How can we retain the locks set or
waiting on the record? Because a record lock is indexed in the bitmap
by the heap number of the record, when we remove the record from the
record list, it is possible still to keep the lock bits. If the page
is reorganized, we could make a table of old and new heap numbers,
and permute the bitmaps in the locks accordingly. We can add to the
table a row telling where the updated record ended. If the update does
not require a reorganization of the page, we can simply move the lock
bits for the updated record to the position determined by its new heap
number (we may have to allocate a new lock, if we run out of the bitmap
in the old one).
	A more complicated case is the one where the reinsertion of the
updated record is done pessimistically, because the structure of the
tree may change.

PROBLEM: If a supremum record is removed in a page merge, or a record
---------------------------------------------------------------------
removed in a purge, what to do to the waiting lock requests? In a split to
the right, we just move the lock requests to the new supremum. If a record
is removed, we could move the waiting lock request to its inheritor, the
next record in the index. But, the next record may already have lock
requests on its own queue. A new deadlock check should be made then. Maybe
it is easier just to release the waiting transactions. They can then enqueue
new lock requests on appropriate records.

PROBLEM: When a record is inserted, what locks should it inherit from the
-------------------------------------------------------------------------
upper neighbor? An insert of a new supremum record in a page split is
always possible, but an insert of a new user record requires that the upper
neighbor does not have any lock requests by other transactions, granted or
waiting, in its lock queue. Solution: We can copy the locks as gap type
locks, so that also the waiting locks are transformed to granted gap type
locks on the inserted record. */

#define LOCK_STACK_SIZE		OS_THREAD_MAX_N

/* LOCK COMPATIBILITY MATRIX
 *    IS IX S  X  AI
 * IS +	 +  +  -  +
 * IX +	 +  -  -  +
 * S  +	 -  +  -  -
 * X  -	 -  -  -  -
 * AI +	 +  -  -  -
 *
 * Note that for rows, InnoDB only acquires S or X locks.
 * For tables, InnoDB normally acquires IS or IX locks.
 * S or X table locks are only acquired for LOCK TABLES.
 * Auto-increment (AI) locks are needed because of
 * statement-level MySQL binlog.
 * See also lock_mode_compatible().
 */
static const byte lock_compatibility_matrix[5][5] = {
 /**         IS     IX       S     X       AI */
 /* IS */ {  TRUE,  TRUE,  TRUE,  FALSE,  TRUE},
 /* IX */ {  TRUE,  TRUE,  FALSE, FALSE,  TRUE},
 /* S  */ {  TRUE,  FALSE, TRUE,  FALSE,  FALSE},
 /* X  */ {  FALSE, FALSE, FALSE, FALSE,  FALSE},
 /* AI */ {  TRUE,  TRUE,  FALSE, FALSE,  FALSE}
};

/* STRONGER-OR-EQUAL RELATION (mode1=row, mode2=column)
 *    IS IX S  X  AI
 * IS +  -  -  -  -
 * IX +  +  -  -  -
 * S  +  -  +  -  -
 * X  +  +  +  +  +
 * AI -  -  -  -  +
 * See lock_mode_stronger_or_eq().
 */
static const byte lock_strength_matrix[5][5] = {
 /**         IS     IX       S     X       AI */
 /* IS */ {  TRUE,  FALSE, FALSE,  FALSE, FALSE},
 /* IX */ {  TRUE,  TRUE,  FALSE, FALSE,  FALSE},
 /* S  */ {  TRUE,  FALSE, TRUE,  FALSE,  FALSE},
 /* X  */ {  TRUE,  TRUE,  TRUE,  TRUE,   TRUE},
 /* AI */ {  FALSE, FALSE, FALSE, FALSE,  TRUE}
};

/** Deadlock check context. */
struct lock_deadlock_ctx_t {
	const trx_t*	start;		/*!< Joining transaction that is
					requesting a lock in an incompatible
					mode */

	const lock_t*	wait_lock;	/*!< Lock that trx wants */

	ib_uint64_t	mark_start;	/*!<  Value of lock_mark_count at
					the start of the deadlock check. */

	ulint		depth;		/*!< Stack depth */

	ulint		cost;		/*!< Calculation steps thus far */

	ibool		too_deep;	/*!< TRUE if search was too deep and
					was aborted */
};

/** DFS visited node information used during deadlock checking. */
struct lock_stack_t {
	const lock_t*	lock;			/*!< Current lock */
	const lock_t*	wait_lock;		/*!< Waiting for lock */
	unsigned	heap_no:16;		/*!< heap number if rec lock */
};

/** Stack to use during DFS search. Currently only a single stack is required
because there is no parallel deadlock check. This stack is protected by
the lock_sys_t::mutex. */
static lock_stack_t*	lock_stack;

/** The count of the types of locks. */
static const ulint	lock_types = UT_ARR_SIZE(lock_compatibility_matrix);

#ifdef UNIV_PFS_MUTEX
/* Key to register mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	lock_sys_mutex_key;
/* Key to register mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	lock_sys_wait_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_DEBUG
UNIV_INTERN ibool	lock_print_waits	= FALSE;

/*********************************************************************//**
Validates the lock system.
@return	TRUE if ok */
static
bool
lock_validate();
/*============*/

/*********************************************************************//**
Validates the record lock queues on a page.
@return	TRUE if ok */
static
ibool
lock_rec_validate_page(
/*===================*/
	const buf_block_t*	block)	/*!< in: buffer block */
	__attribute__((nonnull, warn_unused_result));
#endif /* UNIV_DEBUG */

/* The lock system */
UNIV_INTERN lock_sys_t*	lock_sys	= NULL;

/** We store info on the latest deadlock error to this buffer. InnoDB
Monitor will then fetch it and print */
UNIV_INTERN ibool	lock_deadlock_found = FALSE;
/** Only created if !srv_read_only_mode */
static FILE*		lock_latest_err_file;

/********************************************************************//**
Checks if a joining lock request results in a deadlock. If a deadlock is
found this function will resolve the dadlock by choosing a victim transaction
and rolling it back. It will attempt to resolve all deadlocks. The returned
transaction id will be the joining transaction id or 0 if some other
transaction was chosen as a victim and rolled back or no deadlock found.

@return id of transaction chosen as victim or 0 */
static
trx_id_t
lock_deadlock_check_and_resolve(
/*===========================*/
	const lock_t*	lock,	/*!< in: lock the transaction is requesting */
	const trx_t*	trx);	/*!< in: transaction */

/*********************************************************************//**
Gets the nth bit of a record lock.
@return	TRUE if bit set also if i == ULINT_UNDEFINED return FALSE*/
UNIV_INLINE
ibool
lock_rec_get_nth_bit(
/*=================*/
	const lock_t*	lock,	/*!< in: record lock */
	ulint		i)	/*!< in: index of the bit */
{
	const byte*	b;

	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	if (i >= lock->un_member.rec_lock.n_bits) {

		return(FALSE);
	}

	b = ((const byte*) &lock[1]) + (i / 8);

	return(1 & *b >> (i % 8));
}

/*********************************************************************//**
Reports that a transaction id is insensible, i.e., in the future. */
UNIV_INTERN
void
lock_report_trx_id_insanity(
/*========================*/
	trx_id_t	trx_id,		/*!< in: trx id */
	const rec_t*	rec,		/*!< in: user record */
	dict_index_t*	index,		/*!< in: index */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index) */
	trx_id_t	max_trx_id)	/*!< in: trx_sys_get_max_trx_id() */
{
	ut_print_timestamp(stderr);
	fputs("  InnoDB: Error: transaction id associated with record\n",
	      stderr);
	rec_print_new(stderr, rec, offsets);
	fputs("InnoDB: in ", stderr);
	dict_index_name_print(stderr, NULL, index);
	fprintf(stderr, "\n"
		"InnoDB: is " TRX_ID_FMT " which is higher than the"
		" global trx id counter " TRX_ID_FMT "!\n"
		"InnoDB: The table is corrupt. You have to do"
		" dump + drop + reimport.\n",
		trx_id, max_trx_id);
}

/*********************************************************************//**
Checks that a transaction id is sensible, i.e., not in the future.
@return	true if ok */
#ifdef UNIV_DEBUG
UNIV_INTERN
#else
static __attribute__((nonnull, warn_unused_result))
#endif
bool
lock_check_trx_id_sanity(
/*=====================*/
	trx_id_t	trx_id,		/*!< in: trx id */
	const rec_t*	rec,		/*!< in: user record */
	dict_index_t*	index,		/*!< in: index */
	const ulint*	offsets)	/*!< in: rec_get_offsets(rec, index) */
{
	bool		is_ok;
	trx_id_t	max_trx_id;

	ut_ad(rec_offs_validate(rec, index, offsets));

	max_trx_id = trx_sys_get_max_trx_id();
	is_ok = trx_id < max_trx_id;

	if (UNIV_UNLIKELY(!is_ok)) {
		lock_report_trx_id_insanity(trx_id,
					    rec, index, offsets, max_trx_id);
	}

	return(is_ok);
}

/*********************************************************************//**
Checks that a record is seen in a consistent read.
@return true if sees, or false if an earlier version of the record
should be retrieved */
UNIV_INTERN
bool
lock_clust_rec_cons_read_sees(
/*==========================*/
	const rec_t*	rec,	/*!< in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/*!< in: clustered index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	read_view_t*	view)	/*!< in: consistent read view */
{
	trx_id_t	trx_id;

	ut_ad(dict_index_is_clust(index));
	ut_ad(page_rec_is_user_rec(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));

	/* NOTE that we call this function while holding the search
	system latch. */

	trx_id = row_get_rec_trx_id(rec, index, offsets);

	return(read_view_sees_trx_id(view, trx_id));
}

/*********************************************************************//**
Checks that a non-clustered index record is seen in a consistent read.

NOTE that a non-clustered index page contains so little information on
its modifications that also in the case false, the present version of
rec may be the right, but we must check this from the clustered index
record.

@return true if certainly sees, or false if an earlier version of the
clustered index record might be needed */
UNIV_INTERN
bool
lock_sec_rec_cons_read_sees(
/*========================*/
	const rec_t*		rec,	/*!< in: user record which
					should be read or passed over
					by a read cursor */
	const read_view_t*	view)	/*!< in: consistent read view */
{
	trx_id_t	max_trx_id;

	ut_ad(page_rec_is_user_rec(rec));

	/* NOTE that we might call this function while holding the search
	system latch. */

	if (recv_recovery_is_on()) {

		return(false);
	}

	max_trx_id = page_get_max_trx_id(page_align(rec));
	ut_ad(max_trx_id);

	return(max_trx_id < view->up_limit_id);
}

/*********************************************************************//**
Creates the lock system at database start. */
UNIV_INTERN
void
lock_sys_create(
/*============*/
	ulint	n_cells)	/*!< in: number of slots in lock hash table */
{
	ulint	lock_sys_sz;

	lock_sys_sz = sizeof(*lock_sys)
		+ OS_THREAD_MAX_N * sizeof(srv_slot_t);

	lock_sys = static_cast<lock_sys_t*>(mem_zalloc(lock_sys_sz));

	lock_stack = static_cast<lock_stack_t*>(
		mem_zalloc(sizeof(*lock_stack) * LOCK_STACK_SIZE));

	void*	ptr = &lock_sys[1];

	lock_sys->waiting_threads = static_cast<srv_slot_t*>(ptr);

	lock_sys->last_slot = lock_sys->waiting_threads;

	mutex_create(lock_sys_mutex_key, &lock_sys->mutex, SYNC_LOCK_SYS);

	mutex_create(lock_sys_wait_mutex_key,
		     &lock_sys->wait_mutex, SYNC_LOCK_WAIT_SYS);

	lock_sys->timeout_event = os_event_create();

	lock_sys->rec_hash = hash_create(n_cells);

	if (!srv_read_only_mode) {
		lock_latest_err_file = os_file_create_tmpfile();
		ut_a(lock_latest_err_file);
	}
}

/*********************************************************************//**
Closes the lock system at database shutdown. */
UNIV_INTERN
void
lock_sys_close(void)
/*================*/
{
	if (lock_latest_err_file != NULL) {
		fclose(lock_latest_err_file);
		lock_latest_err_file = NULL;
	}

	hash_table_free(lock_sys->rec_hash);

	mutex_free(&lock_sys->mutex);
	mutex_free(&lock_sys->wait_mutex);

	mem_free(lock_stack);
	mem_free(lock_sys);

	lock_sys = NULL;
	lock_stack = NULL;
}

/*********************************************************************//**
Gets the size of a lock struct.
@return	size in bytes */
UNIV_INTERN
ulint
lock_get_size(void)
/*===============*/
{
	return((ulint) sizeof(lock_t));
}

/*********************************************************************//**
Gets the mode of a lock.
@return	mode */
UNIV_INLINE
enum lock_mode
lock_get_mode(
/*==========*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_ad(lock);

	return(static_cast<enum lock_mode>(lock->type_mode & LOCK_MODE_MASK));
}

/*********************************************************************//**
Gets the wait flag of a lock.
@return	LOCK_WAIT if waiting, 0 if not */
UNIV_INLINE
ulint
lock_get_wait(
/*==========*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_ad(lock);

	return(lock->type_mode & LOCK_WAIT);
}

/*********************************************************************//**
Gets the source table of an ALTER TABLE transaction.  The table must be
covered by an IX or IS table lock.
@return the source table of transaction, if it is covered by an IX or
IS table lock; dest if there is no source table, and NULL if the
transaction is locking more than two tables or an inconsistency is
found */
UNIV_INTERN
dict_table_t*
lock_get_src_table(
/*===============*/
	trx_t*		trx,	/*!< in: transaction */
	dict_table_t*	dest,	/*!< in: destination of ALTER TABLE */
	enum lock_mode*	mode)	/*!< out: lock mode of the source table */
{
	dict_table_t*	src;
	lock_t*		lock;

	ut_ad(!lock_mutex_own());

	src = NULL;
	*mode = LOCK_NONE;

	/* The trx mutex protects the trx_locks for our purposes.
	Other transactions could want to convert one of our implicit
	record locks to an explicit one. For that, they would need our
	trx mutex. Waiting locks can be removed while only holding
	lock_sys->mutex, but this is a running transaction and cannot
	thus be holding any waiting locks. */
	trx_mutex_enter(trx);

	for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
	     lock != NULL;
	     lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
		lock_table_t*	tab_lock;
		enum lock_mode	lock_mode;
		if (!(lock_get_type_low(lock) & LOCK_TABLE)) {
			/* We are only interested in table locks. */
			continue;
		}
		tab_lock = &lock->un_member.tab_lock;
		if (dest == tab_lock->table) {
			/* We are not interested in the destination table. */
			continue;
		} else if (!src) {
			/* This presumably is the source table. */
			src = tab_lock->table;
			if (UT_LIST_GET_LEN(src->locks) != 1
			    || UT_LIST_GET_FIRST(src->locks) != lock) {
				/* We only support the case when
				there is only one lock on this table. */
				src = NULL;
				goto func_exit;
			}
		} else if (src != tab_lock->table) {
			/* The transaction is locking more than
			two tables (src and dest): abort */
			src = NULL;
			goto func_exit;
		}

		/* Check that the source table is locked by
		LOCK_IX or LOCK_IS. */
		lock_mode = lock_get_mode(lock);
		if (lock_mode == LOCK_IX || lock_mode == LOCK_IS) {
			if (*mode != LOCK_NONE && *mode != lock_mode) {
				/* There are multiple locks on src. */
				src = NULL;
				goto func_exit;
			}
			*mode = lock_mode;
		}
	}

	if (!src) {
		/* No source table lock found: flag the situation to caller */
		src = dest;
	}

func_exit:
	trx_mutex_exit(trx);
	return(src);
}

/*********************************************************************//**
Determine if the given table is exclusively "owned" by the given
transaction, i.e., transaction holds LOCK_IX and possibly LOCK_AUTO_INC
on the table.
@return TRUE if table is only locked by trx, with LOCK_IX, and
possibly LOCK_AUTO_INC */
UNIV_INTERN
ibool
lock_is_table_exclusive(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	const trx_t*		trx)	/*!< in: transaction */
{
	const lock_t*	lock;
	ibool		ok	= FALSE;

	ut_ad(table);
	ut_ad(trx);

	lock_mutex_enter();

	for (lock = UT_LIST_GET_FIRST(table->locks);
	     lock != NULL;
	     lock = UT_LIST_GET_NEXT(locks, &lock->un_member.tab_lock)) {
		if (lock->trx != trx) {
			/* A lock on the table is held
			by some other transaction. */
			goto not_ok;
		}

		if (!(lock_get_type_low(lock) & LOCK_TABLE)) {
			/* We are interested in table locks only. */
			continue;
		}

		switch (lock_get_mode(lock)) {
		case LOCK_IX:
			ok = TRUE;
			break;
		case LOCK_AUTO_INC:
			/* It is allowed for trx to hold an
			auto_increment lock. */
			break;
		default:
not_ok:
			/* Other table locks than LOCK_IX are not allowed. */
			ok = FALSE;
			goto func_exit;
		}
	}

func_exit:
	lock_mutex_exit();

	return(ok);
}

/*********************************************************************//**
Sets the wait flag of a lock and the back pointer in trx to lock. */
UNIV_INLINE
void
lock_set_lock_and_trx_wait(
/*=======================*/
	lock_t*	lock,	/*!< in: lock */
	trx_t*	trx)	/*!< in/out: trx */
{
	ut_ad(lock);
	ut_ad(lock->trx == trx);
	ut_ad(trx->lock.wait_lock == NULL);
	ut_ad(lock_mutex_own());
	ut_ad(trx_mutex_own(trx));

	trx->lock.wait_lock = lock;
	lock->type_mode |= LOCK_WAIT;
}

/**********************************************************************//**
The back pointer to a waiting lock request in the transaction is set to NULL
and the wait bit in lock type_mode is reset. */
UNIV_INLINE
void
lock_reset_lock_and_trx_wait(
/*=========================*/
	lock_t*	lock)	/*!< in/out: record lock */
{
	ut_ad(lock_get_wait(lock));
	ut_ad(lock_mutex_own());

	/* Reset the back pointer in trx to this waiting lock request */
	if (!(lock->type_mode & LOCK_CONV_BY_OTHER)) {
		ut_ad(lock->trx->lock.wait_lock == lock);
		lock->trx->lock.wait_lock = NULL;
	} else {
		ut_ad(lock_get_type_low(lock) == LOCK_REC);
	}
	lock->type_mode &= ~LOCK_WAIT;
}

/*********************************************************************//**
Gets the gap flag of a record lock.
@return	LOCK_GAP or 0 */
UNIV_INLINE
ulint
lock_rec_get_gap(
/*=============*/
	const lock_t*	lock)	/*!< in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	return(lock->type_mode & LOCK_GAP);
}

/*********************************************************************//**
Gets the LOCK_REC_NOT_GAP flag of a record lock.
@return	LOCK_REC_NOT_GAP or 0 */
UNIV_INLINE
ulint
lock_rec_get_rec_not_gap(
/*=====================*/
	const lock_t*	lock)	/*!< in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	return(lock->type_mode & LOCK_REC_NOT_GAP);
}

/*********************************************************************//**
Gets the waiting insert flag of a record lock.
@return	LOCK_INSERT_INTENTION or 0 */
UNIV_INLINE
ulint
lock_rec_get_insert_intention(
/*==========================*/
	const lock_t*	lock)	/*!< in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	return(lock->type_mode & LOCK_INSERT_INTENTION);
}

/*********************************************************************//**
Calculates if lock mode 1 is stronger or equal to lock mode 2.
@return	nonzero if mode1 stronger or equal to mode2 */
UNIV_INLINE
ulint
lock_mode_stronger_or_eq(
/*=====================*/
	enum lock_mode	mode1,	/*!< in: lock mode */
	enum lock_mode	mode2)	/*!< in: lock mode */
{
	ut_ad((ulint) mode1 < lock_types);
	ut_ad((ulint) mode2 < lock_types);

	return(lock_strength_matrix[mode1][mode2]);
}

/*********************************************************************//**
Calculates if lock mode 1 is compatible with lock mode 2.
@return	nonzero if mode1 compatible with mode2 */
UNIV_INLINE
ulint
lock_mode_compatible(
/*=================*/
	enum lock_mode	mode1,	/*!< in: lock mode */
	enum lock_mode	mode2)	/*!< in: lock mode */
{
	ut_ad((ulint) mode1 < lock_types);
	ut_ad((ulint) mode2 < lock_types);

	return(lock_compatibility_matrix[mode1][mode2]);
}

/*********************************************************************//**
Checks if a lock request for a new lock has to wait for request lock2.
@return	TRUE if new lock has to wait for lock2 to be removed */
UNIV_INLINE
ibool
lock_rec_has_to_wait(
/*=================*/
	const trx_t*	trx,	/*!< in: trx of new lock */
	ulint		type_mode,/*!< in: precise mode of the new lock
				to set: LOCK_S or LOCK_X, possibly
				ORed to LOCK_GAP or LOCK_REC_NOT_GAP,
				LOCK_INSERT_INTENTION */
	const lock_t*	lock2,	/*!< in: another record lock; NOTE that
				it is assumed that this has a lock bit
				set on the same record as in the new
				lock we are setting */
	ibool lock_is_on_supremum)  /*!< in: TRUE if we are setting the
				lock on the 'supremum' record of an
				index page: we know then that the lock
				request is really for a 'gap' type lock */
{
	ut_ad(trx && lock2);
	ut_ad(lock_get_type_low(lock2) == LOCK_REC);

	if (trx != lock2->trx
	    && !lock_mode_compatible(static_cast<enum lock_mode>(
			             LOCK_MODE_MASK & type_mode),
				     lock_get_mode(lock2))) {

		/* We have somewhat complex rules when gap type record locks
		cause waits */

		if ((lock_is_on_supremum || (type_mode & LOCK_GAP))
		    && !(type_mode & LOCK_INSERT_INTENTION)) {

			/* Gap type locks without LOCK_INSERT_INTENTION flag
			do not need to wait for anything. This is because
			different users can have conflicting lock types
			on gaps. */

			return(FALSE);
		}

		if (!(type_mode & LOCK_INSERT_INTENTION)
		    && lock_rec_get_gap(lock2)) {

			/* Record lock (LOCK_ORDINARY or LOCK_REC_NOT_GAP
			does not need to wait for a gap type lock */

			return(FALSE);
		}

		if ((type_mode & LOCK_GAP)
		    && lock_rec_get_rec_not_gap(lock2)) {

			/* Lock on gap does not need to wait for
			a LOCK_REC_NOT_GAP type lock */

			return(FALSE);
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

			return(FALSE);
		}

		return(TRUE);
	}

	return(FALSE);
}

/*********************************************************************//**
Checks if a lock request lock1 has to wait for request lock2.
@return	TRUE if lock1 has to wait for lock2 to be removed */
UNIV_INTERN
ibool
lock_has_to_wait(
/*=============*/
	const lock_t*	lock1,	/*!< in: waiting lock */
	const lock_t*	lock2)	/*!< in: another lock; NOTE that it is
				assumed that this has a lock bit set
				on the same record as in lock1 if the
				locks are record locks */
{
	ut_ad(lock1 && lock2);

	if (lock1->trx != lock2->trx
	    && !lock_mode_compatible(lock_get_mode(lock1),
				     lock_get_mode(lock2))) {
		if (lock_get_type_low(lock1) == LOCK_REC) {
			ut_ad(lock_get_type_low(lock2) == LOCK_REC);

			/* If this lock request is for a supremum record
			then the second bit on the lock bitmap is set */

			return(lock_rec_has_to_wait(lock1->trx,
						    lock1->type_mode, lock2,
						    lock_rec_get_nth_bit(
							    lock1, 1)));
		}

		return(TRUE);
	}

	return(FALSE);
}

/*============== RECORD LOCK BASIC FUNCTIONS ============================*/

/*********************************************************************//**
Gets the number of bits in a record lock bitmap.
@return	number of bits */
UNIV_INLINE
ulint
lock_rec_get_n_bits(
/*================*/
	const lock_t*	lock)	/*!< in: record lock */
{
	return(lock->un_member.rec_lock.n_bits);
}

/**********************************************************************//**
Sets the nth bit of a record lock to TRUE. */
UNIV_INLINE
void
lock_rec_set_nth_bit(
/*=================*/
	lock_t*	lock,	/*!< in: record lock */
	ulint	i)	/*!< in: index of the bit */
{
	ulint	byte_index;
	ulint	bit_index;

	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);
	ut_ad(i < lock->un_member.rec_lock.n_bits);

	byte_index = i / 8;
	bit_index = i % 8;

	((byte*) &lock[1])[byte_index] |= 1 << bit_index;
}

/**********************************************************************//**
Looks for a set bit in a record lock bitmap. Returns ULINT_UNDEFINED,
if none found.
@return bit index == heap number of the record, or ULINT_UNDEFINED if
none found */
UNIV_INTERN
ulint
lock_rec_find_set_bit(
/*==================*/
	const lock_t*	lock)	/*!< in: record lock with at least one bit set */
{
	ulint	i;

	for (i = 0; i < lock_rec_get_n_bits(lock); i++) {

		if (lock_rec_get_nth_bit(lock, i)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Resets the nth bit of a record lock. */
UNIV_INLINE
void
lock_rec_reset_nth_bit(
/*===================*/
	lock_t*	lock,	/*!< in: record lock */
	ulint	i)	/*!< in: index of the bit which must be set to TRUE
			when this function is called */
{
	ulint	byte_index;
	ulint	bit_index;

	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);
	ut_ad(i < lock->un_member.rec_lock.n_bits);

	byte_index = i / 8;
	bit_index = i % 8;

	((byte*) &lock[1])[byte_index] &= ~(1 << bit_index);
}

/*********************************************************************//**
Gets the first or next record lock on a page.
@return	next lock, NULL if none exists */
UNIV_INLINE
const lock_t*
lock_rec_get_next_on_page_const(
/*============================*/
	const lock_t*	lock)	/*!< in: a record lock */
{
	ulint	space;
	ulint	page_no;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	space = lock->un_member.rec_lock.space;
	page_no = lock->un_member.rec_lock.page_no;

	for (;;) {
		lock = static_cast<const lock_t*>(HASH_GET_NEXT(hash, lock));

		if (!lock) {

			break;
		}

		if ((lock->un_member.rec_lock.space == space)
		    && (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}
	}

	return(lock);
}

/*********************************************************************//**
Gets the first or next record lock on a page.
@return	next lock, NULL if none exists */
UNIV_INLINE
lock_t*
lock_rec_get_next_on_page(
/*======================*/
	lock_t*	lock)	/*!< in: a record lock */
{
	return((lock_t*) lock_rec_get_next_on_page_const(lock));
}

/*********************************************************************//**
Gets the first record lock on a page, where the page is identified by its
file address.
@return	first lock, NULL if none exists */
UNIV_INLINE
lock_t*
lock_rec_get_first_on_page_addr(
/*============================*/
	ulint	space,	/*!< in: space */
	ulint	page_no)/*!< in: page number */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());

	for (lock = static_cast<lock_t*>(
			HASH_GET_FIRST(lock_sys->rec_hash,
				       lock_rec_hash(space, page_no)));
	      lock != NULL;
	      lock = static_cast<lock_t*>(HASH_GET_NEXT(hash, lock))) {

		if (lock->un_member.rec_lock.space == space
		    && lock->un_member.rec_lock.page_no == page_no) {

			break;
		}
	}

	return(lock);
}

/*********************************************************************//**
Determines if there are explicit record locks on a page.
@return	an explicit record lock on the page, or NULL if there are none */
UNIV_INTERN
lock_t*
lock_rec_expl_exist_on_page(
/*========================*/
	ulint	space,	/*!< in: space id */
	ulint	page_no)/*!< in: page number */
{
	lock_t*	lock;

	lock_mutex_enter();
	lock = lock_rec_get_first_on_page_addr(space, page_no);
	lock_mutex_exit();

	return(lock);
}

/*********************************************************************//**
Gets the first record lock on a page, where the page is identified by a
pointer to it.
@return	first lock, NULL if none exists */
UNIV_INLINE
lock_t*
lock_rec_get_first_on_page(
/*=======================*/
	const buf_block_t*	block)	/*!< in: buffer block */
{
	ulint	hash;
	lock_t*	lock;
	ulint	space	= buf_block_get_space(block);
	ulint	page_no	= buf_block_get_page_no(block);

	ut_ad(lock_mutex_own());

	hash = buf_block_get_lock_hash_val(block);

	for (lock = static_cast<lock_t*>(
			HASH_GET_FIRST( lock_sys->rec_hash, hash));
	     lock != NULL;
	     lock = static_cast<lock_t*>(HASH_GET_NEXT(hash, lock))) {

		if ((lock->un_member.rec_lock.space == space)
		    && (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}
	}

	return(lock);
}

/*********************************************************************//**
Gets the next explicit lock request on a record.
@return	next lock, NULL if none exists or if heap_no == ULINT_UNDEFINED */
UNIV_INLINE
lock_t*
lock_rec_get_next(
/*==============*/
	ulint	heap_no,/*!< in: heap number of the record */
	lock_t*	lock)	/*!< in: lock */
{
	ut_ad(lock_mutex_own());

	do {
		ut_ad(lock_get_type_low(lock) == LOCK_REC);
		lock = lock_rec_get_next_on_page(lock);
	} while (lock && !lock_rec_get_nth_bit(lock, heap_no));

	return(lock);
}

/*********************************************************************//**
Gets the next explicit lock request on a record.
@return	next lock, NULL if none exists or if heap_no == ULINT_UNDEFINED */
UNIV_INLINE
const lock_t*
lock_rec_get_next_const(
/*====================*/
	ulint		heap_no,/*!< in: heap number of the record */
	const lock_t*	lock)	/*!< in: lock */
{
	return(lock_rec_get_next(heap_no, (lock_t*) lock));
}

/*********************************************************************//**
Gets the first explicit lock request on a record.
@return	first lock, NULL if none exists */
UNIV_INLINE
lock_t*
lock_rec_get_first(
/*===============*/
	const buf_block_t*	block,	/*!< in: block containing the record */
	ulint			heap_no)/*!< in: heap number of the record */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());

	for (lock = lock_rec_get_first_on_page(block); lock;
	     lock = lock_rec_get_next_on_page(lock)) {
		if (lock_rec_get_nth_bit(lock, heap_no)) {
			break;
		}
	}

	return(lock);
}

/*********************************************************************//**
Resets the record lock bitmap to zero. NOTE: does not touch the wait_lock
pointer in the transaction! This function is used in lock object creation
and resetting. */
static
void
lock_rec_bitmap_reset(
/*==================*/
	lock_t*	lock)	/*!< in: record lock */
{
	ulint	n_bytes;

	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	/* Reset to zero the bitmap which resides immediately after the lock
	struct */

	n_bytes = lock_rec_get_n_bits(lock) / 8;

	ut_ad((lock_rec_get_n_bits(lock) % 8) == 0);

	memset(&lock[1], 0, n_bytes);
}

/*********************************************************************//**
Copies a record lock to heap.
@return	copy of lock */
static
lock_t*
lock_rec_copy(
/*==========*/
	const lock_t*	lock,	/*!< in: record lock */
	mem_heap_t*	heap)	/*!< in: memory heap */
{
	ulint	size;

	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	size = sizeof(lock_t) + lock_rec_get_n_bits(lock) / 8;

	return(static_cast<lock_t*>(mem_heap_dup(heap, lock, size)));
}

/*********************************************************************//**
Gets the previous record lock set on a record.
@return	previous lock on the same record, NULL if none exists */
UNIV_INTERN
const lock_t*
lock_rec_get_prev(
/*==============*/
	const lock_t*	in_lock,/*!< in: record lock */
	ulint		heap_no)/*!< in: heap number of the record */
{
	lock_t*	lock;
	ulint	space;
	ulint	page_no;
	lock_t*	found_lock	= NULL;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	for (lock = lock_rec_get_first_on_page_addr(space, page_no);
	     /* No op */;
	     lock = lock_rec_get_next_on_page(lock)) {

		ut_ad(lock);

		if (lock == in_lock) {

			return(found_lock);
		}

		if (lock_rec_get_nth_bit(lock, heap_no)) {

			found_lock = lock;
		}
	}
}

/*============= FUNCTIONS FOR ANALYZING TABLE LOCK QUEUE ================*/

/*********************************************************************//**
Checks if a transaction has the specified table lock, or stronger. This
function should only be called by the thread that owns the transaction.
@return	lock or NULL */
UNIV_INLINE
const lock_t*
lock_table_has(
/*===========*/
	const trx_t*		trx,	/*!< in: transaction */
	const dict_table_t*	table,	/*!< in: table */
	enum lock_mode		mode)	/*!< in: lock mode */
{
	lint			i;

	if (ib_vector_is_empty(trx->lock.table_locks)) {
		return(NULL);
	}

	/* Look for stronger locks the same trx already has on the table */

	for (i = ib_vector_size(trx->lock.table_locks) - 1; i >= 0; --i) {
		const lock_t*	lock;
		enum lock_mode	lock_mode;

		lock = *static_cast<const lock_t**>(
			ib_vector_get(trx->lock.table_locks, i));

		if (lock == NULL) {
			continue;
		}

		lock_mode = lock_get_mode(lock);

		ut_ad(trx == lock->trx);
		ut_ad(lock_get_type_low(lock) & LOCK_TABLE);
		ut_ad(lock->un_member.tab_lock.table != NULL);

		if (table == lock->un_member.tab_lock.table
		    && lock_mode_stronger_or_eq(lock_mode, mode)) {

			ut_ad(!lock_get_wait(lock));

			return(lock);
		}
	}

	return(NULL);
}

/*============= FUNCTIONS FOR ANALYZING RECORD LOCK QUEUE ================*/

/*********************************************************************//**
Checks if a transaction has a GRANTED explicit lock on rec stronger or equal
to precise_mode.
@return	lock or NULL */
UNIV_INLINE
lock_t*
lock_rec_has_expl(
/*==============*/
	ulint			precise_mode,/*!< in: LOCK_S or LOCK_X
					possibly ORed to LOCK_GAP or
					LOCK_REC_NOT_GAP, for a
					supremum record we regard this
					always a gap type request */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	const trx_t*		trx)	/*!< in: transaction */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());
	ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S
	      || (precise_mode & LOCK_MODE_MASK) == LOCK_X);
	ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {

		if (lock->trx == trx
		    && !lock_is_wait_not_by_other(lock->type_mode)
		    && lock_mode_stronger_or_eq(
			    lock_get_mode(lock),
			    static_cast<enum lock_mode>(
				    precise_mode & LOCK_MODE_MASK))
		    && (!lock_rec_get_rec_not_gap(lock)
			|| (precise_mode & LOCK_REC_NOT_GAP)
			|| heap_no == PAGE_HEAP_NO_SUPREMUM)
		    && (!lock_rec_get_gap(lock)
			|| (precise_mode & LOCK_GAP)
			|| heap_no == PAGE_HEAP_NO_SUPREMUM)
		    && (!lock_rec_get_insert_intention(lock))) {

			return(lock);
		}
	}

	return(NULL);
}

#ifdef UNIV_DEBUG
/*********************************************************************//**
Checks if some other transaction has a lock request in the queue.
@return	lock or NULL */
static
const lock_t*
lock_rec_other_has_expl_req(
/*========================*/
	enum lock_mode		mode,	/*!< in: LOCK_S or LOCK_X */
	ulint			gap,	/*!< in: LOCK_GAP if also gap
					locks are taken into account,
					or 0 if not */
	ulint			wait,	/*!< in: LOCK_WAIT if also
					waiting locks are taken into
					account, or 0 if not */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	const trx_t*		trx)	/*!< in: transaction, or NULL if
					requests by all transactions
					are taken into account */
{
	const lock_t*	lock;

	ut_ad(lock_mutex_own());
	ut_ad(mode == LOCK_X || mode == LOCK_S);
	ut_ad(gap == 0 || gap == LOCK_GAP);
	ut_ad(wait == 0 || wait == LOCK_WAIT);

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next_const(heap_no, lock)) {

		if (lock->trx != trx
		    && (gap
			|| !(lock_rec_get_gap(lock)
			     || heap_no == PAGE_HEAP_NO_SUPREMUM))
		    && (wait || !lock_get_wait(lock))
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)) {

			return(lock);
		}
	}

	return(NULL);
}
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Checks if some other transaction has a conflicting explicit lock request
in the queue, so that we have to wait.
@return	lock or NULL */
static
const lock_t*
lock_rec_other_has_conflicting(
/*===========================*/
	enum lock_mode		mode,	/*!< in: LOCK_S or LOCK_X,
					possibly ORed to LOCK_GAP or
					LOC_REC_NOT_GAP,
					LOCK_INSERT_INTENTION */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	const trx_t*		trx)	/*!< in: our transaction */
{
	const lock_t*		lock;
	ibool			is_supremum;

	ut_ad(lock_mutex_own());

	is_supremum = (heap_no == PAGE_HEAP_NO_SUPREMUM);

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next_const(heap_no, lock)) {

		if (lock_rec_has_to_wait(trx, mode, lock, is_supremum)) {
			return(lock);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Looks for a suitable type record lock struct by the same trx on the same page.
This can be used to save space when a new record lock should be set on a page:
no new struct is needed, if a suitable old is found.
@return	lock or NULL */
UNIV_INLINE
lock_t*
lock_rec_find_similar_on_page(
/*==========================*/
	ulint		type_mode,	/*!< in: lock type_mode field */
	ulint		heap_no,	/*!< in: heap number of the record */
	lock_t*		lock,		/*!< in: lock_rec_get_first_on_page() */
	const trx_t*	trx)		/*!< in: transaction */
{
	ut_ad(lock_mutex_own());

	for (/* No op */;
	     lock != NULL;
	     lock = lock_rec_get_next_on_page(lock)) {

		if (lock->trx == trx
		    && lock->type_mode == type_mode
		    && lock_rec_get_n_bits(lock) > heap_no) {

			return(lock);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Checks if some transaction has an implicit x-lock on a record in a secondary
index.
@return	transaction id of the transaction which has the x-lock, or 0;
NOTE that this function can return false positives but never false
negatives. The caller must confirm all positive results by calling
trx_is_active(). */
static
trx_id_t
lock_sec_rec_some_has_impl(
/*=======================*/
	const rec_t*	rec,	/*!< in: user record */
	dict_index_t*	index,	/*!< in: secondary index */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec, index) */
{
	trx_id_t	trx_id;
	trx_id_t	max_trx_id;
	const page_t*	page = page_align(rec);

	ut_ad(!lock_mutex_own());
	ut_ad(!mutex_own(&trx_sys->mutex));
	ut_ad(!dict_index_is_clust(index));
	ut_ad(page_rec_is_user_rec(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));

	max_trx_id = page_get_max_trx_id(page);

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list, or
	database recovery is running. We do not write the changes of a page
	max trx id to the log, and therefore during recovery, this value
	for a page may be incorrect. */

	if (max_trx_id < trx_rw_min_trx_id() && !recv_recovery_is_on()) {

		trx_id = 0;

	} else if (!lock_check_trx_id_sanity(max_trx_id, rec, index, offsets)) {

		buf_page_print(page, 0, 0);

		/* The page is corrupt: try to avoid a crash by returning 0 */
		trx_id = 0;

	/* In this case it is possible that some transaction has an implicit
	x-lock. We have to look in the clustered index. */

	} else {
		trx_id = row_vers_impl_x_locked(rec, index, offsets);
	}

	return(trx_id);
}

/*********************************************************************//**
Return approximate number or record locks (bits set in the bitmap) for
this transaction. Since delete-marked records may be removed, the
record count will not be precise.
The caller must be holding lock_sys->mutex. */
UNIV_INTERN
ulint
lock_number_of_rows_locked(
/*=======================*/
	const trx_lock_t*	trx_lock)	/*!< in: transaction locks */
{
	const lock_t*	lock;
	ulint		n_records = 0;

	ut_ad(lock_mutex_own());

	for (lock = UT_LIST_GET_FIRST(trx_lock->trx_locks);
	     lock != NULL;
	     lock = UT_LIST_GET_NEXT(trx_locks, lock)) {

		if (lock_get_type_low(lock) == LOCK_REC) {
			ulint	n_bit;
			ulint	n_bits = lock_rec_get_n_bits(lock);

			for (n_bit = 0; n_bit < n_bits; n_bit++) {
				if (lock_rec_get_nth_bit(lock, n_bit)) {
					n_records++;
				}
			}
		}
	}

	return(n_records);
}

/*============== RECORD LOCK CREATION AND QUEUE MANAGEMENT =============*/

/*********************************************************************//**
Creates a new record lock and inserts it to the lock queue. Does NOT check
for deadlocks or lock compatibility!
@return	created lock */
static
lock_t*
lock_rec_create(
/*============*/
	ulint			type_mode,/*!< in: lock mode and wait
					flag, type is ignored and
					replaced by LOCK_REC */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	dict_index_t*		index,	/*!< in: index of record */
	trx_t*			trx,	/*!< in/out: transaction */
	ibool			caller_owns_trx_mutex)
					/*!< in: TRUE if caller owns
					trx mutex */
{
	lock_t*		lock;
	ulint		page_no;
	ulint		space;
	ulint		n_bits;
	ulint		n_bytes;
	const page_t*	page;

	ut_ad(lock_mutex_own());
	ut_ad(caller_owns_trx_mutex == trx_mutex_own(trx));
	ut_ad(dict_index_is_clust(index) || !dict_index_is_online_ddl(index));

	/* Non-locking autocommit read-only transactions should not set
	any locks. */
	assert_trx_in_list(trx);

	space = buf_block_get_space(block);
	page_no	= buf_block_get_page_no(block);
	page = block->frame;

	btr_assert_not_corrupted(block, index);

	/* If rec is the supremum record, then we reset the gap and
	LOCK_REC_NOT_GAP bits, as all locks on the supremum are
	automatically of the gap type */

	if (UNIV_UNLIKELY(heap_no == PAGE_HEAP_NO_SUPREMUM)) {
		ut_ad(!(type_mode & LOCK_REC_NOT_GAP));

		type_mode = type_mode & ~(LOCK_GAP | LOCK_REC_NOT_GAP);
	}

	/* Make lock bitmap bigger by a safety margin */
	n_bits = page_dir_get_n_heap(page) + LOCK_PAGE_BITMAP_MARGIN;
	n_bytes = 1 + n_bits / 8;

	lock = static_cast<lock_t*>(
		mem_heap_alloc(trx->lock.lock_heap, sizeof(lock_t) + n_bytes));

	lock->trx = trx;

	lock->type_mode = (type_mode & ~LOCK_TYPE_MASK) | LOCK_REC;
	lock->index = index;

	lock->un_member.rec_lock.space = space;
	lock->un_member.rec_lock.page_no = page_no;
	lock->un_member.rec_lock.n_bits = n_bytes * 8;

	/* Reset to zero the bitmap which resides immediately after the
	lock struct */

	lock_rec_bitmap_reset(lock);

	/* Set the bit corresponding to rec */
	lock_rec_set_nth_bit(lock, heap_no);

	index->table->n_rec_locks++;

	ut_ad(index->table->n_ref_count > 0 || !index->table->can_be_evicted);

	HASH_INSERT(lock_t, hash, lock_sys->rec_hash,
		    lock_rec_fold(space, page_no), lock);

	if (!caller_owns_trx_mutex) {
		trx_mutex_enter(trx);
	}
	ut_ad(trx_mutex_own(trx));

	if (lock_is_wait_not_by_other(type_mode)) {

		lock_set_lock_and_trx_wait(lock, trx);
	}

	UT_LIST_ADD_LAST(trx_locks, trx->lock.trx_locks, lock);

	if (!caller_owns_trx_mutex) {
		trx_mutex_exit(trx);
	}

	MONITOR_INC(MONITOR_RECLOCK_CREATED);
	MONITOR_INC(MONITOR_NUM_RECLOCK);

	return(lock);
}

/*********************************************************************//**
Enqueues a waiting request for a lock which cannot be granted immediately.
Checks for deadlocks.
@return DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED, or
DB_SUCCESS_LOCKED_REC; DB_SUCCESS_LOCKED_REC means that
there was a deadlock, but another transaction was chosen as a victim,
and we got the lock immediately: no need to wait then */
static
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
	lock_t*			lock,	/*!< in: lock object; NULL if a new
					one should be created. */
	dict_index_t*		index,	/*!< in: index of record */
	que_thr_t*		thr)	/*!< in: query thread */
{
	trx_t*			trx;
	trx_id_t		victim_trx_id;

	ut_ad(lock_mutex_own());
	ut_ad(dict_index_is_clust(index) || !dict_index_is_online_ddl(index));

	trx = thr_get_trx(thr);

	ut_ad(trx_mutex_own(trx));

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (que_thr_stop(thr)) {
		ut_error;

		return(DB_QUE_THR_SUSPENDED);
	}

	switch (trx_get_dict_operation(trx)) {
	case TRX_DICT_OP_NONE:
		break;
	case TRX_DICT_OP_TABLE:
	case TRX_DICT_OP_INDEX:
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: a record lock wait happens"
		      " in a dictionary operation!\n"
		      "InnoDB: ", stderr);
		dict_index_name_print(stderr, trx, index);
		fputs(".\n"
		      "InnoDB: Submit a detailed bug report"
		      " to http://bugs.mysql.com\n",
		      stderr);
		ut_ad(0);
	}

	if (lock == NULL) {
		/* Enqueue the lock request that will wait
		to be granted, note that we already own
		the trx mutex. */
		lock = lock_rec_create(
			type_mode | LOCK_WAIT, block, heap_no,
			index, trx, TRUE);
	} else {
		ut_ad(lock->type_mode & LOCK_WAIT);
		ut_ad(lock->type_mode & LOCK_CONV_BY_OTHER);

		lock->type_mode &= ~LOCK_CONV_BY_OTHER;
		lock_set_lock_and_trx_wait(lock, trx);
	}

	/* Release the mutex to obey the latching order.
	This is safe, because lock_deadlock_check_and_resolve()
	is invoked when a lock wait is enqueued for the currently
	running transaction. Because trx is a running transaction
	(it is not currently suspended because of a lock wait),
	its state can only be changed by this thread, which is
	currently associated with the transaction. */

	trx_mutex_exit(trx);

	victim_trx_id = lock_deadlock_check_and_resolve(lock, trx);

	trx_mutex_enter(trx);

	if (victim_trx_id != 0) {

		ut_ad(victim_trx_id == trx->id);

		lock_reset_lock_and_trx_wait(lock);
		lock_rec_reset_nth_bit(lock, heap_no);

		return(DB_DEADLOCK);

	} else if (trx->lock.wait_lock == NULL) {

		/* If there was a deadlock but we chose another
		transaction as a victim, it is possible that we
		already have the lock now granted! */

		return(DB_SUCCESS_LOCKED_REC);
	}

	trx->lock.que_state = TRX_QUE_LOCK_WAIT;

	trx->lock.was_chosen_as_deadlock_victim = FALSE;
	trx->lock.wait_started = ut_time();

	ut_a(que_thr_stop(thr));

#ifdef UNIV_DEBUG
	if (lock_print_waits) {
		fprintf(stderr, "Lock wait for trx " TRX_ID_FMT " in index ",
			trx->id);
		ut_print_name(stderr, trx, FALSE, index->name);
	}
#endif /* UNIV_DEBUG */

	MONITOR_INC(MONITOR_LOCKREC_WAIT);

	return(DB_LOCK_WAIT);
}

/*********************************************************************//**
Adds a record lock request in the record queue. The request is normally
added as the last in the queue, but if there are no waiting lock requests
on the record, and the request to be added is not a waiting request, we
can reuse a suitable record lock object already existing on the same page,
just setting the appropriate bit in its bitmap. This is a low-level function
which does NOT check for deadlocks or lock compatibility!
@return	lock where the bit was set */
static
lock_t*
lock_rec_add_to_queue(
/*==================*/
	ulint			type_mode,/*!< in: lock mode, wait, gap
					etc. flags; type is ignored
					and replaced by LOCK_REC */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	dict_index_t*		index,	/*!< in: index of record */
	trx_t*			trx,	/*!< in/out: transaction */
	ibool			caller_owns_trx_mutex)
					/*!< in: TRUE if caller owns the
					transaction mutex */
{
	lock_t*	lock;
	lock_t*	first_lock;

	ut_ad(lock_mutex_own());
	ut_ad(caller_owns_trx_mutex == trx_mutex_own(trx));
	ut_ad(dict_index_is_clust(index) || !dict_index_is_online_ddl(index));
#ifdef UNIV_DEBUG
	switch (type_mode & LOCK_MODE_MASK) {
	case LOCK_X:
	case LOCK_S:
		break;
	default:
		ut_error;
	}

	if (!(type_mode & (LOCK_WAIT | LOCK_GAP))) {
		enum lock_mode	mode = (type_mode & LOCK_MODE_MASK) == LOCK_S
			? LOCK_X
			: LOCK_S;
		const lock_t*	other_lock
			= lock_rec_other_has_expl_req(mode, 0, LOCK_WAIT,
						      block, heap_no, trx);
		ut_a(!other_lock);
	}
#endif /* UNIV_DEBUG */

	type_mode |= LOCK_REC;

	/* If rec is the supremum record, then we can reset the gap bit, as
	all locks on the supremum are automatically of the gap type, and we
	try to avoid unnecessary memory consumption of a new record lock
	struct for a gap type lock */

	if (UNIV_UNLIKELY(heap_no == PAGE_HEAP_NO_SUPREMUM)) {
		ut_ad(!(type_mode & LOCK_REC_NOT_GAP));

		/* There should never be LOCK_REC_NOT_GAP on a supremum
		record, but let us play safe */

		type_mode = type_mode & ~(LOCK_GAP | LOCK_REC_NOT_GAP);
	}

	/* Look for a waiting lock request on the same record or on a gap */

	for (first_lock = lock = lock_rec_get_first_on_page(block);
	     lock != NULL;
	     lock = lock_rec_get_next_on_page(lock)) {

		if (lock_get_wait(lock)
		    && lock_rec_get_nth_bit(lock, heap_no)) {

			goto somebody_waits;
		}
	}

	if (UNIV_LIKELY(!(type_mode & LOCK_WAIT))) {

		/* Look for a similar record lock on the same page:
		if one is found and there are no waiting lock requests,
		we can just set the bit */

		lock = lock_rec_find_similar_on_page(
			type_mode, heap_no, first_lock, trx);

		if (lock) {

			lock_rec_set_nth_bit(lock, heap_no);

			return(lock);
		}
	}

somebody_waits:
	return(lock_rec_create(
			type_mode, block, heap_no, index, trx,
			caller_owns_trx_mutex));
}

/** Record locking request status */
enum lock_rec_req_status {
	/** Failed to acquire a lock */
	LOCK_REC_FAIL,
	/** Succeeded in acquiring a lock (implicit or already acquired) */
	LOCK_REC_SUCCESS,
	/** Explicitly created a new lock */
	LOCK_REC_SUCCESS_CREATED
};

/*********************************************************************//**
This is a fast routine for locking a record in the most common cases:
there are no explicit locks on the page, or there is just one lock, owned
by this transaction, and of the right type_mode. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. This function sets a normal next-key lock, or in the case of
a page supremum record, a gap type lock.
@return whether the locking succeeded */
UNIV_INLINE
enum lock_rec_req_status
lock_rec_lock_fast(
/*===============*/
	ibool			impl,	/*!< in: if TRUE, no lock is set
					if no wait is necessary: we
					assume that the caller will
					set an implicit lock */
	ulint			mode,	/*!< in: lock mode: LOCK_X or
					LOCK_S possibly ORed to either
					LOCK_GAP or LOCK_REC_NOT_GAP */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of record */
	dict_index_t*		index,	/*!< in: index of record */
	que_thr_t*		thr)	/*!< in: query thread */
{
	lock_t*			lock;
	trx_t*			trx;
	enum lock_rec_req_status status = LOCK_REC_SUCCESS;

	ut_ad(lock_mutex_own());
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
	      || (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
	      || mode - (LOCK_MODE_MASK & mode) == 0
	      || mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);
	ut_ad(dict_index_is_clust(index) || !dict_index_is_online_ddl(index));

	lock = lock_rec_get_first_on_page(block);

	trx = thr_get_trx(thr);

	if (lock == NULL) {
		if (!impl) {
			/* Note that we don't own the trx mutex. */
			lock = lock_rec_create(
				mode, block, heap_no, index, trx, FALSE);

		}
		status = LOCK_REC_SUCCESS_CREATED;
	} else {
		trx_mutex_enter(trx);

		if (lock_rec_get_next_on_page(lock)
		     || lock->trx != trx
		     || lock->type_mode != (mode | LOCK_REC)
		     || lock_rec_get_n_bits(lock) <= heap_no) {

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

	return(status);
}

/*********************************************************************//**
This is the general, and slower, routine for locking a record. This is a
low-level function which does NOT look at implicit locks! Checks lock
compatibility within explicit locks. This function sets a normal next-key
lock, or in the case of a page supremum record, a gap type lock.
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
static
dberr_t
lock_rec_lock_slow(
/*===============*/
	ibool			impl,	/*!< in: if TRUE, no lock is set
					if no wait is necessary: we
					assume that the caller will
					set an implicit lock */
	ulint			mode,	/*!< in: lock mode: LOCK_X or
					LOCK_S possibly ORed to either
					LOCK_GAP or LOCK_REC_NOT_GAP */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of record */
	dict_index_t*		index,	/*!< in: index of record */
	que_thr_t*		thr)	/*!< in: query thread */
{
	trx_t*			trx;
	lock_t*			lock;
	dberr_t			err = DB_SUCCESS;

	ut_ad(lock_mutex_own());
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
	      || (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
	      || mode - (LOCK_MODE_MASK & mode) == 0
	      || mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);
	ut_ad(dict_index_is_clust(index) || !dict_index_is_online_ddl(index));

	trx = thr_get_trx(thr);

	trx_mutex_enter(trx);

	lock = lock_rec_has_expl(mode, block, heap_no, trx);
	if (lock) {
		if (lock->type_mode & LOCK_CONV_BY_OTHER) {
			/* This lock or lock waiting was created by the other
			transaction, not by the transaction (trx) itself.
			So, the transaction (trx) should treat it collectly
			according as whether granted or not. */

			if (lock->type_mode & LOCK_WAIT) {
				/* This lock request was not granted yet.
				Should wait for granted. */

				goto enqueue_waiting;
			} else {
				/* This lock request was already granted.
				Just clearing the flag. */

				lock->type_mode &= ~LOCK_CONV_BY_OTHER;
			}
		}

		/* The trx already has a strong enough lock on rec: do
		nothing */

	} else if (lock_rec_other_has_conflicting(
			static_cast<enum lock_mode>(mode),
			block, heap_no, trx)) {

		/* If another transaction has a non-gap conflicting
		request in the queue, as this transaction does not
		have a lock strong enough already granted on the
		record, we have to wait. */

		ut_ad(lock == NULL);
enqueue_waiting:
		err = lock_rec_enqueue_waiting(
			mode, block, heap_no, lock, index, thr);

	} else if (!impl) {
		/* Set the requested lock on the record, note that
		we already own the transaction mutex. */

		lock_rec_add_to_queue(
			LOCK_REC | mode, block, heap_no, index, trx, TRUE);

		err = DB_SUCCESS_LOCKED_REC;
	}

	trx_mutex_exit(trx);

	return(err);
}

/*********************************************************************//**
Tries to lock the specified record in the mode requested. If not immediately
possible, enqueues a waiting lock request. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. This function sets a normal next-key lock, or in the case
of a page supremum record, a gap type lock.
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
static
dberr_t
lock_rec_lock(
/*==========*/
	ibool			impl,	/*!< in: if TRUE, no lock is set
					if no wait is necessary: we
					assume that the caller will
					set an implicit lock */
	ulint			mode,	/*!< in: lock mode: LOCK_X or
					LOCK_S possibly ORed to either
					LOCK_GAP or LOCK_REC_NOT_GAP */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of record */
	dict_index_t*		index,	/*!< in: index of record */
	que_thr_t*		thr)	/*!< in: query thread */
{
	ut_ad(lock_mutex_own());
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
	      || (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
	      || mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP
	      || mode - (LOCK_MODE_MASK & mode) == 0);
	ut_ad(dict_index_is_clust(index) || !dict_index_is_online_ddl(index));

	/* We try a simplified and faster subroutine for the most
	common cases */
	switch (lock_rec_lock_fast(impl, mode, block, heap_no, index, thr)) {
	case LOCK_REC_SUCCESS:
		return(DB_SUCCESS);
	case LOCK_REC_SUCCESS_CREATED:
		return(DB_SUCCESS_LOCKED_REC);
	case LOCK_REC_FAIL:
		return(lock_rec_lock_slow(impl, mode, block,
					  heap_no, index, thr));
	}

	ut_error;
	return(DB_ERROR);
}

/*********************************************************************//**
Checks if a waiting record lock request still has to wait in a queue.
@return	lock that is causing the wait */
static
const lock_t*
lock_rec_has_to_wait_in_queue(
/*==========================*/
	const lock_t*	wait_lock)	/*!< in: waiting record lock */
{
	const lock_t*	lock;
	ulint		space;
	ulint		page_no;
	ulint		heap_no;
	ulint		bit_mask;
	ulint		bit_offset;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_wait(wait_lock));
	ut_ad(lock_get_type_low(wait_lock) == LOCK_REC);

	space = wait_lock->un_member.rec_lock.space;
	page_no = wait_lock->un_member.rec_lock.page_no;
	heap_no = lock_rec_find_set_bit(wait_lock);

	bit_offset = heap_no / 8;
	bit_mask = 1 << (heap_no % 8);

	for (lock = lock_rec_get_first_on_page_addr(space, page_no);
	     lock != wait_lock;
	     lock = lock_rec_get_next_on_page_const(lock)) {

		const byte*	p = (const byte*) &lock[1];

		if (heap_no < lock_rec_get_n_bits(lock)
		    && (p[bit_offset] & bit_mask)
		    && lock_has_to_wait(wait_lock, lock)) {

			return(lock);
		}
	}

	return(NULL);
}

/*************************************************************//**
Grants a lock to a waiting lock request and releases the waiting transaction.
The caller must hold lock_sys->mutex but not lock->trx->mutex. */
static
void
lock_grant(
/*=======*/
	lock_t*	lock)	/*!< in/out: waiting lock request */
{
	ut_ad(lock_mutex_own());

	lock_reset_lock_and_trx_wait(lock);

	trx_mutex_enter(lock->trx);

	if (lock_get_mode(lock) == LOCK_AUTO_INC) {
		dict_table_t*	table = lock->un_member.tab_lock.table;

		if (UNIV_UNLIKELY(table->autoinc_trx == lock->trx)) {
			fprintf(stderr,
				"InnoDB: Error: trx already had"
				" an AUTO-INC lock!\n");
		} else {
			table->autoinc_trx = lock->trx;

			ib_vector_push(lock->trx->autoinc_locks, &lock);
		}
	}

#ifdef UNIV_DEBUG
	if (lock_print_waits) {
		fprintf(stderr, "Lock wait for trx " TRX_ID_FMT " ends\n",
			lock->trx->id);
	}
#endif /* UNIV_DEBUG */

	/* If we are resolving a deadlock by choosing another transaction
	as a victim, then our original transaction may not be in the
	TRX_QUE_LOCK_WAIT state, and there is no need to end the lock wait
	for it */

	if (!(lock->type_mode & LOCK_CONV_BY_OTHER)
	    && lock->trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
		que_thr_t*	thr;

		thr = que_thr_end_lock_wait(lock->trx);

		if (thr != NULL) {
			lock_wait_release_thread_if_suspended(thr);
		}
	}

	trx_mutex_exit(lock->trx);
}

/*************************************************************//**
Cancels a waiting record lock request and releases the waiting transaction
that requested it. NOTE: does NOT check if waiting lock requests behind this
one can now be granted! */
static
void
lock_rec_cancel(
/*============*/
	lock_t*	lock)	/*!< in: waiting record lock request */
{
	que_thr_t*	thr;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_type_low(lock) == LOCK_REC);
	ut_ad(!(lock->type_mode & LOCK_CONV_BY_OTHER));

	/* Reset the bit (there can be only one set bit) in the lock bitmap */
	lock_rec_reset_nth_bit(lock, lock_rec_find_set_bit(lock));

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_mutex_enter(lock->trx);

	thr = que_thr_end_lock_wait(lock->trx);

	if (thr != NULL) {
		lock_wait_release_thread_if_suspended(thr);
	}

	trx_mutex_exit(lock->trx);
}

/*************************************************************//**
Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed. */
static
void
lock_rec_dequeue_from_page(
/*=======================*/
	lock_t*		in_lock)	/*!< in: record lock object: all
					record locks which are contained in
					this lock object are removed;
					transactions waiting behind will
					get their lock requests granted,
					if they are now qualified to it */
{
	ulint		space;
	ulint		page_no;
	lock_t*		lock;
	trx_lock_t*	trx_lock;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_type_low(in_lock) == LOCK_REC);
	/* We may or may not be holding in_lock->trx->mutex here. */

	trx_lock = &in_lock->trx->lock;

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	in_lock->index->table->n_rec_locks--;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
		    lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx_lock->trx_locks, in_lock);

	MONITOR_INC(MONITOR_RECLOCK_REMOVED);
	MONITOR_DEC(MONITOR_NUM_RECLOCK);

	/* Check if waiting locks in the queue can now be granted: grant
	locks if there are no conflicting locks ahead. Stop at the first
	X lock that is waiting or has been granted. */

	for (lock = lock_rec_get_first_on_page_addr(space, page_no);
	     lock != NULL;
	     lock = lock_rec_get_next_on_page(lock)) {

		if (lock_get_wait(lock)
		    && !lock_rec_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			ut_ad(lock->trx != in_lock->trx);
			lock_grant(lock);
		}
	}
}

/*************************************************************//**
Removes a record lock request, waiting or granted, from the queue. */
static
void
lock_rec_discard(
/*=============*/
	lock_t*		in_lock)	/*!< in: record lock object: all
					record locks which are contained
					in this lock object are removed */
{
	ulint		space;
	ulint		page_no;
	trx_lock_t*	trx_lock;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

	trx_lock = &in_lock->trx->lock;

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	in_lock->index->table->n_rec_locks--;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
		    lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx_lock->trx_locks, in_lock);

	MONITOR_INC(MONITOR_RECLOCK_REMOVED);
	MONITOR_DEC(MONITOR_NUM_RECLOCK);
}

/*************************************************************//**
Removes record lock objects set on an index page which is discarded. This
function does not move locks, or check for waiting locks, therefore the
lock bitmaps must already be reset when this function is called. */
static
void
lock_rec_free_all_from_discard_page(
/*================================*/
	const buf_block_t*	block)	/*!< in: page to be discarded */
{
	ulint	space;
	ulint	page_no;
	lock_t*	lock;
	lock_t*	next_lock;

	ut_ad(lock_mutex_own());

	space = buf_block_get_space(block);
	page_no = buf_block_get_page_no(block);

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	while (lock != NULL) {
		ut_ad(lock_rec_find_set_bit(lock) == ULINT_UNDEFINED);
		ut_ad(!lock_get_wait(lock));

		next_lock = lock_rec_get_next_on_page(lock);

		lock_rec_discard(lock);

		lock = next_lock;
	}
}

/*============= RECORD LOCK MOVING AND INHERITING ===================*/

/*************************************************************//**
Resets the lock bits for a single record. Releases transactions waiting for
lock requests here. */
static
void
lock_rec_reset_and_release_wait(
/*============================*/
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no)/*!< in: heap number of record */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {

		if (lock_is_wait_not_by_other(lock->type_mode)) {
			lock_rec_cancel(lock);
		} else if (lock_get_wait(lock)) {
			/* just reset LOCK_WAIT */
			lock_rec_reset_nth_bit(lock, heap_no);
			lock_reset_lock_and_trx_wait(lock);
		} else {
			lock_rec_reset_nth_bit(lock, heap_no);
		}
	}
}

/*************************************************************//**
Makes a record to inherit the locks (except LOCK_INSERT_INTENTION type)
of another record as gap type locks, but does not reset the lock bits of
the other record. Also waiting lock requests on rec are inherited as
GRANTED gap locks. */
static
void
lock_rec_inherit_to_gap(
/*====================*/
	const buf_block_t*	heir_block,	/*!< in: block containing the
						record which inherits */
	const buf_block_t*	block,		/*!< in: block containing the
						record from which inherited;
						does NOT reset the locks on
						this record */
	ulint			heir_heap_no,	/*!< in: heap_no of the
						inheriting record */
	ulint			heap_no)	/*!< in: heap_no of the
						donating record */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());

	/* If srv_locks_unsafe_for_binlog is TRUE or session is using
	READ COMMITTED isolation level, we do not want locks set
	by an UPDATE or a DELETE to be inherited as gap type locks. But we
	DO want S-locks set by a consistency constraint to be inherited also
	then. */

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {

		if (!lock_rec_get_insert_intention(lock)
		    && !((srv_locks_unsafe_for_binlog
			  || lock->trx->isolation_level
			  <= TRX_ISO_READ_COMMITTED)
			 && lock_get_mode(lock) == LOCK_X)) {

			lock_rec_add_to_queue(
				LOCK_REC | LOCK_GAP | lock_get_mode(lock),
				heir_block, heir_heap_no, lock->index,
				lock->trx, FALSE);
		}
	}
}

/*************************************************************//**
Makes a record to inherit the gap locks (except LOCK_INSERT_INTENTION type)
of another record as gap type locks, but does not reset the lock bits of the
other record. Also waiting lock requests are inherited as GRANTED gap locks. */
static
void
lock_rec_inherit_to_gap_if_gap_lock(
/*================================*/
	const buf_block_t*	block,		/*!< in: buffer block */
	ulint			heir_heap_no,	/*!< in: heap_no of
						record which inherits */
	ulint			heap_no)	/*!< in: heap_no of record
						from which inherited;
						does NOT reset the locks
						on this record */
{
	lock_t*	lock;

	lock_mutex_enter();

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {

		if (!lock_rec_get_insert_intention(lock)
		    && (heap_no == PAGE_HEAP_NO_SUPREMUM
			|| !lock_rec_get_rec_not_gap(lock))) {

			lock_rec_add_to_queue(
				LOCK_REC | LOCK_GAP | lock_get_mode(lock),
				block, heir_heap_no, lock->index,
				lock->trx, FALSE);
		}
	}

	lock_mutex_exit();
}

/*************************************************************//**
Moves the locks of a record to another record and resets the lock bits of
the donating record. */
static
void
lock_rec_move(
/*==========*/
	const buf_block_t*	receiver,	/*!< in: buffer block containing
						the receiving record */
	const buf_block_t*	donator,	/*!< in: buffer block containing
						the donating record */
	ulint			receiver_heap_no,/*!< in: heap_no of the record
						which gets the locks; there
						must be no lock requests
						on it! */
	ulint			donator_heap_no)/*!< in: heap_no of the record
						which gives the locks */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());

	ut_ad(lock_rec_get_first(receiver, receiver_heap_no) == NULL);

	for (lock = lock_rec_get_first(donator, donator_heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next(donator_heap_no, lock)) {

		const ulint	type_mode = lock->type_mode;

		lock_rec_reset_nth_bit(lock, donator_heap_no);

		if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {
			lock_reset_lock_and_trx_wait(lock);
		}

		/* Note that we FIRST reset the bit, and then set the lock:
		the function works also if donator == receiver */

		lock_rec_add_to_queue(
			type_mode, receiver, receiver_heap_no,
			lock->index, lock->trx, FALSE);
	}

	ut_ad(lock_rec_get_first(donator, donator_heap_no) == NULL);
}

/*************************************************************//**
Updates the lock table when we have reorganized a page. NOTE: we copy
also the locks set on the infimum of the page; the infimum may carry
locks if an update of a record is occurring on the page, and its locks
were temporarily stored on the infimum. */
UNIV_INTERN
void
lock_move_reorganize_page(
/*======================*/
	const buf_block_t*	block,	/*!< in: old index page, now
					reorganized */
	const buf_block_t*	oblock)	/*!< in: copy of the old, not
					reorganized page */
{
	lock_t*		lock;
	UT_LIST_BASE_NODE_T(lock_t)	old_locks;
	mem_heap_t*	heap		= NULL;
	ulint		comp;

	lock_mutex_enter();

	lock = lock_rec_get_first_on_page(block);

	if (lock == NULL) {
		lock_mutex_exit();

		return;
	}

	heap = mem_heap_create(256);

	/* Copy first all the locks on the page to heap and reset the
	bitmaps in the original locks; chain the copies of the locks
	using the trx_locks field in them. */

	UT_LIST_INIT(old_locks);

	do {
		/* Make a copy of the lock */
		lock_t*	old_lock = lock_rec_copy(lock, heap);

		UT_LIST_ADD_LAST(trx_locks, old_locks, old_lock);

		/* Reset bitmap of lock */
		lock_rec_bitmap_reset(lock);

		if (lock_get_wait(lock)) {

			lock_reset_lock_and_trx_wait(lock);
		}

		lock = lock_rec_get_next_on_page(lock);
	} while (lock != NULL);

	comp = page_is_comp(block->frame);
	ut_ad(comp == page_is_comp(oblock->frame));

	for (lock = UT_LIST_GET_FIRST(old_locks); lock;
	     lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
		/* NOTE: we copy also the locks set on the infimum and
		supremum of the page; the infimum may carry locks if an
		update of a record is occurring on the page, and its locks
		were temporarily stored on the infimum */
		page_cur_t	cur1;
		page_cur_t	cur2;

		page_cur_set_before_first(block, &cur1);
		page_cur_set_before_first(oblock, &cur2);

		/* Set locks according to old locks */
		for (;;) {
			ulint	old_heap_no;
			ulint	new_heap_no;

			ut_ad(comp || !memcmp(page_cur_get_rec(&cur1),
					      page_cur_get_rec(&cur2),
					      rec_get_data_size_old(
						      page_cur_get_rec(
							      &cur2))));
			if (UNIV_LIKELY(comp)) {
				old_heap_no = rec_get_heap_no_new(
					page_cur_get_rec(&cur2));
				new_heap_no = rec_get_heap_no_new(
					page_cur_get_rec(&cur1));
			} else {
				old_heap_no = rec_get_heap_no_old(
					page_cur_get_rec(&cur2));
				new_heap_no = rec_get_heap_no_old(
					page_cur_get_rec(&cur1));
			}

			if (lock_rec_get_nth_bit(lock, old_heap_no)) {

				/* Clear the bit in old_lock. */
				ut_d(lock_rec_reset_nth_bit(lock,
							    old_heap_no));

				/* NOTE that the old lock bitmap could be too
				small for the new heap number! */

				lock_rec_add_to_queue(
					lock->type_mode, block, new_heap_no,
					lock->index, lock->trx, FALSE);

				/* if (new_heap_no == PAGE_HEAP_NO_SUPREMUM
				&& lock_get_wait(lock)) {
				fprintf(stderr,
				"---\n--\n!!!Lock reorg: supr type %lu\n",
				lock->type_mode);
				} */
			}

			if (UNIV_UNLIKELY
			    (new_heap_no == PAGE_HEAP_NO_SUPREMUM)) {

				ut_ad(old_heap_no == PAGE_HEAP_NO_SUPREMUM);
				break;
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

#ifdef UNIV_DEBUG
		{
			ulint	i = lock_rec_find_set_bit(lock);

			/* Check that all locks were moved. */
			if (UNIV_UNLIKELY(i != ULINT_UNDEFINED)) {
				fprintf(stderr,
					"lock_move_reorganize_page():"
					" %lu not moved in %p\n",
					(ulong) i, (void*) lock);
				ut_error;
			}
		}
#endif /* UNIV_DEBUG */
	}

	lock_mutex_exit();

	mem_heap_free(heap);

#ifdef UNIV_DEBUG_LOCK_VALIDATE
	ut_ad(lock_rec_validate_page(block));
#endif
}

/*************************************************************//**
Moves the explicit locks on user records to another page if a record
list end is moved to another page. */
UNIV_INTERN
void
lock_move_rec_list_end(
/*===================*/
	const buf_block_t*	new_block,	/*!< in: index page to move to */
	const buf_block_t*	block,		/*!< in: index page */
	const rec_t*		rec)		/*!< in: record on page: this
						is the first record moved */
{
	lock_t*		lock;
	const ulint	comp	= page_rec_is_comp(rec);

	lock_mutex_enter();

	/* Note: when we move locks from record to record, waiting locks
	and possible granted gap type locks behind them are enqueued in
	the original order, because new elements are inserted to a hash
	table to the end of the hash chain, and lock_rec_add_to_queue
	does not reuse locks if there are waiters in the queue. */

	for (lock = lock_rec_get_first_on_page(block); lock;
	     lock = lock_rec_get_next_on_page(lock)) {
		page_cur_t	cur1;
		page_cur_t	cur2;
		const ulint	type_mode = lock->type_mode;

		page_cur_position(rec, block, &cur1);

		if (page_cur_is_before_first(&cur1)) {
			page_cur_move_to_next(&cur1);
		}

		page_cur_set_before_first(new_block, &cur2);
		page_cur_move_to_next(&cur2);

		/* Copy lock requests on user records to new page and
		reset the lock bits on the old */

		while (!page_cur_is_after_last(&cur1)) {
			ulint	heap_no;

			if (comp) {
				heap_no = rec_get_heap_no_new(
					page_cur_get_rec(&cur1));
			} else {
				heap_no = rec_get_heap_no_old(
					page_cur_get_rec(&cur1));
				ut_ad(!memcmp(page_cur_get_rec(&cur1),
					 page_cur_get_rec(&cur2),
					 rec_get_data_size_old(
						 page_cur_get_rec(&cur2))));
			}

			if (lock_rec_get_nth_bit(lock, heap_no)) {
				lock_rec_reset_nth_bit(lock, heap_no);

				if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {
					lock_reset_lock_and_trx_wait(lock);
				}

				if (comp) {
					heap_no = rec_get_heap_no_new(
						page_cur_get_rec(&cur2));
				} else {
					heap_no = rec_get_heap_no_old(
						page_cur_get_rec(&cur2));
				}

				lock_rec_add_to_queue(
					type_mode, new_block, heap_no,
					lock->index, lock->trx, FALSE);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}
	}

	lock_mutex_exit();

#ifdef UNIV_DEBUG_LOCK_VALIDATE
	ut_ad(lock_rec_validate_page(block));
	ut_ad(lock_rec_validate_page(new_block));
#endif
}

/*************************************************************//**
Moves the explicit locks on user records to another page if a record
list start is moved to another page. */
UNIV_INTERN
void
lock_move_rec_list_start(
/*=====================*/
	const buf_block_t*	new_block,	/*!< in: index page to
						move to */
	const buf_block_t*	block,		/*!< in: index page */
	const rec_t*		rec,		/*!< in: record on page:
						this is the first
						record NOT copied */
	const rec_t*		old_end)	/*!< in: old
						previous-to-last
						record on new_page
						before the records
						were copied */
{
	lock_t*		lock;
	const ulint	comp	= page_rec_is_comp(rec);

	ut_ad(block->frame == page_align(rec));
	ut_ad(new_block->frame == page_align(old_end));

	lock_mutex_enter();

	for (lock = lock_rec_get_first_on_page(block); lock;
	     lock = lock_rec_get_next_on_page(lock)) {
		page_cur_t	cur1;
		page_cur_t	cur2;
		const ulint	type_mode = lock->type_mode;

		page_cur_set_before_first(block, &cur1);
		page_cur_move_to_next(&cur1);

		page_cur_position(old_end, new_block, &cur2);
		page_cur_move_to_next(&cur2);

		/* Copy lock requests on user records to new page and
		reset the lock bits on the old */

		while (page_cur_get_rec(&cur1) != rec) {
			ulint	heap_no;

			if (comp) {
				heap_no = rec_get_heap_no_new(
					page_cur_get_rec(&cur1));
			} else {
				heap_no = rec_get_heap_no_old(
					page_cur_get_rec(&cur1));
				ut_ad(!memcmp(page_cur_get_rec(&cur1),
					      page_cur_get_rec(&cur2),
					      rec_get_data_size_old(
						      page_cur_get_rec(
							      &cur2))));
			}

			if (lock_rec_get_nth_bit(lock, heap_no)) {
				lock_rec_reset_nth_bit(lock, heap_no);

				if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {
					lock_reset_lock_and_trx_wait(lock);
				}

				if (comp) {
					heap_no = rec_get_heap_no_new(
						page_cur_get_rec(&cur2));
				} else {
					heap_no = rec_get_heap_no_old(
						page_cur_get_rec(&cur2));
				}

				lock_rec_add_to_queue(
					type_mode, new_block, heap_no,
					lock->index, lock->trx, FALSE);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

#ifdef UNIV_DEBUG
		if (page_rec_is_supremum(rec)) {
			ulint	i;

			for (i = PAGE_HEAP_NO_USER_LOW;
			     i < lock_rec_get_n_bits(lock); i++) {
				if (UNIV_UNLIKELY
				    (lock_rec_get_nth_bit(lock, i))) {

					fprintf(stderr,
						"lock_move_rec_list_start():"
						" %lu not moved in %p\n",
						(ulong) i, (void*) lock);
					ut_error;
				}
			}
		}
#endif /* UNIV_DEBUG */
	}

	lock_mutex_exit();

#ifdef UNIV_DEBUG_LOCK_VALIDATE
	ut_ad(lock_rec_validate_page(block));
#endif
}

/*************************************************************//**
Updates the lock table when a page is split to the right. */
UNIV_INTERN
void
lock_update_split_right(
/*====================*/
	const buf_block_t*	right_block,	/*!< in: right page */
	const buf_block_t*	left_block)	/*!< in: left page */
{
	ulint	heap_no = lock_get_min_heap_no(right_block);

	lock_mutex_enter();

	/* Move the locks on the supremum of the left page to the supremum
	of the right page */

	lock_rec_move(right_block, left_block,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);

	/* Inherit the locks to the supremum of left page from the successor
	of the infimum on right page */

	lock_rec_inherit_to_gap(left_block, right_block,
				PAGE_HEAP_NO_SUPREMUM, heap_no);

	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when a page is merged to the right. */
UNIV_INTERN
void
lock_update_merge_right(
/*====================*/
	const buf_block_t*	right_block,	/*!< in: right page to
						which merged */
	const rec_t*		orig_succ,	/*!< in: original
						successor of infimum
						on the right page
						before merge */
	const buf_block_t*	left_block)	/*!< in: merged index
						page which will be
						discarded */
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

	lock_rec_reset_and_release_wait(left_block,
					PAGE_HEAP_NO_SUPREMUM);

	lock_rec_free_all_from_discard_page(left_block);

	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when the root page is copied to another in
btr_root_raise_and_insert. Note that we leave lock structs on the
root page, even though they do not make sense on other than leaf
pages: the reason is that in a pessimistic update the infimum record
of the root page will act as a dummy carrier of the locks of the record
to be updated. */
UNIV_INTERN
void
lock_update_root_raise(
/*===================*/
	const buf_block_t*	block,	/*!< in: index page to which copied */
	const buf_block_t*	root)	/*!< in: root page */
{
	lock_mutex_enter();

	/* Move the locks on the supremum of the root to the supremum
	of block */

	lock_rec_move(block, root,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when a page is copied to another and the original page
is removed from the chain of leaf pages, except if page is the root! */
UNIV_INTERN
void
lock_update_copy_and_discard(
/*=========================*/
	const buf_block_t*	new_block,	/*!< in: index page to
						which copied */
	const buf_block_t*	block)		/*!< in: index page;
						NOT the root! */
{
	lock_mutex_enter();

	/* Move the locks on the supremum of the old page to the supremum
	of new_page */

	lock_rec_move(new_block, block,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
	lock_rec_free_all_from_discard_page(block);

	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when a page is split to the left. */
UNIV_INTERN
void
lock_update_split_left(
/*===================*/
	const buf_block_t*	right_block,	/*!< in: right page */
	const buf_block_t*	left_block)	/*!< in: left page */
{
	ulint	heap_no = lock_get_min_heap_no(right_block);

	lock_mutex_enter();

	/* Inherit the locks to the supremum of the left page from the
	successor of the infimum on the right page */

	lock_rec_inherit_to_gap(left_block, right_block,
				PAGE_HEAP_NO_SUPREMUM, heap_no);

	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when a page is merged to the left. */
UNIV_INTERN
void
lock_update_merge_left(
/*===================*/
	const buf_block_t*	left_block,	/*!< in: left page to
						which merged */
	const rec_t*		orig_pred,	/*!< in: original predecessor
						of supremum on the left page
						before merge */
	const buf_block_t*	right_block)	/*!< in: merged index page
						which will be discarded */
{
	const rec_t*	left_next_rec;

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

		lock_rec_reset_and_release_wait(left_block,
						PAGE_HEAP_NO_SUPREMUM);
	}

	/* Move the locks from the supremum of right page to the supremum
	of the left page */

	lock_rec_move(left_block, right_block,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);

	lock_rec_free_all_from_discard_page(right_block);

	lock_mutex_exit();
}

/*************************************************************//**
Resets the original locks on heir and replaces them with gap type locks
inherited from rec. */
UNIV_INTERN
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
	ulint			heap_no)	/*!< in: heap_no of the
						donating record */
{
	lock_mutex_enter();

	lock_rec_reset_and_release_wait(heir_block, heir_heap_no);

	lock_rec_inherit_to_gap(heir_block, block, heir_heap_no, heap_no);

	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when a page is discarded. */
UNIV_INTERN
void
lock_update_discard(
/*================*/
	const buf_block_t*	heir_block,	/*!< in: index page
						which will inherit the locks */
	ulint			heir_heap_no,	/*!< in: heap_no of the record
						which will inherit the locks */
	const buf_block_t*	block)		/*!< in: index page
						which will be discarded */
{
	const page_t*	page = block->frame;
	const rec_t*	rec;
	ulint		heap_no;

	lock_mutex_enter();

	if (!lock_rec_get_first_on_page(block)) {
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

			lock_rec_inherit_to_gap(heir_block, block,
						heir_heap_no, heap_no);

			lock_rec_reset_and_release_wait(block, heap_no);

			rec = page + rec_get_next_offs(rec, TRUE);
		} while (heap_no != PAGE_HEAP_NO_SUPREMUM);
	} else {
		rec = page + PAGE_OLD_INFIMUM;

		do {
			heap_no = rec_get_heap_no_old(rec);

			lock_rec_inherit_to_gap(heir_block, block,
						heir_heap_no, heap_no);

			lock_rec_reset_and_release_wait(block, heap_no);

			rec = page + rec_get_next_offs(rec, FALSE);
		} while (heap_no != PAGE_HEAP_NO_SUPREMUM);
	}

	lock_rec_free_all_from_discard_page(block);

	lock_mutex_exit();
}

/*************************************************************//**
Updates the lock table when a new user record is inserted. */
UNIV_INTERN
void
lock_update_insert(
/*===============*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec)	/*!< in: the inserted record */
{
	ulint	receiver_heap_no;
	ulint	donator_heap_no;

	ut_ad(block->frame == page_align(rec));

	/* Inherit the gap-locking locks for rec, in gap mode, from the next
	record */

	if (page_rec_is_comp(rec)) {
		receiver_heap_no = rec_get_heap_no_new(rec);
		donator_heap_no = rec_get_heap_no_new(
			page_rec_get_next_low(rec, TRUE));
	} else {
		receiver_heap_no = rec_get_heap_no_old(rec);
		donator_heap_no = rec_get_heap_no_old(
			page_rec_get_next_low(rec, FALSE));
	}

	lock_rec_inherit_to_gap_if_gap_lock(
		block, receiver_heap_no, donator_heap_no);
}

/*************************************************************//**
Updates the lock table when a record is removed. */
UNIV_INTERN
void
lock_update_delete(
/*===============*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec)	/*!< in: the record to be removed */
{
	const page_t*	page = block->frame;
	ulint		heap_no;
	ulint		next_heap_no;

	ut_ad(page == page_align(rec));

	if (page_is_comp(page)) {
		heap_no = rec_get_heap_no_new(rec);
		next_heap_no = rec_get_heap_no_new(page
						   + rec_get_next_offs(rec,
								       TRUE));
	} else {
		heap_no = rec_get_heap_no_old(rec);
		next_heap_no = rec_get_heap_no_old(page
						   + rec_get_next_offs(rec,
								       FALSE));
	}

	lock_mutex_enter();

	/* Let the next record inherit the locks from rec, in gap mode */

	lock_rec_inherit_to_gap(block, block, next_heap_no, heap_no);

	/* Reset the lock bits on rec and release waiting transactions */

	lock_rec_reset_and_release_wait(block, heap_no);

	lock_mutex_exit();
}

/*********************************************************************//**
Stores on the page infimum record the explicit locks of another record.
This function is used to store the lock state of a record when it is
updated and the size of the record changes in the update. The record
is moved in such an update, perhaps to another page. The infimum record
acts as a dummy carrier record, taking care of lock releases while the
actual record is being moved. */
UNIV_INTERN
void
lock_rec_store_on_page_infimum(
/*===========================*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec)	/*!< in: record whose lock state
					is stored on the infimum
					record of the same page; lock
					bits are reset on the
					record */
{
	ulint	heap_no = page_rec_get_heap_no(rec);

	ut_ad(block->frame == page_align(rec));

	lock_mutex_enter();

	lock_rec_move(block, block, PAGE_HEAP_NO_INFIMUM, heap_no);

	lock_mutex_exit();
}

/*********************************************************************//**
Restores the state of explicit lock requests on a single record, where the
state was stored on the infimum of the page. */
UNIV_INTERN
void
lock_rec_restore_from_page_infimum(
/*===============================*/
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record whose lock state
					is restored */
	const buf_block_t*	donator)/*!< in: page (rec is not
					necessarily on this page)
					whose infimum stored the lock
					state; lock bits are reset on
					the infimum */
{
	ulint	heap_no = page_rec_get_heap_no(rec);

	lock_mutex_enter();

	lock_rec_move(block, donator, heap_no, PAGE_HEAP_NO_INFIMUM);

	lock_mutex_exit();
}

/*=========== DEADLOCK CHECKING ======================================*/

/*********************************************************************//**
rewind(3) the file used for storing the latest detected deadlock and
print a heading message to stderr if printing of all deadlocks to stderr
is enabled. */
UNIV_INLINE
void
lock_deadlock_start_print()
/*=======================*/
{
	ut_ad(lock_mutex_own());
	ut_ad(!srv_read_only_mode);

	rewind(lock_latest_err_file);
	ut_print_timestamp(lock_latest_err_file);

	if (srv_print_all_deadlocks) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "InnoDB: transactions deadlock detected, "
			"dumping detailed information.\n");
		ut_print_timestamp(stderr);
	}
}

/*********************************************************************//**
Print a message to the deadlock file and possibly to stderr. */
UNIV_INLINE
void
lock_deadlock_fputs(
/*================*/
	const char*	msg)	/*!< in: message to print */
{
	if (!srv_read_only_mode) {
		fputs(msg, lock_latest_err_file);

		if (srv_print_all_deadlocks) {
			fputs(msg, stderr);
		}
	}
}

/*********************************************************************//**
Print transaction data to the deadlock file and possibly to stderr. */
UNIV_INLINE
void
lock_deadlock_trx_print(
/*====================*/
	const trx_t*	trx,		/*!< in: transaction */
	ulint		max_query_len)	/*!< in: max query length to print,
					or 0 to use the default max length */
{
	ut_ad(lock_mutex_own());
	ut_ad(!srv_read_only_mode);

	ulint	n_rec_locks = lock_number_of_rows_locked(&trx->lock);
	ulint	n_trx_locks = UT_LIST_GET_LEN(trx->lock.trx_locks);
	ulint	heap_size = mem_heap_get_size(trx->lock.lock_heap);

	mutex_enter(&trx_sys->mutex);

	trx_print_low(lock_latest_err_file, trx, max_query_len,
		      n_rec_locks, n_trx_locks, heap_size);

	if (srv_print_all_deadlocks) {
		trx_print_low(stderr, trx, max_query_len,
			      n_rec_locks, n_trx_locks, heap_size);
	}

	mutex_exit(&trx_sys->mutex);
}

/*********************************************************************//**
Print lock data to the deadlock file and possibly to stderr. */
UNIV_INLINE
void
lock_deadlock_lock_print(
/*=====================*/
	const lock_t*	lock)	/*!< in: record or table type lock */
{
	ut_ad(lock_mutex_own());
	ut_ad(!srv_read_only_mode);

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

/** Used in deadlock tracking. Protected by lock_sys->mutex. */
static ib_uint64_t	lock_mark_counter = 0;

/** Check if the search is too deep. */
#define lock_deadlock_too_deep(c)				\
	(c->depth > LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK		\
	 || c->cost > LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK)

/********************************************************************//**
Get the next lock in the queue that is owned by a transaction whose
sub-tree has not already been searched.
@return next lock or NULL if at end of queue */
static
const lock_t*
lock_get_next_lock(
/*===============*/
	const lock_deadlock_ctx_t*
				ctx,	/*!< in: deadlock context */
	const lock_t*		lock,	/*!< in: lock in the queue */
	ulint			heap_no)/*!< in: heap no if rec lock else
					ULINT_UNDEFINED */
{
	ut_ad(lock_mutex_own());

	do {
		if (lock_get_type_low(lock) == LOCK_REC) {
			ut_ad(heap_no != ULINT_UNDEFINED);
			lock = lock_rec_get_next_const(heap_no, lock);
		} else {
			ut_ad(heap_no == ULINT_UNDEFINED);
			ut_ad(lock_get_type_low(lock) == LOCK_TABLE);
			lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
		}

		if (lock == NULL) {
			return(NULL);
		}

	} while (lock->trx->lock.deadlock_mark > ctx->mark_start);

	ut_ad(lock_get_type_low(lock) == lock_get_type_low(ctx->wait_lock));

	return(lock);
}

/********************************************************************//**
Get the first lock to search. The search starts from the current
wait_lock. What we are really interested in is an edge from the
current wait_lock's owning transaction to another transaction that has
a lock ahead in the queue. We skip locks where the owning transaction's
sub-tree has already been searched.
@return first lock or NULL */
static
const lock_t*
lock_get_first_lock(
/*================*/
	const lock_deadlock_ctx_t*
				ctx,	/*!< in: deadlock context */
	ulint*			heap_no)/*!< out: heap no if rec lock,
					else ULINT_UNDEFINED */
{
	const lock_t*		lock;

	ut_ad(lock_mutex_own());

	lock = ctx->wait_lock;

	if (lock_get_type_low(lock) == LOCK_REC) {

		*heap_no = lock_rec_find_set_bit(lock);
		ut_ad(*heap_no != ULINT_UNDEFINED);

		lock = lock_rec_get_first_on_page_addr(
			lock->un_member.rec_lock.space,
			lock->un_member.rec_lock.page_no);
	} else {
		*heap_no = ULINT_UNDEFINED;
		ut_ad(lock_get_type_low(lock) == LOCK_TABLE);
		lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
	}

	ut_ad(lock != NULL);

	/* Skip sub-trees that have already been searched. */

	if (lock->trx->lock.deadlock_mark > ctx->mark_start) {
		return(lock_get_next_lock(ctx, lock, *heap_no));
	}

	ut_ad(lock_get_type_low(lock) == lock_get_type_low(ctx->wait_lock));

	return(lock);
}

/********************************************************************//**
Notify that a deadlock has been detected and print the conflicting
transaction info. */
static
void
lock_deadlock_notify(
/*=================*/
	const lock_deadlock_ctx_t*	ctx,	/*!< in: deadlock context */
	const lock_t*			lock)	/*!< in: lock causing
						deadlock */
{
	ut_ad(lock_mutex_own());
	ut_ad(!srv_read_only_mode);

	lock_deadlock_start_print();

	lock_deadlock_fputs("\n*** (1) TRANSACTION:\n");

	lock_deadlock_trx_print(ctx->wait_lock->trx, 3000);

	lock_deadlock_fputs("*** (1) WAITING FOR THIS LOCK TO BE GRANTED:\n");

	lock_deadlock_lock_print(ctx->wait_lock);

	lock_deadlock_fputs("*** (2) TRANSACTION:\n");

	lock_deadlock_trx_print(lock->trx, 3000);

	lock_deadlock_fputs("*** (2) HOLDS THE LOCK(S):\n");

	lock_deadlock_lock_print(lock);

	lock_deadlock_fputs("*** (2) WAITING FOR THIS LOCK TO BE GRANTED:\n");

	lock_deadlock_lock_print(ctx->start->lock.wait_lock);

#ifdef UNIV_DEBUG
	if (lock_print_waits) {
		fputs("Deadlock detected\n", stderr);
	}
#endif /* UNIV_DEBUG */
}

/********************************************************************//**
Select the victim transaction that should be rolledback.
@return victim transaction */
static
const trx_t*
lock_deadlock_select_victim(
/*========================*/
	const lock_deadlock_ctx_t*	ctx)	/*!< in: deadlock context */
{
	ut_ad(lock_mutex_own());
	ut_ad(ctx->wait_lock->trx != ctx->start);

	if (trx_weight_ge(ctx->wait_lock->trx, ctx->start)) {
		/* The joining  transaction is 'smaller',
		choose it as the victim and roll it back. */

		return(ctx->start);
	}

	return(ctx->wait_lock->trx);
}

/********************************************************************//**
Check whether the current waiting lock in the context has to wait for
the given lock that is ahead in the queue.
@return lock instance that could cause potential deadlock. */
static
const lock_t*
lock_deadlock_check(
/*================*/
	const lock_deadlock_ctx_t*	ctx,	/*!< in: deadlock context */
	const lock_t*			lock)	/*!< in: lock to check */
{
	ut_ad(lock_mutex_own());

	/* If it is the joining transaction wait lock. */
	if (lock == ctx->start->lock.wait_lock) {
		; /* Skip */
	} else if (lock == ctx->wait_lock) {

		/* We can mark this subtree as searched */
		ut_ad(lock->trx->lock.deadlock_mark <= ctx->mark_start);
		lock->trx->lock.deadlock_mark = ++lock_mark_counter;

		/* We are not prepared for an overflow. This 64-bit
		counter should never wrap around. At 10^9 increments
		per second, it would take 10^3 years of uptime. */

		ut_ad(lock_mark_counter > 0);

	} else if (lock_has_to_wait(ctx->wait_lock, lock)) {

		return(lock);
	}

	return(NULL);
}

/********************************************************************//**
Pop the deadlock search state from the stack.
@return stack slot instance that was on top of the stack. */
static
const lock_stack_t*
lock_deadlock_pop(
/*==============*/
	lock_deadlock_ctx_t*	ctx)		/*!< in/out: context */
{
	const lock_stack_t*	stack;
	const trx_lock_t*	trx_lock;

	ut_ad(lock_mutex_own());

	ut_ad(ctx->depth > 0);

	do {
		/* Restore search state. */

		stack = &lock_stack[--ctx->depth];
		trx_lock = &stack->lock->trx->lock;

		/* Skip sub-trees that have already been searched. */
	} while (ctx->depth > 0 && trx_lock->deadlock_mark > ctx->mark_start);

	return(ctx->depth == 0) ? NULL : stack;
}

/********************************************************************//**
Push the deadlock search state onto the stack.
@return slot that was used in the stack */
static
lock_stack_t*
lock_deadlock_push(
/*===============*/
	lock_deadlock_ctx_t*	ctx,		/*!< in/out: context */
	const lock_t*		lock,		/*!< in: current lock */
	ulint			heap_no)	/*!< in: heap number */
{
	ut_ad(lock_mutex_own());

	/* Save current search state. */

	if (LOCK_STACK_SIZE > ctx->depth) {
		lock_stack_t*	stack;

		stack = &lock_stack[ctx->depth++];

		stack->lock = lock;
		stack->heap_no = heap_no;
		stack->wait_lock = ctx->wait_lock;

		return(stack);
	}

	return(NULL);
}

/********************************************************************//**
Looks iteratively for a deadlock.
@return 0 if no deadlock else the victim transaction id.*/
static
trx_id_t
lock_deadlock_search(
/*=================*/
	lock_deadlock_ctx_t*	ctx)	/*!< in/out: deadlock context */
{
	const lock_t*	lock;
	ulint		heap_no;

	ut_ad(lock_mutex_own());
	ut_ad(!trx_mutex_own(ctx->start));

	ut_ad(ctx->start != NULL);
	ut_ad(ctx->wait_lock != NULL);
	assert_trx_in_list(ctx->wait_lock->trx);
	ut_ad(ctx->mark_start <= lock_mark_counter);

	/* Look at the locks ahead of wait_lock in the lock queue. */
	lock = lock_get_first_lock(ctx, &heap_no);
	do {
		/* We should never visit the same sub-tree more than once. */
		ut_ad(lock->trx->lock.deadlock_mark <= ctx->mark_start);

		++ctx->cost;

		if (lock_deadlock_check(ctx, lock) == NULL) {

			/* No conflict found, skip this lock. */

		} else if (lock->trx == ctx->start) {

			/* Found a cycle. */

			if (!srv_read_only_mode) {
				lock_deadlock_notify(ctx, lock);
			}

			return(lock_deadlock_select_victim(ctx)->id);

		} else if (lock_deadlock_too_deep(ctx)) {

			/* Search too deep to continue. */

			ctx->too_deep = TRUE;

			/* Select the joining transaction as the victim. */
			return(ctx->start->id);

		} else if (lock->trx->lock.que_state == TRX_QUE_LOCK_WAIT) {

			/* Another trx ahead has requested a lock in an
			incompatible mode, and is itself waiting for a lock. */

			/* Save current search state. */
			if (!lock_deadlock_push(ctx, lock, heap_no)) {

				/* Unable to save current search state, stack
				size not big enough. */

				ctx->too_deep = TRUE;

				return(ctx->start->id);
			}

			ctx->wait_lock = lock->trx->lock.wait_lock;
			lock = lock_get_first_lock(ctx, &heap_no);

			if (lock != NULL) {
				continue;
			}
		}

		if (lock != NULL) {
			lock = lock_get_next_lock(ctx, lock, heap_no);
		}

		if (lock == NULL && ctx->depth > 0) {
			const lock_stack_t*	stack;

			/* Restore previous search state. */

			stack = lock_deadlock_pop(ctx);

			if (stack != NULL) {
				lock = stack->lock;
				heap_no = stack->heap_no;
				ctx->wait_lock = stack->wait_lock;
			}
		}

	} while (lock != NULL || ctx->depth > 0);

	/* No deadlock found. */
	return(0);
}

/********************************************************************//**
Print info about transaction that was rolled back. */
static
void
lock_deadlock_joining_trx_print(
/*============================*/
	const trx_t*	trx,		/*!< in: transaction rolled back */
	const lock_t*	lock)		/*!< in: lock trx wants */
{
	ut_ad(lock_mutex_own());
	ut_ad(!srv_read_only_mode);

	/* If the lock search exceeds the max step
	or the max depth, the current trx will be
	the victim. Print its information. */
	lock_deadlock_start_print();

	lock_deadlock_fputs(
		"TOO DEEP OR LONG SEARCH IN THE LOCK TABLE"
		" WAITS-FOR GRAPH, WE WILL ROLL BACK"
		" FOLLOWING TRANSACTION \n\n"
		"*** TRANSACTION:\n");

	lock_deadlock_trx_print(trx, 3000);

	lock_deadlock_fputs("*** WAITING FOR THIS LOCK TO BE GRANTED:\n");

	lock_deadlock_lock_print(lock);
}

/********************************************************************//**
Rollback transaction selected as the victim. */
static
void
lock_deadlock_trx_rollback(
/*=======================*/
	lock_deadlock_ctx_t*	ctx)		/*!< in: deadlock context */
{
	trx_t*			trx;

	ut_ad(lock_mutex_own());

	trx = ctx->wait_lock->trx;

	lock_deadlock_fputs("*** WE ROLL BACK TRANSACTION (1)\n");

	trx_mutex_enter(trx);

	trx->lock.was_chosen_as_deadlock_victim = TRUE;

	lock_cancel_waiting_and_release(trx->lock.wait_lock);

	trx_mutex_exit(trx);
}

/********************************************************************//**
Checks if a joining lock request results in a deadlock. If a deadlock is
found this function will resolve the dadlock by choosing a victim transaction
and rolling it back. It will attempt to resolve all deadlocks. The returned
transaction id will be the joining transaction id or 0 if some other
transaction was chosen as a victim and rolled back or no deadlock found.

@return id of transaction chosen as victim or 0 */
static
trx_id_t
lock_deadlock_check_and_resolve(
/*============================*/
	const lock_t*	lock,	/*!< in: lock the transaction is requesting */
	const trx_t*	trx)	/*!< in: transaction */
{
	trx_id_t	victim_trx_id;

	ut_ad(trx != NULL);
	ut_ad(lock != NULL);
	ut_ad(lock_mutex_own());
	assert_trx_in_list(trx);

	/* Try and resolve as many deadlocks as possible. */
	do {
		lock_deadlock_ctx_t	ctx;

		/* Reset the context. */
		ctx.cost = 0;
		ctx.depth = 0;
		ctx.start = trx;
		ctx.too_deep = FALSE;
		ctx.wait_lock = lock;
		ctx.mark_start = lock_mark_counter;

		victim_trx_id = lock_deadlock_search(&ctx);

		/* Search too deep, we rollback the joining transaction. */
		if (ctx.too_deep) {

			ut_a(trx == ctx.start);
			ut_a(victim_trx_id == trx->id);

			if (!srv_read_only_mode) {
				lock_deadlock_joining_trx_print(trx, lock);
			}

			MONITOR_INC(MONITOR_DEADLOCK);

		} else if (victim_trx_id != 0 && victim_trx_id != trx->id) {

			ut_ad(victim_trx_id == ctx.wait_lock->trx->id);
			lock_deadlock_trx_rollback(&ctx);

			lock_deadlock_found = TRUE;

			MONITOR_INC(MONITOR_DEADLOCK);
		}

	} while (victim_trx_id != 0 && victim_trx_id != trx->id);

	/* If the joining transaction was selected as the victim. */
	if (victim_trx_id != 0) {
		ut_a(victim_trx_id == trx->id);

		lock_deadlock_fputs("*** WE ROLL BACK TRANSACTION (2)\n");

		lock_deadlock_found = TRUE;
	}

	return(victim_trx_id);
}

/*========================= TABLE LOCKS ==============================*/

/*********************************************************************//**
Creates a table lock object and adds it as the last in the lock queue
of the table. Does NOT check for deadlocks or lock compatibility.
@return	own: new lock object */
UNIV_INLINE
lock_t*
lock_table_create(
/*==============*/
	dict_table_t*	table,	/*!< in/out: database table
				in dictionary cache */
	ulint		type_mode,/*!< in: lock mode possibly ORed with
				LOCK_WAIT */
	trx_t*		trx)	/*!< in: trx */
{
	lock_t*	lock;

	ut_ad(table && trx);
	ut_ad(lock_mutex_own());
	ut_ad(trx_mutex_own(trx));
	ut_ad(!(type_mode & LOCK_CONV_BY_OTHER));

	/* Non-locking autocommit read-only transactions should not set
	any locks. */
	assert_trx_in_list(trx);

	if ((type_mode & LOCK_MODE_MASK) == LOCK_AUTO_INC) {
		++table->n_waiting_or_granted_auto_inc_locks;
	}

	/* For AUTOINC locking we reuse the lock instance only if
	there is no wait involved else we allocate the waiting lock
	from the transaction lock heap. */
	if (type_mode == LOCK_AUTO_INC) {

		lock = table->autoinc_lock;

		table->autoinc_trx = trx;

		ib_vector_push(trx->autoinc_locks, &lock);
	} else {
		lock = static_cast<lock_t*>(
			mem_heap_alloc(trx->lock.lock_heap, sizeof(*lock)));
	}

	lock->type_mode = type_mode | LOCK_TABLE;
	lock->trx = trx;

	lock->un_member.tab_lock.table = table;

	ut_ad(table->n_ref_count > 0 || !table->can_be_evicted);

	UT_LIST_ADD_LAST(trx_locks, trx->lock.trx_locks, lock);
	UT_LIST_ADD_LAST(un_member.tab_lock.locks, table->locks, lock);

	if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {

		lock_set_lock_and_trx_wait(lock, trx);
	}

	ib_vector_push(lock->trx->lock.table_locks, &lock);

	MONITOR_INC(MONITOR_TABLELOCK_CREATED);
	MONITOR_INC(MONITOR_NUM_TABLELOCK);

	return(lock);
}

/*************************************************************//**
Pops autoinc lock requests from the transaction's autoinc_locks. We
handle the case where there are gaps in the array and they need to
be popped off the stack. */
UNIV_INLINE
void
lock_table_pop_autoinc_locks(
/*=========================*/
	trx_t*	trx)	/*!< in/out: transaction that owns the AUTOINC locks */
{
	ut_ad(lock_mutex_own());
	ut_ad(!ib_vector_is_empty(trx->autoinc_locks));

	/* Skip any gaps, gaps are NULL lock entries in the
	trx->autoinc_locks vector. */

	do {
		ib_vector_pop(trx->autoinc_locks);

		if (ib_vector_is_empty(trx->autoinc_locks)) {
			return;
		}

	} while (*(lock_t**) ib_vector_get_last(trx->autoinc_locks) == NULL);
}

/*************************************************************//**
Removes an autoinc lock request from the transaction's autoinc_locks. */
UNIV_INLINE
void
lock_table_remove_autoinc_lock(
/*===========================*/
	lock_t*	lock,	/*!< in: table lock */
	trx_t*	trx)	/*!< in/out: transaction that owns the lock */
{
	lock_t*	autoinc_lock;
	lint	i = ib_vector_size(trx->autoinc_locks) - 1;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_mode(lock) == LOCK_AUTO_INC);
	ut_ad(lock_get_type_low(lock) & LOCK_TABLE);
	ut_ad(!ib_vector_is_empty(trx->autoinc_locks));

	/* With stored functions and procedures the user may drop
	a table within the same "statement". This special case has
	to be handled by deleting only those AUTOINC locks that were
	held by the table being dropped. */

	autoinc_lock = *static_cast<lock_t**>(
		ib_vector_get(trx->autoinc_locks, i));

	/* This is the default fast case. */

	if (autoinc_lock == lock) {
		lock_table_pop_autoinc_locks(trx);
	} else {
		/* The last element should never be NULL */
		ut_a(autoinc_lock != NULL);

		/* Handle freeing the locks from within the stack. */

		while (--i >= 0) {
			autoinc_lock = *static_cast<lock_t**>(
				ib_vector_get(trx->autoinc_locks, i));

			if (UNIV_LIKELY(autoinc_lock == lock)) {
				void*	null_var = NULL;
				ib_vector_set(trx->autoinc_locks, i, &null_var);
				return;
			}
		}

		/* Must find the autoinc lock. */
		ut_error;
	}
}

/*************************************************************//**
Removes a table lock request from the queue and the trx list of locks;
this is a low-level function which does NOT check if waiting requests
can now be granted. */
UNIV_INLINE
void
lock_table_remove_low(
/*==================*/
	lock_t*	lock)	/*!< in/out: table lock */
{
	trx_t*		trx;
	dict_table_t*	table;

	ut_ad(lock_mutex_own());

	trx = lock->trx;
	table = lock->un_member.tab_lock.table;

	/* Remove the table from the transaction's AUTOINC vector, if
	the lock that is being released is an AUTOINC lock. */
	if (lock_get_mode(lock) == LOCK_AUTO_INC) {

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

		if (!lock_get_wait(lock)
		    && !ib_vector_is_empty(trx->autoinc_locks)) {

			lock_table_remove_autoinc_lock(lock, trx);
		}

		ut_a(table->n_waiting_or_granted_auto_inc_locks > 0);
		table->n_waiting_or_granted_auto_inc_locks--;
	}

	UT_LIST_REMOVE(trx_locks, trx->lock.trx_locks, lock);
	UT_LIST_REMOVE(un_member.tab_lock.locks, table->locks, lock);

	MONITOR_INC(MONITOR_TABLELOCK_REMOVED);
	MONITOR_DEC(MONITOR_NUM_TABLELOCK);
}

/*********************************************************************//**
Enqueues a waiting request for a table lock which cannot be granted
immediately. Checks for deadlocks.
@return DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED, or
DB_SUCCESS; DB_SUCCESS means that there was a deadlock, but another
transaction was chosen as a victim, and we got the lock immediately:
no need to wait then */
static
dberr_t
lock_table_enqueue_waiting(
/*=======================*/
	ulint		mode,	/*!< in: lock mode this transaction is
				requesting */
	dict_table_t*	table,	/*!< in/out: table */
	que_thr_t*	thr)	/*!< in: query thread */
{
	trx_t*		trx;
	lock_t*		lock;
	trx_id_t	victim_trx_id;

	ut_ad(lock_mutex_own());

	trx = thr_get_trx(thr);
	ut_ad(trx_mutex_own(trx));

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (que_thr_stop(thr)) {
		ut_error;

		return(DB_QUE_THR_SUSPENDED);
	}

	switch (trx_get_dict_operation(trx)) {
	case TRX_DICT_OP_NONE:
		break;
	case TRX_DICT_OP_TABLE:
	case TRX_DICT_OP_INDEX:
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: a table lock wait happens"
		      " in a dictionary operation!\n"
		      "InnoDB: Table name ", stderr);
		ut_print_name(stderr, trx, TRUE, table->name);
		fputs(".\n"
		      "InnoDB: Submit a detailed bug report"
		      " to http://bugs.mysql.com\n",
		      stderr);
		ut_ad(0);
	}

	/* Enqueue the lock request that will wait to be granted */

	lock = lock_table_create(table, mode | LOCK_WAIT, trx);

	/* Release the mutex to obey the latching order.
	This is safe, because lock_deadlock_check_and_resolve()
	is invoked when a lock wait is enqueued for the currently
	running transaction. Because trx is a running transaction
	(it is not currently suspended because of a lock wait),
	its state can only be changed by this thread, which is
	currently associated with the transaction. */

	trx_mutex_exit(trx);

	victim_trx_id = lock_deadlock_check_and_resolve(lock, trx);

	trx_mutex_enter(trx);

	if (victim_trx_id != 0) {
		ut_ad(victim_trx_id == trx->id);

		/* The order here is important, we don't want to
		lose the state of the lock before calling remove. */
		lock_table_remove_low(lock);
		lock_reset_lock_and_trx_wait(lock);

		return(DB_DEADLOCK);
	} else if (trx->lock.wait_lock == NULL) {
		/* Deadlock resolution chose another transaction as a victim,
		and we accidentally got our lock granted! */

		return(DB_SUCCESS);
	}

	trx->lock.que_state = TRX_QUE_LOCK_WAIT;

	trx->lock.wait_started = ut_time();
	trx->lock.was_chosen_as_deadlock_victim = FALSE;

	ut_a(que_thr_stop(thr));

	MONITOR_INC(MONITOR_TABLELOCK_WAIT);

	return(DB_LOCK_WAIT);
}

/*********************************************************************//**
Checks if other transactions have an incompatible mode lock request in
the lock queue.
@return	lock or NULL */
UNIV_INLINE
const lock_t*
lock_table_other_has_incompatible(
/*==============================*/
	const trx_t*		trx,	/*!< in: transaction, or NULL if all
					transactions should be included */
	ulint			wait,	/*!< in: LOCK_WAIT if also
					waiting locks are taken into
					account, or 0 if not */
	const dict_table_t*	table,	/*!< in: table */
	enum lock_mode		mode)	/*!< in: lock mode */
{
	const lock_t*	lock;

	ut_ad(lock_mutex_own());

	for (lock = UT_LIST_GET_LAST(table->locks);
	     lock != NULL;
	     lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock)) {

		if (lock->trx != trx
		    && !lock_mode_compatible(lock_get_mode(lock), mode)
		    && (wait || !lock_get_wait(lock))) {

			return(lock);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Locks the specified database table in the mode given. If the lock cannot
be granted immediately, the query thread is put to wait.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
dberr_t
lock_table(
/*=======*/
	ulint		flags,	/*!< in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	dict_table_t*	table,	/*!< in/out: database table
				in dictionary cache */
	enum lock_mode	mode,	/*!< in: lock mode */
	que_thr_t*	thr)	/*!< in: query thread */
{
	trx_t*		trx;
	dberr_t		err;
	const lock_t*	wait_for;

	ut_ad(table && thr);

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_a(flags == 0);

	trx = thr_get_trx(thr);

	/* Look for equal or stronger locks the same trx already
	has on the table. No need to acquire the lock mutex here
	because only this transacton can add/access table locks
	to/from trx_t::table_locks. */

	if (lock_table_has(trx, table, mode)) {

		return(DB_SUCCESS);
	}

	lock_mutex_enter();

	/* We have to check if the new lock is compatible with any locks
	other transactions have in the table lock queue. */

	wait_for = lock_table_other_has_incompatible(
		trx, LOCK_WAIT, table, mode);

	trx_mutex_enter(trx);

	/* Another trx has a request on the table in an incompatible
	mode: this trx may have to wait */

	if (wait_for != NULL) {
		err = lock_table_enqueue_waiting(mode | flags, table, thr);
	} else {
		lock_table_create(table, mode | flags, trx);

		ut_a(!flags || mode == LOCK_S || mode == LOCK_X);

		err = DB_SUCCESS;
	}

	lock_mutex_exit();

	trx_mutex_exit(trx);

	return(err);
}

/*********************************************************************//**
Checks if a waiting table lock request still has to wait in a queue.
@return	TRUE if still has to wait */
static
ibool
lock_table_has_to_wait_in_queue(
/*============================*/
	const lock_t*	wait_lock)	/*!< in: waiting table lock */
{
	const dict_table_t*	table;
	const lock_t*		lock;

	ut_ad(lock_mutex_own());
	ut_ad(lock_get_wait(wait_lock));

	table = wait_lock->un_member.tab_lock.table;

	for (lock = UT_LIST_GET_FIRST(table->locks);
	     lock != wait_lock;
	     lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock)) {

		if (lock_has_to_wait(wait_lock, lock)) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/*************************************************************//**
Removes a table lock request, waiting or granted, from the queue and grants
locks to other transactions in the queue, if they now are entitled to a
lock. */
static
void
lock_table_dequeue(
/*===============*/
	lock_t*	in_lock)/*!< in/out: table lock object; transactions waiting
			behind will get their lock requests granted, if
			they are now qualified to it */
{
	lock_t*	lock;

	ut_ad(lock_mutex_own());
	ut_a(lock_get_type_low(in_lock) == LOCK_TABLE);

	lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, in_lock);

	lock_table_remove_low(in_lock);

	/* Check if waiting locks in the queue can now be granted: grant
	locks if there are no conflicting locks ahead. */

	for (/* No op */;
	     lock != NULL;
	     lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock)) {

		if (lock_get_wait(lock)
		    && !lock_table_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			ut_ad(in_lock->trx != lock->trx);
			lock_grant(lock);
		}
	}
}

/*=========================== LOCK RELEASE ==============================*/

/*************************************************************//**
Removes a granted record lock of a transaction from the queue and grants
locks to other transactions waiting in the queue if they now are entitled
to a lock. */
UNIV_INTERN
void
lock_rec_unlock(
/*============*/
	trx_t*			trx,	/*!< in/out: transaction that has
					set a record lock */
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record */
	enum lock_mode		lock_mode)/*!< in: LOCK_S or LOCK_X */
{
	lock_t*		first_lock;
	lock_t*		lock;
	ulint		heap_no;
	const char*	stmt;
	size_t		stmt_len;

	ut_ad(trx);
	ut_ad(rec);
	ut_ad(block->frame == page_align(rec));
	ut_ad(!trx->lock.wait_lock);
	ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));

	heap_no = page_rec_get_heap_no(rec);

	lock_mutex_enter();
	trx_mutex_enter(trx);

	first_lock = lock_rec_get_first(block, heap_no);

	/* Find the last lock with the same lock_mode and transaction
	on the record. */

	for (lock = first_lock; lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {
		if (lock->trx == trx && lock_get_mode(lock) == lock_mode) {
			goto released;
		}
	}

	lock_mutex_exit();
	trx_mutex_exit(trx);

	stmt = innobase_get_stmt(trx->mysql_thd, &stmt_len);
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: Error: unlock row could not"
		" find a %lu mode lock on the record\n",
		(ulong) lock_mode);
	ut_print_timestamp(stderr);
	fprintf(stderr, " InnoDB: current statement: %.*s\n",
		(int) stmt_len, stmt);

	return;

released:
	ut_a(!lock_get_wait(lock));
	lock_rec_reset_nth_bit(lock, heap_no);

	/* Check if we can now grant waiting lock requests */

	for (lock = first_lock; lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {
		if (lock_get_wait(lock)
		    && !lock_rec_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			ut_ad(trx != lock->trx);
			lock_grant(lock);
		}
	}

	lock_mutex_exit();
	trx_mutex_exit(trx);
}

/*********************************************************************//**
Releases transaction locks, and releases possible other transactions waiting
because of these locks. */
static
void
lock_release(
/*=========*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	lock_t*		lock;
	ulint		count = 0;
	trx_id_t	max_trx_id;

	ut_ad(lock_mutex_own());
	ut_ad(!trx_mutex_own(trx));

	max_trx_id = trx_sys_get_max_trx_id();

	for (lock = UT_LIST_GET_LAST(trx->lock.trx_locks);
	     lock != NULL;
	     lock = UT_LIST_GET_LAST(trx->lock.trx_locks)) {

		if (lock_get_type_low(lock) == LOCK_REC) {

#ifdef UNIV_DEBUG
			/* Check if the transcation locked a record
			in a system table in X mode. It should have set
			the dict_op code correctly if it did. */
			if (lock->index->table->id < DICT_HDR_FIRST_ID
			    && lock_get_mode(lock) == LOCK_X) {

				ut_ad(lock_get_mode(lock) != LOCK_IX);
				ut_ad(trx->dict_operation != TRX_DICT_OP_NONE);
			}
#endif /* UNIV_DEBUG */

			lock_rec_dequeue_from_page(lock);
		} else {
			dict_table_t*	table;

			table = lock->un_member.tab_lock.table;
#ifdef UNIV_DEBUG
			ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

			/* Check if the transcation locked a system table
			in IX mode. It should have set the dict_op code
			correctly if it did. */
			if (table->id < DICT_HDR_FIRST_ID
			    && (lock_get_mode(lock) == LOCK_X
				|| lock_get_mode(lock) == LOCK_IX)) {

				ut_ad(trx->dict_operation != TRX_DICT_OP_NONE);
			}
#endif /* UNIV_DEBUG */

			if (lock_get_mode(lock) != LOCK_IS
			    && trx->undo_no != 0) {

				/* The trx may have modified the table. We
				block the use of the MySQL query cache for
				all currently active transactions. */

				table->query_cache_inv_trx_id = max_trx_id;
			}

			lock_table_dequeue(lock);
		}

		if (count == LOCK_RELEASE_INTERVAL) {
			/* Release the  mutex for a while, so that we
			do not monopolize it */

			lock_mutex_exit();

			lock_mutex_enter();

			count = 0;
		}

		++count;
	}

	/* We don't remove the locks one by one from the vector for
	efficiency reasons. We simply reset it because we would have
	released all the locks anyway. */

	ib_vector_reset(trx->lock.table_locks);

	ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);
	ut_a(ib_vector_is_empty(trx->autoinc_locks));
	ut_a(ib_vector_is_empty(trx->lock.table_locks));

	mem_heap_empty(trx->lock.lock_heap);
}

/* True if a lock mode is S or X */
#define IS_LOCK_S_OR_X(lock) \
	(lock_get_mode(lock) == LOCK_S \
	 || lock_get_mode(lock) == LOCK_X)

/*********************************************************************//**
Removes table locks of the transaction on a table to be dropped. */
static
void
lock_trx_table_locks_remove(
/*========================*/
	const lock_t*	lock_to_remove)		/*!< in: lock to remove */
{
	lint		i;
	trx_t*		trx = lock_to_remove->trx;

	ut_ad(lock_mutex_own());

	/* It is safe to read this because we are holding the lock mutex */
	if (!trx->lock.cancel) {
		trx_mutex_enter(trx);
	} else {
		ut_ad(trx_mutex_own(trx));
	}

	for (i = ib_vector_size(trx->lock.table_locks) - 1; i >= 0; --i) {
		const lock_t*	lock;

		lock = *static_cast<lock_t**>(
			ib_vector_get(trx->lock.table_locks, i));

		if (lock == NULL) {
			continue;
		}

		ut_a(trx == lock->trx);
		ut_a(lock_get_type_low(lock) & LOCK_TABLE);
		ut_a(lock->un_member.tab_lock.table != NULL);

		if (lock == lock_to_remove) {
			void*	null_var = NULL;
			ib_vector_set(trx->lock.table_locks, i, &null_var);

			if (!trx->lock.cancel) {
				trx_mutex_exit(trx);
			}

			return;
		}
	}

	if (!trx->lock.cancel) {
		trx_mutex_exit(trx);
	}

	/* Lock must exist in the vector. */
	ut_error;
}

/*********************************************************************//**
Removes locks of a transaction on a table to be dropped.
If remove_also_table_sx_locks is TRUE then table-level S and X locks are
also removed in addition to other table-level and record-level locks.
No lock that is going to be removed is allowed to be a wait lock. */
static
void
lock_remove_all_on_table_for_trx(
/*=============================*/
	dict_table_t*	table,			/*!< in: table to be dropped */
	trx_t*		trx,			/*!< in: a transaction */
	ibool		remove_also_table_sx_locks)/*!< in: also removes
						table S and X locks */
{
	lock_t*		lock;
	lock_t*		prev_lock;

	ut_ad(lock_mutex_own());

	for (lock = UT_LIST_GET_LAST(trx->lock.trx_locks);
	     lock != NULL;
	     lock = prev_lock) {

		prev_lock = UT_LIST_GET_PREV(trx_locks, lock);

		if (lock_get_type_low(lock) == LOCK_REC
		    && lock->index->table == table) {
			ut_a(!lock_get_wait(lock));

			lock_rec_discard(lock);
		} else if (lock_get_type_low(lock) & LOCK_TABLE
			   && lock->un_member.tab_lock.table == table
			   && (remove_also_table_sx_locks
			       || !IS_LOCK_S_OR_X(lock))) {

			ut_a(!lock_get_wait(lock));

			lock_trx_table_locks_remove(lock);
			lock_table_remove_low(lock);
		}
	}
}

/*******************************************************************//**
Remove any explicit record locks held by recovering transactions on
the table.
@return number of recovered transactions examined */
static
ulint
lock_remove_recovered_trx_record_locks(
/*===================================*/
	dict_table_t*	table)	/*!< in: check if there are any locks
				held on records in this table or on the
				table itself */
{
	trx_t*		trx;
	ulint		n_recovered_trx = 0;

	ut_a(table != NULL);
	ut_ad(lock_mutex_own());

	mutex_enter(&trx_sys->mutex);

	for (trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		lock_t*	lock;
		lock_t*	next_lock;

		assert_trx_in_rw_list(trx);

		if (!trx->is_recovered) {
			continue;
		}

		/* Because we are holding the lock_sys->mutex,
		implicit locks cannot be converted to explicit ones
		while we are scanning the explicit locks. */

		for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
		     lock != NULL;
		     lock = next_lock) {

			ut_a(lock->trx == trx);

			/* Recovered transactions can't wait on a lock. */

			ut_a(!lock_get_wait(lock));

			/* Recovered transactions don't have any
			table level locks. */

			ut_a(lock_get_type_low(lock) == LOCK_REC);

			next_lock = UT_LIST_GET_NEXT(trx_locks, lock);

			if (lock->index->table == table) {
				lock_rec_discard(lock);
			}
		}

		++n_recovered_trx;
	}

	mutex_exit(&trx_sys->mutex);

	return(n_recovered_trx);
}

/*********************************************************************//**
Removes locks on a table to be dropped or truncated.
If remove_also_table_sx_locks is TRUE then table-level S and X locks are
also removed in addition to other table-level and record-level locks.
No lock, that is going to be removed, is allowed to be a wait lock. */
UNIV_INTERN
void
lock_remove_all_on_table(
/*=====================*/
	dict_table_t*	table,			/*!< in: table to be dropped
						or truncated */
	ibool		remove_also_table_sx_locks)/*!< in: also removes
						table S and X locks */
{
	lock_t*		lock;

	lock_mutex_enter();

	for (lock = UT_LIST_GET_FIRST(table->locks);
	     lock != NULL;
	     /* No op */) {

		lock_t*	prev_lock;

		prev_lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);

		/* If we should remove all locks (remove_also_table_sx_locks
		is TRUE), or if the lock is not table-level S or X lock,
		then check we are not going to remove a wait lock. */
		if (remove_also_table_sx_locks
		    || !(lock_get_type(lock) == LOCK_TABLE
			 && IS_LOCK_S_OR_X(lock))) {

			ut_a(!lock_get_wait(lock));
		}

		lock_remove_all_on_table_for_trx(
			table, lock->trx, remove_also_table_sx_locks);

		if (prev_lock == NULL) {
			if (lock == UT_LIST_GET_FIRST(table->locks)) {
				/* lock was not removed, pick its successor */
				lock = UT_LIST_GET_NEXT(
					un_member.tab_lock.locks, lock);
			} else {
				/* lock was removed, pick the first one */
				lock = UT_LIST_GET_FIRST(table->locks);
			}
		} else if (UT_LIST_GET_NEXT(un_member.tab_lock.locks,
					    prev_lock) != lock) {
			/* If lock was removed by
			lock_remove_all_on_table_for_trx() then pick the
			successor of prev_lock ... */
			lock = UT_LIST_GET_NEXT(
				un_member.tab_lock.locks, prev_lock);
		} else {
			/* ... otherwise pick the successor of lock. */
			lock = UT_LIST_GET_NEXT(
				un_member.tab_lock.locks, lock);
		}
	}

	/* Note: Recovered transactions don't have table level IX or IS locks
	but can have implicit record locks that have been converted to explicit
	record locks. Such record locks cannot be freed by traversing the
	transaction lock list in dict_table_t (as above). */

	if (!lock_sys->rollback_complete
	    && lock_remove_recovered_trx_record_locks(table) == 0) {

		lock_sys->rollback_complete = TRUE;
	}

	lock_mutex_exit();
}

/*===================== VALIDATION AND DEBUGGING  ====================*/

/*********************************************************************//**
Prints info of a table lock. */
UNIV_INTERN
void
lock_table_print(
/*=============*/
	FILE*		file,	/*!< in: file where to print */
	const lock_t*	lock)	/*!< in: table type lock */
{
	ut_ad(lock_mutex_own());
	ut_a(lock_get_type_low(lock) == LOCK_TABLE);

	fputs("TABLE LOCK table ", file);
	ut_print_name(file, lock->trx, TRUE,
		      lock->un_member.tab_lock.table->name);
	fprintf(file, " trx id " TRX_ID_FMT, lock->trx->id);

	if (lock_get_mode(lock) == LOCK_S) {
		fputs(" lock mode S", file);
	} else if (lock_get_mode(lock) == LOCK_X) {
		fputs(" lock mode X", file);
	} else if (lock_get_mode(lock) == LOCK_IS) {
		fputs(" lock mode IS", file);
	} else if (lock_get_mode(lock) == LOCK_IX) {
		fputs(" lock mode IX", file);
	} else if (lock_get_mode(lock) == LOCK_AUTO_INC) {
		fputs(" lock mode AUTO-INC", file);
	} else {
		fprintf(file, " unknown lock mode %lu",
			(ulong) lock_get_mode(lock));
	}

	if (lock_get_wait(lock)) {
		fputs(" waiting", file);
	}

	putc('\n', file);
}

/*********************************************************************//**
Prints info of a record lock. */
UNIV_INTERN
void
lock_rec_print(
/*===========*/
	FILE*		file,	/*!< in: file where to print */
	const lock_t*	lock)	/*!< in: record type lock */
{
	const buf_block_t*	block;
	ulint			space;
	ulint			page_no;
	ulint			i;
	mtr_t			mtr;
	mem_heap_t*		heap		= NULL;
	ulint			offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*			offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_ad(lock_mutex_own());
	ut_a(lock_get_type_low(lock) == LOCK_REC);

	space = lock->un_member.rec_lock.space;
	page_no = lock->un_member.rec_lock.page_no;

	fprintf(file, "RECORD LOCKS space id %lu page no %lu n bits %lu ",
		(ulong) space, (ulong) page_no,
		(ulong) lock_rec_get_n_bits(lock));
	dict_index_name_print(file, lock->trx, lock->index);
	fprintf(file, " trx id " TRX_ID_FMT, lock->trx->id);

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

	block = buf_page_try_get(space, page_no, &mtr);

	for (i = 0; i < lock_rec_get_n_bits(lock); ++i) {

		if (!lock_rec_get_nth_bit(lock, i)) {
			continue;
		}

		fprintf(file, "Record lock, heap no %lu", (ulong) i);

		if (block) {
			const rec_t*	rec;

			rec = page_find_rec_with_heap_no(
				buf_block_get_frame(block), i);

			offsets = rec_get_offsets(
				rec, lock->index, offsets,
				ULINT_UNDEFINED, &heap);

			putc(' ', file);
			rec_print_new(file, rec, offsets);
		}

		putc('\n', file);
	}

	mtr_commit(&mtr);
	if (UNIV_LIKELY_NULL(heap)) {
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
/*********************************************************************//**
Calculates the number of record lock structs in the record lock hash table.
@return	number of record locks */
static
ulint
lock_get_n_rec_locks(void)
/*======================*/
{
	ulint	n_locks	= 0;
	ulint	i;

	ut_ad(lock_mutex_own());

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
		const lock_t*	lock;

		for (lock = static_cast<const lock_t*>(
				HASH_GET_FIRST(lock_sys->rec_hash, i));
		     lock != 0;
		     lock = static_cast<const lock_t*>(
				HASH_GET_NEXT(hash, lock))) {

			n_locks++;
		}
	}

	return(n_locks);
}
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */

/*********************************************************************//**
Prints info of locks for all transactions.
@return FALSE if not able to obtain lock mutex
and exits without printing info */
UNIV_INTERN
ibool
lock_print_info_summary(
/*====================*/
	FILE*	file,	/*!< in: file where to print */
	ibool   nowait)	/*!< in: whether to wait for the lock mutex */
{
	/* if nowait is FALSE, wait on the lock mutex,
	otherwise return immediately if fail to obtain the
	mutex. */
	if (!nowait) {
		lock_mutex_enter();
	} else if (lock_mutex_enter_nowait()) {
		fputs("FAIL TO OBTAIN LOCK MUTEX, "
		      "SKIP LOCK INFO PRINTING\n", file);
		return(FALSE);
	}

	if (lock_deadlock_found) {
		fputs("------------------------\n"
		      "LATEST DETECTED DEADLOCK\n"
		      "------------------------\n", file);

		if (!srv_read_only_mode) {
			ut_copy_file(file, lock_latest_err_file);
		}
	}

	fputs("------------\n"
	      "TRANSACTIONS\n"
	      "------------\n", file);

	fprintf(file, "Trx id counter " TRX_ID_FMT "\n",
		trx_sys_get_max_trx_id());

	fprintf(file,
		"Purge done for trx's n:o < " TRX_ID_FMT
		" undo n:o < " TRX_ID_FMT " state: ",
		purge_sys->iter.trx_no,
		purge_sys->iter.undo_no);

	/* Note: We are reading the state without the latch. One because it
	will violate the latching order and two because we are merely querying
	the state of the variable for display. */

	switch (purge_sys->state){
	case PURGE_STATE_EXIT:
	case PURGE_STATE_INIT:
		/* Should never be in this state while the system is running. */
		ut_error;

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

	fprintf(file,
		"History list length %lu\n",
		(ulong) trx_sys->rseg_history_len);

#ifdef PRINT_NUM_OF_LOCK_STRUCTS
	fprintf(file,
		"Total number of lock structs in row lock hash table %lu\n",
		(ulong) lock_get_n_rec_locks());
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */
	return(TRUE);
}

/*********************************************************************//**
Prints info of locks for each transaction. This function assumes that the
caller holds the lock mutex and more importantly it will release the lock
mutex on behalf of the caller. (This should be fixed in the future). */
UNIV_INTERN
void
lock_print_info_all_transactions(
/*=============================*/
	FILE*	file)	/*!< in: file where to print */
{
	const lock_t*	lock;
	ibool		load_page_first = TRUE;
	ulint		nth_trx		= 0;
	ulint		nth_lock	= 0;
	ulint		i;
	mtr_t		mtr;
	const trx_t*	trx;
	trx_list_t*	trx_list = &trx_sys->rw_trx_list;

	fprintf(file, "LIST OF TRANSACTIONS FOR EACH SESSION:\n");

	ut_ad(lock_mutex_own());

	mutex_enter(&trx_sys->mutex);

	/* First print info on non-active transactions */

	/* NOTE: information of auto-commit non-locking read-only
	transactions will be omitted here. The information will be
	available from INFORMATION_SCHEMA.INNODB_TRX. */

	for (trx = UT_LIST_GET_FIRST(trx_sys->mysql_trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(mysql_trx_list, trx)) {

		ut_ad(trx->in_mysql_trx_list);

		/* See state transitions and locking rules in trx0trx.h */

		if (trx_state_eq(trx, TRX_STATE_NOT_STARTED)) {
			fputs("---", file);
			trx_print_latched(file, trx, 600);
		}
	}

loop:
	/* Since we temporarily release lock_sys->mutex and
	trx_sys->mutex when reading a database page in below,
	variable trx may be obsolete now and we must loop
	through the trx list to get probably the same trx,
	or some other trx. */

	for (trx = UT_LIST_GET_FIRST(*trx_list), i = 0;
	     trx && (i < nth_trx);
	     trx = UT_LIST_GET_NEXT(trx_list, trx), i++) {

		assert_trx_in_list(trx);
		ut_ad(trx->read_only == (trx_list == &trx_sys->ro_trx_list));
	}

	ut_ad(trx == NULL
	      || trx->read_only == (trx_list == &trx_sys->ro_trx_list));

	if (trx == NULL) {
		/* Check the read-only transaction list next. */
		if (trx_list == &trx_sys->rw_trx_list) {
			trx_list = &trx_sys->ro_trx_list;
			nth_trx = 0;
			nth_lock = 0;
			goto loop;
		}

		lock_mutex_exit();
		mutex_exit(&trx_sys->mutex);

		ut_ad(lock_validate());

		return;
	}

	assert_trx_in_list(trx);

	if (nth_lock == 0) {
		fputs("---", file);

		trx_print_latched(file, trx, 600);

		if (trx->read_view) {
			fprintf(file,
				"Trx read view will not see trx with"
				" id >= " TRX_ID_FMT
				", sees < " TRX_ID_FMT "\n",
				trx->read_view->low_limit_id,
				trx->read_view->up_limit_id);
		}

		if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {

			fprintf(file,
				"------- TRX HAS BEEN WAITING %lu SEC"
				" FOR THIS LOCK TO BE GRANTED:\n",
				(ulong) difftime(ut_time(),
						 trx->lock.wait_started));

			if (lock_get_type_low(trx->lock.wait_lock) == LOCK_REC) {
				lock_rec_print(file, trx->lock.wait_lock);
			} else {
				lock_table_print(file, trx->lock.wait_lock);
			}

			fputs("------------------\n", file);
		}
	}

	if (!srv_print_innodb_lock_monitor) {
		nth_trx++;
		goto loop;
	}

	i = 0;

	/* Look at the note about the trx loop above why we loop here:
	lock may be an obsolete pointer now. */

	lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);

	while (lock && (i < nth_lock)) {
		lock = UT_LIST_GET_NEXT(trx_locks, lock);
		i++;
	}

	if (lock == NULL) {
		nth_trx++;
		nth_lock = 0;

		goto loop;
	}

	if (lock_get_type_low(lock) == LOCK_REC) {
		if (load_page_first) {
			ulint	space	= lock->un_member.rec_lock.space;
			ulint	zip_size= fil_space_get_zip_size(space);
			ulint	page_no = lock->un_member.rec_lock.page_no;

			if (UNIV_UNLIKELY(zip_size == ULINT_UNDEFINED)) {

				/* It is a single table tablespace and
				the .ibd file is missing (TRUNCATE
				TABLE probably stole the locks): just
				print the lock without attempting to
				load the page in the buffer pool. */

				fprintf(file, "RECORD LOCKS on"
					" non-existing space %lu\n",
					(ulong) space);
				goto print_rec;
			}

			lock_mutex_exit();
			mutex_exit(&trx_sys->mutex);

			mtr_start(&mtr);

			buf_page_get_with_no_latch(
				space, zip_size, page_no, &mtr);

			mtr_commit(&mtr);

			load_page_first = FALSE;

			lock_mutex_enter();

			mutex_enter(&trx_sys->mutex);

			goto loop;
		}

print_rec:
		lock_rec_print(file, lock);
	} else {
		ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

		lock_table_print(file, lock);
	}

	load_page_first = TRUE;

	nth_lock++;

	if (nth_lock >= 10) {
		fputs("10 LOCKS PRINTED FOR THIS TRX:"
		      " SUPPRESSING FURTHER PRINTS\n",
		      file);

		nth_trx++;
		nth_lock = 0;
	}

	goto loop;
}

#ifdef UNIV_DEBUG
/*********************************************************************//**
Find the the lock in the trx_t::trx_lock_t::table_locks vector.
@return TRUE if found */
static
ibool
lock_trx_table_locks_find(
/*======================*/
	trx_t*		trx,		/*!< in: trx to validate */
	const lock_t*	find_lock)	/*!< in: lock to find */
{
	lint		i;
	ibool		found = FALSE;

	trx_mutex_enter(trx);

	for (i = ib_vector_size(trx->lock.table_locks) - 1; i >= 0; --i) {
		const lock_t*	lock;

		lock = *static_cast<const lock_t**>(
			ib_vector_get(trx->lock.table_locks, i));

		if (lock == NULL) {
			continue;
		} else if (lock == find_lock) {
			/* Can't be duplicates. */
			ut_a(!found);
			found = TRUE;
		}

		ut_a(trx == lock->trx);
		ut_a(lock_get_type_low(lock) & LOCK_TABLE);
		ut_a(lock->un_member.tab_lock.table != NULL);
	}

	trx_mutex_exit(trx);

	return(found);
}

/*********************************************************************//**
Validates the lock queue on a table.
@return	TRUE if ok */
static
ibool
lock_table_queue_validate(
/*======================*/
	const dict_table_t*	table)	/*!< in: table */
{
	const lock_t*	lock;

	ut_ad(lock_mutex_own());
	ut_ad(mutex_own(&trx_sys->mutex));

	for (lock = UT_LIST_GET_FIRST(table->locks);
	     lock != NULL;
	     lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock)) {

		/* lock->trx->state cannot change from or to NOT_STARTED
		while we are holding the trx_sys->mutex. It may change
		from ACTIVE to PREPARED, but it may not change to
		COMMITTED, because we are holding the lock_sys->mutex. */
		ut_ad(trx_assert_started(lock->trx));

		if (!lock_get_wait(lock)) {

			ut_a(!lock_table_other_has_incompatible(
				     lock->trx, 0, table,
				     lock_get_mode(lock)));
		} else {

			ut_a(lock_table_has_to_wait_in_queue(lock));
		}

		ut_a(lock_trx_table_locks_find(lock->trx, lock));
	}

	return(TRUE);
}

/*********************************************************************//**
Validates the lock queue on a single record.
@return	TRUE if ok */
static
ibool
lock_rec_queue_validate(
/*====================*/
	ibool			locked_lock_trx_sys,
					/*!< in: if the caller holds
					both the lock mutex and
					trx_sys_t->lock. */
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record to look at */
	const dict_index_t*	index,	/*!< in: index, or NULL if not known */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
{
	const trx_t*	impl_trx;
	const lock_t*	lock;
	ulint		heap_no;

	ut_a(rec);
	ut_a(block->frame == page_align(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));
	ut_ad(lock_mutex_own() == locked_lock_trx_sys);
	ut_ad(!index || dict_index_is_clust(index)
	      || !dict_index_is_online_ddl(index));

	heap_no = page_rec_get_heap_no(rec);

	if (!locked_lock_trx_sys) {
		lock_mutex_enter();
		mutex_enter(&trx_sys->mutex);
	}

	if (!page_rec_is_user_rec(rec)) {

		for (lock = lock_rec_get_first(block, heap_no);
		     lock != NULL;
		     lock = lock_rec_get_next_const(heap_no, lock)) {

			ut_a(trx_in_trx_list(lock->trx));

			if (lock_get_wait(lock)) {
				ut_a(lock_rec_has_to_wait_in_queue(lock));
			}

			if (index) {
				ut_a(lock->index == index);
			}
		}

		goto func_exit;
	}

	if (!index);
	else if (dict_index_is_clust(index)) {
		trx_id_t	trx_id;

		/* Unlike the non-debug code, this invariant can only succeed
		if the check and assertion are covered by the lock mutex. */

		trx_id = lock_clust_rec_some_has_impl(rec, index, offsets);
		impl_trx = trx_rw_is_active_low(trx_id, NULL);

		ut_ad(lock_mutex_own());
		/* impl_trx cannot be committed until lock_mutex_exit()
		because lock_trx_release_locks() acquires lock_sys->mutex */

		if (impl_trx != NULL
		    && lock_rec_other_has_expl_req(LOCK_S, 0, LOCK_WAIT,
						   block, heap_no, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP,
					       block, heap_no, impl_trx));
		}
	}

	for (lock = lock_rec_get_first(block, heap_no);
	     lock != NULL;
	     lock = lock_rec_get_next_const(heap_no, lock)) {

		ut_a(trx_in_trx_list(lock->trx));

		if (index) {
			ut_a(lock->index == index);
		}

		if (!lock_rec_get_gap(lock) && !lock_get_wait(lock)) {

			enum lock_mode	mode;

			if (lock_get_mode(lock) == LOCK_S) {
				mode = LOCK_X;
			} else {
				mode = LOCK_S;
			}
			ut_a(!lock_rec_other_has_expl_req(
				     mode, 0, 0, block, heap_no, lock->trx));

		} else if (lock_get_wait(lock) && !lock_rec_get_gap(lock)) {

			ut_a(lock_rec_has_to_wait_in_queue(lock));
		}
	}

func_exit:
	if (!locked_lock_trx_sys) {
		lock_mutex_exit();
		mutex_exit(&trx_sys->mutex);
	}

	return(TRUE);
}

/*********************************************************************//**
Validates the record lock queues on a page.
@return	TRUE if ok */
static
ibool
lock_rec_validate_page(
/*===================*/
	const buf_block_t*	block)	/*!< in: buffer block */
{
	const lock_t*	lock;
	const rec_t*	rec;
	ulint		nth_lock	= 0;
	ulint		nth_bit		= 0;
	ulint		i;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_ad(!lock_mutex_own());

	lock_mutex_enter();
	mutex_enter(&trx_sys->mutex);
loop:
	lock = lock_rec_get_first_on_page_addr(buf_block_get_space(block),
					       buf_block_get_page_no(block));

	if (!lock) {
		goto function_exit;
	}

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	ut_a(!block->page.file_page_was_freed);
#endif

	for (i = 0; i < nth_lock; i++) {

		lock = lock_rec_get_next_on_page_const(lock);

		if (!lock) {
			goto function_exit;
		}
	}

	ut_a(trx_in_trx_list(lock->trx));

# ifdef UNIV_SYNC_DEBUG
	/* Only validate the record queues when this thread is not
	holding a space->latch.  Deadlocks are possible due to
	latching order violation when UNIV_DEBUG is defined while
	UNIV_SYNC_DEBUG is not. */
	if (!sync_thread_levels_contains(SYNC_FSP))
# endif /* UNIV_SYNC_DEBUG */
	for (i = nth_bit; i < lock_rec_get_n_bits(lock); i++) {

		if (i == 1 || lock_rec_get_nth_bit(lock, i)) {

			rec = page_find_rec_with_heap_no(block->frame, i);
			ut_a(rec);
			offsets = rec_get_offsets(rec, lock->index, offsets,
						  ULINT_UNDEFINED, &heap);
#if 0
			fprintf(stderr,
				"Validating %u %u\n",
				block->page.space, block->page.offset);
#endif
			/* If this thread is holding the file space
			latch (fil_space_t::latch), the following
			check WILL break the latching order and may
			cause a deadlock of threads. */

			lock_rec_queue_validate(
				TRUE, block, rec, lock->index, offsets);

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

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(TRUE);
}

/*********************************************************************//**
Validates the table locks.
@return	TRUE if ok */
static
ibool
lock_validate_table_locks(
/*======================*/
	const trx_list_t*	trx_list)	/*!< in: trx list */
{
	const trx_t*	trx;

	ut_ad(lock_mutex_own());
	ut_ad(mutex_own(&trx_sys->mutex));

	ut_ad(trx_list == &trx_sys->rw_trx_list
	      || trx_list == &trx_sys->ro_trx_list);

	for (trx = UT_LIST_GET_FIRST(*trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		const lock_t*	lock;

		assert_trx_in_list(trx);
		ut_ad(trx->read_only == (trx_list == &trx_sys->ro_trx_list));

		for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
		     lock != NULL;
		     lock = UT_LIST_GET_NEXT(trx_locks, lock)) {

			if (lock_get_type_low(lock) & LOCK_TABLE) {

				lock_table_queue_validate(
					lock->un_member.tab_lock.table);
			}
		}
	}

	return(TRUE);
}

/*********************************************************************//**
Validate record locks up to a limit.
@return lock at limit or NULL if no more locks in the hash bucket */
static __attribute__((nonnull, warn_unused_result))
const lock_t*
lock_rec_validate(
/*==============*/
	ulint		start,		/*!< in: lock_sys->rec_hash
					bucket */
	ib_uint64_t*	limit)		/*!< in/out: upper limit of
					(space, page_no) */
{
	ut_ad(lock_mutex_own());
	ut_ad(mutex_own(&trx_sys->mutex));

	for (const lock_t* lock = static_cast<const lock_t*>(
			HASH_GET_FIRST(lock_sys->rec_hash, start));
	     lock != NULL;
	     lock = static_cast<const lock_t*>(HASH_GET_NEXT(hash, lock))) {

		ib_uint64_t	current;

		ut_a(trx_in_trx_list(lock->trx));
		ut_a(lock_get_type(lock) == LOCK_REC);

		current = ut_ull_create(
			lock->un_member.rec_lock.space,
			lock->un_member.rec_lock.page_no);

		if (current > *limit) {
			*limit = current + 1;
			return(lock);
		}
	}

	return(0);
}

/*********************************************************************//**
Validate a record lock's block */
static
void
lock_rec_block_validate(
/*====================*/
	ulint		space,
	ulint		page_no)
{
	/* The lock and the block that it is referring to may be freed at
	this point. We pass BUF_GET_POSSIBLY_FREED to skip a debug check.
	If the lock exists in lock_rec_validate_page() we assert
	!block->page.file_page_was_freed. */

	buf_block_t*	block;
	mtr_t		mtr;

	/* Make sure that the tablespace is not deleted while we are
	trying to access the page. */
	if (!fil_inc_pending_ops(space)) {
		mtr_start(&mtr);
		block = buf_page_get_gen(
			space, fil_space_get_zip_size(space),
			page_no, RW_X_LATCH, NULL,
			BUF_GET_POSSIBLY_FREED,
			__FILE__, __LINE__, &mtr);

		buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

		ut_ad(lock_rec_validate_page(block));
		mtr_commit(&mtr);

		fil_decr_pending_ops(space);
	}
}

/*********************************************************************//**
Validates the lock system.
@return	TRUE if ok */
static
bool
lock_validate()
/*===========*/
{
	lock_mutex_enter();

	mutex_enter(&trx_sys->mutex);

	ut_a(lock_validate_table_locks(&trx_sys->rw_trx_list));
	ut_a(lock_validate_table_locks(&trx_sys->ro_trx_list));

	/* Iterate over all the record locks and validate the locks. We
	don't want to hog the lock_sys_t::mutex and the trx_sys_t::mutex.
	Release both mutexes during the validation check. */

	for (ulint i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
		const lock_t*	lock;
		ib_uint64_t	limit = 0;

		while ((lock = lock_rec_validate(i, &limit)) != 0) {

			ulint	space = lock->un_member.rec_lock.space;
			ulint	page_no = lock->un_member.rec_lock.page_no;

			lock_mutex_exit();
			mutex_exit(&trx_sys->mutex);

			lock_rec_block_validate(space, page_no);

			lock_mutex_enter();
			mutex_enter(&trx_sys->mutex);
		}
	}

	mutex_exit(&trx_sys->mutex);

	lock_mutex_exit();

	return(true);
}
#endif /* UNIV_DEBUG */
/*============ RECORD LOCK CHECKS FOR ROW OPERATIONS ====================*/

/*********************************************************************//**
Checks if locks of other transactions prevent an immediate insert of
a record. If they do, first tests if the query thread should anyway
be suspended for some reason; if not, then puts the transaction and
the query thread to the lock wait state and inserts a waiting request
for a gap x-lock to the lock queue.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
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
{
	const rec_t*	next_rec;
	trx_t*		trx;
	lock_t*		lock;
	dberr_t		err;
	ulint		next_rec_heap_no;

	ut_ad(block->frame == page_align(rec));
	ut_ad(!dict_index_is_online_ddl(index)
	      || dict_index_is_clust(index)
	      || (flags & BTR_CREATE_FLAG));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	trx = thr_get_trx(thr);
	next_rec = page_rec_get_next_const(rec);
	next_rec_heap_no = page_rec_get_heap_no(next_rec);

	lock_mutex_enter();
	/* Because this code is invoked for a running transaction by
	the thread that is serving the transaction, it is not necessary
	to hold trx->mutex here. */

	/* When inserting a record into an index, the table must be at
	least IX-locked. When we are building an index, we would pass
	BTR_NO_LOCKING_FLAG and skip the locking altogether. */
	ut_ad(lock_table_has(trx, index->table, LOCK_IX));

	lock = lock_rec_get_first(block, next_rec_heap_no);

	if (UNIV_LIKELY(lock == NULL)) {
		/* We optimize CPU time usage in the simplest case */

		lock_mutex_exit();

		if (!dict_index_is_clust(index)) {
			/* Update the page max trx id field */
			page_update_max_trx_id(block,
					       buf_block_get_page_zip(block),
					       trx->id, mtr);
		}

		*inherit = FALSE;

		return(DB_SUCCESS);
	}

	*inherit = TRUE;

	/* If another transaction has an explicit lock request which locks
	the gap, waiting or granted, on the successor, the insert has to wait.

	An exception is the case where the lock by the another transaction
	is a gap type lock which it placed to wait for its turn to insert. We
	do not consider that kind of a lock conflicting with our insert. This
	eliminates an unnecessary deadlock which resulted when 2 transactions
	had to wait for their insert. Both had waiting gap type lock requests
	on the successor, which produced an unnecessary deadlock. */

	if (lock_rec_other_has_conflicting(
		    static_cast<enum lock_mode>(
			    LOCK_X | LOCK_GAP | LOCK_INSERT_INTENTION),
		    block, next_rec_heap_no, trx)) {

		/* Note that we may get DB_SUCCESS also here! */
		trx_mutex_enter(trx);

		err = lock_rec_enqueue_waiting(
			LOCK_X | LOCK_GAP | LOCK_INSERT_INTENTION,
			block, next_rec_heap_no, NULL, index, thr);

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
		if (dict_index_is_clust(index)) {
			break;
		}
		/* Update the page max trx id field */
		page_update_max_trx_id(block,
				       buf_block_get_page_zip(block),
				       trx->id, mtr);
	default:
		/* We only care about the two return values. */
		break;
	}

#ifdef UNIV_DEBUG
	{
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		const ulint*	offsets;
		rec_offs_init(offsets_);

		offsets = rec_get_offsets(next_rec, index, offsets_,
					  ULINT_UNDEFINED, &heap);

		ut_ad(lock_rec_queue_validate(
				FALSE, block, next_rec, index, offsets));

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}
#endif /* UNIV_DEBUG */

	return(err);
}

/*********************************************************************//**
If a transaction has an implicit x-lock on a record, but no explicit x-lock
set on the record, sets one for it. */
static
void
lock_rec_convert_impl_to_expl(
/*==========================*/
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: user record on page */
	dict_index_t*		index,	/*!< in: index of record */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
{
	trx_id_t		trx_id;

	ut_ad(!lock_mutex_own());
	ut_ad(page_rec_is_user_rec(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));

	if (dict_index_is_clust(index)) {
		trx_id = lock_clust_rec_some_has_impl(rec, index, offsets);
		/* The clustered index record was last modified by
		this transaction. The transaction may have been
		committed a long time ago. */
	} else {
		ut_ad(!dict_index_is_online_ddl(index));
		trx_id = lock_sec_rec_some_has_impl(rec, index, offsets);
		/* The transaction can be committed before the
		trx_is_active(trx_id, NULL) check below, because we are not
		holding lock_mutex. */
	}

	if (trx_id != 0) {
		trx_t*	impl_trx;
		ulint	heap_no = page_rec_get_heap_no(rec);

		lock_mutex_enter();

		/* If the transaction is still active and has no
		explicit x-lock set on the record, set one for it */

		impl_trx = trx_rw_is_active(trx_id, NULL);

		/* impl_trx cannot be committed until lock_mutex_exit()
		because lock_trx_release_locks() acquires lock_sys->mutex */

		if (impl_trx != NULL
		    && !lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block,
				       heap_no, impl_trx)) {
			ulint	type_mode = (LOCK_REC | LOCK_X
					     | LOCK_REC_NOT_GAP);

			/* If the delete-marked record was locked already,
			we should reserve lock waiting for impl_trx as
			implicit lock. Because cannot lock at this moment.*/

			if (rec_get_deleted_flag(rec, rec_offs_comp(offsets))
			    && lock_rec_other_has_conflicting(
					static_cast<enum lock_mode>
					(LOCK_X | LOCK_REC_NOT_GAP), block,
					heap_no, impl_trx)) {

				type_mode |= (LOCK_WAIT
					      | LOCK_CONV_BY_OTHER);
			}

			lock_rec_add_to_queue(
				type_mode, block, heap_no, index,
				impl_trx, FALSE);
		}

		lock_mutex_exit();
	}
}

/*********************************************************************//**
Checks if locks of other transactions prevent an immediate modify (update,
delete mark, or delete unmark) of a clustered index record. If they do,
first tests if the query thread should anyway be suspended for some
reason; if not, then puts the transaction and the query thread to the
lock wait state and inserts a waiting request for a record x-lock to the
lock queue.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
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
{
	dberr_t	err;
	ulint	heap_no;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(dict_index_is_clust(index));
	ut_ad(block->frame == page_align(rec));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	heap_no = rec_offs_comp(offsets)
		? rec_get_heap_no_new(rec)
		: rec_get_heap_no_old(rec);

	/* If a transaction has no explicit x-lock set on the record, set one
	for it */

	lock_rec_convert_impl_to_expl(block, rec, index, offsets);

	lock_mutex_enter();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	err = lock_rec_lock(TRUE, LOCK_X | LOCK_REC_NOT_GAP,
			    block, heap_no, index, thr);

	MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);

	lock_mutex_exit();

	ut_ad(lock_rec_queue_validate(FALSE, block, rec, index, offsets));

	if (UNIV_UNLIKELY(err == DB_SUCCESS_LOCKED_REC)) {
		err = DB_SUCCESS;
	}

	return(err);
}

/*********************************************************************//**
Checks if locks of other transactions prevent an immediate modify (delete
mark or delete unmark) of a secondary index record.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
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
{
	dberr_t	err;
	ulint	heap_no;

	ut_ad(!dict_index_is_clust(index));
	ut_ad(!dict_index_is_online_ddl(index) || (flags & BTR_CREATE_FLAG));
	ut_ad(block->frame == page_align(rec));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	heap_no = page_rec_get_heap_no(rec);

	/* Another transaction cannot have an implicit lock on the record,
	because when we come here, we already have modified the clustered
	index record, and this would not have been possible if another active
	transaction had modified this secondary index record. */

	lock_mutex_enter();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	err = lock_rec_lock(TRUE, LOCK_X | LOCK_REC_NOT_GAP,
			    block, heap_no, index, thr);

	MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);

	lock_mutex_exit();

#ifdef UNIV_DEBUG
	{
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		const ulint*	offsets;
		rec_offs_init(offsets_);

		offsets = rec_get_offsets(rec, index, offsets_,
					  ULINT_UNDEFINED, &heap);

		ut_ad(lock_rec_queue_validate(
			FALSE, block, rec, index, offsets));

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}
#endif /* UNIV_DEBUG */

	if (err == DB_SUCCESS || err == DB_SUCCESS_LOCKED_REC) {
		/* Update the page max trx id field */
		/* It might not be necessary to do this if
		err == DB_SUCCESS (no new lock created),
		but it should not cost too much performance. */
		page_update_max_trx_id(block,
				       buf_block_get_page_zip(block),
				       thr_get_trx(thr)->id, mtr);
		err = DB_SUCCESS;
	}

	return(err);
}

/*********************************************************************//**
Like lock_clust_rec_read_check_and_lock(), but reads a
secondary index record.
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
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
	enum lock_mode		mode,	/*!< in: mode of the lock which
					the read cursor should set on
					records: LOCK_S or LOCK_X; the
					latter is possible in
					SELECT FOR UPDATE */
	ulint			gap_mode,/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP */
	que_thr_t*		thr)	/*!< in: query thread */
{
	dberr_t	err;
	ulint	heap_no;

	ut_ad(!dict_index_is_clust(index));
	ut_ad(!dict_index_is_online_ddl(index));
	ut_ad(block->frame == page_align(rec));
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	heap_no = page_rec_get_heap_no(rec);

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list or a
	database recovery is running. */

	if ((page_get_max_trx_id(block->frame) >= trx_rw_min_trx_id()
	     || recv_recovery_is_on())
	    && !page_rec_is_supremum(rec)) {

		lock_rec_convert_impl_to_expl(block, rec, index, offsets);
	}

	lock_mutex_enter();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

	err = lock_rec_lock(FALSE, mode | gap_mode,
			    block, heap_no, index, thr);

	MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);

	lock_mutex_exit();

	ut_ad(lock_rec_queue_validate(FALSE, block, rec, index, offsets));

	return(err);
}

/*********************************************************************//**
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record.
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
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
	enum lock_mode		mode,	/*!< in: mode of the lock which
					the read cursor should set on
					records: LOCK_S or LOCK_X; the
					latter is possible in
					SELECT FOR UPDATE */
	ulint			gap_mode,/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP */
	que_thr_t*		thr)	/*!< in: query thread */
{
	dberr_t	err;
	ulint	heap_no;

	ut_ad(dict_index_is_clust(index));
	ut_ad(block->frame == page_align(rec));
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
	ut_ad(gap_mode == LOCK_ORDINARY || gap_mode == LOCK_GAP
	      || gap_mode == LOCK_REC_NOT_GAP);
	ut_ad(rec_offs_validate(rec, index, offsets));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	heap_no = page_rec_get_heap_no(rec);

	if (UNIV_LIKELY(heap_no != PAGE_HEAP_NO_SUPREMUM)) {

		lock_rec_convert_impl_to_expl(block, rec, index, offsets);
	}

	lock_mutex_enter();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

	err = lock_rec_lock(FALSE, mode | gap_mode,
			    block, heap_no, index, thr);

	MONITOR_INC(MONITOR_NUM_RECLOCK_REQ);

	lock_mutex_exit();

	ut_ad(lock_rec_queue_validate(FALSE, block, rec, index, offsets));

	return(err);
}
/*********************************************************************//**
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record. This is an alternative version of
lock_clust_rec_read_check_and_lock() that does not require the parameter
"offsets".
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
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
	enum lock_mode		mode,	/*!< in: mode of the lock which
					the read cursor should set on
					records: LOCK_S or LOCK_X; the
					latter is possible in
					SELECT FOR UPDATE */
	ulint			gap_mode,/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP */
	que_thr_t*		thr)	/*!< in: query thread */
{
	mem_heap_t*	tmp_heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	dberr_t		err;
	rec_offs_init(offsets_);

	offsets = rec_get_offsets(rec, index, offsets,
				  ULINT_UNDEFINED, &tmp_heap);
	err = lock_clust_rec_read_check_and_lock(flags, block, rec, index,
						 offsets, mode, gap_mode, thr);
	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	if (UNIV_UNLIKELY(err == DB_SUCCESS_LOCKED_REC)) {
		err = DB_SUCCESS;
	}

	return(err);
}

/*******************************************************************//**
Release the last lock from the transaction's autoinc locks. */
UNIV_INLINE
void
lock_release_autoinc_last_lock(
/*===========================*/
	ib_vector_t*	autoinc_locks)	/*!< in/out: vector of AUTOINC locks */
{
	ulint		last;
	lock_t*		lock;

	ut_ad(lock_mutex_own());
	ut_a(!ib_vector_is_empty(autoinc_locks));

	/* The lock to be release must be the last lock acquired. */
	last = ib_vector_size(autoinc_locks) - 1;
	lock = *static_cast<lock_t**>(ib_vector_get(autoinc_locks, last));

	/* Should have only AUTOINC locks in the vector. */
	ut_a(lock_get_mode(lock) == LOCK_AUTO_INC);
	ut_a(lock_get_type(lock) == LOCK_TABLE);

	ut_a(lock->un_member.tab_lock.table != NULL);

	/* This will remove the lock from the trx autoinc_locks too. */
	lock_table_dequeue(lock);

	/* Remove from the table vector too. */
	lock_trx_table_locks_remove(lock);
}

/*******************************************************************//**
Check if a transaction holds any autoinc locks.
@return TRUE if the transaction holds any AUTOINC locks. */
static
ibool
lock_trx_holds_autoinc_locks(
/*=========================*/
	const trx_t*	trx)		/*!< in: transaction */
{
	ut_a(trx->autoinc_locks != NULL);

	return(!ib_vector_is_empty(trx->autoinc_locks));
}

/*******************************************************************//**
Release all the transaction's autoinc locks. */
static
void
lock_release_autoinc_locks(
/*=======================*/
	trx_t*		trx)		/*!< in/out: transaction */
{
	ut_ad(lock_mutex_own());
	/* If this is invoked for a running transaction by the thread
	that is serving the transaction, then it is not necessary to
	hold trx->mutex here. */

	ut_a(trx->autoinc_locks != NULL);

	/* We release the locks in the reverse order. This is to
	avoid searching the vector for the element to delete at
	the lower level. See (lock_table_remove_low()) for details. */
	while (!ib_vector_is_empty(trx->autoinc_locks)) {

		/* lock_table_remove_low() will also remove the lock from
		the transaction's autoinc_locks vector. */
		lock_release_autoinc_last_lock(trx->autoinc_locks);
	}

	/* Should release all locks. */
	ut_a(ib_vector_is_empty(trx->autoinc_locks));
}

/*******************************************************************//**
Gets the type of a lock. Non-inline version for using outside of the
lock module.
@return	LOCK_TABLE or LOCK_REC */
UNIV_INTERN
ulint
lock_get_type(
/*==========*/
	const lock_t*	lock)	/*!< in: lock */
{
	return(lock_get_type_low(lock));
}

/*******************************************************************//**
Gets the id of the transaction owning a lock.
@return	transaction id */
UNIV_INTERN
trx_id_t
lock_get_trx_id(
/*============*/
	const lock_t*	lock)	/*!< in: lock */
{
	return(lock->trx->id);
}

/*******************************************************************//**
Gets the mode of a lock in a human readable string.
The string should not be free()'d or modified.
@return	lock mode */
UNIV_INTERN
const char*
lock_get_mode_str(
/*==============*/
	const lock_t*	lock)	/*!< in: lock */
{
	ibool	is_gap_lock;

	is_gap_lock = lock_get_type_low(lock) == LOCK_REC
		&& lock_rec_get_gap(lock);

	switch (lock_get_mode(lock)) {
	case LOCK_S:
		if (is_gap_lock) {
			return("S,GAP");
		} else {
			return("S");
		}
	case LOCK_X:
		if (is_gap_lock) {
			return("X,GAP");
		} else {
			return("X");
		}
	case LOCK_IS:
		if (is_gap_lock) {
			return("IS,GAP");
		} else {
			return("IS");
		}
	case LOCK_IX:
		if (is_gap_lock) {
			return("IX,GAP");
		} else {
			return("IX");
		}
	case LOCK_AUTO_INC:
		return("AUTO_INC");
	default:
		return("UNKNOWN");
	}
}

/*******************************************************************//**
Gets the type of a lock in a human readable string.
The string should not be free()'d or modified.
@return	lock type */
UNIV_INTERN
const char*
lock_get_type_str(
/*==============*/
	const lock_t*	lock)	/*!< in: lock */
{
	switch (lock_get_type_low(lock)) {
	case LOCK_REC:
		return("RECORD");
	case LOCK_TABLE:
		return("TABLE");
	default:
		return("UNKNOWN");
	}
}

/*******************************************************************//**
Gets the table on which the lock is.
@return	table */
UNIV_INLINE
dict_table_t*
lock_get_table(
/*===========*/
	const lock_t*	lock)	/*!< in: lock */
{
	switch (lock_get_type_low(lock)) {
	case LOCK_REC:
		ut_ad(dict_index_is_clust(lock->index)
		      || !dict_index_is_online_ddl(lock->index));
		return(lock->index->table);
	case LOCK_TABLE:
		return(lock->un_member.tab_lock.table);
	default:
		ut_error;
		return(NULL);
	}
}

/*******************************************************************//**
Gets the id of the table on which the lock is.
@return	id of the table */
UNIV_INTERN
table_id_t
lock_get_table_id(
/*==============*/
	const lock_t*	lock)	/*!< in: lock */
{
	dict_table_t*	table;

	table = lock_get_table(lock);

	return(table->id);
}

/*******************************************************************//**
Gets the name of the table on which the lock is.
The string should not be free()'d or modified.
@return	name of the table */
UNIV_INTERN
const char*
lock_get_table_name(
/*================*/
	const lock_t*	lock)	/*!< in: lock */
{
	dict_table_t*	table;

	table = lock_get_table(lock);

	return(table->name);
}

/*******************************************************************//**
For a record lock, gets the index on which the lock is.
@return	index */
UNIV_INTERN
const dict_index_t*
lock_rec_get_index(
/*===============*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_a(lock_get_type_low(lock) == LOCK_REC);
	ut_ad(dict_index_is_clust(lock->index)
	      || !dict_index_is_online_ddl(lock->index));

	return(lock->index);
}

/*******************************************************************//**
For a record lock, gets the name of the index on which the lock is.
The string should not be free()'d or modified.
@return	name of the index */
UNIV_INTERN
const char*
lock_rec_get_index_name(
/*====================*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_a(lock_get_type_low(lock) == LOCK_REC);
	ut_ad(dict_index_is_clust(lock->index)
	      || !dict_index_is_online_ddl(lock->index));

	return(lock->index->name);
}

/*******************************************************************//**
For a record lock, gets the tablespace number on which the lock is.
@return	tablespace number */
UNIV_INTERN
ulint
lock_rec_get_space_id(
/*==================*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_a(lock_get_type_low(lock) == LOCK_REC);

	return(lock->un_member.rec_lock.space);
}

/*******************************************************************//**
For a record lock, gets the page number on which the lock is.
@return	page number */
UNIV_INTERN
ulint
lock_rec_get_page_no(
/*=================*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_a(lock_get_type_low(lock) == LOCK_REC);

	return(lock->un_member.rec_lock.page_no);
}

/*********************************************************************//**
Cancels a waiting lock request and releases possible other transactions
waiting behind it. */
UNIV_INTERN
void
lock_cancel_waiting_and_release(
/*============================*/
	lock_t*	lock)	/*!< in/out: waiting lock request */
{
	que_thr_t*	thr;

	ut_ad(lock_mutex_own());
	ut_ad(trx_mutex_own(lock->trx));
	ut_ad(!(lock->type_mode & LOCK_CONV_BY_OTHER));

	lock->trx->lock.cancel = TRUE;

	if (lock_get_type_low(lock) == LOCK_REC) {

		lock_rec_dequeue_from_page(lock);
	} else {
		ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

		if (lock->trx->autoinc_locks != NULL) {
			/* Release the transaction's AUTOINC locks. */
			lock_release_autoinc_locks(lock->trx);
		}

		lock_table_dequeue(lock);
	}

	/* Reset the wait flag and the back pointer to lock in trx. */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait. */

	thr = que_thr_end_lock_wait(lock->trx);

	if (thr != NULL) {
		lock_wait_release_thread_if_suspended(thr);
	}

	lock->trx->lock.cancel = FALSE;
}

/*********************************************************************//**
Unlocks AUTO_INC type locks that were possibly reserved by a trx. This
function should be called at the the end of an SQL statement, by the
connection thread that owns the transaction (trx->mysql_thd). */
UNIV_INTERN
void
lock_unlock_table_autoinc(
/*======================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	ut_ad(!lock_mutex_own());
	ut_ad(!trx_mutex_own(trx));
	ut_ad(!trx->lock.wait_lock);
	/* This can be invoked on NOT_STARTED, ACTIVE, PREPARED,
	but not COMMITTED transactions. */
	ut_ad(trx_state_eq(trx, TRX_STATE_NOT_STARTED)
	      || !trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));

	/* This function is invoked for a running transaction by the
	thread that is serving the transaction. Therefore it is not
	necessary to hold trx->mutex here. */

	if (lock_trx_holds_autoinc_locks(trx)) {
		lock_mutex_enter();

		lock_release_autoinc_locks(trx);

		lock_mutex_exit();
	}
}

/*********************************************************************//**
Releases a transaction's locks, and releases possible other transactions
waiting because of these locks. Change the state of the transaction to
TRX_STATE_COMMITTED_IN_MEMORY. */
UNIV_INTERN
void
lock_trx_release_locks(
/*===================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	assert_trx_in_list(trx);

	if (trx_state_eq(trx, TRX_STATE_PREPARED)) {
		mutex_enter(&trx_sys->mutex);
		ut_a(trx_sys->n_prepared_trx > 0);
		trx_sys->n_prepared_trx--;
		if (trx->is_recovered) {
			ut_a(trx_sys->n_prepared_recovered_trx > 0);
			trx_sys->n_prepared_recovered_trx--;
		}
		mutex_exit(&trx_sys->mutex);
	} else {
		ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
	}

	/* The transition of trx->state to TRX_STATE_COMMITTED_IN_MEMORY
	is protected by both the lock_sys->mutex and the trx->mutex. */
	lock_mutex_enter();
	trx_mutex_enter(trx);

	/* The following assignment makes the transaction committed in memory
	and makes its changes to data visible to other transactions.
	NOTE that there is a small discrepancy from the strict formal
	visibility rules here: a human user of the database can see
	modifications made by another transaction T even before the necessary
	log segment has been flushed to the disk. If the database happens to
	crash before the flush, the user has seen modifications from T which
	will never be a committed transaction. However, any transaction T2
	which sees the modifications of the committing transaction T, and
	which also itself makes modifications to the database, will get an lsn
	larger than the committing transaction T. In the case where the log
	flush fails, and T never gets committed, also T2 will never get
	committed. */

	/*--------------------------------------*/
	trx->state = TRX_STATE_COMMITTED_IN_MEMORY;
	/*--------------------------------------*/

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

	trx->is_recovered = FALSE;

	trx_mutex_exit(trx);

	lock_release(trx);

	lock_mutex_exit();
}

/*********************************************************************//**
Check whether the transaction has already been rolled back because it
was selected as a deadlock victim, or if it has to wait then cancel
the wait lock.
@return DB_DEADLOCK, DB_LOCK_WAIT or DB_SUCCESS */
UNIV_INTERN
dberr_t
lock_trx_handle_wait(
/*=================*/
	trx_t*	trx)	/*!< in/out: trx lock state */
{
	dberr_t	err;

	lock_mutex_enter();

	trx_mutex_enter(trx);

	if (trx->lock.was_chosen_as_deadlock_victim) {
		err = DB_DEADLOCK;
	} else if (trx->lock.wait_lock != NULL) {
		lock_cancel_waiting_and_release(trx->lock.wait_lock);
		err = DB_LOCK_WAIT;
	} else {
		/* The lock was probably granted before we got here. */
		err = DB_SUCCESS;
	}

	lock_mutex_exit();
	trx_mutex_exit(trx);

	return(err);
}

/*********************************************************************//**
Get the number of locks on a table.
@return number of locks */
UNIV_INTERN
ulint
lock_table_get_n_locks(
/*===================*/
	const dict_table_t*	table)	/*!< in: table */
{
	ulint		n_table_locks;

	lock_mutex_enter();

	n_table_locks = UT_LIST_GET_LEN(table->locks);

	lock_mutex_exit();

	return(n_table_locks);
}

#ifdef UNIV_DEBUG
/*******************************************************************//**
Do an exhaustive check for any locks (table or rec) against the table.
@return	lock if found */
static
const lock_t*
lock_table_locks_lookup(
/*====================*/
	const dict_table_t*	table,		/*!< in: check if there are
						any locks held on records in
						this table or on the table
						itself */
	const trx_list_t*	trx_list)	/*!< in: trx list to check */
{
	trx_t*			trx;

	ut_a(table != NULL);
	ut_ad(lock_mutex_own());
	ut_ad(mutex_own(&trx_sys->mutex));

	ut_ad(trx_list == &trx_sys->rw_trx_list
	      || trx_list == &trx_sys->ro_trx_list);

	for (trx = UT_LIST_GET_FIRST(*trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		const lock_t*	lock;

		assert_trx_in_list(trx);
		ut_ad(trx->read_only == (trx_list == &trx_sys->ro_trx_list));

		for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
		     lock != NULL;
		     lock = UT_LIST_GET_NEXT(trx_locks, lock)) {

			ut_a(lock->trx == trx);

			if (lock_get_type_low(lock) == LOCK_REC) {
				ut_ad(!dict_index_is_online_ddl(lock->index)
				      || dict_index_is_clust(lock->index));
				if (lock->index->table == table) {
					return(lock);
				}
			} else if (lock->un_member.tab_lock.table == table) {
				return(lock);
			}
		}
	}

	return(NULL);
}
#endif /* UNIV_DEBUG */

/*******************************************************************//**
Check if there are any locks (table or rec) against table.
@return	TRUE if table has either table or record locks. */
UNIV_INTERN
ibool
lock_table_has_locks(
/*=================*/
	const dict_table_t*	table)	/*!< in: check if there are any locks
					held on records in this table or on the
					table itself */
{
	ibool			has_locks;

	lock_mutex_enter();

	has_locks = UT_LIST_GET_LEN(table->locks) > 0 || table->n_rec_locks > 0;

#ifdef UNIV_DEBUG
	if (!has_locks) {
		mutex_enter(&trx_sys->mutex);

		ut_ad(!lock_table_locks_lookup(table, &trx_sys->rw_trx_list));
		ut_ad(!lock_table_locks_lookup(table, &trx_sys->ro_trx_list));

		mutex_exit(&trx_sys->mutex);
	}
#endif /* UNIV_DEBUG */

	lock_mutex_exit();

	return(has_locks);
}

#ifdef UNIV_DEBUG
/*******************************************************************//**
Check if the transaction holds any locks on the sys tables
or its records.
@return	the strongest lock found on any sys table or 0 for none */
UNIV_INTERN
const lock_t*
lock_trx_has_sys_table_locks(
/*=========================*/
	const trx_t*	trx)	/*!< in: transaction to check */
{
	lint		i;
	const lock_t*	strongest_lock = 0;
	lock_mode	strongest = LOCK_NONE;

	lock_mutex_enter();

	/* Find a valid mode. Note: ib_vector_size() can be 0. */
	for (i = ib_vector_size(trx->lock.table_locks) - 1; i >= 0; --i) {
		const lock_t*	lock;

		lock = *static_cast<const lock_t**>(
			ib_vector_get(trx->lock.table_locks, i));

		if (lock != NULL
		    && dict_is_sys_table(lock->un_member.tab_lock.table->id)) {

			strongest = lock_get_mode(lock);
			ut_ad(strongest != LOCK_NONE);
			strongest_lock = lock;
			break;
		}
	}

	if (strongest == LOCK_NONE) {
		lock_mutex_exit();
		return(NULL);
	}

	for (/* No op */; i >= 0; --i) {
		const lock_t*	lock;

		lock = *static_cast<const lock_t**>(
			ib_vector_get(trx->lock.table_locks, i));

		if (lock == NULL) {
			continue;
		}

		ut_ad(trx == lock->trx);
		ut_ad(lock_get_type_low(lock) & LOCK_TABLE);
		ut_ad(lock->un_member.tab_lock.table != NULL);

		lock_mode	mode = lock_get_mode(lock);

		if (dict_is_sys_table(lock->un_member.tab_lock.table->id)
		    && lock_mode_stronger_or_eq(mode, strongest)) {

			strongest = mode;
			strongest_lock = lock;
		}
	}

	lock_mutex_exit();

	return(strongest_lock);
}
#endif /* UNIV_DEBUG */
