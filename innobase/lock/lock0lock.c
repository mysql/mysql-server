/******************************************************
The transaction lock system

(c) 1996 Innobase Oy

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#include "lock0lock.h"

#ifdef UNIV_NONINL
#include "lock0lock.ic"
#endif

#include "usr0sess.h"
#include "trx0purge.h"
#include "dict0mem.h"
#include "trx0sys.h"


/* 2 function prototypes copied from ha_innodb.cc: */

/*****************************************************************
If you want to print a thd that is not associated with the current thread,
you must call this function before reserving the InnoDB kernel_mutex, to
protect MySQL from setting thd->query NULL. If you print a thd of the current
thread, we know that MySQL cannot modify thd->query, and it is not necessary
to call this. Call innobase_mysql_end_print_arbitrary_thd() after you release
the kernel_mutex.
NOTE that /mysql/innobase/lock/lock0lock.c must contain the prototype for this
function! */

void
innobase_mysql_prepare_print_arbitrary_thd(void);
/*============================================*/

/*****************************************************************
Relases the mutex reserved by innobase_mysql_prepare_print_arbitrary_thd().
NOTE that /mysql/innobase/lock/lock0lock.c must contain the prototype for this
function! */

void
innobase_mysql_end_print_arbitrary_thd(void);
/*========================================*/

/* Restricts the length of search we will do in the waits-for
graph of transactions */
#define LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK 1000000

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
move the read cursor backward from the the next record for it to pass over
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

ibool	lock_print_waits	= FALSE;

/* The lock system */
lock_sys_t*	lock_sys	= NULL;

/* A table lock */
typedef struct lock_table_struct	lock_table_t;
struct lock_table_struct{
	dict_table_t*	table;	/* database table in dictionary cache */
	UT_LIST_NODE_T(lock_t)
			locks; 	/* list of locks on the same table */
};

/* Record lock for a page */
typedef struct lock_rec_struct		lock_rec_t;
struct lock_rec_struct{
	ulint	space;		/* space id */
	ulint	page_no;	/* page number */
	ulint	n_bits;		/* number of bits in the lock bitmap */
				/* NOTE: the lock bitmap is placed immediately
				after the lock struct */
};

/* Lock struct */
struct lock_struct{
	trx_t*		trx;		/* transaction owning the lock */
	UT_LIST_NODE_T(lock_t)		
			trx_locks;	/* list of the locks of the
					transaction */
	ulint		type_mode;	/* lock type, mode, LOCK_GAP or
					LOCK_REC_NOT_GAP,
					LOCK_INSERT_INTENTION,
					wait flag, ORed */
	hash_node_t	hash;		/* hash chain node for a record lock */
	dict_index_t*	index;		/* index for a record lock */
	union {
		lock_table_t	tab_lock;/* table lock */
		lock_rec_t	rec_lock;/* record lock */
	} un_member;
};

/* We store info on the latest deadlock error to this buffer. InnoDB
Monitor will then fetch it and print */
ibool	lock_deadlock_found = FALSE;
FILE*	lock_latest_err_file;

/* Flags for recursive deadlock search */
#define LOCK_VICTIM_IS_START	1
#define LOCK_VICTIM_IS_OTHER	2

/************************************************************************
Checks if a lock request results in a deadlock. */
static
ibool
lock_deadlock_occurs(
/*=================*/
			/* out: TRUE if a deadlock was detected */
	lock_t*	lock,	/* in: lock the transaction is requesting */
	trx_t*	trx);	/* in: transaction */
/************************************************************************
Looks recursively for a deadlock. */
static
ibool
lock_deadlock_recursive(
/*====================*/
				/* out: TRUE if a deadlock was detected
				or the calculation took too long */
	trx_t*	start,		/* in: recursion starting point */
	trx_t*	trx,		/* in: a transaction waiting for a lock */
	lock_t*	wait_lock,	/* in: the lock trx is waiting to be granted */
	ulint*	cost);		/* in/out: number of calculation steps thus
				far: if this exceeds LOCK_MAX_N_STEPS_...
				we return TRUE */
/*************************************************************************
Gets the nth bit of a record lock. */
UNIV_INLINE
ibool
lock_rec_get_nth_bit(
/*=================*/
			/* out: TRUE if bit set */
	lock_t*	lock,	/* in: record lock */
	ulint	i)	/* in: index of the bit */
{
	ulint	byte_index;
	ulint	bit_index;
	ulint	b;

	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (i >= lock->un_member.rec_lock.n_bits) {

		return(FALSE);
	}

	byte_index = i / 8;
	bit_index = i % 8;

	b = (ulint)*((byte*)lock + sizeof(lock_t) + byte_index);

	return(ut_bit_get_nth(b, bit_index));
}	

/*************************************************************************/

#define lock_mutex_enter_kernel()	mutex_enter(&kernel_mutex)
#define lock_mutex_exit_kernel()	mutex_exit(&kernel_mutex)

/*************************************************************************
Checks that a transaction id is sensible, i.e., not in the future. */

ibool
lock_check_trx_id_sanity(
/*=====================*/
					/* out: TRUE if ok */
	dulint		trx_id,		/* in: trx id */
	rec_t*		rec,		/* in: user record */
	dict_index_t*	index,		/* in: clustered index */
	ibool		has_kernel_mutex)/* in: TRUE if the caller owns the
					kernel mutex */
{
	ibool	is_ok		= TRUE;
	
	if (!has_kernel_mutex) {
		mutex_enter(&kernel_mutex);
	}

	/* A sanity check: the trx_id in rec must be smaller than the global
	trx id counter */

	if (ut_dulint_cmp(trx_id, trx_sys->max_trx_id) >= 0) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: transaction id associated"
			" with record\n",
			stderr);
		rec_print(stderr, rec);
		fputs("InnoDB: in ", stderr);
		dict_index_name_print(stderr, NULL, index);
		fprintf(stderr, "\n"
"InnoDB: is %lu %lu which is higher than the global trx id counter %lu %lu!\n"
"InnoDB: The table is corrupt. You have to do dump + drop + reimport.\n",
			       (ulong) ut_dulint_get_high(trx_id),
			       (ulong) ut_dulint_get_low(trx_id),
			       (ulong) ut_dulint_get_high(trx_sys->max_trx_id),
			       (ulong) ut_dulint_get_low(trx_sys->max_trx_id));

		is_ok = FALSE;
	}
	
	if (!has_kernel_mutex) {
		mutex_exit(&kernel_mutex);
	}

	return(is_ok);
}

/*************************************************************************
Checks that a record is seen in a consistent read. */

ibool
lock_clust_rec_cons_read_sees(
/*==========================*/
				/* out: TRUE if sees, or FALSE if an earlier
				version of the record should be retrieved */
	rec_t*		rec,	/* in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/* in: clustered index */
	read_view_t*	view)	/* in: consistent read view */
{
	dulint	trx_id;

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(page_rec_is_user_rec(rec));

	/* NOTE that we call this function while holding the search
	system latch. To obey the latching order we must NOT reserve the
	kernel mutex here! */

	trx_id = row_get_rec_trx_id(rec, index);
	
	if (read_view_sees_trx_id(view, trx_id)) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Checks that a non-clustered index record is seen in a consistent read. */

ulint
lock_sec_rec_cons_read_sees(
/*========================*/
				/* out: TRUE if certainly sees, or FALSE if an
				earlier version of the clustered index record
				might be needed: NOTE that a non-clustered
				index page contains so little information on
				its modifications that also in the case FALSE,
				the present version of rec may be the right,
				but we must check this from the clustered
				index record */
	rec_t*		rec,	/* in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/* in: non-clustered index */
	read_view_t*	view)	/* in: consistent read view */
{
	dulint	max_trx_id;
	
	UT_NOT_USED(index);
	
	ut_ad(!(index->type & DICT_CLUSTERED));
	ut_ad(page_rec_is_user_rec(rec));

	/* NOTE that we might call this function while holding the search
	system latch. To obey the latching order we must NOT reserve the
	kernel mutex here! */

	if (recv_recovery_is_on()) {

		return(FALSE);
	}

	max_trx_id = page_get_max_trx_id(buf_frame_align(rec));

	if (ut_dulint_cmp(max_trx_id, view->up_limit_id) >= 0) {

		return(FALSE);
	}

	return(TRUE);
}

/*************************************************************************
Creates the lock system at database start. */

void
lock_sys_create(
/*============*/
	ulint	n_cells)	/* in: number of slots in lock hash table */
{
	lock_sys = mem_alloc(sizeof(lock_sys_t));

	lock_sys->rec_hash = hash_create(n_cells);

	/* hash_create_mutexes(lock_sys->rec_hash, 2, SYNC_REC_LOCK); */

	lock_latest_err_file = os_file_create_tmpfile();
	ut_a(lock_latest_err_file);
}

/*************************************************************************
Gets the size of a lock struct. */

ulint
lock_get_size(void)
/*===============*/
			/* out: size in bytes */
{
	return((ulint)sizeof(lock_t));
}

/*************************************************************************
Gets the mode of a lock. */
UNIV_INLINE
ulint
lock_get_mode(
/*==========*/
			/* out: mode */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(lock);

	return(lock->type_mode & LOCK_MODE_MASK);
}

/*************************************************************************
Gets the type of a lock. */
UNIV_INLINE
ulint
lock_get_type(
/*==========*/
			/* out: LOCK_TABLE or LOCK_REC */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(lock);

	return(lock->type_mode & LOCK_TYPE_MASK);
}

/*************************************************************************
Gets the wait flag of a lock. */
UNIV_INLINE
ibool
lock_get_wait(
/*==========*/
			/* out: TRUE if waiting */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(lock);

	if (lock->type_mode & LOCK_WAIT) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Sets the wait flag of a lock and the back pointer in trx to lock. */
UNIV_INLINE
void
lock_set_lock_and_trx_wait(
/*=======================*/
	lock_t*	lock,	/* in: lock */
	trx_t*	trx)	/* in: trx */
{
	ut_ad(lock);
	ut_ad(trx->wait_lock == NULL);
	
	trx->wait_lock = lock;
 	lock->type_mode = lock->type_mode | LOCK_WAIT;
}

/**************************************************************************
The back pointer to a waiting lock request in the transaction is set to NULL
and the wait bit in lock type_mode is reset. */
UNIV_INLINE
void
lock_reset_lock_and_trx_wait(
/*=========================*/
	lock_t*	lock)	/* in: record lock */
{
	ut_ad((lock->trx)->wait_lock == lock);
	ut_ad(lock_get_wait(lock));

	/* Reset the back pointer in trx to this waiting lock request */

	(lock->trx)->wait_lock = NULL;
 	lock->type_mode = lock->type_mode & ~LOCK_WAIT;
}

/*************************************************************************
Gets the gap flag of a record lock. */
UNIV_INLINE
ibool
lock_rec_get_gap(
/*=============*/
			/* out: TRUE if gap flag set */
	lock_t*	lock)	/* in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_GAP) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Gets the LOCK_REC_NOT_GAP flag of a record lock. */
UNIV_INLINE
ibool
lock_rec_get_rec_not_gap(
/*=====================*/
			/* out: TRUE if LOCK_REC_NOT_GAP flag set */
	lock_t*	lock)	/* in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_REC_NOT_GAP) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Gets the waiting insert flag of a record lock. */
UNIV_INLINE
ibool
lock_rec_get_insert_intention(
/*==========================*/
			/* out: TRUE if gap flag set */
	lock_t*	lock)	/* in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_INSERT_INTENTION) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Calculates if lock mode 1 is stronger or equal to lock mode 2. */
UNIV_INLINE
ibool
lock_mode_stronger_or_eq(
/*=====================*/
			/* out: TRUE if mode1 stronger or equal to mode2 */
	ulint	mode1,	/* in: lock mode */
	ulint	mode2)	/* in: lock mode */
{
	ut_ad(mode1 == LOCK_X || mode1 == LOCK_S || mode1 == LOCK_IX
				|| mode1 == LOCK_IS || mode1 == LOCK_AUTO_INC);
	ut_ad(mode2 == LOCK_X || mode2 == LOCK_S || mode2 == LOCK_IX
				|| mode2 == LOCK_IS || mode2 == LOCK_AUTO_INC);
	if (mode1 == LOCK_X) {

		return(TRUE);

	} else if (mode1 == LOCK_AUTO_INC && mode2 == LOCK_AUTO_INC) {

		return(TRUE);

	} else if (mode1 == LOCK_S
				&& (mode2 == LOCK_S || mode2 == LOCK_IS)) {
		return(TRUE);

	} else if (mode1 == LOCK_IS && mode2 == LOCK_IS) {

		return(TRUE);

	} else if (mode1 == LOCK_IX && (mode2 == LOCK_IX
						|| mode2 == LOCK_IS)) {
		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Calculates if lock mode 1 is compatible with lock mode 2. */
UNIV_INLINE
ibool
lock_mode_compatible(
/*=================*/
			/* out: TRUE if mode1 compatible with mode2 */
	ulint	mode1,	/* in: lock mode */
	ulint	mode2)	/* in: lock mode */
{
	ut_ad(mode1 == LOCK_X || mode1 == LOCK_S || mode1 == LOCK_IX
				|| mode1 == LOCK_IS || mode1 == LOCK_AUTO_INC);
	ut_ad(mode2 == LOCK_X || mode2 == LOCK_S || mode2 == LOCK_IX
				|| mode2 == LOCK_IS || mode2 == LOCK_AUTO_INC);

	if (mode1 == LOCK_S && (mode2 == LOCK_IS || mode2 == LOCK_S)) {

		return(TRUE);

	} else if (mode1 == LOCK_X) {

		return(FALSE);

	} else if (mode1 == LOCK_AUTO_INC && (mode2 == LOCK_IS
					  	|| mode2 == LOCK_IX)) {
		return(TRUE);

	} else if (mode1 == LOCK_IS && (mode2 == LOCK_IS
					  	|| mode2 == LOCK_IX
					  	|| mode2 == LOCK_AUTO_INC
					  	|| mode2 == LOCK_S)) {
		return(TRUE);

	} else if (mode1 == LOCK_IX && (mode2 == LOCK_IS
					  	|| mode2 == LOCK_AUTO_INC
					  	|| mode2 == LOCK_IX)) {
		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Checks if a lock request for a new lock has to wait for request lock2. */
UNIV_INLINE
ibool
lock_rec_has_to_wait(
/*=================*/
			/* out: TRUE if new lock has to wait for lock2 to be
			removed */
	trx_t*	trx,	/* in: trx of new lock */
	ulint	type_mode,/* in: precise mode of the new lock to set:
			LOCK_S or LOCK_X, possibly ORed to
			LOCK_GAP or LOCK_REC_NOT_GAP, LOCK_INSERT_INTENTION */
	lock_t*	lock2,	/* in: another record lock; NOTE that it is assumed
			that this has a lock bit set on the same record as
			in the new lock we are setting */
	ibool lock_is_on_supremum)  /* in: TRUE if we are setting the lock
			on the 'supremum' record of an index
			page: we know then that the lock request
			is really for a 'gap' type lock */
{
	ut_ad(trx && lock2);
	ut_ad(lock_get_type(lock2) == LOCK_REC);

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

/*************************************************************************
Checks if a lock request lock1 has to wait for request lock2. */
static
ibool
lock_has_to_wait(
/*=============*/
			/* out: TRUE if lock1 has to wait for lock2 to be
			removed */
	lock_t*	lock1,	/* in: waiting lock */
	lock_t*	lock2)	/* in: another lock; NOTE that it is assumed that this
			has a lock bit set on the same record as in lock1 if
			the locks are record locks */
{
	ut_ad(lock1 && lock2);

	if (lock1->trx != lock2->trx
			&& !lock_mode_compatible(lock_get_mode(lock1),
				     		lock_get_mode(lock2))) {
		if (lock_get_type(lock1) == LOCK_REC) {
			ut_ad(lock_get_type(lock2) == LOCK_REC);

			/* If this lock request is for a supremum record
			then the second bit on the lock bitmap is set */
			
			return(lock_rec_has_to_wait(lock1->trx,
					lock1->type_mode, lock2,
					lock_rec_get_nth_bit(lock1,1)));
		}

		return(TRUE);
	}

	return(FALSE);
}

/*============== RECORD LOCK BASIC FUNCTIONS ============================*/

/*************************************************************************
Gets the number of bits in a record lock bitmap. */
UNIV_INLINE
ulint
lock_rec_get_n_bits(
/*================*/
			/* out: number of bits */
	lock_t*	lock)	/* in: record lock */
{
	return(lock->un_member.rec_lock.n_bits);
}

/**************************************************************************
Sets the nth bit of a record lock to TRUE. */
UNIV_INLINE
void
lock_rec_set_nth_bit(
/*==================*/
	lock_t*	lock,	/* in: record lock */
	ulint	i)	/* in: index of the bit */
{
	ulint	byte_index;
	ulint	bit_index;
	byte*	ptr;
	ulint	b;
	
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);
	ut_ad(i < lock->un_member.rec_lock.n_bits);
	
	byte_index = i / 8;
	bit_index = i % 8;

	ptr = (byte*)lock + sizeof(lock_t) + byte_index;
		
	b = (ulint)*ptr;

	b = ut_bit_set_nth(b, bit_index, TRUE);

	*ptr = (byte)b;
}	

/**************************************************************************
Looks for a set bit in a record lock bitmap. Returns ULINT_UNDEFINED,
if none found. */
static
ulint
lock_rec_find_set_bit(
/*==================*/
			/* out: bit index == heap number of the record, or
			ULINT_UNDEFINED if none found */
	lock_t*	lock)	/* in: record lock with at least one bit set */
{
	ulint	i;

	for (i = 0; i < lock_rec_get_n_bits(lock); i++) {

		if (lock_rec_get_nth_bit(lock, i)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Resets the nth bit of a record lock. */
UNIV_INLINE
void
lock_rec_reset_nth_bit(
/*===================*/
	lock_t*	lock,	/* in: record lock */
	ulint	i)	/* in: index of the bit which must be set to TRUE
			when this function is called */
{
	ulint	byte_index;
	ulint	bit_index;
	byte*	ptr;
	ulint	b;
	
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);
	ut_ad(i < lock->un_member.rec_lock.n_bits);
	
	byte_index = i / 8;
	bit_index = i % 8;

	ptr = (byte*)lock + sizeof(lock_t) + byte_index;
		
	b = (ulint)*ptr;

	b = ut_bit_set_nth(b, bit_index, FALSE);

	*ptr = (byte)b;
}	

/*************************************************************************
Gets the first or next record lock on a page. */
UNIV_INLINE
lock_t*
lock_rec_get_next_on_page(
/*======================*/
			/* out: next lock, NULL if none exists */
	lock_t*	lock)	/* in: a record lock */
{
	ulint	space;
	ulint	page_no;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(lock_get_type(lock) == LOCK_REC);

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

/*************************************************************************
Gets the first record lock on a page, where the page is identified by its
file address. */
UNIV_INLINE
lock_t*
lock_rec_get_first_on_page_addr(
/*============================*/
			/* out: first lock, NULL if none exists */
	ulint	space,	/* in: space */
	ulint	page_no)/* in: page number */
{
	lock_t*	lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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
	
/*************************************************************************
Returns TRUE if there are explicit record locks on a page. */

ibool
lock_rec_expl_exist_on_page(
/*========================*/
			/* out: TRUE if there are explicit record locks on
			the page */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
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

/*************************************************************************
Gets the first record lock on a page, where the page is identified by a
pointer to it. */
UNIV_INLINE
lock_t*
lock_rec_get_first_on_page(
/*=======================*/
			/* out: first lock, NULL if none exists */
	byte*	ptr)	/* in: pointer to somewhere on the page */
{
	ulint	hash;
	lock_t*	lock;
	ulint	space;
	ulint	page_no;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
	hash = buf_frame_get_lock_hash_val(ptr);

	lock = HASH_GET_FIRST(lock_sys->rec_hash, hash);

	while (lock) {
		space = buf_frame_get_space_id(ptr);
		page_no = buf_frame_get_page_no(ptr);

		if ((lock->un_member.rec_lock.space == space) 
	    		&& (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}

		lock = HASH_GET_NEXT(hash, lock);
	}

	return(lock);
}

/*************************************************************************
Gets the next explicit lock request on a record. */
UNIV_INLINE
lock_t*
lock_rec_get_next(
/*==============*/
			/* out: next lock, NULL if none exists */
	rec_t*	rec,	/* in: record on a page */
	lock_t*	lock)	/* in: lock */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(lock_get_type(lock) == LOCK_REC);

	for (;;) {
		lock = lock_rec_get_next_on_page(lock);

		if (lock == NULL) {

			return(NULL);
		}

		if (lock_rec_get_nth_bit(lock, rec_get_heap_no(rec))) {

			return(lock);
		}
	}
}

/*************************************************************************
Gets the first explicit lock request on a record. */
UNIV_INLINE
lock_t*
lock_rec_get_first(
/*===============*/
			/* out: first lock, NULL if none exists */
	rec_t*	rec)	/* in: record on a page */
{
	lock_t*	lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock = lock_rec_get_first_on_page(rec);

	while (lock) {
		if (lock_rec_get_nth_bit(lock, rec_get_heap_no(rec))) {

			break;
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	return(lock);
}

/*************************************************************************
Resets the record lock bitmap to zero. NOTE: does not touch the wait_lock
pointer in the transaction! This function is used in lock object creation
and resetting. */
static
void
lock_rec_bitmap_reset(
/*==================*/
	lock_t*	lock)	/* in: record lock */
{
	byte*	ptr;
	ulint	n_bytes;
	ulint	i;

	ut_ad(lock_get_type(lock) == LOCK_REC);

	/* Reset to zero the bitmap which resides immediately after the lock
	struct */

	ptr = (byte*)lock + sizeof(lock_t);

	n_bytes = lock_rec_get_n_bits(lock) / 8;

	ut_ad((lock_rec_get_n_bits(lock) % 8) == 0);
	
	for (i = 0; i < n_bytes; i++) {

		*ptr = 0;
		ptr++;
	}
}

/*************************************************************************
Copies a record lock to heap. */
static
lock_t*
lock_rec_copy(
/*==========*/
				/* out: copy of lock */
	lock_t*		lock,	/* in: record lock */
	mem_heap_t*	heap)	/* in: memory heap */
{
	lock_t*	dupl_lock;
	ulint	size;

	ut_ad(lock_get_type(lock) == LOCK_REC);

	size = sizeof(lock_t) + lock_rec_get_n_bits(lock) / 8;	

	dupl_lock = mem_heap_alloc(heap, size);

	ut_memcpy(dupl_lock, lock, size);

	return(dupl_lock);
}

/*************************************************************************
Gets the previous record lock set on a record. */
static
lock_t*
lock_rec_get_prev(
/*==============*/
			/* out: previous lock on the same record, NULL if
			none exists */
	lock_t*	in_lock,/* in: record lock */
	ulint	heap_no)/* in: heap number of the record */
{
	lock_t*	lock;
	ulint	space;
	ulint	page_no;
	lock_t*	found_lock 	= NULL;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(lock_get_type(in_lock) == LOCK_REC);

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

/*************************************************************************
Checks if a transaction has the specified table lock, or stronger. */
UNIV_INLINE
lock_t*
lock_table_has(
/*===========*/
				/* out: lock or NULL */
	trx_t*		trx,	/* in: transaction */
	dict_table_t*	table,	/* in: table */
	ulint		mode)	/* in: lock mode */
{
	lock_t*	lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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

/*************************************************************************
Checks if a transaction has a GRANTED explicit lock on rec stronger or equal
to precise_mode. */
UNIV_INLINE
lock_t*
lock_rec_has_expl(
/*==============*/
			/* out: lock or NULL */
	ulint	precise_mode,/* in: LOCK_S or LOCK_X possibly ORed to
			LOCK_GAP or LOCK_REC_NOT_GAP,
			for a supremum record we regard this always a gap
			type request */
	rec_t*	rec,	/* in: record */
	trx_t*	trx)	/* in: transaction */
{
	lock_t*	lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad((precise_mode & LOCK_MODE_MASK) == LOCK_S
	      || (precise_mode & LOCK_MODE_MASK) == LOCK_X);
	ut_ad(!(precise_mode & LOCK_INSERT_INTENTION));
	
	lock = lock_rec_get_first(rec);

	while (lock) {
		if (lock->trx == trx
		    && lock_mode_stronger_or_eq(lock_get_mode(lock),
		    				precise_mode & LOCK_MODE_MASK)
		    && !lock_get_wait(lock)
		    && (!lock_rec_get_rec_not_gap(lock)
		    		|| (precise_mode & LOCK_REC_NOT_GAP)
		    		|| page_rec_is_supremum(rec))
		    && (!lock_rec_get_gap(lock)
				|| (precise_mode & LOCK_GAP)
				|| page_rec_is_supremum(rec))
		    && (!lock_rec_get_insert_intention(lock))) {

		    	return(lock);
		}

		lock = lock_rec_get_next(rec, lock);
	}

	return(NULL);
}
			
/*************************************************************************
Checks if some other transaction has a lock request in the queue. */
static
lock_t*
lock_rec_other_has_expl_req(
/*========================*/
			/* out: lock or NULL */
	ulint	mode,	/* in: LOCK_S or LOCK_X */
	ulint	gap,	/* in: LOCK_GAP if also gap locks are taken
			into account, or 0 if not */
	ulint	wait,	/* in: LOCK_WAIT if also waiting locks are
			taken into account, or 0 if not */
	rec_t*	rec,	/* in: record to look at */	
	trx_t*	trx)	/* in: transaction, or NULL if requests by all
			transactions are taken into account */
{
	lock_t*	lock;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(mode == LOCK_X || mode == LOCK_S);
	ut_ad(gap == 0 || gap == LOCK_GAP);
	ut_ad(wait == 0 || wait == LOCK_WAIT);

	lock = lock_rec_get_first(rec);

	while (lock) {
		if (lock->trx != trx
		    && (gap ||
			!(lock_rec_get_gap(lock) || page_rec_is_supremum(rec)))
		    && (wait || !lock_get_wait(lock))
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)) {

		    	return(lock);
		}

		lock = lock_rec_get_next(rec, lock);
	}

	return(NULL);
}

/*************************************************************************
Checks if some other transaction has a conflicting explicit lock request
in the queue, so that we have to wait. */
static
lock_t*
lock_rec_other_has_conflicting(
/*===========================*/
			/* out: lock or NULL */
	ulint	mode,	/* in: LOCK_S or LOCK_X,
			possibly ORed to LOCK_GAP or LOC_REC_NOT_GAP,
			LOCK_INSERT_INTENTION */
	rec_t*	rec,	/* in: record to look at */	
	trx_t*	trx)	/* in: our transaction */
{
	lock_t*	lock;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock = lock_rec_get_first(rec);

	while (lock) {
		if (lock_rec_has_to_wait(trx, mode, lock,
			page_rec_is_supremum(rec))) {

			return(lock);
		}
		
		lock = lock_rec_get_next(rec, lock);
	}

	return(NULL);
}

/*************************************************************************
Looks for a suitable type record lock struct by the same trx on the same page.
This can be used to save space when a new record lock should be set on a page:
no new struct is needed, if a suitable old is found. */
UNIV_INLINE
lock_t*
lock_rec_find_similar_on_page(
/*==========================*/
				/* out: lock or NULL */
	ulint	type_mode,	/* in: lock type_mode field */
	rec_t*	rec,		/* in: record */
	trx_t*	trx)		/* in: transaction */
{
	lock_t*	lock;
	ulint	heap_no;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	heap_no = rec_get_heap_no(rec);
	
	lock = lock_rec_get_first_on_page(rec);

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

/*************************************************************************
Checks if some transaction has an implicit x-lock on a record in a secondary
index. */

trx_t*
lock_sec_rec_some_has_impl_off_kernel(
/*==================================*/
				/* out: transaction which has the x-lock, or
				NULL */
	rec_t*		rec,	/* in: user record */
	dict_index_t*	index)	/* in: secondary index */
{
	page_t*	page;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!(index->type & DICT_CLUSTERED));
	ut_ad(page_rec_is_user_rec(rec));

	page = buf_frame_align(rec);

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list, or
	database recovery is running. We do not write the changes of a page
	max trx id to the log, and therefore during recovery, this value
	for a page may be incorrect. */

	if (!(ut_dulint_cmp(page_get_max_trx_id(page),
					trx_list_get_min_trx_id()) >= 0)
	   		&& !recv_recovery_is_on()) {

		return(NULL);
	}

	/* Ok, in this case it is possible that some transaction has an
	implicit x-lock. We have to look in the clustered index. */
			
	if (!lock_check_trx_id_sanity(page_get_max_trx_id(page), rec, index,
								     TRUE)) {
		buf_page_print(page);
		
		/* The page is corrupt: try to avoid a crash by returning
		NULL */
		return(NULL);
	}

	return(row_vers_impl_x_locked_off_kernel(rec, index));
}

/*============== RECORD LOCK CREATION AND QUEUE MANAGEMENT =============*/

/*************************************************************************
Creates a new record lock and inserts it to the lock queue. Does NOT check
for deadlocks or lock compatibility! */
static
lock_t*
lock_rec_create(
/*============*/
				/* out: created lock, NULL if out of memory */
	ulint		type_mode,/* in: lock mode and wait flag, type is
				ignored and replaced by LOCK_REC */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: index of record */
	trx_t*		trx)	/* in: transaction */
{
	page_t*	page;
	lock_t*	lock;
	ulint	page_no;
	ulint	heap_no;
	ulint	space;
	ulint	n_bits;
	ulint	n_bytes;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	page = buf_frame_align(rec);
	space = buf_frame_get_space_id(page);
	page_no	= buf_frame_get_page_no(page);
	heap_no = rec_get_heap_no(rec);

	/* If rec is the supremum record, then we reset the gap and
	LOCK_REC_NOT_GAP bits, as all locks on the supremum are
	automatically of the gap type */

	if (rec == page_get_supremum_rec(page)) {
		ut_ad(!(type_mode & LOCK_REC_NOT_GAP));

		type_mode = type_mode & ~(LOCK_GAP | LOCK_REC_NOT_GAP);
	}

	/* Make lock bitmap bigger by a safety margin */
	n_bits = page_header_get_field(page, PAGE_N_HEAP)
						+ LOCK_PAGE_BITMAP_MARGIN;
	n_bytes = 1 + n_bits / 8;

	lock = mem_heap_alloc(trx->lock_heap, sizeof(lock_t) + n_bytes);
	
	if (lock == NULL) {

		return(NULL);
	}

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
	/* Note that we have create a new lock */
	trx->trx_create_lock = TRUE;

	if (type_mode & LOCK_WAIT) {

		lock_set_lock_and_trx_wait(lock, trx);
	}
	
	return(lock);
}

/*************************************************************************
Enqueues a waiting request for a lock which cannot be granted immediately.
Checks for deadlocks. */
static
ulint
lock_rec_enqueue_waiting(
/*=====================*/
				/* out: DB_LOCK_WAIT, DB_DEADLOCK, or
				DB_QUE_THR_SUSPENDED, or DB_SUCCESS;
				DB_SUCCESS means that there was a deadlock,
				but another transaction was chosen as a
				victim, and we got the lock immediately:
				no need to wait then */
	ulint		type_mode,/* in: lock mode this transaction is
				requesting: LOCK_S or LOCK_X, possibly ORed
				with LOCK_GAP or LOCK_REC_NOT_GAP, ORed
				with LOCK_INSERT_INTENTION if this waiting
				lock request is set when performing an
				insert of an index record */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t*	thr)	/* in: query thread */
{
	lock_t*	lock;
	trx_t*	trx;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (que_thr_stop(thr)) {

		ut_error;

		return(DB_QUE_THR_SUSPENDED);
	}
		
	trx = thr_get_trx(thr);

	if (trx->dict_operation) {
		ut_print_timestamp(stderr);
		fputs(
"  InnoDB: Error: a record lock wait happens in a dictionary operation!\n"
"InnoDB: Table name ", stderr);
		ut_print_name(stderr, trx, index->table_name);
		fputs(".\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n",
			stderr);
	}
	
	/* Enqueue the lock request that will wait to be granted */
	lock = lock_rec_create(type_mode | LOCK_WAIT, rec, index, trx);

	/* Check if a deadlock occurs: if yes, remove the lock request and
	return an error code */
	
	if (lock_deadlock_occurs(lock, trx)) {

		lock_reset_lock_and_trx_wait(lock);
		lock_rec_reset_nth_bit(lock, rec_get_heap_no(rec));

		return(DB_DEADLOCK);
	}

	/* If there was a deadlock but we chose another transaction as a
	victim, it is possible that we already have the lock now granted! */

	if (trx->wait_lock == NULL) {

		return(DB_SUCCESS);
	}

	trx->que_state = TRX_QUE_LOCK_WAIT;
	trx->was_chosen_as_deadlock_victim = FALSE;
	trx->wait_started = time(NULL);

	ut_a(que_thr_stop(thr));

	if (lock_print_waits) {
		fprintf(stderr, "Lock wait for trx %lu in index ",
			(ulong) ut_dulint_get_low(trx->id));
		ut_print_name(stderr, trx, index->name);
	}
	
	return(DB_LOCK_WAIT);	
}

/*************************************************************************
Adds a record lock request in the record queue. The request is normally
added as the last in the queue, but if there are no waiting lock requests
on the record, and the request to be added is not a waiting request, we
can reuse a suitable record lock object already existing on the same page,
just setting the appropriate bit in its bitmap. This is a low-level function
which does NOT check for deadlocks or lock compatibility! */
static
lock_t*
lock_rec_add_to_queue(
/*==================*/
				/* out: lock where the bit was set, NULL if out
				of memory */
	ulint		type_mode,/* in: lock mode, wait, gap etc. flags;
				type is ignored and replaced by LOCK_REC */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: index of record */
	trx_t*		trx)	/* in: transaction */
{
	lock_t*	lock;
	lock_t*	similar_lock	= NULL;
	ulint	heap_no;
	page_t*	page;
	ibool	somebody_waits	= FALSE;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad((type_mode & (LOCK_WAIT | LOCK_GAP))
	      || ((type_mode & LOCK_MODE_MASK) != LOCK_S)
	      || !lock_rec_other_has_expl_req(LOCK_X, 0, LOCK_WAIT, rec, trx));
	ut_ad((type_mode & (LOCK_WAIT | LOCK_GAP))
	      || ((type_mode & LOCK_MODE_MASK) != LOCK_X)
	      || !lock_rec_other_has_expl_req(LOCK_S, 0, LOCK_WAIT, rec, trx));

	type_mode = type_mode | LOCK_REC;

	page = buf_frame_align(rec);

	/* If rec is the supremum record, then we can reset the gap bit, as
	all locks on the supremum are automatically of the gap type, and we
	try to avoid unnecessary memory consumption of a new record lock
	struct for a gap type lock */

	if (rec == page_get_supremum_rec(page)) {
		ut_ad(!(type_mode & LOCK_REC_NOT_GAP));

		/* There should never be LOCK_REC_NOT_GAP on a supremum
		record, but let us play safe */
		
		type_mode = type_mode & ~(LOCK_GAP | LOCK_REC_NOT_GAP);
	}

	/* Look for a waiting lock request on the same record or on a gap */

	heap_no = rec_get_heap_no(rec);
	lock = lock_rec_get_first_on_page(rec);

	while (lock != NULL) {
		if (lock_get_wait(lock)
				&& (lock_rec_get_nth_bit(lock, heap_no))) {

			somebody_waits = TRUE;
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	/* Look for a similar record lock on the same page: if one is found
	and there are no waiting lock requests, we can just set the bit */

	similar_lock = lock_rec_find_similar_on_page(type_mode, rec, trx);

	if (similar_lock && !somebody_waits && !(type_mode & LOCK_WAIT)) {

		/* If the nth bit of a record lock is already set then we
		do not set a new lock bit, otherwice we set */

		if (lock_rec_get_nth_bit(similar_lock, heap_no)) {
			trx->trx_create_lock = FALSE;
		} else {
			trx->trx_create_lock = TRUE;
		}

		lock_rec_set_nth_bit(similar_lock, heap_no);

		return(similar_lock);
	}

	return(lock_rec_create(type_mode, rec, index, trx));
}

/*************************************************************************
This is a fast routine for locking a record in the most common cases:
there are no explicit locks on the page, or there is just one lock, owned
by this transaction, and of the right type_mode. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. This function sets a normal next-key lock, or in the case of
a page supremum record, a gap type lock. */
UNIV_INLINE
ibool
lock_rec_lock_fast(
/*===============*/
				/* out: TRUE if locking succeeded */
	ibool		impl,	/* in: if TRUE, no lock is set if no wait
				is necessary: we assume that the caller will
				set an implicit lock */
	ulint		mode,	/* in: lock mode: LOCK_X or LOCK_S possibly
				ORed to either LOCK_GAP or LOCK_REC_NOT_GAP */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t* 	thr)	/* in: query thread */
{
	lock_t*	lock;
	ulint	heap_no;
	trx_t*  trx;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
		|| lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
		|| lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
		|| (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
			|| mode - (LOCK_MODE_MASK & mode) == 0
			|| mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP);
			
	heap_no = rec_get_heap_no(rec);
	
	lock = lock_rec_get_first_on_page(rec);

	trx = thr_get_trx(thr);
	trx->trx_create_lock = FALSE;

	if (lock == NULL) {
		if (!impl) {
			lock_rec_create(mode, rec, index, trx);
		}
		
		return(TRUE);
	}
	
	if (lock_rec_get_next_on_page(lock)) {

		return(FALSE);
	}

	if (lock->trx != trx
				|| lock->type_mode != (mode | LOCK_REC)
				|| lock_rec_get_n_bits(lock) <= heap_no) {
	    	return(FALSE);
	}

	if (!impl) {

		/* If the nth bit of a record lock is already set then we
		do not set a new lock bit, otherwice we set */

		if (lock_rec_get_nth_bit(lock, heap_no)) {
			trx->trx_create_lock = FALSE;
		} else {
			trx->trx_create_lock = TRUE;
		}

		lock_rec_set_nth_bit(lock, heap_no);
	}

	return(TRUE);
}

/*************************************************************************
This is the general, and slower, routine for locking a record. This is a
low-level function which does NOT look at implicit locks! Checks lock
compatibility within explicit locks. This function sets a normal next-key
lock, or in the case of a page supremum record, a gap type lock. */
static
ulint
lock_rec_lock_slow(
/*===============*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				code */
	ibool		impl,	/* in: if TRUE, no lock is set if no wait is
				necessary: we assume that the caller will set
				an implicit lock */
	ulint		mode,	/* in: lock mode: LOCK_X or LOCK_S possibly
				ORed to either LOCK_GAP or LOCK_REC_NOT_GAP */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t* 	thr)	/* in: query thread */
{
	trx_t*	trx;
	ulint	err;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
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
		
	if (lock_rec_has_expl(mode, rec, trx)) {
		/* The trx already has a strong enough lock on rec: do
		nothing */

		err = DB_SUCCESS;
	} else if (lock_rec_other_has_conflicting(mode, rec, trx)) {

		/* If another transaction has a non-gap conflicting request in
		the queue, as this transaction does not have a lock strong
		enough already granted on the record, we have to wait. */
    				
		err = lock_rec_enqueue_waiting(mode, rec, index, thr);
	} else {
		if (!impl) {
			/* Set the requested lock on the record */

			lock_rec_add_to_queue(LOCK_REC | mode, rec, index,
									trx);
		}

		err = DB_SUCCESS;
	}

	return(err);
}

/*************************************************************************
Tries to lock the specified record in the mode requested. If not immediately
possible, enqueues a waiting lock request. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. This function sets a normal next-key lock, or in the case
of a page supremum record, a gap type lock. */
static
ulint
lock_rec_lock(
/*==========*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				code */
	ibool		impl,	/* in: if TRUE, no lock is set if no wait is
				necessary: we assume that the caller will set
				an implicit lock */
	ulint		mode,	/* in: lock mode: LOCK_X or LOCK_S possibly
				ORed to either LOCK_GAP or LOCK_REC_NOT_GAP */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t* 	thr)	/* in: query thread */
{
	ulint	err;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_S
		|| lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	ut_ad((LOCK_MODE_MASK & mode) != LOCK_X
		|| lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad((LOCK_MODE_MASK & mode) == LOCK_S
		|| (LOCK_MODE_MASK & mode) == LOCK_X);
	ut_ad(mode - (LOCK_MODE_MASK & mode) == LOCK_GAP
			|| mode - (LOCK_MODE_MASK & mode) == LOCK_REC_NOT_GAP
			|| mode - (LOCK_MODE_MASK & mode) == 0);
			
	if (lock_rec_lock_fast(impl, mode, rec, index, thr)) {

		/* We try a simplified and faster subroutine for the most
		common cases */

		err = DB_SUCCESS;
	} else {
		err = lock_rec_lock_slow(impl, mode, rec, index, thr);
	}

	return(err);
}

/*************************************************************************
Checks if a waiting record lock request still has to wait in a queue. */
static
ibool
lock_rec_has_to_wait_in_queue(
/*==========================*/
				/* out: TRUE if still has to wait */
	lock_t*	wait_lock)	/* in: waiting record lock */
{
	lock_t*	lock;
	ulint	space;
	ulint	page_no;
	ulint	heap_no;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
 	ut_ad(lock_get_wait(wait_lock));
	ut_ad(lock_get_type(wait_lock) == LOCK_REC);
 	
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

/*****************************************************************
Grants a lock to a waiting lock request and releases the waiting
transaction. */
static
void
lock_grant(
/*=======*/
	lock_t*	lock)	/* in: waiting lock request */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock_reset_lock_and_trx_wait(lock);

        if (lock_get_mode(lock) == LOCK_AUTO_INC) {

                if (lock->trx->auto_inc_lock != NULL) {
                        fprintf(stderr,
                   "InnoDB: Error: trx already had an AUTO-INC lock!\n");
                }

                /* Store pointer to lock to trx so that we know to
                release it at the end of the SQL statement */

                lock->trx->auto_inc_lock = lock;
        } else if (lock_get_type(lock) == LOCK_TABLE_EXP) {
		ut_a(lock_get_mode(lock) == LOCK_S
			|| lock_get_mode(lock) == LOCK_X);
	}

	if (lock_print_waits) {
		fprintf(stderr, "Lock wait for trx %lu ends\n",
		       (ulong) ut_dulint_get_low(lock->trx->id));
	}

	/* If we are resolving a deadlock by choosing another transaction
	as a victim, then our original transaction may not be in the
	TRX_QUE_LOCK_WAIT state, and there is no need to end the lock wait
	for it */
	
	if (lock->trx->que_state == TRX_QUE_LOCK_WAIT) {	
		trx_end_lock_wait(lock->trx);
	}
}

/*****************************************************************
Cancels a waiting record lock request and releases the waiting transaction
that requested it. NOTE: does NOT check if waiting lock requests behind this
one can now be granted! */
static
void
lock_rec_cancel(
/*============*/
	lock_t*	lock)	/* in: waiting record lock request */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(lock_get_type(lock) == LOCK_REC);

	/* Reset the bit (there can be only one set bit) in the lock bitmap */
	lock_rec_reset_nth_bit(lock, lock_rec_find_set_bit(lock));

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_end_lock_wait(lock->trx);
}
	
/*****************************************************************
Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed. */
static
void
lock_rec_dequeue_from_page(
/*=======================*/
	lock_t*	in_lock)/* in: record lock object: all record locks which
			are contained in this lock object are removed;
			transactions waiting behind will get their lock
			requests granted, if they are now qualified to it */
{
	ulint	space;
	ulint	page_no;
	lock_t*	lock;
	trx_t*	trx;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(lock_get_type(in_lock) == LOCK_REC);

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

/*****************************************************************
Removes a record lock request, waiting or granted, from the queue. */
static
void
lock_rec_discard(
/*=============*/
	lock_t*	in_lock)/* in: record lock object: all record locks which
			are contained in this lock object are removed */
{
	ulint	space;
	ulint	page_no;
	trx_t*	trx;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(lock_get_type(in_lock) == LOCK_REC);

	trx = in_lock->trx;
	
	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
				lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, in_lock);
}

/*****************************************************************
Removes record lock objects set on an index page which is discarded. This
function does not move locks, or check for waiting locks, therefore the
lock bitmaps must already be reset when this function is called. */
static
void
lock_rec_free_all_from_discard_page(
/*================================*/
	page_t*	page)	/* in: page to be discarded */
{
	ulint	space;
	ulint	page_no;
	lock_t*	lock;
	lock_t*	next_lock;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
	space = buf_frame_get_space_id(page);
	page_no = buf_frame_get_page_no(page);

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

/*****************************************************************
Resets the lock bits for a single record. Releases transactions waiting for
lock requests here. */

void
lock_rec_reset_and_release_wait(
/*============================*/
	rec_t*	rec)	/* in: record whose locks bits should be reset */
{
	lock_t*	lock;
	ulint	heap_no;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	heap_no = rec_get_heap_no(rec);
	
	lock = lock_rec_get_first(rec);

	while (lock != NULL) {
		if (lock_get_wait(lock)) {
			lock_rec_cancel(lock);
		} else {
			lock_rec_reset_nth_bit(lock, heap_no);
		}

		lock = lock_rec_get_next(rec, lock);
	}
}	

/*****************************************************************
Makes a record to inherit the locks (except LOCK_INSERT_INTENTION type)
of another record as gap type locks, but does not reset the lock bits of
the other record. Also waiting lock requests on rec are inherited as
GRANTED gap locks. */

void
lock_rec_inherit_to_gap(
/*====================*/
	rec_t*	heir,	/* in: record which inherits */
	rec_t*	rec)	/* in: record from which inherited; does NOT reset
			the locks on this record */
{
	lock_t*	lock;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
	lock = lock_rec_get_first(rec);

	while (lock != NULL) {
		if (!lock_rec_get_insert_intention(lock)) {
			
			lock_rec_add_to_queue(LOCK_REC | lock_get_mode(lock)
						| LOCK_GAP,
	 			     		heir, lock->index, lock->trx);
	 	}
	 	
		lock = lock_rec_get_next(rec, lock);
	}
}	

/*****************************************************************
Makes a record to inherit the gap locks (except LOCK_INSERT_INTENTION type)
of another record as gap type locks, but does not reset the lock bits of the
other record. Also waiting lock requests are inherited as GRANTED gap locks. */
static
void
lock_rec_inherit_to_gap_if_gap_lock(
/*================================*/
	rec_t*	heir,	/* in: record which inherits */
	rec_t*	rec)	/* in: record from which inherited; does NOT reset
			the locks on this record */
{
	lock_t*	lock;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
	lock = lock_rec_get_first(rec);

	while (lock != NULL) {
		if (!lock_rec_get_insert_intention(lock)
		    && (page_rec_is_supremum(rec)
			|| !lock_rec_get_rec_not_gap(lock))) {
			
			lock_rec_add_to_queue(LOCK_REC | lock_get_mode(lock)
						| LOCK_GAP,
	 			     		heir, lock->index, lock->trx);
	 	}

		lock = lock_rec_get_next(rec, lock);
	}
}	

/*****************************************************************
Moves the locks of a record to another record and resets the lock bits of
the donating record. */
static
void
lock_rec_move(
/*==========*/
	rec_t*	receiver,	/* in: record which gets locks; this record
				must have no lock requests on it! */
	rec_t*	donator)	/* in: record which gives locks */
{
	lock_t*	lock;
	ulint	heap_no;
	ulint	type_mode;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	heap_no = rec_get_heap_no(donator);
	
	lock = lock_rec_get_first(donator);

	ut_ad(lock_rec_get_first(receiver) == NULL);

	while (lock != NULL) {
		type_mode = lock->type_mode;
	
		lock_rec_reset_nth_bit(lock, heap_no);

		if (lock_get_wait(lock)) {
			lock_reset_lock_and_trx_wait(lock);
		}	

		/* Note that we FIRST reset the bit, and then set the lock:
		the function works also if donator == receiver */

		lock_rec_add_to_queue(type_mode, receiver, lock->index,
								lock->trx);
		lock = lock_rec_get_next(donator, lock);
	}

	ut_ad(lock_rec_get_first(donator) == NULL);
}	

/*****************************************************************
Updates the lock table when we have reorganized a page. NOTE: we copy
also the locks set on the infimum of the page; the infimum may carry
locks if an update of a record is occurring on the page, and its locks
were temporarily stored on the infimum. */

void
lock_move_reorganize_page(
/*======================*/
	page_t*	page,		/* in: old index page, now reorganized */
	page_t*	old_page)	/* in: copy of the old, not reorganized page */
{
	lock_t*		lock;
	lock_t*		old_lock;
	page_cur_t	cur1;
	page_cur_t	cur2;
	ulint		old_heap_no;
	UT_LIST_BASE_NODE_T(lock_t)	old_locks;
	mem_heap_t*	heap		= NULL;
	rec_t*		sup;

	lock_mutex_enter_kernel();

	lock = lock_rec_get_first_on_page(page);

	if (lock == NULL) {
		lock_mutex_exit_kernel();

		return;
	}

	heap = mem_heap_create(256);
	
	/* Copy first all the locks on the page to heap and reset the
	bitmaps in the original locks; chain the copies of the locks
	using the trx_locks field in them. */

	UT_LIST_INIT(old_locks);
	
	while (lock != NULL) {

		/* Make a copy of the lock */
		old_lock = lock_rec_copy(lock, heap);

		UT_LIST_ADD_LAST(trx_locks, old_locks, old_lock);

		/* Reset bitmap of lock */
		lock_rec_bitmap_reset(lock);

		if (lock_get_wait(lock)) {
			lock_reset_lock_and_trx_wait(lock);
		}		

		lock = lock_rec_get_next_on_page(lock);
	}

	sup = page_get_supremum_rec(page);
	
	lock = UT_LIST_GET_FIRST(old_locks);

	while (lock) {
		/* NOTE: we copy also the locks set on the infimum and
		supremum of the page; the infimum may carry locks if an
		update of a record is occurring on the page, and its locks
		were temporarily stored on the infimum */
		
		page_cur_set_before_first(page, &cur1);
		page_cur_set_before_first(old_page, &cur2);

		/* Set locks according to old locks */
		for (;;) {
			ut_ad(0 == ut_memcmp(page_cur_get_rec(&cur1),
						page_cur_get_rec(&cur2),
						rec_get_data_size(
						   page_cur_get_rec(&cur2))));
		
			old_heap_no = rec_get_heap_no(page_cur_get_rec(&cur2));

			if (lock_rec_get_nth_bit(lock, old_heap_no)) {

				/* NOTE that the old lock bitmap could be too
				small for the new heap number! */

				lock_rec_add_to_queue(lock->type_mode,
						page_cur_get_rec(&cur1),
						lock->index, lock->trx);

				/* if ((page_cur_get_rec(&cur1) == sup)
						&& lock_get_wait(lock)) {
					fprintf(stderr,
				"---\n--\n!!!Lock reorg: supr type %lu\n",
					lock->type_mode);
				} */
			}

			if (page_cur_get_rec(&cur1) == sup) {

				break;
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

		/* Remember that we chained old locks on the trx_locks field: */

		lock = UT_LIST_GET_NEXT(trx_locks, lock);
	}

	lock_mutex_exit_kernel();

	mem_heap_free(heap);

/* 	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page))); */
}	

/*****************************************************************
Moves the explicit locks on user records to another page if a record
list end is moved to another page. */

void
lock_move_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page to move to */
	page_t*	page,		/* in: index page */
	rec_t*	rec)		/* in: record on page: this is the
				first record moved */
{
	lock_t*		lock;
	page_cur_t	cur1;
	page_cur_t	cur2;
	ulint		heap_no;
	rec_t*		sup;
	ulint		type_mode;
	
	lock_mutex_enter_kernel();

	/* Note: when we move locks from record to record, waiting locks
	and possible granted gap type locks behind them are enqueued in
	the original order, because new elements are inserted to a hash
	table to the end of the hash chain, and lock_rec_add_to_queue
	does not reuse locks if there are waiters in the queue. */

	sup = page_get_supremum_rec(page);
	
	lock = lock_rec_get_first_on_page(page);

	while (lock != NULL) {
		
		page_cur_position(rec, &cur1);

		if (page_cur_is_before_first(&cur1)) {
			page_cur_move_to_next(&cur1);
		}

		page_cur_set_before_first(new_page, &cur2);
		page_cur_move_to_next(&cur2);
	
		/* Copy lock requests on user records to new page and
		reset the lock bits on the old */

		while (page_cur_get_rec(&cur1) != sup) {

			ut_ad(0 == ut_memcmp(page_cur_get_rec(&cur1),
						page_cur_get_rec(&cur2),
						rec_get_data_size(
						   page_cur_get_rec(&cur2))));
		
			heap_no = rec_get_heap_no(page_cur_get_rec(&cur1));

			if (lock_rec_get_nth_bit(lock, heap_no)) {
				type_mode = lock->type_mode;

				lock_rec_reset_nth_bit(lock, heap_no);

				if (lock_get_wait(lock)) {
					lock_reset_lock_and_trx_wait(lock);
				}	

				lock_rec_add_to_queue(type_mode,
						page_cur_get_rec(&cur2),
						lock->index, lock->trx);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

		lock = lock_rec_get_next_on_page(lock);
	}
	
	lock_mutex_exit_kernel();

/*	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page)));
	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(new_page),
					buf_frame_get_page_no(new_page))); */
}	

/*****************************************************************
Moves the explicit locks on user records to another page if a record
list start is moved to another page. */

void
lock_move_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page to move to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page: this is the
				first record NOT copied */
	rec_t*	old_end)	/* in: old previous-to-last record on
				new_page before the records were copied */
{
	lock_t*		lock;
	page_cur_t	cur1;
	page_cur_t	cur2;
	ulint		heap_no;
	ulint		type_mode;

	ut_a(new_page);

	lock_mutex_enter_kernel();

	lock = lock_rec_get_first_on_page(page);

	while (lock != NULL) {
		
		page_cur_set_before_first(page, &cur1);
		page_cur_move_to_next(&cur1);

		page_cur_position(old_end, &cur2);
		page_cur_move_to_next(&cur2);

		/* Copy lock requests on user records to new page and
		reset the lock bits on the old */

		while (page_cur_get_rec(&cur1) != rec) {

			ut_ad(0 == ut_memcmp(page_cur_get_rec(&cur1),
						page_cur_get_rec(&cur2),
						rec_get_data_size(
						   page_cur_get_rec(&cur2))));
		
			heap_no = rec_get_heap_no(page_cur_get_rec(&cur1));

			if (lock_rec_get_nth_bit(lock, heap_no)) {
				type_mode = lock->type_mode;

				lock_rec_reset_nth_bit(lock, heap_no);

				if (lock_get_wait(lock)) {
					lock_reset_lock_and_trx_wait(lock);
				}			

				lock_rec_add_to_queue(type_mode,
						page_cur_get_rec(&cur2),
						lock->index, lock->trx);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

		lock = lock_rec_get_next_on_page(lock);
	}
	
	lock_mutex_exit_kernel();

/*	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page)));
	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(new_page),
					buf_frame_get_page_no(new_page))); */
}	

/*****************************************************************
Updates the lock table when a page is split to the right. */

void
lock_update_split_right(
/*====================*/
	page_t*	right_page,	/* in: right page */
	page_t*	left_page)	/* in: left page */
{
	lock_mutex_enter_kernel();
	
	/* Move the locks on the supremum of the left page to the supremum
	of the right page */

	lock_rec_move(page_get_supremum_rec(right_page),
					page_get_supremum_rec(left_page));
	
	/* Inherit the locks to the supremum of left page from the successor
	of the infimum on right page */

	lock_rec_inherit_to_gap(page_get_supremum_rec(left_page),
			page_rec_get_next(page_get_infimum_rec(right_page)));

	lock_mutex_exit_kernel();
}	

/*****************************************************************
Updates the lock table when a page is merged to the right. */

void
lock_update_merge_right(
/*====================*/
	rec_t*	orig_succ,	/* in: original successor of infimum
				on the right page before merge */
	page_t*	left_page)	/* in: merged index page which will be
				discarded */
{
	lock_mutex_enter_kernel();
	
	/* Inherit the locks from the supremum of the left page to the
	original successor of infimum on the right page, to which the left
	page was merged */

	lock_rec_inherit_to_gap(orig_succ, page_get_supremum_rec(left_page));

	/* Reset the locks on the supremum of the left page, releasing
	waiting transactions */

	lock_rec_reset_and_release_wait(page_get_supremum_rec(left_page));
	
	lock_rec_free_all_from_discard_page(left_page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when the root page is copied to another in
btr_root_raise_and_insert. Note that we leave lock structs on the
root page, even though they do not make sense on other than leaf
pages: the reason is that in a pessimistic update the infimum record
of the root page will act as a dummy carrier of the locks of the record
to be updated. */

void
lock_update_root_raise(
/*===================*/
	page_t*	new_page,	/* in: index page to which copied */
	page_t*	root)		/* in: root page */
{
	lock_mutex_enter_kernel();
	
	/* Move the locks on the supremum of the root to the supremum
	of new_page */

	lock_rec_move(page_get_supremum_rec(new_page),
						page_get_supremum_rec(root));
	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a page is copied to another and the original page
is removed from the chain of leaf pages, except if page is the root! */

void
lock_update_copy_and_discard(
/*=========================*/
	page_t*	new_page,	/* in: index page to which copied */
	page_t*	page)		/* in: index page; NOT the root! */
{
	lock_mutex_enter_kernel();
	
	/* Move the locks on the supremum of the old page to the supremum
	of new_page */

	lock_rec_move(page_get_supremum_rec(new_page),
						page_get_supremum_rec(page));
	lock_rec_free_all_from_discard_page(page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a page is split to the left. */

void
lock_update_split_left(
/*===================*/
	page_t*	right_page,	/* in: right page */
	page_t*	left_page)	/* in: left page */
{
	lock_mutex_enter_kernel();
	
	/* Inherit the locks to the supremum of the left page from the
	successor of the infimum on the right page */

	lock_rec_inherit_to_gap(page_get_supremum_rec(left_page),
			page_rec_get_next(page_get_infimum_rec(right_page)));

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a page is merged to the left. */

void
lock_update_merge_left(
/*===================*/
	page_t*	left_page,	/* in: left page to which merged */
	rec_t*	orig_pred,	/* in: original predecessor of supremum
				on the left page before merge */
	page_t*	right_page)	/* in: merged index page which will be
				discarded */
{
	lock_mutex_enter_kernel();
	
	if (page_rec_get_next(orig_pred) != page_get_supremum_rec(left_page)) {

		/* Inherit the locks on the supremum of the left page to the
		first record which was moved from the right page */

		lock_rec_inherit_to_gap(page_rec_get_next(orig_pred),
					page_get_supremum_rec(left_page));

		/* Reset the locks on the supremum of the left page,
		releasing waiting transactions */

		lock_rec_reset_and_release_wait(page_get_supremum_rec(
								left_page));
	}

	/* Move the locks from the supremum of right page to the supremum
	of the left page */
	
	lock_rec_move(page_get_supremum_rec(left_page),
					 page_get_supremum_rec(right_page));

	lock_rec_free_all_from_discard_page(right_page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Resets the original locks on heir and replaces them with gap type locks
inherited from rec. */

void
lock_rec_reset_and_inherit_gap_locks(
/*=================================*/
	rec_t*	heir,	/* in: heir record */
	rec_t*	rec)	/* in: record */
{
	mutex_enter(&kernel_mutex);	      				

	lock_rec_reset_and_release_wait(heir);
	
	lock_rec_inherit_to_gap(heir, rec);

	mutex_exit(&kernel_mutex);	      				
}

/*****************************************************************
Updates the lock table when a page is discarded. */

void
lock_update_discard(
/*================*/
	rec_t*	heir,	/* in: record which will inherit the locks */
	page_t*	page)	/* in: index page which will be discarded */
{
	rec_t*	rec;

	lock_mutex_enter_kernel();
	
	if (NULL == lock_rec_get_first_on_page(page)) {
		/* No locks exist on page, nothing to do */

		lock_mutex_exit_kernel();

		return;
	}
	
	/* Inherit all the locks on the page to the record and reset all
	the locks on the page */

	rec = page_get_infimum_rec(page);

	for (;;) {
		lock_rec_inherit_to_gap(heir, rec);

		/* Reset the locks on rec, releasing waiting transactions */

		lock_rec_reset_and_release_wait(rec);

		if (rec == page_get_supremum_rec(page)) {

			break;
		}
		
		rec = page_rec_get_next(rec);
	}

	lock_rec_free_all_from_discard_page(page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a new user record is inserted. */

void
lock_update_insert(
/*===============*/
	rec_t*	rec)	/* in: the inserted record */
{
	lock_mutex_enter_kernel();

	/* Inherit the gap-locking locks for rec, in gap mode, from the next
	record */

	lock_rec_inherit_to_gap_if_gap_lock(rec, page_rec_get_next(rec));

	lock_mutex_exit_kernel();
}	

/*****************************************************************
Updates the lock table when a record is removed. */

void
lock_update_delete(
/*===============*/
	rec_t*	rec)	/* in: the record to be removed */
{
	lock_mutex_enter_kernel();

	/* Let the next record inherit the locks from rec, in gap mode */

	lock_rec_inherit_to_gap(page_rec_get_next(rec), rec);

	/* Reset the lock bits on rec and release waiting transactions */

	lock_rec_reset_and_release_wait(rec);	

	lock_mutex_exit_kernel();
}
 
/*************************************************************************
Stores on the page infimum record the explicit locks of another record.
This function is used to store the lock state of a record when it is
updated and the size of the record changes in the update. The record
is moved in such an update, perhaps to another page. The infimum record
acts as a dummy carrier record, taking care of lock releases while the
actual record is being moved. */

void
lock_rec_store_on_page_infimum(
/*===========================*/
	rec_t*	rec)	/* in: record whose lock state is stored
			on the infimum record of the same page; lock
			bits are reset on the record */
{
	page_t*	page;

	page = buf_frame_align(rec);

	lock_mutex_enter_kernel();
	
	lock_rec_move(page_get_infimum_rec(page), rec);

	lock_mutex_exit_kernel();	
}

/*************************************************************************
Restores the state of explicit lock requests on a single record, where the
state was stored on the infimum of the page. */

void
lock_rec_restore_from_page_infimum(
/*===============================*/
	rec_t*	rec,	/* in: record whose lock state is restored */
	page_t*	page)	/* in: page (rec is not necessarily on this page)
			whose infimum stored the lock state; lock bits are
			reset on the infimum */ 
{
	lock_mutex_enter_kernel();
	
	lock_rec_move(rec, page_get_infimum_rec(page));
	
	lock_mutex_exit_kernel();
}

/*=========== DEADLOCK CHECKING ======================================*/

/************************************************************************
Checks if a lock request results in a deadlock. */
static
ibool
lock_deadlock_occurs(
/*=================*/
			/* out: TRUE if a deadlock was detected and we
			chose trx as a victim; FALSE if no deadlock, or
			there was a deadlock, but we chose other
			transaction(s) as victim(s) */
	lock_t*	lock,	/* in: lock the transaction is requesting */
	trx_t*	trx)	/* in: transaction */
{
	dict_table_t*	table;
	dict_index_t*	index;
	trx_t*		mark_trx;
	ulint		ret;
	ulint		cost	= 0;

	ut_ad(trx && lock);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
retry:
	/* We check that adding this trx to the waits-for graph
	does not produce a cycle. First mark all active transactions
	with 0: */

	mark_trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (mark_trx) {
		mark_trx->deadlock_mark = 0;
		mark_trx = UT_LIST_GET_NEXT(trx_list, mark_trx);
	}

	ret = lock_deadlock_recursive(trx, trx, lock, &cost);

	if (ret == LOCK_VICTIM_IS_OTHER) {
		/* We chose some other trx as a victim: retry if there still
		is a deadlock */

		goto retry;
	}

	if (ret == LOCK_VICTIM_IS_START) {
		if (lock_get_type(lock) & LOCK_TABLE) {
			table = lock->un_member.tab_lock.table;
			index = NULL;
		} else {
			index = lock->index;
			table = index->table;
		}

		lock_deadlock_found = TRUE;

		fputs("*** WE ROLL BACK TRANSACTION (2)\n",
			lock_latest_err_file);

		return(TRUE);
	}
	
	return(FALSE);
}

/************************************************************************
Looks recursively for a deadlock. */
static
ulint
lock_deadlock_recursive(
/*====================*/
				/* out: 0 if no deadlock found,
				LOCK_VICTIM_IS_START if there was a deadlock
				and we chose 'start' as the victim,
				LOCK_VICTIM_IS_OTHER if a deadlock
				was found and we chose some other trx as a
				victim: we must do the search again in this
				last case because there may be another
				deadlock! */
	trx_t*	start,		/* in: recursion starting point */
	trx_t*	trx,		/* in: a transaction waiting for a lock */
	lock_t*	wait_lock,	/* in: the lock trx is waiting to be granted */
	ulint*	cost)		/* in/out: number of calculation steps thus
				far: if this exceeds LOCK_MAX_N_STEPS_...
				we return LOCK_VICTIM_IS_START */
{
	lock_t*	lock;
	ulint	bit_no		= ULINT_UNDEFINED;
	trx_t*	lock_trx;
	ulint	ret;
	
	ut_a(trx && start && wait_lock);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
	if (trx->deadlock_mark == 1) {
		/* We have already exhaustively searched the subtree starting
		from this trx */

		return(0);
	}

	*cost = *cost + 1;

	if (*cost > LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK) {

		return(LOCK_VICTIM_IS_START);
	}

	lock = wait_lock;

	if (lock_get_type(wait_lock) == LOCK_REC) {

		bit_no = lock_rec_find_set_bit(wait_lock);

		ut_a(bit_no != ULINT_UNDEFINED);
	}

	/* Look at the locks ahead of wait_lock in the lock queue */

	for (;;) {
		if (lock_get_type(lock) & LOCK_TABLE) {

			lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
		} else {
			ut_ad(lock_get_type(lock) == LOCK_REC);
			ut_a(bit_no != ULINT_UNDEFINED);

			lock = lock_rec_get_prev(lock, bit_no);
		}

		if (lock == NULL) {
			/* We can mark this subtree as searched */
			trx->deadlock_mark = 1;

			return(FALSE);
		}

		if (lock_has_to_wait(wait_lock, lock)) {

			lock_trx = lock->trx;

			if (lock_trx == start) {
				/* We came back to the recursion starting
				point: a deadlock detected */
				FILE*	ef = lock_latest_err_file;
				
				rewind(ef);
				ut_print_timestamp(ef);

				fputs("\n*** (1) TRANSACTION:\n", ef);

				trx_print(ef, wait_lock->trx);

				fputs(
			"*** (1) WAITING FOR THIS LOCK TO BE GRANTED:\n", ef);
			
				if (lock_get_type(wait_lock) == LOCK_REC) {
					lock_rec_print(ef, wait_lock);
				} else {
					lock_table_print(ef, wait_lock);
				}
			
				fputs("*** (2) TRANSACTION:\n", ef);

				trx_print(ef, lock->trx);

				fputs("*** (2) HOLDS THE LOCK(S):\n", ef);
			
				if (lock_get_type(lock) == LOCK_REC) {
					lock_rec_print(ef, lock);
				} else {
					lock_table_print(ef, lock);
				}
			
				fputs(
			"*** (2) WAITING FOR THIS LOCK TO BE GRANTED:\n", ef);
			
				if (lock_get_type(start->wait_lock)
								== LOCK_REC) {
					lock_rec_print(ef, start->wait_lock);
				} else {
					lock_table_print(ef, start->wait_lock);
				}

				if (lock_print_waits) {
					fputs("Deadlock detected\n", stderr);
				}

				if (ut_dulint_cmp(wait_lock->trx->undo_no,
							start->undo_no) >= 0) {
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
				
				fputs("*** WE ROLL BACK TRANSACTION (1)\n",
					ef);
				
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
	
			if (lock_trx->que_state == TRX_QUE_LOCK_WAIT) {

				/* Another trx ahead has requested lock	in an
				incompatible mode, and is itself waiting for
				a lock */

				ret = lock_deadlock_recursive(start, lock_trx,
						lock_trx->wait_lock, cost);
				if (ret != 0) {

					return(ret);
				}
			}
		}
	}/* end of the 'for (;;)'-loop */
}

/*========================= TABLE LOCKS ==============================*/

/*************************************************************************
Creates a table lock object and adds it as the last in the lock queue
of the table. Does NOT check for deadlocks or lock compatibility. */
UNIV_INLINE
lock_t*
lock_table_create(
/*==============*/
				/* out, own: new lock object, or NULL if
				out of memory */
	dict_table_t*	table,	/* in: database table in dictionary cache */
	ulint		type_mode,/* in: lock mode possibly ORed with
				LOCK_WAIT */
	trx_t*		trx)	/* in: trx */
{
	lock_t*	lock;

	ut_ad(table && trx);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (type_mode == LOCK_AUTO_INC) {
		/* Only one trx can have the lock on the table
		at a time: we may use the memory preallocated
		to the table object */

		lock = table->auto_inc_lock;

		ut_a(trx->auto_inc_lock == NULL);
		trx->auto_inc_lock = lock;
	} else {
		lock = mem_heap_alloc(trx->lock_heap, sizeof(lock_t));
	}

	if (lock == NULL) {

		return(NULL);
	}

	UT_LIST_ADD_LAST(trx_locks, trx->trx_locks, lock);

	lock->type_mode = type_mode | LOCK_TABLE;
	lock->trx = trx;

	if (lock_get_type(lock) == LOCK_TABLE_EXP) {
		lock->trx->n_lock_table_exp++;
	}

	lock->un_member.tab_lock.table = table;

	UT_LIST_ADD_LAST(un_member.tab_lock.locks, table->locks, lock);

	if (type_mode & LOCK_WAIT) {

		lock_set_lock_and_trx_wait(lock, trx);
	}

	return(lock);
}

/*****************************************************************
Removes a table lock request from the queue and the trx list of locks;
this is a low-level function which does NOT check if waiting requests
can now be granted. */
UNIV_INLINE
void
lock_table_remove_low(
/*==================*/
	lock_t*	lock)	/* in: table lock */
{
	dict_table_t*	table;
	trx_t*		trx;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	table = lock->un_member.tab_lock.table;
	trx = lock->trx;

	if (lock == trx->auto_inc_lock) {
		trx->auto_inc_lock = NULL;
	}

	if (lock_get_type(lock) == LOCK_TABLE_EXP) {
		lock->trx->n_lock_table_exp--;
	}

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, lock);
	UT_LIST_REMOVE(un_member.tab_lock.locks, table->locks, lock);
}	

/*************************************************************************
Enqueues a waiting request for a table lock which cannot be granted
immediately. Checks for deadlocks. */
static
ulint
lock_table_enqueue_waiting(
/*=======================*/
				/* out: DB_LOCK_WAIT, DB_DEADLOCK, or
				DB_QUE_THR_SUSPENDED, or DB_SUCCESS;
				DB_SUCCESS means that there was a deadlock,
				but another transaction was chosen as a
				victim, and we got the lock immediately:
				no need to wait then */
	ulint		mode,	/* in: lock mode this transaction is
				requesting */
	dict_table_t*	table,	/* in: table */
	que_thr_t*	thr)	/* in: query thread */
{
	lock_t*	lock;
	trx_t*	trx;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (que_thr_stop(thr)) {
		ut_error;

		return(DB_QUE_THR_SUSPENDED);
	}

	trx = thr_get_trx(thr);

	if (trx->dict_operation) {
		ut_print_timestamp(stderr);
		fputs(
"  InnoDB: Error: a table lock wait happens in a dictionary operation!\n"
"InnoDB: Table name ", stderr);
		ut_print_name(stderr, trx, table->name);
		fputs(".\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n",
			stderr);
	}
	
	/* Enqueue the lock request that will wait to be granted */

	lock = lock_table_create(table, mode | LOCK_WAIT, trx);

	/* Check if a deadlock occurs: if yes, remove the lock request and
	return an error code */

	if (lock_deadlock_occurs(lock, trx)) {

		lock_reset_lock_and_trx_wait(lock);
		lock_table_remove_low(lock);

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

/*************************************************************************
Checks if other transactions have an incompatible mode lock request in
the lock queue. */
UNIV_INLINE
ibool
lock_table_other_has_incompatible(
/*==============================*/
	trx_t*		trx,	/* in: transaction, or NULL if all
				transactions should be included */
	ulint		wait,	/* in: LOCK_WAIT if also waiting locks are
				taken into account, or 0 if not */
	dict_table_t*	table,	/* in: table */
	ulint		mode)	/* in: lock mode */
{
	lock_t*	lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock = UT_LIST_GET_LAST(table->locks);

	while (lock != NULL) {

		if ((lock->trx != trx) 
		    && (!lock_mode_compatible(lock_get_mode(lock), mode))
		    && (wait || !(lock_get_wait(lock)))) {

		    	return(TRUE);
		}

		lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
	}

	return(FALSE);
}

/*************************************************************************
Locks the specified database table in the mode given. If the lock cannot
be granted immediately, the query thread is put to wait. */

ulint
lock_table(
/*=======*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing;
				if LOCK_TABLE_EXP bits are set,
				creates an explicit table lock */
	dict_table_t*	table,	/* in: database table in dictionary cache */
	ulint		mode,	/* in: lock mode */
	que_thr_t*	thr)	/* in: query thread */
{
	trx_t*	trx;
	ulint	err;
	
	ut_ad(table && thr);

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_a(flags == 0 || flags == LOCK_TABLE_EXP);

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

		err = lock_table_enqueue_waiting(mode, table, thr);
			
		lock_mutex_exit_kernel();

		return(err);
	}

	lock_table_create(table, mode | flags, trx);

	ut_a(!flags || mode == LOCK_S || mode == LOCK_X);

	lock_mutex_exit_kernel();

	return(DB_SUCCESS);
}

/*************************************************************************
Checks if there are any locks set on the table. */

ibool
lock_is_on_table(
/*=============*/
				/* out: TRUE if there are lock(s) */
	dict_table_t*	table)	/* in: database table in dictionary cache */
{
	ibool	ret;

	ut_ad(table);

	lock_mutex_enter_kernel();

	if (UT_LIST_GET_LAST(table->locks)) {
		ret = TRUE;
	} else {
		ret = FALSE;
	}

	lock_mutex_exit_kernel();

	return(ret);
}

/*************************************************************************
Checks if a waiting table lock request still has to wait in a queue. */
static
ibool
lock_table_has_to_wait_in_queue(
/*============================*/
				/* out: TRUE if still has to wait */
	lock_t*	wait_lock)	/* in: waiting table lock */
{
	dict_table_t*	table;
	lock_t*		lock;

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

/*****************************************************************
Removes a table lock request, waiting or granted, from the queue and grants
locks to other transactions in the queue, if they now are entitled to a
lock. */
static
void
lock_table_dequeue(
/*===============*/
	lock_t*	in_lock)/* in: table lock object; transactions waiting
			behind will get their lock requests granted, if
			they are now qualified to it */
{
	lock_t*	lock;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(lock_get_type(in_lock) == LOCK_TABLE ||
		lock_get_type(in_lock) == LOCK_TABLE_EXP);

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

/*************************************************************************
Releases a table lock.
Releases possible other transactions waiting for this lock. */

void
lock_table_unlock(
/*==============*/
	lock_t*	lock)	/* in: lock */
{
	mutex_enter(&kernel_mutex);

	lock_table_dequeue(lock);

	mutex_exit(&kernel_mutex);
}

/*************************************************************************
Releases an auto-inc lock a transaction possibly has on a table.
Releases possible other transactions waiting for this lock. */

void
lock_table_unlock_auto_inc(
/*=======================*/
	trx_t*	trx)	/* in: transaction */
{
	if (trx->auto_inc_lock) {
		mutex_enter(&kernel_mutex);

		lock_table_dequeue(trx->auto_inc_lock);

		mutex_exit(&kernel_mutex);
	}
}

/*************************************************************************
Releases transaction locks, and releases possible other transactions waiting
because of these locks. */

void
lock_release_off_kernel(
/*====================*/
	trx_t*	trx)	/* in: transaction */
{
	dict_table_t*	table;
	ulint		count;
	lock_t*		lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock = UT_LIST_GET_LAST(trx->trx_locks);
	
	count = 0;

	while (lock != NULL) {

		count++;

		if (lock_get_type(lock) == LOCK_REC) {
			
			lock_rec_dequeue_from_page(lock);
		} else {
			ut_ad(lock_get_type(lock) & LOCK_TABLE);

			if (lock_get_mode(lock) != LOCK_IS
			    && 0 != ut_dulint_cmp(trx->undo_no,
						  ut_dulint_zero)) {

				/* The trx may have modified the table.
				We block the use of the MySQL query cache
				for all currently active transactions. */

				table = lock->un_member.tab_lock.table;
			
				table->query_cache_inv_trx_id =
							trx_sys->max_trx_id;
			}

			lock_table_dequeue(lock);
			if (lock_get_type(lock) == LOCK_TABLE_EXP) {
				ut_a(lock_get_mode(lock) == LOCK_S
					|| lock_get_mode(lock) == LOCK_X);
			}
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

	mem_heap_empty(trx->lock_heap);

	ut_a(trx->auto_inc_lock == NULL);
	ut_a(trx->n_lock_table_exp == 0);
}

/*************************************************************************
Releases table locks explicitly requested with LOCK TABLES (indicated by
lock type LOCK_TABLE_EXP), and releases possible other transactions waiting
because of these locks. */

void
lock_release_tables_off_kernel(
/*===========================*/
	trx_t*	trx)	/* in: transaction */
{
	dict_table_t*	table;
	ulint		count;
	lock_t*		lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock = UT_LIST_GET_LAST(trx->trx_locks);

	count = 0;

	while (lock != NULL) {

		count++;

		if (lock_get_type(lock) == LOCK_TABLE_EXP) {
			ut_a(lock_get_mode(lock) == LOCK_S
				|| lock_get_mode(lock) == LOCK_X);
			if (trx->insert_undo || trx->update_undo) {

				/* The trx may have modified the table.
				We block the use of the MySQL query
				cache for all currently active
				transactions. */

				table = lock->un_member.tab_lock.table;

				table->query_cache_inv_trx_id =
							trx_sys->max_trx_id;
			}

			lock_table_dequeue(lock);

			lock = UT_LIST_GET_LAST(trx->trx_locks);
			continue;
		}

		if (count == LOCK_RELEASE_KERNEL_INTERVAL) {
			/* Release the kernel mutex for a while, so that we
			do not monopolize it */

			lock_mutex_exit_kernel();

			lock_mutex_enter_kernel();

			count = 0;
		}

		lock = UT_LIST_GET_PREV(trx_locks, lock);
	}

	ut_a(trx->n_lock_table_exp == 0);
}

/*************************************************************************
Cancels a waiting lock request and releases possible other transactions
waiting behind it. */

void
lock_cancel_waiting_and_release(
/*============================*/
	lock_t*	lock)	/* in: waiting lock request */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (lock_get_type(lock) == LOCK_REC) {
			
		lock_rec_dequeue_from_page(lock);
	} else {
		ut_ad(lock_get_type(lock) & LOCK_TABLE);

		lock_table_dequeue(lock);
	}

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_end_lock_wait(lock->trx);
}

/*************************************************************************
Resets all record and table locks of a transaction on a table to be dropped.
No lock is allowed to be a wait lock. */
static
void
lock_reset_all_on_table_for_trx(
/*============================*/
	dict_table_t*	table,	/* in: table to be dropped */
	trx_t*		trx)	/* in: a transaction */
{
	lock_t*	lock;
	lock_t*	prev_lock;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	lock = UT_LIST_GET_LAST(trx->trx_locks);
	
	while (lock != NULL) {
		prev_lock = UT_LIST_GET_PREV(trx_locks, lock);
		
		if (lock_get_type(lock) == LOCK_REC
				&& lock->index->table == table) {
			ut_a(!lock_get_wait(lock));
			
			lock_rec_discard(lock);
		} else if (lock_get_type(lock) & LOCK_TABLE
				&& lock->un_member.tab_lock.table == table) {

			ut_a(!lock_get_wait(lock));
			
			lock_table_remove_low(lock);
		}

		lock = prev_lock;
	}
}

/*************************************************************************
Resets all locks, both table and record locks, on a table to be dropped.
No lock is allowed to be a wait lock. */

void
lock_reset_all_on_table(
/*====================*/
	dict_table_t*	table)	/* in: table to be dropped */
{
	lock_t*	lock;

	mutex_enter(&kernel_mutex);

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock) {	
		ut_a(!lock_get_wait(lock));

		lock_reset_all_on_table_for_trx(table, lock->trx);

		lock = UT_LIST_GET_FIRST(table->locks);
	}

	mutex_exit(&kernel_mutex);
}

/*===================== VALIDATION AND DEBUGGING  ====================*/

/*************************************************************************
Prints info of a table lock. */

void
lock_table_print(
/*=============*/
	FILE*	file,	/* in: file where to print */
	lock_t*	lock)	/* in: table type lock */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(lock_get_type(lock) == LOCK_TABLE ||
		lock_get_type(lock) == LOCK_TABLE_EXP);

	if (lock_get_type(lock) == LOCK_TABLE_EXP) {
		fputs("EXPLICIT ", file);
	}
	fputs("TABLE LOCK table ", file);
	ut_print_name(file, lock->trx, lock->un_member.tab_lock.table->name);
	fprintf(file, " trx id %lu %lu",
		(ulong) (lock->trx)->id.high, (ulong) (lock->trx)->id.low);

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
		fprintf(file, " unknown lock mode %lu", (ulong) lock_get_mode(lock));
	}

	if (lock_get_wait(lock)) {
		fputs(" waiting", file);
	}

	putc('\n', file);
}						
				
/*************************************************************************
Prints info of a record lock. */

void
lock_rec_print(
/*===========*/
	FILE*	file,	/* in: file where to print */
	lock_t*	lock)	/* in: record type lock */
{
	page_t*	page;
	ulint	space;
	ulint	page_no;
	ulint	i;
	mtr_t	mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(lock_get_type(lock) == LOCK_REC);

	space = lock->un_member.rec_lock.space;
 	page_no = lock->un_member.rec_lock.page_no;

	fprintf(file, "RECORD LOCKS space id %lu page no %lu n bits %lu ",
		       (ulong) space, (ulong) page_no,
		       (ulong) lock_rec_get_n_bits(lock));
	dict_index_name_print(file, lock->trx, lock->index);
	fprintf(file, " trx id %lu %lu",
		       (ulong) (lock->trx)->id.high,
		       (ulong) (lock->trx)->id.low);

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

	/* If the page is not in the buffer pool, we cannot load it
	because we have the kernel mutex and ibuf operations would
	break the latching order */
	
	page = buf_page_get_gen(space, page_no, RW_NO_LATCH,
					NULL, BUF_GET_IF_IN_POOL,
					__FILE__, __LINE__, &mtr);
	if (page) {
		page = buf_page_get_nowait(space, page_no, RW_S_LATCH, &mtr);

		if (!page) {
			/* Let us try to get an X-latch. If the current thread
			is holding an X-latch on the page, we cannot get an
			S-latch. */
			
			page = buf_page_get_nowait(space, page_no, RW_X_LATCH,
									&mtr);
		}
	}
				
	if (page) {
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
	}

	for (i = 0; i < lock_rec_get_n_bits(lock); i++) {

		if (lock_rec_get_nth_bit(lock, i)) {

			fprintf(file, "Record lock, heap no %lu ", (ulong) i);

			if (page) {
				rec_print(file,
				      page_find_rec_with_heap_no(page, i));
			}

			putc('\n', file);
		}
	}

	mtr_commit(&mtr);
}						
				
/*************************************************************************
Calculates the number of record lock structs in the record lock hash table. */
static
ulint
lock_get_n_rec_locks(void)
/*======================*/
{
	lock_t*	lock;
	ulint	n_locks	= 0;
	ulint	i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {

		lock = HASH_GET_FIRST(lock_sys->rec_hash, i);

		while (lock) {
			n_locks++;

			lock = HASH_GET_NEXT(hash, lock);
		}
	}

	return(n_locks);
}
	
/*************************************************************************
Prints info of locks for all transactions. */

void
lock_print_info(
/*============*/
	FILE*	file)	/* in: file where to print */
{
	lock_t*	lock;
	trx_t*	trx;
	ulint	space;
	ulint	page_no;
	page_t*	page;
	ibool	load_page_first = TRUE;
	ulint	nth_trx		= 0;
	ulint	nth_lock	= 0;
	ulint	i;
	mtr_t	mtr;

	/* We must protect the MySQL thd->query field with a MySQL mutex, and
	because the MySQL mutex must be reserved before the kernel_mutex of
	InnoDB, we call innobase_mysql_prepare_print_arbitrary_thd() here. */

	innobase_mysql_prepare_print_arbitrary_thd();
	lock_mutex_enter_kernel();

	if (lock_deadlock_found) {
		fputs(
"------------------------\n" 
"LATEST DETECTED DEADLOCK\n"
"------------------------\n", file);

		ut_copy_file(file, lock_latest_err_file);
	}

	fputs(
"------------\n" 
"TRANSACTIONS\n"
"------------\n", file);

	fprintf(file, "Trx id counter %lu %lu\n",
		       (ulong) ut_dulint_get_high(trx_sys->max_trx_id),
		       (ulong) ut_dulint_get_low(trx_sys->max_trx_id));

	fprintf(file,
	"Purge done for trx's n:o < %lu %lu undo n:o < %lu %lu\n",
		(ulong) ut_dulint_get_high(purge_sys->purge_trx_no),
		(ulong) ut_dulint_get_low(purge_sys->purge_trx_no),
		(ulong) ut_dulint_get_high(purge_sys->purge_undo_no),
		(ulong) ut_dulint_get_low(purge_sys->purge_undo_no));

	fprintf(file,
		"Total number of lock structs in row lock hash table %lu\n",
					 (ulong) lock_get_n_rec_locks());

	fprintf(file, "LIST OF TRANSACTIONS FOR EACH SESSION:\n");

	/* First print info on non-active transactions */

	trx = UT_LIST_GET_FIRST(trx_sys->mysql_trx_list);

	while (trx) {
		if (trx->conc_state == TRX_NOT_STARTED) {
			fputs("---", file);
			trx_print(file, trx);
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
		innobase_mysql_end_print_arbitrary_thd();

		ut_ad(lock_validate());

		return;
	}

	if (nth_lock == 0) {
		fputs("---", file);
		trx_print(file, trx);
		
	        if (trx->read_view) {
			fprintf(file,
       "Trx read view will not see trx with id >= %lu %lu, sees < %lu %lu\n",
		      (ulong) ut_dulint_get_high(trx->read_view->low_limit_id),
       		      (ulong) ut_dulint_get_low(trx->read_view->low_limit_id),
       		      (ulong) ut_dulint_get_high(trx->read_view->up_limit_id),
       		      (ulong) ut_dulint_get_low(trx->read_view->up_limit_id));
	        }

		if (trx->que_state == TRX_QUE_LOCK_WAIT) {
			fprintf(file,
 "------- TRX HAS BEEN WAITING %lu SEC FOR THIS LOCK TO BE GRANTED:\n",
		   (ulong)difftime(time(NULL), trx->wait_started));

			if (lock_get_type(trx->wait_lock) == LOCK_REC) {
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

	if (lock_get_type(lock) == LOCK_REC) {
		space = lock->un_member.rec_lock.space;
 		page_no = lock->un_member.rec_lock.page_no;

 		if (load_page_first) {
			lock_mutex_exit_kernel();
			innobase_mysql_end_print_arbitrary_thd();

			mtr_start(&mtr);
			
			page = buf_page_get_with_no_latch(space, page_no, &mtr);

			mtr_commit(&mtr);

			load_page_first = FALSE;

			innobase_mysql_prepare_print_arbitrary_thd();
			lock_mutex_enter_kernel();

			goto loop;
		}
		
		lock_rec_print(file, lock);
	} else {
		ut_ad(lock_get_type(lock) & LOCK_TABLE);
	
		lock_table_print(file, lock);
	}

	load_page_first = TRUE;

	nth_lock++;

	if (nth_lock >= 10) {
		fputs(
		"10 LOCKS PRINTED FOR THIS TRX: SUPPRESSING FURTHER PRINTS\n",
			file);
	
		nth_trx++;
		nth_lock = 0;

		goto loop;
	}

	goto loop;
}

/*************************************************************************
Validates the lock queue on a table. */

ibool
lock_table_queue_validate(
/*======================*/
				/* out: TRUE if ok */
	dict_table_t*	table)	/* in: table */
{
	lock_t*	lock;
	ibool	is_waiting;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	is_waiting = FALSE;

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock) {
		ut_a(((lock->trx)->conc_state == TRX_ACTIVE)
		     || ((lock->trx)->conc_state == TRX_COMMITTED_IN_MEMORY));
	
		if (!lock_get_wait(lock)) {

			ut_a(!is_waiting);
		
			ut_a(!lock_table_other_has_incompatible(lock->trx, 0,
						table, lock_get_mode(lock)));
		} else {
			is_waiting = TRUE;

			ut_a(lock_table_has_to_wait_in_queue(lock));
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
	}

	return(TRUE);
}

/*************************************************************************
Validates the lock queue on a single record. */

ibool
lock_rec_queue_validate(
/*====================*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: record to look at */
	dict_index_t*	index)	/* in: index, or NULL if not known */
{
	trx_t*	impl_trx;	
	lock_t*	lock;
	
	ut_a(rec);

	lock_mutex_enter_kernel();

	if (page_rec_is_supremum(rec) || page_rec_is_infimum(rec)) {

		lock = lock_rec_get_first(rec);

		while (lock) {
			ut_a(lock->trx->conc_state == TRX_ACTIVE
		     	     || lock->trx->conc_state
						== TRX_COMMITTED_IN_MEMORY);
	
			ut_a(trx_in_trx_list(lock->trx));
			
			if (lock_get_wait(lock)) {
				ut_a(lock_rec_has_to_wait_in_queue(lock));
			}

			if (index) {
				ut_a(lock->index == index);
			}

			lock = lock_rec_get_next(rec, lock);
		}

		lock_mutex_exit_kernel();

	    	return(TRUE);
	}

	if (index && (index->type & DICT_CLUSTERED)) {
	
		impl_trx = lock_clust_rec_some_has_impl(rec, index);

		if (impl_trx && lock_rec_other_has_expl_req(LOCK_S, 0,
				LOCK_WAIT, rec, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, rec,
								impl_trx));
		}
	}

	if (index && !(index->type & DICT_CLUSTERED)) {
		
		/* The kernel mutex may get released temporarily in the
		next function call: we have to release lock table mutex
		to obey the latching order */
		
		impl_trx = lock_sec_rec_some_has_impl_off_kernel(rec, index);

		if (impl_trx && lock_rec_other_has_expl_req(LOCK_S, 0,
				LOCK_WAIT, rec, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, rec,
								impl_trx));
		}
	}

	lock = lock_rec_get_first(rec);

	while (lock) {
		ut_a(lock->trx->conc_state == TRX_ACTIVE
		     || lock->trx->conc_state == TRX_COMMITTED_IN_MEMORY);
		ut_a(trx_in_trx_list(lock->trx));
	
		if (index) {
			ut_a(lock->index == index);
		}

		if (!lock_rec_get_gap(lock) && !lock_get_wait(lock)) {
		
			if (lock_get_mode(lock) == LOCK_S) {
				ut_a(!lock_rec_other_has_expl_req(LOCK_X,
						0, 0, rec, lock->trx));
			} else {
				ut_a(!lock_rec_other_has_expl_req(LOCK_S,
						0, 0, rec, lock->trx));
			}

		} else if (lock_get_wait(lock) && !lock_rec_get_gap(lock)) {

			ut_a(lock_rec_has_to_wait_in_queue(lock));
		}

		lock = lock_rec_get_next(rec, lock);
	}

	lock_mutex_exit_kernel();

	return(TRUE);
}

/*************************************************************************
Validates the record lock queues on a page. */

ibool
lock_rec_validate_page(
/*===================*/
			/* out: TRUE if ok */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	dict_index_t*	index;
	page_t*	page;
	lock_t*	lock;
	rec_t*	rec;
	ulint	nth_lock	= 0;
	ulint	nth_bit		= 0;
	ulint	i;
	mtr_t	mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	mtr_start(&mtr);
	
	page = buf_page_get(space, page_no, RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

	lock_mutex_enter_kernel();
loop:	
	lock = lock_rec_get_first_on_page_addr(space, page_no);

	if (!lock) {
		goto function_exit;
	}

	for (i = 0; i < nth_lock; i++) {

		lock = lock_rec_get_next_on_page(lock);

		if (!lock) {
			goto function_exit;
		}
	}

	ut_a(trx_in_trx_list(lock->trx));
	ut_a(lock->trx->conc_state == TRX_ACTIVE
		     || lock->trx->conc_state == TRX_COMMITTED_IN_MEMORY);
	
	for (i = nth_bit; i < lock_rec_get_n_bits(lock); i++) {

		if (i == 1 || lock_rec_get_nth_bit(lock, i)) {

			index = lock->index;
			rec = page_find_rec_with_heap_no(page, i);

			fprintf(stderr,
				"Validating %lu %lu\n", (ulong) space, (ulong) page_no);

			lock_mutex_exit_kernel();

			lock_rec_queue_validate(rec, index);

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

	mtr_commit(&mtr);

	return(TRUE);
}						
				
/*************************************************************************
Validates the lock system. */

ibool
lock_validate(void)
/*===============*/
			/* out: TRUE if ok */
{
	lock_t*	lock;
	trx_t*	trx;
	dulint	limit;
	ulint	space;
	ulint	page_no;
	ulint	i;

	lock_mutex_enter_kernel();
	
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		lock = UT_LIST_GET_FIRST(trx->trx_locks);
		
		while (lock) {
			if (lock_get_type(lock) & LOCK_TABLE) {
	
				lock_table_queue_validate(
					lock->un_member.tab_lock.table);
			}
	
			lock = UT_LIST_GET_NEXT(trx_locks, lock);
		}
	
		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {

		limit = ut_dulint_zero;

		for (;;) {
			lock = HASH_GET_FIRST(lock_sys->rec_hash, i);

			while (lock) {
				ut_a(trx_in_trx_list(lock->trx));

				space = lock->un_member.rec_lock.space;
				page_no = lock->un_member.rec_lock.page_no;
		
				if (ut_dulint_cmp(
					ut_dulint_create(space, page_no),
							limit) >= 0) {
					break;
				}

				lock = HASH_GET_NEXT(hash, lock);
			}

			if (!lock) {

				break;
			}
			
			lock_mutex_exit_kernel();

			lock_rec_validate_page(space, page_no);

			lock_mutex_enter_kernel();

			limit = ut_dulint_create(space, page_no + 1);
		}
	}

	lock_mutex_exit_kernel();

	return(TRUE);
}

/*============ RECORD LOCK CHECKS FOR ROW OPERATIONS ====================*/

/*************************************************************************
Checks if locks of other transactions prevent an immediate insert of
a record. If they do, first tests if the query thread should anyway
be suspended for some reason; if not, then puts the transaction and
the query thread to the lock wait state and inserts a waiting request
for a gap x-lock to the lock queue. */

ulint
lock_rec_insert_check_and_lock(
/*===========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record after which to insert */
	dict_index_t*	index,	/* in: index */
	que_thr_t*	thr,	/* in: query thread */
	ibool*		inherit)/* out: set to TRUE if the new inserted
				record maybe should inherit LOCK_GAP type
				locks from the successor record */
{
	rec_t*	next_rec;
	trx_t*	trx;
	lock_t*	lock;
	ulint	err;

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_ad(rec);

	trx = thr_get_trx(thr);
	next_rec = page_rec_get_next(rec);

	*inherit = FALSE;

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	lock = lock_rec_get_first(next_rec);

	if (lock == NULL) {
		/* We optimize CPU time usage in the simplest case */

		lock_mutex_exit_kernel();

		if (!(index->type & DICT_CLUSTERED)) {

			/* Update the page max trx id field */
			page_update_max_trx_id(buf_frame_align(rec),
							thr_get_trx(thr)->id);
		}
		
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

	if (lock_rec_other_has_conflicting(LOCK_X | LOCK_GAP
				| LOCK_INSERT_INTENTION, next_rec, trx)) {

		/* Note that we may get DB_SUCCESS also here! */
		err = lock_rec_enqueue_waiting(LOCK_X | LOCK_GAP
						| LOCK_INSERT_INTENTION,
						next_rec, index, thr);
	} else {
		err = DB_SUCCESS;
	}

	lock_mutex_exit_kernel();

	if (!(index->type & DICT_CLUSTERED) && (err == DB_SUCCESS)) {

		/* Update the page max trx id field */
		page_update_max_trx_id(buf_frame_align(rec),
							thr_get_trx(thr)->id);
	}
	
	ut_ad(lock_rec_queue_validate(next_rec, index));

	return(err);
}

/*************************************************************************
If a transaction has an implicit x-lock on a record, but no explicit x-lock
set on the record, sets one for it. NOTE that in the case of a secondary
index, the kernel mutex may get temporarily released. */
static
void
lock_rec_convert_impl_to_expl(
/*==========================*/
	rec_t*		rec,	/* in: user record on page */
	dict_index_t*	index)	/* in: index of record */
{
	trx_t*	impl_trx;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(page_rec_is_user_rec(rec));

	if (index->type & DICT_CLUSTERED) {
		impl_trx = lock_clust_rec_some_has_impl(rec, index);
	} else {
		impl_trx = lock_sec_rec_some_has_impl_off_kernel(rec, index);
	}

	if (impl_trx) {
		/* If the transaction has no explicit x-lock set on the
		record, set one for it */

		if (!lock_rec_has_expl(LOCK_X | LOCK_REC_NOT_GAP, rec,
								impl_trx)) {

			lock_rec_add_to_queue(LOCK_REC | LOCK_X
					      | LOCK_REC_NOT_GAP, rec, index,
								impl_trx);
		}
	}
}

/*************************************************************************
Checks if locks of other transactions prevent an immediate modify (update,
delete mark, or delete unmark) of a clustered index record. If they do,
first tests if the query thread should anyway be suspended for some
reason; if not, then puts the transaction and the query thread to the
lock wait state and inserts a waiting request for a record x-lock to the
lock queue. */

ulint
lock_clust_rec_modify_check_and_lock(
/*=================================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record which should be modified */
	dict_index_t*	index,	/* in: clustered index */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;
	
	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_ad(index->type & DICT_CLUSTERED);

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	/* If a transaction has no explicit x-lock set on the record, set one
	for it */

	lock_rec_convert_impl_to_expl(rec, index);

	err = lock_rec_lock(TRUE, LOCK_X | LOCK_REC_NOT_GAP, rec, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(rec, index));

	return(err);
}

/*************************************************************************
Checks if locks of other transactions prevent an immediate modify (delete
mark or delete unmark) of a secondary index record. */

ulint
lock_sec_rec_modify_check_and_lock(
/*===============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record which should be modified;
				NOTE: as this is a secondary index, we
				always have to modify the clustered index
				record first: see the comment below */
	dict_index_t*	index,	/* in: secondary index */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;
	
	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_ad(!(index->type & DICT_CLUSTERED));

	/* Another transaction cannot have an implicit lock on the record,
	because when we come here, we already have modified the clustered
	index record, and this would not have been possible if another active
	transaction had modified this secondary index record. */

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	err = lock_rec_lock(TRUE, LOCK_X | LOCK_REC_NOT_GAP, rec, index, thr);

	lock_mutex_exit_kernel();
	
	ut_ad(lock_rec_queue_validate(rec, index));

	if (err == DB_SUCCESS) {
		/* Update the page max trx id field */

		page_update_max_trx_id(buf_frame_align(rec),
							thr_get_trx(thr)->id);
	}

	return(err);
}

/*************************************************************************
Like the counterpart for a clustered index below, but now we read a
secondary index record. */

ulint
lock_sec_rec_read_check_and_lock(
/*=============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: user record or page supremum record
				which should be read or passed over by a read
				cursor */
	dict_index_t*	index,	/* in: secondary index */
	ulint		mode,	/* in: mode of the lock which the read cursor
				should set on records: LOCK_S or LOCK_X; the
				latter is possible in SELECT FOR UPDATE */
	ulint		gap_mode,/* in: LOCK_ORDINARY, LOCK_GAP, or
				LOCK_REC_NOT_GAP */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(!(index->type & DICT_CLUSTERED));
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	lock_mutex_enter_kernel();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list or a
	database recovery is running. */

	if (((ut_dulint_cmp(page_get_max_trx_id(buf_frame_align(rec)),
					trx_list_get_min_trx_id()) >= 0)
	     		|| recv_recovery_is_on())
	     && !page_rec_is_supremum(rec)) {

 		lock_rec_convert_impl_to_expl(rec, index);
	}

	err = lock_rec_lock(FALSE, mode | gap_mode, rec, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(rec, index));

	return(err);
}

/*************************************************************************
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record. */

ulint
lock_clust_rec_read_check_and_lock(
/*===============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: user record or page supremum record
				which should be read or passed over by a read
				cursor */
	dict_index_t*	index,	/* in: clustered index */
	ulint		mode,	/* in: mode of the lock which the read cursor
				should set on records: LOCK_S or LOCK_X; the
				latter is possible in SELECT FOR UPDATE */
	ulint		gap_mode,/* in: LOCK_ORDINARY, LOCK_GAP, or
				LOCK_REC_NOT_GAP */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
	ut_ad(gap_mode == LOCK_ORDINARY || gap_mode == LOCK_GAP
					|| gap_mode == LOCK_REC_NOT_GAP);
	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	lock_mutex_enter_kernel();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	
	if (!page_rec_is_supremum(rec)) {
	      
		lock_rec_convert_impl_to_expl(rec, index);
	}

	err = lock_rec_lock(FALSE, mode | gap_mode, rec, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(rec, index));
	
	return(err);
}
