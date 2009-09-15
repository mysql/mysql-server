/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-04-10	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_xaction_h__
#define __xt_xaction_h__

#include "filesys_xt.h"
#include "lock_xt.h"

struct XTThread;
struct XTDatabase;
struct XTOpenTable;

#ifdef DEBUG
//#define XT_USE_XACTION_DEBUG_SIZES
#endif

#ifdef XT_USE_XACTION_DEBUG_SIZES

#define XT_TN_NUMBER_INCREMENT	20
#define XT_TN_MAX_TO_FREE		20
#define XT_TN_MAX_TO_FREE_WASTE	3
#define XT_TN_MAX_TO_FREE_CHECK	3
#define XT_TN_MAX_TO_FREE_INC	3

#define XT_XN_SEGMENT_SHIFTS	1

#else

#define XT_TN_NUMBER_INCREMENT	100		// The increment of the transaction number on restart
#define XT_TN_MAX_TO_FREE		800		// The maximum size of the "to free" list
#define XT_TN_MAX_TO_FREE_WASTE	400
#define XT_TN_MAX_TO_FREE_CHECK	100		// Once we have exceeded the limit, we only try in intervals
#define XT_TN_MAX_TO_FREE_INC	100

//#define XT_XN_SEGMENT_SHIFTS	5		// (32)
//#define XT_XN_SEGMENT_SHIFTS	6		// (64)
//#define XT_XN_SEGMENT_SHIFTS	7		// (128)
#define XT_XN_SEGMENT_SHIFTS	8		// (256)
//#define XT_XN_SEGMENT_SHIFTS	9		// (512)

#endif

/* The hash table size (a prime number) */
#if XT_XN_SEGMENT_SHIFTS == 1		// (1)
#define XT_XN_HASH_TABLE_SIZE	1301
#elif XT_XN_SEGMENT_SHIFTS == 5		// (32)
#define XT_XN_HASH_TABLE_SIZE	1009
#elif XT_XN_SEGMENT_SHIFTS == 6		// (64)
#define XT_XN_HASH_TABLE_SIZE	503
#elif XT_XN_SEGMENT_SHIFTS == 7		// (128)
#define XT_XN_HASH_TABLE_SIZE	251
#elif XT_XN_SEGMENT_SHIFTS == 8		// (256)
#define XT_XN_HASH_TABLE_SIZE	127
#elif XT_XN_SEGMENT_SHIFTS == 9		// (512)
#define XT_XN_HASH_TABLE_SIZE	67
#endif

/* Number of pre-allocated transaction data structures per segment */
#define XT_XN_DATA_ALLOC_COUNT	XT_XN_HASH_TABLE_SIZE

#define XT_XN_NO_OF_SEGMENTS	(1 << XT_XN_SEGMENT_SHIFTS)
#define XT_XN_SEGMENT_MASK		(XT_XN_NO_OF_SEGMENTS - 1)

#define XT_XN_XAC_LOGGED		1
#define XT_XN_XAC_ENDED			2					/* The transaction has ended. */
#define XT_XN_XAC_COMMITTED		4					/* The transaction was committed. */
#define XT_XN_XAC_CLEANED		8					/* The transaction has been cleaned. */
#define XT_XN_XAC_RECOVERED		16					/* This transaction was detected on recovery. */
#define XT_XN_XAC_SWEEP			32					/* End ID has been set, OK to sweep. */

#define XT_XN_VISIBLE			0					/* The transaction is committed, and the record is visible. */
#define XT_XN_NOT_VISIBLE		1					/* The transaction is committed, but not visible. */
#define XT_XN_ABORTED			2					/* Transaction was aborted. */
#define XT_XN_MY_UPDATE			3					/* The record was update by me. */
#define XT_XN_OTHER_UPDATE		4					/* The record was updated by someone else. */
#define XT_XN_REREAD			5					/* The transaction is not longer in RAM, status is unkown, retry. */

typedef struct XTXactData {
	xtXactID					xd_start_xn_id;			/* Note: may be zero!. */
	xtXactID					xd_end_xn_id;			/* Note: may be zero!. */

	/* The begin position: */
	xtLogID						xd_begin_log;			/* Non-zero if begin has been logged. */
	xtLogOffset					xd_begin_offset;
	int							xd_flags;
	xtWord4						xd_end_time;
	xtThreadID					xd_thread_id;

	/* A transaction may be indexed twice in the hash table.
	 * Once on the start sequence number, and once on the
	 * end sequence number.
	 */
	struct XTXactData			*xd_next_xact;		/* Next pointer in the hash table, also used by the free list. */

} XTXactDataRec, *XTXactDataPtr;

#ifdef XT_NO_ATOMICS
#define XT_XACT_USE_PTHREAD_RW
#else
//#define XT_XACT_USE_SKEWRWLOCK
#define XT_XACT_USE_SPINXSLOCK
#endif

#if defined(XT_XACT_USE_PTHREAD_RW)
#define XT_XACT_LOCK_TYPE				xt_rwlock_type
#define XT_XACT_INIT_LOCK(s, i)			xt_init_rwlock(s, i)
#define XT_XACT_FREE_LOCK(s, i)			xt_free_rwlock(i)	
#define XT_XACT_READ_LOCK(i, s)			xt_slock_rwlock_ns(i)
#define XT_XACT_WRITE_LOCK(i, s)		xt_xlock_rwlock_ns(i)
#define XT_XACT_UNLOCK(i, s, b)			xt_unlock_rwlock_ns(i)
#elif defined(XT_XACT_USE_SPINXSLOCK)
#define XT_XACT_LOCK_TYPE				XTSpinXSLockRec
#define XT_XACT_INIT_LOCK(s, i)			xt_spinxslock_init_with_autoname(s, i)
#define XT_XACT_FREE_LOCK(s, i)			xt_spinxslock_free(s, i)	
#define XT_XACT_READ_LOCK(i, s)			xt_spinxslock_slock(i)
#define XT_XACT_WRITE_LOCK(i, s)		xt_spinxslock_xlock(i, (s)->t_id)
#define XT_XACT_UNLOCK(i, s, b)			xt_spinxslock_unlock(i, b)
#else
#define XT_XACT_LOCK_TYPE				XTSkewRWLockRec
#define XT_XACT_INIT_LOCK(s, i)			xt_skewrwlock_init_with_autoname(s, i)
#define XT_XACT_FREE_LOCK(s, i)			xt_skewrwlock_free(s, i)	
#define XT_XACT_READ_LOCK(i, s)			xt_skewrwlock_slock(i)
#define XT_XACT_WRITE_LOCK(i, s)		xt_skewrwlock_xlock(i, (s)->t_id)
#define XT_XACT_UNLOCK(i, s, b)			xt_skewrwlock_unlock(i, b)
#endif

/* We store the transactions in a number of segments, each
 * segment has a hash table.
 */
typedef struct XTXactSeg {
	XT_XACT_LOCK_TYPE			xs_tab_lock;						/* Lock for hash table. */
	xtXactID					xs_last_xn_id;						/* The last transaction ID added. */
	XTXactDataPtr				xs_free_list;						/* List of transaction data structures. */
	XTXactDataPtr				xs_table[XT_XN_HASH_TABLE_SIZE];	/* Hash table containing the transaction data structures. */
} XTXactSegRec, *XTXactSegPtr;

typedef struct XTXactWait {
	xtXactID					xw_xn_id;
} XTXactWaitRec, *XTXactWaitPtr;

void			xt_thread_wait_init(struct XTThread *self);
void			xt_thread_wait_exit(struct XTThread *self);

void			xt_xn_init_db(struct XTThread *self, struct XTDatabase *db);
void			xt_xn_exit_db(struct XTThread *self, struct XTDatabase *db);
void			xt_start_sweeper(struct XTThread *self, struct XTDatabase *db);
void			xt_wait_for_sweeper(struct XTThread *self, struct XTDatabase *db, int abort_time);
void			xt_stop_sweeper(struct XTThread *self, struct XTDatabase *db);

void			xt_xn_init_thread(struct XTThread *self, int what_for);
void			xt_xn_exit_thread(struct XTThread *self);
void			xt_wakeup_sweeper(struct XTDatabase *db);

xtBool			xt_xn_begin(struct XTThread *self);
xtBool			xt_xn_commit(struct XTThread *self);
xtBool			xt_xn_rollback(struct XTThread *self);
xtBool			xt_xn_log_tab_id(struct XTThread *self, xtTableID tab_id);
int				xt_xn_status(struct XTOpenTable *ot, xtXactID xn_id, xtRecordID rec_id);
xtBool			xt_xn_wait_for_xact(struct XTThread *self, XTXactWaitPtr xw, struct XTLockWait *lw);
void			xt_xn_wakeup_waiting_threads(struct XTThread *thread);
void			xt_xn_wakeup_thread_list(struct XTThread *thread);
void			xt_xn_wakeup_thread(xtThreadID thd_id);
xtXactID		xt_xn_get_curr_id(struct XTDatabase *db);
xtWord8			xt_xn_bytes_to_sweep(struct XTDatabase *db, struct XTThread *thread);

XTXactDataPtr	xt_xn_add_old_xact(struct XTDatabase *db, xtXactID xn_id, struct XTThread *thread);
XTXactDataPtr	xt_xn_get_xact(struct XTDatabase *db, xtXactID xn_id, struct XTThread *thread);
xtBool			xt_xn_delete_xact(struct XTDatabase *db, xtXactID xn_id, struct XTThread *thread);

inline xtBool	xt_xn_is_before(register xtXactID now, register xtXactID then)
{
	if (now >= then) {
		if ((now - then) > (xtXactID) 0xFFFFFFFF/2)
			return TRUE;
		return FALSE;
	}
	if ((then - now) > (xtXactID) 0xFFFFFFFF/2)
		return FALSE;
	return TRUE;
}

#endif
