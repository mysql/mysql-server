/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file lock/lock0lock.c
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
#include "trx0sys.h"
#include "btr0btr.h"

/* Restricts the length of search we will do in the waits-for
graph of transactions */
#define LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK 1000000

/* Restricts the recursion depth of the search we will do in the waits-for
graph of transactions */
#define LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK 200

/* When releasing transaction locks, this specifies how often we release
the kernel mutex for a moment to give also others access to it */

#define LOCK_RELEASE_KERNEL_INTERVAL	1000

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
#define LK(a,b) (1 << ((a) * LOCK_NUM + (b)))
#define LKS(a,b) LK(a,b) | LK(b,a)

/* Define the lock compatibility matrix in a ulint.  The first line below
defines the diagonal entries.  The following lines define the compatibility
for LOCK_IX, LOCK_S, and LOCK_AUTO_INC using LKS(), since the matrix
is symmetric. */
#define LOCK_MODE_COMPATIBILITY 0					\
 | LK(LOCK_IS, LOCK_IS) | LK(LOCK_IX, LOCK_IX) | LK(LOCK_S, LOCK_S)	\
 | LKS(LOCK_IX, LOCK_IS) | LKS(LOCK_IS, LOCK_AUTO_INC)			\
 | LKS(LOCK_S, LOCK_IS)							\
 | LKS(LOCK_AUTO_INC, LOCK_IS) | LKS(LOCK_AUTO_INC, LOCK_IX)

/* STRONGER-OR-EQUAL RELATION (mode1=row, mode2=column)
 *    IS IX S  X  AI
 * IS +  -  -  -  -
 * IX +  +  -  -  -
 * S  +  -  +  -  -
 * X  +  +  +  +  +
 * AI -  -  -  -  +
 * See lock_mode_stronger_or_eq().
 */

/* Define the stronger-or-equal lock relation in a ulint.  This relation
contains all pairs LK(mode1, mode2) where mode1 is stronger than or
equal to mode2. */
#define LOCK_MODE_STRONGER_OR_EQ 0					\
 | LK(LOCK_IS, LOCK_IS)							\
 | LK(LOCK_IX, LOCK_IS) | LK(LOCK_IX, LOCK_IX)				\
 | LK(LOCK_S, LOCK_IS) | LK(LOCK_S, LOCK_S)				\
 | LK(LOCK_AUTO_INC, LOCK_AUTO_INC)					\
 | LK(LOCK_X, LOCK_IS) | LK(LOCK_X, LOCK_IX) | LK(LOCK_X, LOCK_S)	\
 | LK(LOCK_X, LOCK_AUTO_INC) | LK(LOCK_X, LOCK_X)

#ifdef UNIV_DEBUG
UNIV_INTERN ibool	lock_print_waits	= FALSE;

/*********************************************************************//**
Validates the lock system.
@return	TRUE if ok */
static
ibool
lock_validate(void);
/*===============*/

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

/* We store info on the latest deadlock error to this buffer. InnoDB
Monitor will then fetch it and print */
UNIV_INTERN ibool	lock_deadlock_found = FALSE;
UNIV_INTERN FILE*	lock_latest_err_file;

/* Flags for recursive deadlock search */
#define LOCK_VICTIM_IS_START	1
#define LOCK_VICTIM_IS_OTHER	2
#define LOCK_EXCEED_MAX_DEPTH	3

/********************************************************************//**
Checks if a lock request results in a deadlock.
@return TRUE if a deadlock was detected and we chose trx as a victim;
FALSE if no deadlock, or there was a deadlock, but we chose other
transaction(s) as victim(s) */
static
ibool
lock_deadlock_occurs(
/*=================*/
	lock_t*	lock,	/*!< in: lock the transaction is requesting */
	trx_t*	trx);	/*!< in: transaction */
/********************************************************************//**
Looks recursively for a deadlock.
@return 0 if no deadlock found, LOCK_VICTIM_IS_START if there was a
deadlock and we chose 'start' as the victim, LOCK_VICTIM_IS_OTHER if a
deadlock was found and we chose some other trx as a victim: we must do
the search again in this last case because there may be another
deadlock!
LOCK_EXCEED_MAX_DEPTH if the lock search exceeds max steps or max depth. */
static
ulint
lock_deadlock_recursive(
/*====================*/
	trx_t*	start,		/*!< in: recursion starting point */
	trx_t*	trx,		/*!< in: a transaction waiting for a lock */
	lock_t*	wait_lock,	/*!< in:  lock that is waiting to be granted */
	ulint*	cost,		/*!< in/out: number of calculation steps thus
				far: if this exceeds LOCK_MAX_N_STEPS_...
				we return LOCK_EXCEED_MAX_DEPTH */
	ulint	depth);		/*!< in: recursion depth: if this exceeds
				LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK, we
				return LOCK_EXCEED_MAX_DEPTH */

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
	ulint	byte_index;
	ulint	bit_index;

	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	if (i >= lock->un_member.rec_lock.n_bits) {

		return(FALSE);
	}

	byte_index = i / 8;
	bit_index = i % 8;

	return(1 & ((const byte*) &lock[1])[byte_index] >> bit_index);
}

/*************************************************************************/

#define lock_mutex_enter_kernel()	mutex_enter(&kernel_mutex)
#define lock_mutex_exit_kernel()	mutex_exit(&kernel_mutex)

/*********************************************************************//**
Checks that a transaction id is sensible, i.e., not in the future.
@return	TRUE if ok */
UNIV_INTERN
ibool
lock_check_trx_id_sanity(
/*=====================*/
	trx_id_t	trx_id,		/*!< in: trx id */
	const rec_t*	rec,		/*!< in: user record */
	dict_index_t*	index,		/*!< in: index */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index) */
	ibool		has_kernel_mutex)/*!< in: TRUE if the caller owns the
					kernel mutex */
{
	ibool	is_ok		= TRUE;

	ut_ad(rec_offs_validate(rec, index, offsets));

	if (!has_kernel_mutex) {
		mutex_enter(&kernel_mutex);
	}

	/* A sanity check: the trx_id in rec must be smaller than the global
	trx id counter */

	if (UNIV_UNLIKELY(trx_id >= trx_sys->max_trx_id)) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: transaction id associated"
		      " with record\n",
		      stderr);
		rec_print_new(stderr, rec, offsets);
		fputs("InnoDB: in ", stderr);
		dict_index_name_print(stderr, NULL, index);
		fprintf(stderr, "\n"
			"InnoDB: is " TRX_ID_FMT " which is higher than the"
			" global trx id counter " TRX_ID_FMT "!\n"
			"InnoDB: The table is corrupt. You have to do"
			" dump + drop + reimport.\n",
			(ullint) trx_id, (ullint) trx_sys->max_trx_id);

		is_ok = FALSE;
	}

	if (!has_kernel_mutex) {
		mutex_exit(&kernel_mutex);
	}

	return(is_ok);
}

/*********************************************************************//**
Checks that a record is seen in a consistent read.
@return TRUE if sees, or FALSE if an earlier version of the record
should be retrieved */
UNIV_INTERN
ibool
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
	system latch. To obey the latching order we must NOT reserve the
	kernel mutex here! */

	trx_id = row_get_rec_trx_id(rec, index, offsets);

	return(read_view_sees_trx_id(view, trx_id));
}

/*********************************************************************//**
Checks that a non-clustered index record is seen in a consistent read.

NOTE that a non-clustered index page contains so little information on
its modifications that also in the case FALSE, the present version of
rec may be the right, but we must check this from the clustered index
record.

@return TRUE if certainly sees, or FALSE if an earlier version of the
clustered index record might be needed */
UNIV_INTERN
ulint
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
	system latch. To obey the latching order we must NOT reserve the
	kernel mutex here! */

	if (recv_recovery_is_on()) {

		return(FALSE);
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
	lock_sys = mem_alloc(sizeof(lock_sys_t));

	lock_sys->rec_hash = hash_create(n_cells);

	/* hash_create_mutexes(lock_sys->rec_hash, 2, SYNC_REC_LOCK); */

	lock_latest_err_file = os_file_create_tmpfile();
	ut_a(lock_latest_err_file);
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
	mem_free(lock_sys);
	lock_sys = NULL;
}

/*********************************************************************//**
Gets the size of a lock struct.
@return	size in bytes */
UNIV_INTERN
ulint
lock_get_size(void)
/*===============*/
{
	return((ulint)sizeof(lock_t));
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

	return(lock->type_mode & LOCK_MODE_MASK);
}

/*********************************************************************//**
Gets the wait flag of a lock.
@return	TRUE if waiting */
UNIV_INLINE
ibool
lock_get_wait(
/*==========*/
	const lock_t*	lock)	/*!< in: lock */
{
	ut_ad(lock);

	if (UNIV_UNLIKELY(lock->type_mode & LOCK_WAIT)) {

		return(TRUE);
	}

	return(FALSE);
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

	src = NULL;
	*mode = LOCK_NONE;

	for (lock = UT_LIST_GET_FIRST(trx->trx_locks);
	     lock;
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
				return(NULL);
			}
		} else if (src != tab_lock->table) {
			/* The transaction is locking more than
			two tables (src and dest): abort */
			return(NULL);
		}

		/* Check that the source table is locked by
		LOCK_IX or LOCK_IS. */
		lock_mode = lock_get_mode(lock);
		if (lock_mode == LOCK_IX || lock_mode == LOCK_IS) {
			if (*mode != LOCK_NONE && *mode != lock_mode) {
				/* There are multiple locks on src. */
				return(NULL);
			}
			*mode = lock_mode;
		}
	}

	if (!src) {
		/* No source table lock found: flag the situation to caller */
		src = dest;
	}

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
	dict_table_t*	table,	/*!< in: table */
	trx_t*		trx)	/*!< in: transaction */
{
	const lock_t*	lock;
	ibool		ok	= FALSE;

	ut_ad(table);
	ut_ad(trx);

	lock_mutex_enter_kernel();

	for (lock = UT_LIST_GET_FIRST(table->locks);
	     lock;
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
	lock_mutex_exit_kernel();

	return(ok);
}

/*********************************************************************//**
Sets the wait flag of a lock and the back pointer in trx to lock. */
UNIV_INLINE
void
lock_set_lock_and_trx_wait(
/*=======================*/
	lock_t*	lock,	/*!< in: lock */
	trx_t*	trx)	/*!< in: trx */
{
	ut_ad(lock);
	ut_ad(trx->wait_lock == NULL);

	trx->wait_lock = lock;
	lock->type_mode |= LOCK_WAIT;
}

/**********************************************************************//**
The back pointer to a waiting lock request in the transaction is set to NULL
and the wait bit in lock type_mode is reset. */
UNIV_INLINE
void
lock_reset_lock_and_trx_wait(
/*=========================*/
	lock_t*	lock)	/*!< in: record lock */
{
	ut_ad((lock->trx)->wait_lock == lock);
	ut_ad(lock_get_wait(lock));

	/* Reset the back pointer in trx to this waiting lock request */

	(lock->trx)->wait_lock = NULL;
	lock->type_mode &= ~LOCK_WAIT;
}

/*********************************************************************//**
Gets the gap flag of a record lock.
@return	TRUE if gap flag set */
UNIV_INLINE
ibool
lock_rec_get_gap(
/*=============*/
	const lock_t*	lock)	/*!< in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_GAP) {

		return(TRUE);
	}

	return(FALSE);
}

/*********************************************************************//**
Gets the LOCK_REC_NOT_GAP flag of a record lock.
@return	TRUE if LOCK_REC_NOT_GAP flag set */
UNIV_INLINE
ibool
lock_rec_get_rec_not_gap(
/*=====================*/
	const lock_t*	lock)	/*!< in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_REC_NOT_GAP) {

		return(TRUE);
	}

	return(FALSE);
}

/*********************************************************************//**
Gets the waiting insert flag of a record lock.
@return	TRUE if gap flag set */
UNIV_INLINE
ibool
lock_rec_get_insert_intention(
/*==========================*/
	const lock_t*	lock)	/*!< in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_INSERT_INTENTION) {

		return(TRUE);
	}

	return(FALSE);
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
	ut_ad(mode1 == LOCK_X || mode1 == LOCK_S || mode1 == LOCK_IX
	      || mode1 == LOCK_IS || mode1 == LOCK_AUTO_INC);
	ut_ad(mode2 == LOCK_X || mode2 == LOCK_S || mode2 == LOCK_IX
	      || mode2 == LOCK_IS || mode2 == LOCK_AUTO_INC);

	return((LOCK_MODE_STRONGER_OR_EQ) & LK(mode1, mode2));
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
	ut_ad(mode1 == LOCK_X || mode1 == LOCK_S || mode1 == LOCK_IX
	      || mode1 == LOCK_IS || mode1 == LOCK_AUTO_INC);
	ut_ad(mode2 == LOCK_X || mode2 == LOCK_S || mode2 == LOCK_IX
	      || mode2 == LOCK_IS || mode2 == LOCK_AUTO_INC);

	return((LOCK_MODE_COMPATIBILITY) & LK(mode1, mode2));
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
	    && !lock_mode_compatible(LOCK_MODE_MASK & type_mode,
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

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	space = lock->un_member.rec_lock.space;
	page_no = lock->un_member.rec_lock.page_no;

	for (;;) {
		lock = HASH_GET_NEXT(hash, lock);

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

	ut_ad(mutex_own(&kernel_mutex));

	lock = HASH_GET_FIRST(lock_sys->rec_hash,
			      lock_rec_hash(space, page_no));
	while (lock) {
		if ((lock->un_member.rec_lock.space == space)
		    && (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}

		lock = HASH_GET_NEXT(hash, lock);
	}

	return(lock);
}

/*********************************************************************//**
Returns TRUE if there are explicit record locks on a page.
@return	TRUE if there are explicit record locks on the page */
UNIV_INTERN
ibool
lock_rec_expl_exist_on_page(
/*========================*/
	ulint	space,	/*!< in: space id */
	ulint	page_no)/*!< in: page number */
{
	ibool	ret;

	mutex_enter(&kernel_mutex);

	if (lock_rec_get_first_on_page_addr(space, page_no)) {
		ret = TRUE;
	} else {
		ret = FALSE;
	}

	mutex_exit(&kernel_mutex);

	return(ret);
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

	ut_ad(mutex_own(&kernel_mutex));

	hash = buf_block_get_lock_hash_val(block);

	lock = HASH_GET_FIRST(lock_sys->rec_hash, hash);

	while (lock) {
		if ((lock->un_member.rec_lock.space == space)
		    && (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}

		lock = HASH_GET_NEXT(hash, lock);
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
	ut_ad(mutex_own(&kernel_mutex));

	do {
		ut_ad(lock_get_type_low(lock) == LOCK_REC);
		lock = lock_rec_get_next_on_page(lock);
	} while (lock && !lock_rec_get_nth_bit(lock, heap_no));

	return(lock);
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

	ut_ad(mutex_own(&kernel_mutex));

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

	return(mem_heap_dup(heap, lock, size));
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

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	for (;;) {
		ut_ad(lock);

		if (lock == in_lock) {

			return(found_lock);
		}

		if (lock_rec_get_nth_bit(lock, heap_no)) {

			found_lock = lock;
		}

		lock = lock_rec_get_next_on_page(lock);
	}
}

/*============= FUNCTIONS FOR ANALYZING TABLE LOCK QUEUE ================*/

/*********************************************************************//**
Checks if a transaction has the specified table lock, or stronger.
@return	lock or NULL */
UNIV_INLINE
lock_t*
lock_table_has(
/*===========*/
	trx_t*		trx,	/*!< in: transaction */
	dict_table_t*	table,	/*!< in: table */
	enum lock_mode	mode)	/*!< in: lock mode */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	/* Look for stronger locks the same trx already has on the table */

	lock = UT_LIST_GET_LAST(table->locks);

	while (lock != NULL) {

		if (lock->trx == trx
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)) {

			/* The same trx already has locked the table in
			a mode stronger or equal to the mode given */

			ut_ad(!lock_get_wait(lock));

			return(lock);
		}

		lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
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
	trx_t*			trx)	/*!< in: transaction */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S
	      || (precise_mode & LOCK_MODE_MASK) == LOCK_X);
	ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));

	lock = lock_rec_get_first(block, heap_no);

	while (lock) {
		if (lock->trx == trx
		    && lock_mode_stronger_or_eq(lock_get_mode(lock),
						precise_mode & LOCK_MODE_MASK)
		    && !lock_get_wait(lock)
		    && (!lock_rec_get_rec_not_gap(lock)
			|| (precise_mode & LOCK_REC_NOT_GAP)
			|| heap_no == PAGE_HEAP_NO_SUPREMUM)
		    && (!lock_rec_get_gap(lock)
			|| (precise_mode & LOCK_GAP)
			|| heap_no == PAGE_HEAP_NO_SUPREMUM)
		    && (!lock_rec_get_insert_intention(lock))) {

			return(lock);
		}

		lock = lock_rec_get_next(heap_no, lock);
	}

	return(NULL);
}

#ifdef UNIV_DEBUG
/*********************************************************************//**
Checks if some other transaction has a lock request in the queue.
@return	lock or NULL */
static
lock_t*
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
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(mode == LOCK_X || mode == LOCK_S);
	ut_ad(gap == 0 || gap == LOCK_GAP);
	ut_ad(wait == 0 || wait == LOCK_WAIT);

	lock = lock_rec_get_first(block, heap_no);

	while (lock) {
		if (lock->trx != trx
		    && (gap
			|| !(lock_rec_get_gap(lock)
			     || heap_no == PAGE_HEAP_NO_SUPREMUM))
		    && (wait || !lock_get_wait(lock))
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)) {

			return(lock);
		}

		lock = lock_rec_get_next(heap_no, lock);
	}

	return(NULL);
}
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Checks if some other transaction has a conflicting explicit lock request
in the queue, so that we have to wait.
@return	lock or NULL */
static
lock_t*
lock_rec_other_has_conflicting(
/*===========================*/
	enum lock_mode		mode,	/*!< in: LOCK_S or LOCK_X,
					possibly ORed to LOCK_GAP or
					LOC_REC_NOT_GAP,
					LOCK_INSERT_INTENTION */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	ulint			heap_no,/*!< in: heap number of the record */
	trx_t*			trx)	/*!< in: our transaction */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = lock_rec_get_first(block, heap_no);

	if (UNIV_LIKELY_NULL(lock)) {
		if (UNIV_UNLIKELY(heap_no == PAGE_HEAP_NO_SUPREMUM)) {

			do {
				if (lock_rec_has_to_wait(trx, mode, lock,
							 TRUE)) {
					return(lock);
				}

				lock = lock_rec_get_next(heap_no, lock);
			} while (lock);
		} else {

			do {
				if (lock_rec_has_to_wait(trx, mode, lock,
							 FALSE)) {
					return(lock);
				}

				lock = lock_rec_get_next(heap_no, lock);
			} while (lock);
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
	ut_ad(mutex_own(&kernel_mutex));

	while (lock != NULL) {
		if (lock->trx == trx
		    && lock->type_mode == type_mode
		    && lock_rec_get_n_bits(lock) > heap_no) {

			return(lock);
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	return(NULL);
}

/*********************************************************************//**
Checks if some transaction has an implicit x-lock on a record in a secondary
index.
@return	transaction which has the x-lock, or NULL */
static
trx_t*
lock_sec_rec_some_has_impl_off_kernel(
/*==================================*/
	const rec_t*	rec,	/*!< in: user record */
	dict_index_t*	index,	/*!< in: secondary index */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec, index) */
{
	const page_t*	page = page_align(rec);

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(!dict_index_is_clust(index));
	ut_ad(page_rec_is_user_rec(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list, or
	database recovery is running. We do not write the changes of a page
	max trx id to the log, and therefore during recovery, this value
	for a page may be incorrect. */

	if (page_get_max_trx_id(page) < trx_list_get_min_trx_id()
	    && !recv_recovery_is_on()) {

		return(NULL);
	}

	/* Ok, in this case it is possible that some transaction has an
	implicit x-lock. We have to look in the clustered index. */

	if (!lock_check_trx_id_sanity(page_get_max_trx_id(page),
				      rec, index, offsets, TRUE)) {
		buf_page_print(page, 0, 0);

		/* The page is corrupt: try to avoid a crash by returning
		NULL */
		return(NULL);
	}

	return(row_vers_impl_x_locked_off_kernel(rec, index, offsets));
}

/*********************************************************************//**
Return approximate number or record locks (bits set in the bitmap) for
this transaction. Since delete-marked records may be removed, the
record count will not be precise. */
UNIV_INTERN
ulint
lock_number_of_rows_locked(
/*=======================*/
	const trx_t*	trx)	/*!< in: transaction */
{
	lock_t*	lock;
	ulint   n_records = 0;
	ulint	n_bits;
	ulint	n_bit;

	lock = UT_LIST_GET_FIRST(trx->trx_locks);

	while (lock) {
		if (lock_get_type_low(lock) == LOCK_REC) {
			n_bits = lock_rec_get_n_bits(lock);

			for (n_bit = 0; n_bit < n_bits; n_bit++) {
				if (lock_rec_get_nth_bit(lock, n_bit)) {
					n_records++;
				}
			}
		}

		lock = UT_LIST_GET_NEXT(trx_locks, lock);
	}

	return (n_records);
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
	trx_t*			trx)	/*!< in: transaction */
{
	lock_t*		lock;
	ulint		page_no;
	ulint		space;
	ulint		n_bits;
	ulint		n_bytes;
	const page_t*	page;

	ut_ad(mutex_own(&kernel_mutex));

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

	lock = mem_heap_alloc(trx->lock_heap, sizeof(lock_t) + n_bytes);

	UT_LIST_ADD_LAST(trx_locks, trx->trx_locks, lock);

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

	HASH_INSERT(lock_t, hash, lock_sys->rec_hash,
		    lock_rec_fold(space, page_no), lock);
	if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {

		lock_set_lock_and_trx_wait(lock, trx);
	}

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
enum db_err
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
	que_thr_t*		thr)	/*!< in: query thread */
{
	lock_t*	lock;
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (UNIV_UNLIKELY(que_thr_stop(thr))) {

		ut_error;

		return(DB_QUE_THR_SUSPENDED);
	}

	trx = thr_get_trx(thr);

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

	/* Enqueue the lock request that will wait to be granted */
	lock = lock_rec_create(type_mode | LOCK_WAIT,
			       block, heap_no, index, trx);

	/* Check if a deadlock occurs: if yes, remove the lock request and
	return an error code */

	if (UNIV_UNLIKELY(lock_deadlock_occurs(lock, trx))) {

		lock_reset_lock_and_trx_wait(lock);
		lock_rec_reset_nth_bit(lock, heap_no);

		return(DB_DEADLOCK);
	}

	/* If there was a deadlock but we chose another transaction as a
	victim, it is possible that we already have the lock now granted! */

	if (trx->wait_lock == NULL) {

		return(DB_SUCCESS_LOCKED_REC);
	}

	trx->que_state = TRX_QUE_LOCK_WAIT;
	trx->was_chosen_as_deadlock_victim = FALSE;
	trx->wait_started = time(NULL);

	ut_a(que_thr_stop(thr));

#ifdef UNIV_DEBUG
	if (lock_print_waits) {
		fprintf(stderr, "Lock wait for trx " TRX_ID_FMT " in index ",
			(ullint) trx->id);
		ut_print_name(stderr, trx, FALSE, index->name);
	}
#endif /* UNIV_DEBUG */

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
	trx_t*			trx)	/*!< in: transaction */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));
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
		lock_t*		other_lock
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

	lock = lock_rec_get_first_on_page(block);

	while (lock != NULL) {
		if (lock_get_wait(lock)
		    && (lock_rec_get_nth_bit(lock, heap_no))) {

			goto somebody_waits;
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	if (UNIV_LIKELY(!(type_mode & LOCK_WAIT))) {

		/* Look for a similar record lock on the same page:
		if one is found and there are no waiting lock requests,
		we can just set the bit */

		lock = lock_rec_find_similar_on_page(
			type_mode, heap_no,
			lock_rec_get_first_on_page(block), trx);

		if (lock) {

			lock_rec_set_nth_bit(lock, heap_no);

			return(lock);
		}
	}

somebody_waits:
	return(lock_rec_create(type_mode, block, heap_no, index, trx));
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
	lock_t*	lock;
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
	      || (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
	      || mode - (LOCK_MODE_MASK & mode) == 0
	      || mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);

	lock = lock_rec_get_first_on_page(block);

	trx = thr_get_trx(thr);

	if (lock == NULL) {
		if (!impl) {
			lock_rec_create(mode, block, heap_no, index, trx);
		}

		return(LOCK_REC_SUCCESS_CREATED);
	}

	if (lock_rec_get_next_on_page(lock)) {

		return(LOCK_REC_FAIL);
	}

	if (lock->trx != trx
	    || lock->type_mode != (mode | LOCK_REC)
	    || lock_rec_get_n_bits(lock) <= heap_no) {

		return(LOCK_REC_FAIL);
	}

	if (!impl) {
		/* If the nth bit of the record lock is already set then we
		do not set a new lock bit, otherwise we do set */

		if (!lock_rec_get_nth_bit(lock, heap_no)) {
			lock_rec_set_nth_bit(lock, heap_no);
			return(LOCK_REC_SUCCESS_CREATED);
		}
	}

	return(LOCK_REC_SUCCESS);
}

/*********************************************************************//**
This is the general, and slower, routine for locking a record. This is a
low-level function which does NOT look at implicit locks! Checks lock
compatibility within explicit locks. This function sets a normal next-key
lock, or in the case of a page supremum record, a gap type lock.
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, DB_LOCK_WAIT, DB_DEADLOCK,
or DB_QUE_THR_SUSPENDED */
static
enum db_err
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
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
	      || (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
	      || mode - (LOCK_MODE_MASK & mode) == 0
	      || mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);

	trx = thr_get_trx(thr);

	if (lock_rec_has_expl(mode, block, heap_no, trx)) {
		/* The trx already has a strong enough lock on rec: do
		nothing */

	} else if (lock_rec_other_has_conflicting(mode, block, heap_no, trx)) {

		/* If another transaction has a non-gap conflicting request in
		the queue, as this transaction does not have a lock strong
		enough already granted on the record, we have to wait. */

		return(lock_rec_enqueue_waiting(mode, block, heap_no,
						index, thr));
	} else if (!impl) {
		/* Set the requested lock on the record */

		lock_rec_add_to_queue(LOCK_REC | mode, block,
				      heap_no, index, trx);
		return(DB_SUCCESS_LOCKED_REC);
	}

	return(DB_SUCCESS);
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
enum db_err
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
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
	      || (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
	      || mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP
	      || mode - (LOCK_MODE_MASK & mode) == 0);

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
@return	TRUE if still has to wait */
static
ibool
lock_rec_has_to_wait_in_queue(
/*==========================*/
	lock_t*	wait_lock)	/*!< in: waiting record lock */
{
	lock_t*	lock;
	ulint	space;
	ulint	page_no;
	ulint	heap_no;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_wait(wait_lock));
	ut_ad(lock_get_type_low(wait_lock) == LOCK_REC);

	space = wait_lock->un_member.rec_lock.space;
	page_no = wait_lock->un_member.rec_lock.page_no;
	heap_no = lock_rec_find_set_bit(wait_lock);

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	while (lock != wait_lock) {

		if (lock_rec_get_nth_bit(lock, heap_no)
		    && lock_has_to_wait(wait_lock, lock)) {

			return(TRUE);
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	return(FALSE);
}

/*************************************************************//**
Grants a lock to a waiting lock request and releases the waiting
transaction. */
static
void
lock_grant(
/*=======*/
	lock_t*	lock)	/*!< in/out: waiting lock request */
{
	ut_ad(mutex_own(&kernel_mutex));

	lock_reset_lock_and_trx_wait(lock);

	if (lock_get_mode(lock) == LOCK_AUTO_INC) {
		trx_t*		trx = lock->trx;
		dict_table_t*	table = lock->un_member.tab_lock.table;

		if (table->autoinc_trx == trx) {
			fprintf(stderr,
				"InnoDB: Error: trx already had"
				" an AUTO-INC lock!\n");
		} else {
			table->autoinc_trx = trx;

			ib_vector_push(trx->autoinc_locks, lock);
		}
	}

#ifdef UNIV_DEBUG
	if (lock_print_waits) {
		fprintf(stderr, "Lock wait for trx " TRX_ID_FMT " ends\n",
			(ullint) lock->trx->id);
	}
#endif /* UNIV_DEBUG */

	/* If we are resolving a deadlock by choosing another transaction
	as a victim, then our original transaction may not be in the
	TRX_QUE_LOCK_WAIT state, and there is no need to end the lock wait
	for it */

	if (lock->trx->que_state == TRX_QUE_LOCK_WAIT) {
		trx_end_lock_wait(lock->trx);
	}
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
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type_low(lock) == LOCK_REC);

	/* Reset the bit (there can be only one set bit) in the lock bitmap */
	lock_rec_reset_nth_bit(lock, lock_rec_find_set_bit(lock));

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_end_lock_wait(lock->trx);
}

/*************************************************************//**
Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed. */
static
void
lock_rec_dequeue_from_page(
/*=======================*/
	lock_t*	in_lock)/*!< in: record lock object: all record locks which
			are contained in this lock object are removed;
			transactions waiting behind will get their lock
			requests granted, if they are now qualified to it */
{
	ulint	space;
	ulint	page_no;
	lock_t*	lock;
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

	trx = in_lock->trx;

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
		    lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, in_lock);

	/* Check if waiting locks in the queue can now be granted: grant
	locks if there are no conflicting locks ahead. */

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	while (lock != NULL) {
		if (lock_get_wait(lock)
		    && !lock_rec_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			lock_grant(lock);
		}

		lock = lock_rec_get_next_on_page(lock);
	}
}

/*************************************************************//**
Removes a record lock request, waiting or granted, from the queue. */
static
void
lock_rec_discard(
/*=============*/
	lock_t*	in_lock)/*!< in: record lock object: all record locks which
			are contained in this lock object are removed */
{
	ulint	space;
	ulint	page_no;
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type_low(in_lock) == LOCK_REC);

	trx = in_lock->trx;

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
		    lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, in_lock);
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

	ut_ad(mutex_own(&kernel_mutex));

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

	ut_ad(mutex_own(&kernel_mutex));

	lock = lock_rec_get_first(block, heap_no);

	while (lock != NULL) {
		if (lock_get_wait(lock)) {
			lock_rec_cancel(lock);
		} else {
			lock_rec_reset_nth_bit(lock, heap_no);
		}

		lock = lock_rec_get_next(heap_no, lock);
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

	ut_ad(mutex_own(&kernel_mutex));

	lock = lock_rec_get_first(block, heap_no);

	/* If srv_locks_unsafe_for_binlog is TRUE or session is using
	READ COMMITTED isolation level, we do not want locks set
	by an UPDATE or a DELETE to be inherited as gap type locks. But we
	DO want S-locks set by a consistency constraint to be inherited also
	then. */

	while (lock != NULL) {
		if (!lock_rec_get_insert_intention(lock)
		    && !((srv_locks_unsafe_for_binlog
			  || lock->trx->isolation_level
			  <= TRX_ISO_READ_COMMITTED)
			 && lock_get_mode(lock) == LOCK_X)) {

			lock_rec_add_to_queue(LOCK_REC | LOCK_GAP
					      | lock_get_mode(lock),
					      heir_block, heir_heap_no,
					      lock->index, lock->trx);
		}

		lock = lock_rec_get_next(heap_no, lock);
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

	ut_ad(mutex_own(&kernel_mutex));

	lock = lock_rec_get_first(block, heap_no);

	while (lock != NULL) {
		if (!lock_rec_get_insert_intention(lock)
		    && (heap_no == PAGE_HEAP_NO_SUPREMUM
			|| !lock_rec_get_rec_not_gap(lock))) {

			lock_rec_add_to_queue(LOCK_REC | LOCK_GAP
					      | lock_get_mode(lock),
					      block, heir_heap_no,
					      lock->index, lock->trx);
		}

		lock = lock_rec_get_next(heap_no, lock);
	}
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

	ut_ad(mutex_own(&kernel_mutex));

	lock = lock_rec_get_first(donator, donator_heap_no);

	ut_ad(lock_rec_get_first(receiver, receiver_heap_no) == NULL);

	while (lock != NULL) {
		const ulint	type_mode = lock->type_mode;

		lock_rec_reset_nth_bit(lock, donator_heap_no);

		if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {
			lock_reset_lock_and_trx_wait(lock);
		}

		/* Note that we FIRST reset the bit, and then set the lock:
		the function works also if donator == receiver */

		lock_rec_add_to_queue(type_mode, receiver, receiver_heap_no,
				      lock->index, lock->trx);
		lock = lock_rec_get_next(donator_heap_no, lock);
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

	lock_mutex_enter_kernel();

	lock = lock_rec_get_first_on_page(block);

	if (lock == NULL) {
		lock_mutex_exit_kernel();

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

				lock_rec_add_to_queue(lock->type_mode, block,
						      new_heap_no,
						      lock->index, lock->trx);

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

	lock_mutex_exit_kernel();

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

	lock_mutex_enter_kernel();

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

				lock_rec_add_to_queue(type_mode,
						      new_block, heap_no,
						      lock->index, lock->trx);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}
	}

	lock_mutex_exit_kernel();

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
	const buf_block_t*	new_block,	/*!< in: index page to move to */
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

	lock_mutex_enter_kernel();

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

				lock_rec_add_to_queue(type_mode,
						      new_block, heap_no,
						      lock->index, lock->trx);
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

	lock_mutex_exit_kernel();

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

	lock_mutex_enter_kernel();

	/* Move the locks on the supremum of the left page to the supremum
	of the right page */

	lock_rec_move(right_block, left_block,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);

	/* Inherit the locks to the supremum of left page from the successor
	of the infimum on right page */

	lock_rec_inherit_to_gap(left_block, right_block,
				PAGE_HEAP_NO_SUPREMUM, heap_no);

	lock_mutex_exit_kernel();
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
	lock_mutex_enter_kernel();

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

	lock_mutex_exit_kernel();
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
	lock_mutex_enter_kernel();

	/* Move the locks on the supremum of the root to the supremum
	of block */

	lock_rec_move(block, root,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
	lock_mutex_exit_kernel();
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
	lock_mutex_enter_kernel();

	/* Move the locks on the supremum of the old page to the supremum
	of new_page */

	lock_rec_move(new_block, block,
		      PAGE_HEAP_NO_SUPREMUM, PAGE_HEAP_NO_SUPREMUM);
	lock_rec_free_all_from_discard_page(block);

	lock_mutex_exit_kernel();
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

	lock_mutex_enter_kernel();

	/* Inherit the locks to the supremum of the left page from the
	successor of the infimum on the right page */

	lock_rec_inherit_to_gap(left_block, right_block,
				PAGE_HEAP_NO_SUPREMUM, heap_no);

	lock_mutex_exit_kernel();
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

	lock_mutex_enter_kernel();

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

	lock_mutex_exit_kernel();
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
	mutex_enter(&kernel_mutex);

	lock_rec_reset_and_release_wait(heir_block, heir_heap_no);

	lock_rec_inherit_to_gap(heir_block, block, heir_heap_no, heap_no);

	mutex_exit(&kernel_mutex);
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

	lock_mutex_enter_kernel();

	if (!lock_rec_get_first_on_page(block)) {
		/* No locks exist on page, nothing to do */

		lock_mutex_exit_kernel();

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

	lock_mutex_exit_kernel();
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

	lock_mutex_enter_kernel();
	lock_rec_inherit_to_gap_if_gap_lock(block,
					    receiver_heap_no, donator_heap_no);
	lock_mutex_exit_kernel();
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

	lock_mutex_enter_kernel();

	/* Let the next record inherit the locks from rec, in gap mode */

	lock_rec_inherit_to_gap(block, block, next_heap_no, heap_no);

	/* Reset the lock bits on rec and release waiting transactions */

	lock_rec_reset_and_release_wait(block, heap_no);

	lock_mutex_exit_kernel();
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

	lock_mutex_enter_kernel();

	lock_rec_move(block, block, PAGE_HEAP_NO_INFIMUM, heap_no);

	lock_mutex_exit_kernel();
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

	lock_mutex_enter_kernel();

	lock_rec_move(block, donator, heap_no, PAGE_HEAP_NO_INFIMUM);

	lock_mutex_exit_kernel();
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
	rewind(lock_latest_err_file);
	ut_print_timestamp(lock_latest_err_file);

	if (srv_print_all_deadlocks) {
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
	fputs(msg, lock_latest_err_file);

	if (srv_print_all_deadlocks) {
		fputs(msg, stderr);
	}
}

/*********************************************************************//**
Print transaction data to the deadlock file and possibly to stderr. */
UNIV_INLINE
void
lock_deadlock_trx_print(
/*====================*/
	trx_t*	trx,		/*!< in: transaction */
	ulint	max_query_len)	/*!< in: max query length to print, or 0 to
				use the default max length */
{
	trx_print(lock_latest_err_file, trx, max_query_len);

	if (srv_print_all_deadlocks) {
		trx_print(stderr, trx, max_query_len);
	}
}

/*********************************************************************//**
Print lock data to the deadlock file and possibly to stderr. */
UNIV_INLINE
void
lock_deadlock_lock_print(
/*=====================*/
	const lock_t*	lock)	/*!< in: record or table type lock */
{
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

/********************************************************************//**
Checks if a lock request results in a deadlock.
@return TRUE if a deadlock was detected and we chose trx as a victim;
FALSE if no deadlock, or there was a deadlock, but we chose other
transaction(s) as victim(s) */
static
ibool
lock_deadlock_occurs(
/*=================*/
	lock_t*	lock,	/*!< in: lock the transaction is requesting */
	trx_t*	trx)	/*!< in: transaction */
{
	trx_t*		mark_trx;
	ulint		ret;
	ulint		cost	= 0;

	ut_ad(trx);
	ut_ad(lock);
	ut_ad(mutex_own(&kernel_mutex));
retry:
	/* We check that adding this trx to the waits-for graph
	does not produce a cycle. First mark all active transactions
	with 0: */

	mark_trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (mark_trx) {
		mark_trx->deadlock_mark = 0;
		mark_trx = UT_LIST_GET_NEXT(trx_list, mark_trx);
	}

	ret = lock_deadlock_recursive(trx, trx, lock, &cost, 0);

	switch (ret) {
	case LOCK_VICTIM_IS_OTHER:
		/* We chose some other trx as a victim: retry if there still
		is a deadlock */
		goto retry;

	case LOCK_EXCEED_MAX_DEPTH:
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

		lock_deadlock_fputs(
			"*** WAITING FOR THIS LOCK TO BE GRANTED:\n");

		lock_deadlock_lock_print(lock);

		break;

	case LOCK_VICTIM_IS_START:
		lock_deadlock_fputs("*** WE ROLL BACK TRANSACTION (2)\n");
		break;

	default:
		/* No deadlock detected*/
		return(FALSE);
	}

	lock_deadlock_found = TRUE;

	return(TRUE);
}

/********************************************************************//**
Looks recursively for a deadlock.
@return 0 if no deadlock found, LOCK_VICTIM_IS_START if there was a
deadlock and we chose 'start' as the victim, LOCK_VICTIM_IS_OTHER if a
deadlock was found and we chose some other trx as a victim: we must do
the search again in this last case because there may be another
deadlock!
LOCK_EXCEED_MAX_DEPTH if the lock search exceeds max steps or max depth. */
static
ulint
lock_deadlock_recursive(
/*====================*/
	trx_t*	start,		/*!< in: recursion starting point */
	trx_t*	trx,		/*!< in: a transaction waiting for a lock */
	lock_t*	wait_lock,	/*!< in: lock that is waiting to be granted */
	ulint*	cost,		/*!< in/out: number of calculation steps thus
				far: if this exceeds LOCK_MAX_N_STEPS_...
				we return LOCK_EXCEED_MAX_DEPTH */
	ulint	depth)		/*!< in: recursion depth: if this exceeds
				LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK, we
				return LOCK_EXCEED_MAX_DEPTH */
{
	ulint	ret;
	lock_t*	lock;
	trx_t*	lock_trx;
	ulint	heap_no		= ULINT_UNDEFINED;

	ut_a(trx);
	ut_a(start);
	ut_a(wait_lock);
	ut_ad(mutex_own(&kernel_mutex));

	if (trx->deadlock_mark == 1) {
		/* We have already exhaustively searched the subtree starting
		from this trx */

		return(0);
	}

	*cost = *cost + 1;

	if (lock_get_type_low(wait_lock) == LOCK_REC) {
		ulint		space;
		ulint		page_no;

		heap_no = lock_rec_find_set_bit(wait_lock);
		ut_a(heap_no != ULINT_UNDEFINED);

		space = wait_lock->un_member.rec_lock.space;
		page_no = wait_lock->un_member.rec_lock.page_no;

		lock = lock_rec_get_first_on_page_addr(space, page_no);

		/* Position the iterator on the first matching record lock. */
		while (lock != NULL
		       && lock != wait_lock
		       && !lock_rec_get_nth_bit(lock, heap_no)) {

			lock = lock_rec_get_next_on_page(lock);
		}

		if (lock == wait_lock) {
			lock = NULL;
		}

		ut_ad(lock == NULL || lock_rec_get_nth_bit(lock, heap_no));

	} else {
		lock = wait_lock;
	}

	/* Look at the locks ahead of wait_lock in the lock queue */

	for (;;) {
		/* Get previous table lock. */
		if (heap_no == ULINT_UNDEFINED) {

			lock = UT_LIST_GET_PREV(
				un_member.tab_lock.locks, lock);
		}

		if (lock == NULL) {
			/* We can mark this subtree as searched */
			trx->deadlock_mark = 1;

			return(FALSE);
		}

		if (lock_has_to_wait(wait_lock, lock)) {

			ibool	too_far
				= depth > LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK
				|| *cost > LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK;

			lock_trx = lock->trx;

			if (lock_trx == start) {

				/* We came back to the recursion starting
				point: a deadlock detected; or we have
				searched the waits-for graph too long */

				lock_deadlock_start_print();

				lock_deadlock_fputs("\n*** (1) TRANSACTION:\n");

				lock_deadlock_trx_print(wait_lock->trx, 3000);

				lock_deadlock_fputs(
					"*** (1) WAITING FOR THIS LOCK"
					" TO BE GRANTED:\n");

				lock_deadlock_lock_print(wait_lock);

				lock_deadlock_fputs("*** (2) TRANSACTION:\n");

				lock_deadlock_trx_print(lock->trx, 3000);

				lock_deadlock_fputs(
					"*** (2) HOLDS THE LOCK(S):\n");

				lock_deadlock_lock_print(lock);

				lock_deadlock_fputs(
					"*** (2) WAITING FOR THIS LOCK"
					" TO BE GRANTED:\n");

				lock_deadlock_lock_print(start->wait_lock);

#ifdef UNIV_DEBUG
				if (lock_print_waits) {
					fputs("Deadlock detected\n",
					      stderr);
				}
#endif /* UNIV_DEBUG */

				if (trx_weight_ge(wait_lock->trx, start)) {
					/* Our recursion starting point
					transaction is 'smaller', let us
					choose 'start' as the victim and roll
					back it */

					return(LOCK_VICTIM_IS_START);
				}

				lock_deadlock_found = TRUE;

				/* Let us choose the transaction of wait_lock
				as a victim to try to avoid deadlocking our
				recursion starting point transaction */

				lock_deadlock_fputs(
					"*** WE ROLL BACK TRANSACTION (1)\n");

				wait_lock->trx->was_chosen_as_deadlock_victim
					= TRUE;

				lock_cancel_waiting_and_release(wait_lock);

				/* Since trx and wait_lock are no longer
				in the waits-for graph, we can return FALSE;
				note that our selective algorithm can choose
				several transactions as victims, but still
				we may end up rolling back also the recursion
				starting point transaction! */

				return(LOCK_VICTIM_IS_OTHER);
			}

			if (too_far) {

#ifdef UNIV_DEBUG
				if (lock_print_waits) {
					fputs("Deadlock search exceeds"
					      " max steps or depth.\n",
					      stderr);
				}
#endif /* UNIV_DEBUG */
				/* The information about transaction/lock
				to be rolled back is available in the top
				level. Do not print anything here. */
				return(LOCK_EXCEED_MAX_DEPTH);
			}

			if (lock_trx->que_state == TRX_QUE_LOCK_WAIT) {

				/* Another trx ahead has requested lock	in an
				incompatible mode, and is itself waiting for
				a lock */

				ret = lock_deadlock_recursive(
					start, lock_trx,
					lock_trx->wait_lock, cost, depth + 1);

				if (ret != 0) {

					return(ret);
				}
			}
		}
		/* Get the next record lock to check. */
		if (heap_no != ULINT_UNDEFINED) {

			ut_a(lock != NULL);

			do {
				lock = lock_rec_get_next_on_page(lock);
			} while (lock != NULL
				&& lock != wait_lock
				&& !lock_rec_get_nth_bit(lock, heap_no));

			if (lock == wait_lock) {
				lock = NULL;
			}
		}
	}/* end of the 'for (;;)'-loop */
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
	dict_table_t*	table,	/*!< in: database table in dictionary cache */
	ulint		type_mode,/*!< in: lock mode possibly ORed with
				LOCK_WAIT */
	trx_t*		trx)	/*!< in: trx */
{
	lock_t*	lock;

	ut_ad(table && trx);
	ut_ad(mutex_own(&kernel_mutex));

	if ((type_mode & LOCK_MODE_MASK) == LOCK_AUTO_INC) {
		++table->n_waiting_or_granted_auto_inc_locks;
	}

	/* For AUTOINC locking we reuse the lock instance only if
	there is no wait involved else we allocate the waiting lock
	from the transaction lock heap. */
	if (type_mode == LOCK_AUTO_INC) {

		lock = table->autoinc_lock;

		table->autoinc_trx = trx;

		ib_vector_push(trx->autoinc_locks, lock);
	} else {
		lock = mem_heap_alloc(trx->lock_heap, sizeof(lock_t));
	}

	UT_LIST_ADD_LAST(trx_locks, trx->trx_locks, lock);

	lock->type_mode = type_mode | LOCK_TABLE;
	lock->trx = trx;

	lock->un_member.tab_lock.table = table;

	UT_LIST_ADD_LAST(un_member.tab_lock.locks, table->locks, lock);

	if (UNIV_UNLIKELY(type_mode & LOCK_WAIT)) {

		lock_set_lock_and_trx_wait(lock, trx);
	}

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
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(!ib_vector_is_empty(trx->autoinc_locks));

	/* Skip any gaps, gaps are NULL lock entries in the
	trx->autoinc_locks vector. */

	do {
		ib_vector_pop(trx->autoinc_locks);

		if (ib_vector_is_empty(trx->autoinc_locks)) {
			return;
		}

	} while (ib_vector_get_last(trx->autoinc_locks) == NULL);
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

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_mode(lock) == LOCK_AUTO_INC);
	ut_ad(lock_get_type_low(lock) & LOCK_TABLE);
	ut_ad(!ib_vector_is_empty(trx->autoinc_locks));

	/* With stored functions and procedures the user may drop
	a table within the same "statement". This special case has
	to be handled by deleting only those AUTOINC locks that were
	held by the table being dropped. */

	autoinc_lock = ib_vector_get(trx->autoinc_locks, i);

	/* This is the default fast case. */

	if (autoinc_lock == lock) {
		lock_table_pop_autoinc_locks(trx);
	} else {
		/* The last element should never be NULL */
		ut_a(autoinc_lock != NULL);

		/* Handle freeing the locks from within the stack. */

		while (--i >= 0) {
			autoinc_lock = ib_vector_get(trx->autoinc_locks, i);

			if (UNIV_LIKELY(autoinc_lock == lock)) {
				ib_vector_set(trx->autoinc_locks, i, NULL);
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
	lock_t*	lock)	/*!< in: table lock */
{
	trx_t*		trx;
	dict_table_t*	table;

	ut_ad(mutex_own(&kernel_mutex));

	trx = lock->trx;
	table = lock->un_member.tab_lock.table;

	/* Remove the table from the transaction's AUTOINC vector, if
	the lock that is being release is an AUTOINC lock. */
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
		--table->n_waiting_or_granted_auto_inc_locks;
	}

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, lock);
	UT_LIST_REMOVE(un_member.tab_lock.locks, table->locks, lock);
}

/*********************************************************************//**
Enqueues a waiting request for a table lock which cannot be granted
immediately. Checks for deadlocks.
@return DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED, or
DB_SUCCESS; DB_SUCCESS means that there was a deadlock, but another
transaction was chosen as a victim, and we got the lock immediately:
no need to wait then */
static
ulint
lock_table_enqueue_waiting(
/*=======================*/
	ulint		mode,	/*!< in: lock mode this transaction is
				requesting */
	dict_table_t*	table,	/*!< in: table */
	que_thr_t*	thr)	/*!< in: query thread */
{
	lock_t*	lock;
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (que_thr_stop(thr)) {
		ut_error;

		return(DB_QUE_THR_SUSPENDED);
	}

	trx = thr_get_trx(thr);

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

	/* Check if a deadlock occurs: if yes, remove the lock request and
	return an error code */

	if (lock_deadlock_occurs(lock, trx)) {

		/* The order here is important, we don't want to
		lose the state of the lock before calling remove. */
		lock_table_remove_low(lock);
		lock_reset_lock_and_trx_wait(lock);

		return(DB_DEADLOCK);
	}

	if (trx->wait_lock == NULL) {
		/* Deadlock resolution chose another transaction as a victim,
		and we accidentally got our lock granted! */

		return(DB_SUCCESS);
	}

	trx->que_state = TRX_QUE_LOCK_WAIT;
	trx->was_chosen_as_deadlock_victim = FALSE;
	trx->wait_started = time(NULL);

	ut_a(que_thr_stop(thr));

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

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_LAST(table->locks);

	while (lock != NULL) {

		if ((lock->trx != trx)
		    && (!lock_mode_compatible(lock_get_mode(lock), mode))
		    && (wait || !(lock_get_wait(lock)))) {

			return(lock);
		}

		lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
	}

	return(NULL);
}

/*********************************************************************//**
Locks the specified database table in the mode given. If the lock cannot
be granted immediately, the query thread is put to wait.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
UNIV_INTERN
ulint
lock_table(
/*=======*/
	ulint		flags,	/*!< in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	dict_table_t*	table,	/*!< in: database table in dictionary cache */
	enum lock_mode	mode,	/*!< in: lock mode */
	que_thr_t*	thr)	/*!< in: query thread */
{
	trx_t*	trx;
	ulint	err;

	ut_ad(table && thr);

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_a(flags == 0);

	trx = thr_get_trx(thr);

	lock_mutex_enter_kernel();

	/* Look for stronger locks the same trx already has on the table */

	if (lock_table_has(trx, table, mode)) {

		lock_mutex_exit_kernel();

		return(DB_SUCCESS);
	}

	/* We have to check if the new lock is compatible with any locks
	other transactions have in the table lock queue. */

	if (lock_table_other_has_incompatible(trx, LOCK_WAIT, table, mode)) {

		/* Another trx has a request on the table in an incompatible
		mode: this trx may have to wait */

		err = lock_table_enqueue_waiting(mode | flags, table, thr);

		lock_mutex_exit_kernel();

		return(err);
	}

	lock_table_create(table, mode | flags, trx);

	ut_a(!flags || mode == LOCK_S || mode == LOCK_X);

	lock_mutex_exit_kernel();

	return(DB_SUCCESS);
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

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_wait(wait_lock));

	table = wait_lock->un_member.tab_lock.table;

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock != wait_lock) {

		if (lock_has_to_wait(wait_lock, lock)) {

			return(TRUE);
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
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
	lock_t*	in_lock)/*!< in: table lock object; transactions waiting
			behind will get their lock requests granted, if
			they are now qualified to it */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));
	ut_a(lock_get_type_low(in_lock) == LOCK_TABLE);

	lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, in_lock);

	lock_table_remove_low(in_lock);

	/* Check if waiting locks in the queue can now be granted: grant
	locks if there are no conflicting locks ahead. */

	while (lock != NULL) {

		if (lock_get_wait(lock)
		    && !lock_table_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			lock_grant(lock);
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
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
	trx_t*			trx,	/*!< in: transaction that has
					set a record lock */
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record */
	enum lock_mode		lock_mode)/*!< in: LOCK_S or LOCK_X */
{
	lock_t*	first_lock;
	lock_t*	lock;
	ulint	heap_no;

	ut_ad(trx && rec);
	ut_ad(block->frame == page_align(rec));

	heap_no = page_rec_get_heap_no(rec);

	mutex_enter(&kernel_mutex);

	first_lock = lock_rec_get_first(block, heap_no);

	/* Find the last lock with the same lock_mode and transaction
	from the record. */

	for (lock = first_lock; lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {
		if (lock->trx == trx && lock_get_mode(lock) == lock_mode) {
			ut_a(!lock_get_wait(lock));
			lock_rec_reset_nth_bit(lock, heap_no);
			goto released;
		}
	}

	mutex_exit(&kernel_mutex);
	ut_print_timestamp(stderr);
	fprintf(stderr,
		"  InnoDB: Error: unlock row could not"
		" find a %lu mode lock on the record\n",
		(ulong) lock_mode);

	return;

released:
	/* Check if we can now grant waiting lock requests */

	for (lock = first_lock; lock != NULL;
	     lock = lock_rec_get_next(heap_no, lock)) {
		if (lock_get_wait(lock)
		    && !lock_rec_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			lock_grant(lock);
		}
	}

	mutex_exit(&kernel_mutex);
}

/*********************************************************************//**
Releases transaction locks, and releases possible other transactions waiting
because of these locks. */
UNIV_INTERN
void
lock_release_off_kernel(
/*====================*/
	trx_t*	trx)	/*!< in: transaction */
{
	dict_table_t*	table;
	ulint		count;
	lock_t*		lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_LAST(trx->trx_locks);

	count = 0;

	while (lock != NULL) {

		count++;

		if (lock_get_type_low(lock) == LOCK_REC) {

			lock_rec_dequeue_from_page(lock);
		} else {
			ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

			if (lock_get_mode(lock) != LOCK_IS
			    && trx->undo_no != 0) {

				/* The trx may have modified the table. We
				block the use of the MySQL query cache for
				all currently active transactions. */

				table = lock->un_member.tab_lock.table;

				table->query_cache_inv_trx_id
					= trx_sys->max_trx_id;
			}

			lock_table_dequeue(lock);
		}

		if (count == LOCK_RELEASE_KERNEL_INTERVAL) {
			/* Release the kernel mutex for a while, so that we
			do not monopolize it */

			lock_mutex_exit_kernel();

			lock_mutex_enter_kernel();

			count = 0;
		}

		lock = UT_LIST_GET_LAST(trx->trx_locks);
	}

	ut_a(ib_vector_size(trx->autoinc_locks) == 0);

	mem_heap_empty(trx->lock_heap);
}

/*********************************************************************//**
Cancels a waiting lock request and releases possible other transactions
waiting behind it. */
UNIV_INTERN
void
lock_cancel_waiting_and_release(
/*============================*/
	lock_t*	lock)	/*!< in: waiting lock request */
{
	ut_ad(mutex_own(&kernel_mutex));

	if (lock_get_type_low(lock) == LOCK_REC) {

		lock_rec_dequeue_from_page(lock);
	} else {
		ut_ad(lock_get_type_low(lock) & LOCK_TABLE);

		if (lock->trx->autoinc_locks != NULL) {
			/* Release the transaction's AUTOINC locks/ */
			lock_release_autoinc_locks(lock->trx);
		}

		lock_table_dequeue(lock);
	}

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_end_lock_wait(lock->trx);
}

/* True if a lock mode is S or X */
#define IS_LOCK_S_OR_X(lock) \
	(lock_get_mode(lock) == LOCK_S \
	 || lock_get_mode(lock) == LOCK_X)


/*********************************************************************//**
Removes locks of a transaction on a table to be dropped.
If remove_also_table_sx_locks is TRUE then table-level S and X locks are
also removed in addition to other table-level and record-level locks.
No lock, that is going to be removed, is allowed to be a wait lock. */
static
void
lock_remove_all_on_table_for_trx(
/*=============================*/
	dict_table_t*	table,			/*!< in: table to be dropped */
	trx_t*		trx,			/*!< in: a transaction */
	ibool		remove_also_table_sx_locks)/*!< in: also removes
						table S and X locks */
{
	lock_t*	lock;
	lock_t*	prev_lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_LAST(trx->trx_locks);

	while (lock != NULL) {
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

			lock_table_remove_low(lock);
		}

		lock = prev_lock;
	}
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
	lock_t*	lock;
	lock_t*	prev_lock;

	mutex_enter(&kernel_mutex);

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock != NULL) {

		prev_lock = UT_LIST_GET_PREV(un_member.tab_lock.locks,
					     lock);

		/* If we should remove all locks (remove_also_table_sx_locks
		is TRUE), or if the lock is not table-level S or X lock,
		then check we are not going to remove a wait lock. */
		if (remove_also_table_sx_locks
		    || !(lock_get_type(lock) == LOCK_TABLE
			 && IS_LOCK_S_OR_X(lock))) {

			ut_a(!lock_get_wait(lock));
		}

		lock_remove_all_on_table_for_trx(table, lock->trx,
						 remove_also_table_sx_locks);

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

	mutex_exit(&kernel_mutex);
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
	ut_ad(mutex_own(&kernel_mutex));
	ut_a(lock_get_type_low(lock) == LOCK_TABLE);

	fputs("TABLE LOCK table ", file);
	ut_print_name(file, lock->trx, TRUE,
		      lock->un_member.tab_lock.table->name);
	fprintf(file, " trx id " TRX_ID_FMT, (ullint) lock->trx->id);

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

	ut_ad(mutex_own(&kernel_mutex));
	ut_a(lock_get_type_low(lock) == LOCK_REC);

	space = lock->un_member.rec_lock.space;
	page_no = lock->un_member.rec_lock.page_no;

	fprintf(file, "RECORD LOCKS space id %lu page no %lu n bits %lu ",
		(ulong) space, (ulong) page_no,
		(ulong) lock_rec_get_n_bits(lock));
	dict_index_name_print(file, lock->trx, lock->index);
	fprintf(file, " trx id " TRX_ID_FMT, (ullint) lock->trx->id);

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
	lock_t*	lock;
	ulint	n_locks	= 0;
	ulint	i;

	ut_ad(mutex_own(&kernel_mutex));

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {

		lock = HASH_GET_FIRST(lock_sys->rec_hash, i);

		while (lock) {
			n_locks++;

			lock = HASH_GET_NEXT(hash, lock);
		}
	}

	return(n_locks);
}
#endif /* PRINT_NUM_OF_LOCK_STRUCTS */

/*********************************************************************//**
Prints info of locks for all transactions.
@return FALSE if not able to obtain kernel mutex
and exits without printing info */
UNIV_INTERN
ibool
lock_print_info_summary(
/*====================*/
	FILE*	file,	/*!< in: file where to print */
	ibool   nowait)	/*!< in: whether to wait for the kernel mutex */
{
	/* if nowait is FALSE, wait on the kernel mutex,
	otherwise return immediately if fail to obtain the
	mutex. */
	if (!nowait) {
		lock_mutex_enter_kernel();
	} else if (mutex_enter_nowait(&kernel_mutex)) {
		fputs("FAIL TO OBTAIN KERNEL MUTEX, "
		      "SKIP LOCK INFO PRINTING\n", file);
		return(FALSE);
	}

	if (lock_deadlock_found) {
		fputs("------------------------\n"
		      "LATEST DETECTED DEADLOCK\n"
		      "------------------------\n", file);

		ut_copy_file(file, lock_latest_err_file);
	}

	fputs("------------\n"
	      "TRANSACTIONS\n"
	      "------------\n", file);

	fprintf(file, "Trx id counter " TRX_ID_FMT "\n",
		(ullint) trx_sys->max_trx_id);

	fprintf(file,
		"Purge done for trx's n:o < " TRX_ID_FMT
		" undo n:o < " TRX_ID_FMT "\n",
		(ullint) purge_sys->purge_trx_no,
		(ullint) purge_sys->purge_undo_no);

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
Prints info of locks for each transaction. */
UNIV_INTERN
void
lock_print_info_all_transactions(
/*=============================*/
	FILE*	file)	/*!< in: file where to print */
{
	lock_t*	lock;
	ibool	load_page_first = TRUE;
	ulint	nth_trx		= 0;
	ulint	nth_lock	= 0;
	ulint	i;
	mtr_t	mtr;
	trx_t*	trx;

	fprintf(file, "LIST OF TRANSACTIONS FOR EACH SESSION:\n");

	/* First print info on non-active transactions */

	trx = UT_LIST_GET_FIRST(trx_sys->mysql_trx_list);

	while (trx) {
		if (trx->conc_state == TRX_NOT_STARTED) {
			fputs("---", file);
			trx_print(file, trx, 600);
		}

		trx = UT_LIST_GET_NEXT(mysql_trx_list, trx);
	}

loop:
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	i = 0;

	/* Since we temporarily release the kernel mutex when
	reading a database page in below, variable trx may be
	obsolete now and we must loop through the trx list to
	get probably the same trx, or some other trx. */

	while (trx && (i < nth_trx)) {
		trx = UT_LIST_GET_NEXT(trx_list, trx);
		i++;
	}

	if (trx == NULL) {
		lock_mutex_exit_kernel();

		ut_ad(lock_validate());

		return;
	}

	if (nth_lock == 0) {
		fputs("---", file);
		trx_print(file, trx, 600);

		if (trx->read_view) {
			fprintf(file,
				"Trx read view will not see trx with"
				" id >= " TRX_ID_FMT
				", sees < " TRX_ID_FMT "\n",
				(ullint) trx->read_view->low_limit_id,
				(ullint) trx->read_view->up_limit_id);
		}

		if (trx->que_state == TRX_QUE_LOCK_WAIT) {
			fprintf(file,
				"------- TRX HAS BEEN WAITING %lu SEC"
				" FOR THIS LOCK TO BE GRANTED:\n",
				(ulong) difftime(time(NULL),
						 trx->wait_started));

			if (lock_get_type_low(trx->wait_lock) == LOCK_REC) {
				lock_rec_print(file, trx->wait_lock);
			} else {
				lock_table_print(file, trx->wait_lock);
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

	lock = UT_LIST_GET_FIRST(trx->trx_locks);

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

			lock_mutex_exit_kernel();

			mtr_start(&mtr);

			buf_page_get_with_no_latch(space, zip_size,
						   page_no, &mtr);

			mtr_commit(&mtr);

			load_page_first = FALSE;

			lock_mutex_enter_kernel();

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

		goto loop;
	}

	goto loop;
}

#ifdef UNIV_DEBUG
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

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock) {
		ut_a(((lock->trx)->conc_state == TRX_ACTIVE)
		     || ((lock->trx)->conc_state == TRX_PREPARED)
		     || ((lock->trx)->conc_state == TRX_COMMITTED_IN_MEMORY));

		if (!lock_get_wait(lock)) {

			ut_a(!lock_table_other_has_incompatible(
				     lock->trx, 0, table,
				     lock_get_mode(lock)));
		} else {

			ut_a(lock_table_has_to_wait_in_queue(lock));
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
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
	const buf_block_t*	block,	/*!< in: buffer block containing rec */
	const rec_t*		rec,	/*!< in: record to look at */
	const dict_index_t*	index,	/*!< in: index, or NULL if not known */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
{
	trx_t*	impl_trx;
	lock_t*	lock;
	ulint	heap_no;

	ut_a(rec);
	ut_a(block->frame == page_align(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));

	heap_no = page_rec_get_heap_no(rec);

	lock_mutex_enter_kernel();

	if (!page_rec_is_user_rec(rec)) {

		lock = lock_rec_get_first(block, heap_no);

		while (lock) {
			switch(lock->trx->conc_state) {
			case TRX_ACTIVE:
			case TRX_PREPARED:
			case TRX_COMMITTED_IN_MEMORY:
				break;
			default:
				ut_error;
			}

			ut_a(trx_in_trx_list(lock->trx));

			if (lock_get_wait(lock)) {
				ut_a(lock_rec_has_to_wait_in_queue(lock));
			}

			if (index) {
				ut_a(lock->index == index);
			}

			lock = lock_rec_get_next(heap_no, lock);
		}

		lock_mutex_exit_kernel();

		return(TRUE);
	}

	if (!index);
	else if (dict_index_is_clust(index)) {

		impl_trx = lock_clust_rec_some_has_impl(rec, index, offsets);

		if (impl_trx
		    && lock_rec_other_has_expl_req(LOCK_S, 0, LOCK_WAIT,
						   block, heap_no, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP,
					       block, heap_no, impl_trx));
		}
#if 0
	} else {

		/* The kernel mutex may get released temporarily in the
		next function call: we have to release lock table mutex
		to obey the latching order */

		/* If this thread is holding the file space latch
		(fil_space_t::latch), the following check WILL break
		latching order and may cause a deadlock of threads. */

		/* NOTE: This is a bogus check that would fail in the
		following case: Our transaction is updating a
		row. After it has updated the clustered index record,
		it goes to a secondary index record and finds someone
		else holding an explicit S- or X-lock on that
		secondary index record, presumably from a locking
		read. Our transaction cannot update the secondary
		index immediately, but places a waiting X-lock request
		on the secondary index record. There is nothing
		illegal in this. The assertion is simply too strong. */

		/* From the locking point of view, each secondary
		index is a separate table. A lock that is held on
		secondary index rec does not give any rights to modify
		or read the clustered index rec. Therefore, we can
		think of the sec index as a separate 'table' from the
		clust index 'table'. Conversely, a transaction that
		has acquired a lock on and modified a clustered index
		record may need to wait for a lock on the
		corresponding record in a secondary index. */

		impl_trx = lock_sec_rec_some_has_impl_off_kernel(
			rec, index, offsets);

		if (impl_trx
		    && lock_rec_other_has_expl_req(LOCK_S, 0, LOCK_WAIT,
						   block, heap_no, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP,
					       block, heap_no, impl_trx));
		}
#endif
	}

	lock = lock_rec_get_first(block, heap_no);

	while (lock) {
		ut_a(lock->trx->conc_state == TRX_ACTIVE
		     || lock->trx->conc_state == TRX_PREPARED
		     || lock->trx->conc_state == TRX_COMMITTED_IN_MEMORY);
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

		lock = lock_rec_get_next(heap_no, lock);
	}

	lock_mutex_exit_kernel();

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

	ut_ad(!mutex_own(&kernel_mutex));
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

	lock_mutex_enter_kernel();
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
	ut_a(lock->trx->conc_state == TRX_ACTIVE
	     || lock->trx->conc_state == TRX_PREPARED
	     || lock->trx->conc_state == TRX_COMMITTED_IN_MEMORY);

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
			lock_mutex_exit_kernel();

			/* If this thread is holding the file space
			latch (fil_space_t::latch), the following
			check WILL break the latching order and may
			cause a deadlock of threads. */

			lock_rec_queue_validate(block, rec, lock->index,
						offsets);

			lock_mutex_enter_kernel();

			nth_bit = i + 1;

			goto loop;
		}
	}

	nth_bit = 0;
	nth_lock++;

	goto loop;

function_exit:
	lock_mutex_exit_kernel();

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
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
	lock_t*		lock;
	ut_ad(mutex_own(&kernel_mutex));

	for (lock = HASH_GET_FIRST(lock_sys->rec_hash, start);
	     lock != NULL;
	     lock = HASH_GET_NEXT(hash, lock)) {

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

	return(NULL);
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

	mtr_t		mtr;
	buf_block_t*	block;

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
ibool
lock_validate(void)
/*===============*/
{
	const lock_t*	lock;
	const trx_t*	trx;
	ulint		i;

	lock_mutex_enter_kernel();

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		lock = UT_LIST_GET_FIRST(trx->trx_locks);

		while (lock) {
			if (lock_get_type_low(lock) & LOCK_TABLE) {

				lock_table_queue_validate(
					lock->un_member.tab_lock.table);
			}

			lock = UT_LIST_GET_NEXT(trx_locks, lock);
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	/* Iterate over all the record locks and validate the locks. We
	don't want to hog the lock_sys_t::mutex and the trx_sys_t::mutex.
	Release both mutexes during the validation check. */

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {
		const lock_t*	lock;
		ib_uint64_t	limit = 0;

		while ((lock = lock_rec_validate(i, &limit)) != NULL) {

			ulint	space = lock->un_member.rec_lock.space;
			ulint	page_no = lock->un_member.rec_lock.page_no;

			lock_mutex_exit_kernel();
			lock_rec_block_validate(space, page_no);
			lock_mutex_enter_kernel();
		}
	}

	lock_mutex_exit_kernel();

	return(TRUE);
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
ulint
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
	ulint		err;
	ulint		next_rec_heap_no;

	ut_ad(block->frame == page_align(rec));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	trx = thr_get_trx(thr);
	next_rec = page_rec_get_next_const(rec);
	next_rec_heap_no = page_rec_get_heap_no(next_rec);

	lock_mutex_enter_kernel();

	/* When inserting a record into an index, the table must be at
	least IX-locked or we must be building an index, in which case
	the table must be at least S-locked. */
	ut_ad(lock_table_has(trx, index->table, LOCK_IX)
	      || (*index->name == TEMP_INDEX_PREFIX
		  && lock_table_has(trx, index->table, LOCK_S)));

	lock = lock_rec_get_first(block, next_rec_heap_no);

	if (UNIV_LIKELY(lock == NULL)) {
		/* We optimize CPU time usage in the simplest case */

		lock_mutex_exit_kernel();

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
		    LOCK_X | LOCK_GAP | LOCK_INSERT_INTENTION,
		    block, next_rec_heap_no, trx)) {

		/* Note that we may get DB_SUCCESS also here! */
		err = lock_rec_enqueue_waiting(LOCK_X | LOCK_GAP
					       | LOCK_INSERT_INTENTION,
					       block, next_rec_heap_no,
					       index, thr);
	} else {
		err = DB_SUCCESS;
	}

	lock_mutex_exit_kernel();

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
	}

#ifdef UNIV_DEBUG
	{
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		const ulint*	offsets;
		rec_offs_init(offsets_);

		offsets = rec_get_offsets(next_rec, index, offsets_,
					  ULINT_UNDEFINED, &heap);
		ut_ad(lock_rec_queue_validate(block,
					      next_rec, index, offsets));
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}
#endif /* UNIV_DEBUG */

	return(err);
}

/*********************************************************************//**
If a transaction has an implicit x-lock on a record, but no explicit x-lock
set on the record, sets one for it. NOTE that in the case of a secondary
index, the kernel mutex may get temporarily released. */
static
void
lock_rec_convert_impl_to_expl(
/*==========================*/
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: user record on page */
	dict_index_t*		index,	/*!< in: index of record */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
{
	trx_t*	impl_trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(page_rec_is_user_rec(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));

	if (dict_index_is_clust(index)) {
		impl_trx = lock_clust_rec_some_has_impl(rec, index, offsets);
	} else {
		impl_trx = lock_sec_rec_some_has_impl_off_kernel(
			rec, index, offsets);
	}

	if (impl_trx) {
		ulint	heap_no = page_rec_get_heap_no(rec);

		/* If the transaction has no explicit x-lock set on the
		record, set one for it */

		if (!lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, block,
				       heap_no, impl_trx)) {

			lock_rec_add_to_queue(
				LOCK_REC | LOCK_X | LOCK_REC_NOT_GAP,
				block, heap_no, index, impl_trx);
		}
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
ulint
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
	ulint	err;
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

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	/* If a transaction has no explicit x-lock set on the record, set one
	for it */

	lock_rec_convert_impl_to_expl(block, rec, index, offsets);

	err = lock_rec_lock(TRUE, LOCK_X | LOCK_REC_NOT_GAP,
			    block, heap_no, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(block, rec, index, offsets));

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
ulint
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
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	err;
	ulint	heap_no;

	ut_ad(!dict_index_is_clust(index));
	ut_ad(block->frame == page_align(rec));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	heap_no = page_rec_get_heap_no(rec);

	/* Another transaction cannot have an implicit lock on the record,
	because when we come here, we already have modified the clustered
	index record, and this would not have been possible if another active
	transaction had modified this secondary index record. */

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	err = lock_rec_lock(TRUE, LOCK_X | LOCK_REC_NOT_GAP,
			    block, heap_no, index, thr);

	lock_mutex_exit_kernel();

#ifdef UNIV_DEBUG
	{
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		const ulint*	offsets;
		rec_offs_init(offsets_);

		offsets = rec_get_offsets(rec, index, offsets_,
					  ULINT_UNDEFINED, &heap);
		ut_ad(lock_rec_queue_validate(block, rec, index, offsets));
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
enum db_err
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
	enum db_err	err;
	ulint		heap_no;

	ut_ad(!dict_index_is_clust(index));
	ut_ad(block->frame == page_align(rec));
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	heap_no = page_rec_get_heap_no(rec);

	lock_mutex_enter_kernel();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list or a
	database recovery is running. */

	if ((page_get_max_trx_id(block->frame) >= trx_list_get_min_trx_id()
	     || recv_recovery_is_on())
	    && !page_rec_is_supremum(rec)) {

		lock_rec_convert_impl_to_expl(block, rec, index, offsets);
	}

	err = lock_rec_lock(FALSE, mode | gap_mode,
			    block, heap_no, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(block, rec, index, offsets));

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
enum db_err
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
	enum db_err	err;
	ulint		heap_no;

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

	lock_mutex_enter_kernel();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

	if (UNIV_LIKELY(heap_no != PAGE_HEAP_NO_SUPREMUM)) {

		lock_rec_convert_impl_to_expl(block, rec, index, offsets);
	}

	err = lock_rec_lock(FALSE, mode | gap_mode,
			    block, heap_no, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(block, rec, index, offsets));

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
ulint
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
	ulint		err;
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

	ut_ad(mutex_own(&kernel_mutex));
	ut_a(!ib_vector_is_empty(autoinc_locks));

	/* The lock to be release must be the last lock acquired. */
	last = ib_vector_size(autoinc_locks) - 1;
	lock = ib_vector_get(autoinc_locks, last);

	/* Should have only AUTOINC locks in the vector. */
	ut_a(lock_get_mode(lock) == LOCK_AUTO_INC);
	ut_a(lock_get_type(lock) == LOCK_TABLE);

	ut_a(lock->un_member.tab_lock.table != NULL);

	/* This will remove the lock from the trx autoinc_locks too. */
	lock_table_dequeue(lock);
}

/*******************************************************************//**
Check if a transaction holds any autoinc locks. 
@return TRUE if the transaction holds any AUTOINC locks. */
UNIV_INTERN
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
UNIV_INTERN
void
lock_release_autoinc_locks(
/*=======================*/
	trx_t*		trx)		/*!< in/out: transaction */
{
	ut_ad(mutex_own(&kernel_mutex));

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
