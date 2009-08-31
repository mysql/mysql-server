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

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <time.h>
#include <signal.h>

#include "xaction_xt.h"
#include "database_xt.h"
#include "strutil_xt.h"
#include "heap_xt.h"
#include "trace_xt.h"
#include "myxt_xt.h"
#include "tabcache_xt.h"

#ifdef DEBUG
//#define TRACE_WAIT_FOR
//#define TRACE_VARIATIONS
//#define TRACE_SWEEPER_ACTIVITY

/* Enable to trace the statements executed by the engine: */
//#define TRACE_STATEMENTS
#endif

#if defined(TRACE_STATEMENTS) || defined(TRACE_VARIATIONS)
#define TRACE_TRANSACTION
#endif

static void xn_sw_wait_for_xact(XTThreadPtr self, XTDatabaseHPtr db, u_int hsecs);
static xtBool xn_get_xact_details(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr XT_UNUSED(thread), int *flags, xtXactID *start, xtXactID *end, xtThreadID *thd_id);
static xtBool xn_get_xact_pointer(XTDatabaseHPtr db, xtXactID xn_id, XTXactDataPtr *xact_ptr);

/* ============================================================================================== */

typedef struct XNSWRecItem {
	xtTableID				ri_tab_id;
	xtRecordID				ri_rec_id;
} XNSWRecItemRec, *XNSWRecItemPtr;

typedef struct XNSWToFreeItem {
	xtTableID				ri_tab_id;			/* If non-zero, then this is the table of the data record to be freed.
												 * If zero, then this free the transaction below must be freed.
												 */
	union {
		xtRecordID			ri_rec_id;
		xtXactID			ri_xn_id;
	} x;
	xtXactID				ri_wait_xn_id;		/* Wait for this transaction to be cleaned (or being cleaned up)
												 * before freeing this resource. */
} XNSWToFreeItemRec, *XNSWToFreeItemPtr;

/* ----------------------------------------------------------------------
 * TRANSACTION/THREAD WAIT LIST
 */

typedef struct XNWaitThread {
	/* The wait condition of the thread. */
	xt_mutex_type			wt_lock;
	xt_cond_type			wt_cond;

	/* The list of threads waiting for this thread. */
	XTSpinLockRec			wt_wait_list_lock;
	u_int					wt_wait_list_count;
	u_int					wt_wait_list_size;
	xtThreadID				*wt_wait_list;
} XNWaitThreadRec, *XNWaitThreadPtr;

static XNWaitThreadPtr	xn_wait_thread_array;

xtPublic void xt_thread_wait_init(XTThreadPtr self)
{
	xn_wait_thread_array = (XNWaitThreadPtr) xt_calloc(self, xt_thr_maximum_threads * sizeof(XNWaitThreadRec));
	for (u_int i=0; i<xt_thr_maximum_threads; i++) {
		xt_init_mutex_with_autoname(self, &xn_wait_thread_array[i].wt_lock);
		xt_init_cond(self, &xn_wait_thread_array[i].wt_cond);
		xn_wait_thread_array[i].wt_wait_list = NULL;
		xn_wait_thread_array[i].wt_wait_list_count = 0;
		xn_wait_thread_array[i].wt_wait_list_size = 0;
		xt_spinlock_init_with_autoname(self, &xn_wait_thread_array[i].wt_wait_list_lock);
	}
}

xtPublic void xt_thread_wait_exit(XTThreadPtr self)
{
	if (xn_wait_thread_array) {
		for (u_int i=0; i<xt_thr_maximum_threads; i++) {
			xt_free_mutex(&xn_wait_thread_array[i].wt_lock);
			xt_free_cond(&xn_wait_thread_array[i].wt_cond);
			if (xn_wait_thread_array[i].wt_wait_list)
				xt_free(self, xn_wait_thread_array[i].wt_wait_list);
			xt_spinlock_free(self, &xn_wait_thread_array[i].wt_wait_list_lock);
		}
		xt_free(self, xn_wait_thread_array);
	}
}

static xtBool xn_wait_for_thread(xtThreadID waiting_id, xtThreadID wait_for_id)
{
	XNWaitThreadPtr wt;
	
	wt = &xn_wait_thread_array[wait_for_id];
	xt_spinlock_lock(&wt->wt_wait_list_lock);
	if (wt->wt_wait_list_count == wt->wt_wait_list_size) {
		if (!xt_realloc_ns((void **) &wt->wt_wait_list, (wt->wt_wait_list_size+1) * sizeof(xtThreadID)))
			return FAILED;
		wt->wt_wait_list_size++;
	}
	for (u_int i=0; i<wt->wt_wait_list_count; i++) {
		if (wt->wt_wait_list[i] == waiting_id)
			goto done;
	}
	wt->wt_wait_list[wt->wt_wait_list_count] = waiting_id;
	wt->wt_wait_list_count++;
	done:
	xt_spinlock_unlock(&wt->wt_wait_list_lock);
	return OK;
}

xtPublic void xt_xn_wakeup_thread(xtThreadID thd_id)
{
	XNWaitThreadPtr	target_wt;

	target_wt = &xn_wait_thread_array[thd_id];
	xt_lock_mutex_ns(&target_wt->wt_lock);
	xt_broadcast_cond_ns(&target_wt->wt_cond);
	xt_unlock_mutex_ns(&target_wt->wt_lock);
}

xtPublic void xt_xn_wakeup_thread_list(XTThreadPtr thread)
{
	XNWaitThreadPtr	target_wt;

	for (u_int i=0; i<thread->st_thread_list_count; i++) {
		target_wt = &xn_wait_thread_array[thread->st_thread_list[i]];
		xt_lock_mutex_ns(&target_wt->wt_lock);
		xt_broadcast_cond_ns(&target_wt->wt_cond);
		xt_unlock_mutex_ns(&target_wt->wt_lock);
	}
	thread->st_thread_list_count = 0;
}

xtPublic void xt_xn_wakeup_waiting_threads(XTThreadPtr thread)
{
	XNWaitThreadPtr wt;
	XNWaitThreadPtr	target_wt;
	
	wt = &xn_wait_thread_array[thread->t_id];
	if (!wt->wt_wait_list_count)
		return;

	xt_spinlock_lock(&wt->wt_wait_list_lock);
	if (thread->st_thread_list_size < wt->wt_wait_list_count) {
		if (!xt_realloc_ns((void **) &thread->st_thread_list, wt->wt_wait_list_count * sizeof(xtThreadID)))
			goto failed;
		 thread->st_thread_list_size = wt->wt_wait_list_count;
	}
	memcpy(thread->st_thread_list, wt->wt_wait_list, wt->wt_wait_list_count * sizeof(xtThreadID));
	thread->st_thread_list_count = wt->wt_wait_list_count;
	wt->wt_wait_list_count = 0;
	xt_spinlock_unlock(&wt->wt_wait_list_lock);

	xt_xn_wakeup_thread_list(thread);
	return;
	
	failed:
	for (u_int i=0; i<wt->wt_wait_list_count; i++) {
		target_wt = &xn_wait_thread_array[wt->wt_wait_list[i]];
		xt_lock_mutex_ns(&target_wt->wt_lock);
		xt_broadcast_cond_ns(&target_wt->wt_cond);
		xt_unlock_mutex_ns(&target_wt->wt_lock);
	}
	wt->wt_wait_list_count = 0;
	xt_spinlock_unlock(&wt->wt_wait_list_lock);
}

/* ----------------------------------------------------------------------
 * WAIT FOR TRANSACTIONS
 */

typedef struct XNWaitFor {
	xtXactID				wf_waiting_xn_id;		/* The transaction of the waiting thread. */
	xtXactID				wf_for_me_xn_id;		/* The transaction we are waiting for. */
} XNWaitForRec, *XNWaitForPtr;

static int xn_compare_wait_for(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtXactID		*x = (xtXactID *) a;
	XNWaitForPtr	y = (XNWaitForPtr) b;

	if (*x == y->wf_waiting_xn_id)
		return 0;
	if (xt_xn_is_before(*x, y->wf_waiting_xn_id))
		return -1;
	return 1;
}

static void xn_free_wait_for(XTThreadPtr XT_UNUSED(self), void *XT_UNUSED(thunk), void *XT_UNUSED(item))
{
}

/*
 * A deadlock occurs when a transaction is waiting for itself!
 * For example A is waiting for B which is waiting for A.
 * By repeatedly scanning the wait_for list we can find out if a
 * transaction is waiting for itself.
 */
static xtBool xn_detect_deadlock(XTDatabaseHPtr db, xtXactID waiting, xtXactID for_me)
{
	XNWaitForPtr wf;

	for (;;) {
		if (waiting == for_me) {
#ifdef TRACE_WAIT_FOR
			for (u_int i=0; i<xt_sl_get_size(db->db_xn_wait_for); i++) {
				wf = (XNWaitForPtr) xt_sl_item_at(db->db_xn_wait_for, i);
				xt_trace("T%lu --> T%lu\n", (u_long) wf->wf_waiting_xn_id, (u_long) wf->wf_for_me_xn_id);
			}
			xt_ttracef(xt_get_self(), "DEADLOCK\n");
			xt_dump_trace();
#endif
			xt_register_xterr(XT_REG_CONTEXT, XT_ERR_DEADLOCK);
			return TRUE;
		}
		if (!(wf = (XNWaitForPtr) xt_sl_find(NULL, db->db_xn_wait_for, &for_me)))
			break;
		for_me = wf->wf_for_me_xn_id;
	}
	return FALSE;
}

#ifdef XT_USE_SPINLOCK_WAIT_FOR

#if defined(XT_MAC) || defined(XT_WIN)
#define WAIT_SPIN_COUNT			10
#else
#define WAIT_SPIN_COUNT			50
#endif

/* Should not be required, but we wait for a second,
 * just in case the wakeup is missed!
 */
#ifdef DEBUG
#define WAIT_FOR_XACT_TIME		30000
#else
#define WAIT_FOR_XACT_TIME		1000
#endif

static xtBool xn_add_to_wait_for(XTDatabaseHPtr db, XNWaitForPtr wf, XTThreadPtr thread)
{
	/* If we are waiting for a transaction to end, 
	 * put this thread on the wait list...
	 *
	 * As long as the temporary lock is removed
	 * or turned into a permanent lock before
	 * a thread waits again, all should be OK!
	 */
	xt_spinlock_lock(&db->db_xn_wait_spinlock);

#ifdef TRACE_WAIT_FOR
	xt_ttracef(thread, "T%lu -wait-> T%lu\n", (u_long) thread->st_xact_data->xd_start_xn_id, (u_long) wait_xn_id);
#endif
	/* Check for a deadlock: */
	if (xn_detect_deadlock(db, wf->wf_waiting_xn_id, wf->wf_for_me_xn_id))
		goto failed;

	/* We will wait for this transaction... */
	db->db_xn_wait_count++;
	if (thread->st_xact_writer)
		db->db_xn_writer_wait_count++;

	if (!xt_sl_insert(NULL, db->db_xn_wait_for, &wf->wf_waiting_xn_id, wf)) {
		db->db_xn_wait_count--;
		goto failed;
	}

	xt_spinlock_unlock(&db->db_xn_wait_spinlock);
	return OK;

	failed:
	xt_spinlock_unlock(&db->db_xn_wait_spinlock);
	return FAILED;
}

inline void xn_remove_from_wait_for(XTDatabaseHPtr db, XNWaitForPtr wf, XTThreadPtr thread)
{
	xt_spinlock_lock(&db->db_xn_wait_spinlock);

	xt_sl_delete(NULL, db->db_xn_wait_for, &wf->wf_waiting_xn_id);
	db->db_xn_wait_count--;
	if (thread->st_xact_writer)
		db->db_xn_writer_wait_count--;

#ifdef TRACE_WAIT_FOR
	xt_ttracef(thread, "T%lu -wait-> T%lu FAILED\n", (u_long) thread->st_xact_data->xd_start_xn_id, (u_long) wait_xn_id);
#endif
	xt_spinlock_unlock(&db->db_xn_wait_spinlock);
}

/* Wait for a transation to terminate or a lock to be granted.
 *
 * If term_req is TRUE, then the termination of the transaction is required
 * before continuing.
 *
 * If pw_func is set then this function will not return before this call has
 * succeeded.
 *
 * This function returns FAILE on error.
 */
xtPublic xtBool xt_xn_wait_for_xact(XTThreadPtr thread, XTXactWaitPtr xw, XTLockWaitPtr lw)
{
	XTDatabaseHPtr		db = thread->st_database;
	XNWaitForRec		wf;
	int					flags = 0;
	xtXactID			start = 0;
	XTXactDataPtr		wait_xact_ptr;
	xtBool				on_wait_list = FALSE;
	XTXactWaitRec		xw_new;
	u_int				loop_count = 0;
	XNWaitThreadPtr		my_wt;

	ASSERT_NS(thread->st_xact_data);
	thread->st_statistics.st_wait_for_xact++;

	wf.wf_waiting_xn_id = thread->st_xact_data->xd_start_xn_id;

	if (lw) {
		/* If we are here, then the lw structure is on the wait
		 * queue for the given lock.
		 */
		xtXactID locking_xn_id;
		
		wait_for_locker:
		locking_xn_id = lw->lw_xn_id;
		wf.wf_for_me_xn_id = lw->lw_xn_id;
		if (!xn_add_to_wait_for(db, &wf, thread)) {
			lw->lw_ot->ot_table->tab_locks.xt_cancel_temp_lock(lw);
			return FAILED;
		}

		while (loop_count < WAIT_SPIN_COUNT) {
			loop_count++;

			switch (lw->lw_curr_lock) {
				case XT_LOCK_ERR:
					xn_remove_from_wait_for(db, &wf, thread);
					return FAILED;
				case XT_NO_LOCK:
					/* Got the lock: */
					/* Check if we must also wait for the transaction: */
					if (lw->lw_row_updated) {
						/* This will override the xw passed in.
						 * The reason is, because we are actually waiting
						 * for a lock, and the lock owner may have changed
						 * while we were waiting for the lock.
						 */
						xw_new.xw_xn_id = lw->lw_updating_xn_id;
						xw = &xw_new;
					}
					if (xw) {
						if (wf.wf_for_me_xn_id == xw->xw_xn_id)
							on_wait_list = TRUE;
						else
							xn_remove_from_wait_for(db, &wf, thread);
						goto wait_for_xact;
					}
					xn_remove_from_wait_for(db, &wf, thread);
					return OK;
				case XT_TEMP_LOCK:
				case XT_PERM_LOCK:
					if (locking_xn_id != lw->lw_xn_id) {
						/* Change the transaction that we are waiting for: */
						xn_remove_from_wait_for(db, &wf, thread);
						goto wait_for_locker;
					}
					break;
			}

			xt_critical_wait();
		}


		/* The non-spinning version... */
		wait_for_locker_no_spin:
		my_wt = &xn_wait_thread_array[thread->t_id];
		xt_lock_mutex_ns(&my_wt->wt_lock);

		for (;;) {
			switch (lw->lw_curr_lock) {
				case XT_LOCK_ERR:
					xt_unlock_mutex_ns(&my_wt->wt_lock);
					xn_remove_from_wait_for(db, &wf, thread);
					return FAILED;
				case XT_NO_LOCK:
					xt_unlock_mutex_ns(&my_wt->wt_lock);
					if (lw->lw_row_updated) {
						xw_new.xw_xn_id = lw->lw_updating_xn_id;
						xw = &xw_new;
					}
					if (xw) {
						if (wf.wf_for_me_xn_id == xw->xw_xn_id)
							on_wait_list = TRUE;
						else
							xn_remove_from_wait_for(db, &wf, thread);
						goto wait_for_xact;
					}
					xn_remove_from_wait_for(db, &wf, thread);
					return OK;
				case XT_TEMP_LOCK:
				case XT_PERM_LOCK:
					if (locking_xn_id != lw->lw_xn_id) {
						/* Change the transaction that we are waiting for: */
						xt_unlock_mutex_ns(&my_wt->wt_lock);
						xn_remove_from_wait_for(db, &wf, thread);
						locking_xn_id = lw->lw_xn_id;
						wf.wf_for_me_xn_id = lw->lw_xn_id;
						if (!xn_add_to_wait_for(db, &wf, thread)) {
							lw->lw_ot->ot_table->tab_locks.xt_cancel_temp_lock(lw);
							return FAILED;
						}
						goto wait_for_locker_no_spin;
					}
					break;
			}

			xt_timed_wait_cond_ns(&my_wt->wt_cond, &my_wt->wt_lock, WAIT_FOR_XACT_TIME);
		}

		/* Unreachable
		xt_unlock_mutex_ns(&my_wt->wt_lock);
		*/
	}

	if (xw) {
		xtThreadID		tn_thd_id;

		wait_for_xact:
		wf.wf_for_me_xn_id = xw->xw_xn_id;

		if (!xn_get_xact_pointer(db, xw->xw_xn_id, &wait_xact_ptr))
			/* The transaction was not found... */
			goto wait_done;

		if (wait_xact_ptr) {
			/* This is a dirty read, but it should work! */
			flags = wait_xact_ptr->xd_flags;
			start = wait_xact_ptr->xd_start_xn_id;
			tn_thd_id = wait_xact_ptr->xd_thread_id;
		}
		else {
			tn_thd_id = 0;
			if (!xn_get_xact_details(db, xw->xw_xn_id, thread, &flags, &start, NULL, &tn_thd_id))
				flags = XT_XN_XAC_ENDED | XT_XN_XAC_SWEEP;
		}

		if ((flags & XT_XN_XAC_ENDED) || start != xw->xw_xn_id)
			/* The transaction has terminated! */
			goto wait_done;

		/* Tell the thread we are waiting for it: */
		xn_wait_for_thread(thread->t_id, tn_thd_id);

		if (!on_wait_list) {
			if (!xn_add_to_wait_for(db, &wf, thread))
				return FAILED;
			on_wait_list = TRUE;
		}

		/* The spinning version: */
		while (loop_count < WAIT_SPIN_COUNT) {
			loop_count++;

			xt_critical_wait();

			if (wait_xact_ptr) {
				/* This is a dirty read, but it should work! */
				flags = wait_xact_ptr->xd_flags;
				start = wait_xact_ptr->xd_start_xn_id;
			}
			else {
				if (!xn_get_xact_details(db, xw->xw_xn_id, thread, &flags, &start, NULL, NULL))
					flags = XT_XN_XAC_ENDED | XT_XN_XAC_SWEEP;
			}

			if ((flags & XT_XN_XAC_ENDED) || start != xw->xw_xn_id)
				/* The transaction has terminated! */
				goto wait_done;
		}

		/* The non-spinning version:
		 *
		 * I believe I can avoid missing the wakeup signal
		 * by locking before we check if the transaction
		 * is still running.
		 *
		 * Even though db->db_xn_wait_on_cond is "dirty read".
		 *
		 * The reason is, before the signal is sent the 
		 * lock is also aquired. This is not possible until
		 * this thread is safely sleaping.
		 */
		my_wt = &xn_wait_thread_array[thread->t_id];
		xt_lock_mutex_ns(&my_wt->wt_lock);

		for (;;) {
			if (wait_xact_ptr) {
				/* This is a dirty read, but it should work! */
				flags = wait_xact_ptr->xd_flags;
				start = wait_xact_ptr->xd_start_xn_id;
			}
			else {
				if (!xn_get_xact_details(db, xw->xw_xn_id, thread, &flags, &start, NULL, NULL))
					flags = XT_XN_XAC_ENDED | XT_XN_XAC_SWEEP;
			}

			if ((flags & XT_XN_XAC_ENDED) || start != xw->xw_xn_id)
				/* The transaction has terminated! */
				break;

			xt_timed_wait_cond_ns(&my_wt->wt_cond, &my_wt->wt_lock, WAIT_FOR_XACT_TIME);
		}

		xt_unlock_mutex_ns(&my_wt->wt_lock);

		wait_done:
		if (on_wait_list)
			xn_remove_from_wait_for(db, &wf, thread);
	}

	return OK;
}

#else // XT_USE_SPINLOCK_WAIT_FOR
/*
 * The given thread must wait for the specified transaction to terminate. This
 * function places the transaction of the thread on a list of waiting threads.
 *
 * Before waiting we make a check for deadlocks. A deadlock occurs
 * if waiting would introduce a cycle.
 */
xtPublic xtBool old_xt_xn_wait_for_xact(XTThreadPtr thread, xtXactID xn_id, xtBool will_retry, XTLockWaitFuncPtr pw_func, XTLockWaitPtr pw_data)
{
	XTDatabaseHPtr		db = thread->st_database;
	XNWaitForRec		wf;
	int					flags = 0;
	xtXactID			start = 0;

	ASSERT_NS(thread->st_xact_data);

	thread->st_statistics.st_wait_for_xact++;
	wf.wf_waiting_xn_id = thread->st_xact_data->xd_start_xn_id;
	wf.wf_for_me_xn_id = xn_id;
	wf.wf_thread_id = thread->t_id;

	xt_lock_mutex_ns(&db->db_xn_wait_lock);

#ifdef TRACE_WAIT_FOR
	xt_ttracef(thread, "T%lu -wait-> T%lu\n", (u_long) thread->st_xact_data->xd_start_xn_id, (u_long) xn_id);
#endif
	for (;;) {
		if (!xn_get_xact_details(db, xn_id, thread, &flags, &start, NULL, NULL))
			break;

		/* This is a dirty read, but it should work! */
		if ((flags & XT_XN_XAC_ENDED) || start != xn_id)
			break;

		if (xn_detect_deadlock(db, wf.wf_waiting_xn_id, wf.wf_for_me_xn_id))
			goto failed;

		/* We will wait for this transaction... */
		db->db_xn_wait_count++;
		if (thread->st_xact_writer)
			db->db_xn_writer_wait_count++;

		if (!xt_sl_insert(NULL, db->db_xn_wait_for, &wf.wf_waiting_xn_id, &wf)) {
			db->db_xn_wait_count--;
			goto failed;
		}

		if (!xn_get_xact_details(db, xn_id, thread, &flags, &start, NULL, NULL)) {
			xt_sl_delete(NULL, db->db_xn_wait_for, &wf.wf_waiting_xn_id);
			db->db_xn_wait_count--;
			if (thread->st_xact_writer)
				db->db_xn_writer_wait_count--;
			break;
		}

		if ((flags & XT_XN_XAC_ENDED) || start != xn_id) {
			xt_sl_delete(NULL, db->db_xn_wait_for, &wf.wf_waiting_xn_id);
			db->db_xn_wait_count--;
			if (thread->st_xact_writer)
				db->db_xn_writer_wait_count--;
			break;
		}

		db->db_xn_post_wait[thread->t_id].pw_call_me = pw_func;
		db->db_xn_post_wait[thread->t_id].pw_thread = thread;
		db->db_xn_post_wait[thread->t_id].pw_data = pw_data;

		/* Timed wait because it is possible that transaction quits before
		 * we go to sleep.
		 */
		if (!xt_timed_wait_cond(NULL, &db->db_xn_wait_cond, &db->db_xn_wait_lock, 2 * 1000)) {
			xt_sl_delete(NULL, db->db_xn_wait_for, &wf.wf_waiting_xn_id);
			db->db_xn_wait_count--;
			if (thread->st_xact_writer)
				db->db_xn_writer_wait_count--;
			goto failed;
		}

		db->db_xn_post_wait[thread->t_id].pw_call_me = NULL;
		xt_sl_delete(NULL, db->db_xn_wait_for, &wf.wf_waiting_xn_id);
		db->db_xn_wait_count--;
		if (thread->st_xact_writer)
			db->db_xn_writer_wait_count--;
		
		if (will_retry)
			break;
	}

#ifdef TRACE_WAIT_FOR
	xt_ttracef(thread, "T%lu -wait-> T%lu DONE\n", (u_long) thread->st_xact_data->xd_start_xn_id, (u_long) xn_id);
#endif
	xt_unlock_mutex_ns(&db->db_xn_wait_lock);
	return OK;

	failed:
#ifdef TRACE_WAIT_FOR
	xt_ttracef(self, "T%lu -wait-> T%lu FAILED\n", (u_long) self->st_xact_data->xd_start_xn_id, (u_long) xn_id);
#endif
	xt_unlock_mutex_ns(&db->db_xn_wait_lock);
	return FAILED;
}

xtPublic void old_xt_xn_wakeup_transactions(XTDatabaseHPtr db, XTThreadPtr thread)
{
	u_int			len;
	XNWaitForPtr	wf;

	xt_lock_mutex_ns(&db->db_xn_wait_lock);
	/* The idea here is to release the oldest transactions
	 * first. Although this may not be completely fair
	 * it has the advantage that older transactions are
	 * encouraged to complete first.
	 *
	 * I have found the following problem with this test:
	 * runTest(INCREMENT_TEST, 16, INCREMENT_TEST_UPDATE_COUNT);
	 * with a bit of bad luck a transaction can be starved.
	 * This results in the sweeper stalling because it is
	 * waiting for an old transaction to quite so that
	 * it continue.
	 *
	 * Because the sweeper is waiting, the number of
	 * versions of the record to be updated
	 * begins to increase. In the above test over
	 * 1600 transaction remain uncleaned.
	 *
	 * This means that there are 1600 version of the
	 * row which must be scanned to find the most
	 * recent version.
	 */
	if ((len = (u_int) xt_sl_get_size(db->db_xn_wait_for))) {
		for (u_int i=0; i<len; i++) {
			wf = (XNWaitForPtr) xt_sl_item_at(db->db_xn_wait_for, i);
			if (db->db_xn_post_wait[wf->wf_thread_id].pw_call_me) {
				if (db->db_xn_post_wait[wf->wf_thread_id].pw_call_me(thread, &db->db_xn_post_wait[wf->wf_thread_id]))
					db->db_xn_post_wait[wf->wf_thread_id].pw_call_me = NULL;
			}
		}
		if (!xt_broadcast_cond_ns(&db->db_xn_wait_cond))
			xt_log_and_clear_exception_ns();
	}
	ASSERT_NS(db->db_xn_wait_count == len);
	xt_unlock_mutex_ns(&db->db_xn_wait_lock);
}
#endif  // XT_USE_SPINLOCK_WAIT_FOR

/* ----------------------------------------------------------------------
 * Utilities
 */

//#define HIGH_X
#ifdef HIGH_X
u_long tot_alloced;
u_long high_alloced;
u_long not_clean_max;
u_long in_ram_max;
#endif

static void xn_free_xact(XTDatabaseHPtr db, XTXactSegPtr seg, XTXactDataPtr xact)
{
#ifdef HIGH_X
	tot_alloced--;
#endif
	/* This indicates the structure is free: */
	xact->xd_start_xn_id = 0;
	if ((xtWord1 *) xact >= db->db_xn_data && (xtWord1 *) xact < db->db_xn_data_end) {
		/* Put it in the free list: */
		xact->xd_next_xact = seg->xs_free_list;
		seg->xs_free_list = xact;
		return;
	}
	xt_free_ns(xact);
}

/*
 * GOTCHA: The value db->db_xn_curr_id may be a bit larger
 * than the actual transaction created because there is
 * a gap between the issude of the transaction ID
 * and the creation of a memory structure.
 * (indicated here: {GAP-INC-ADD-XACT})
 *
 * This function returns the actuall current transaction ID.
 * This is the number of the last transaction actually
 * created in memory.
 *
 * This means that if you call xt_xn_get_xact() with any
 * number less than or equal to this value, not finding
 * the transaction means it has already ended!
 */
xtPublic xtXactID xt_xn_get_curr_id(XTDatabaseHPtr db)
{
	int						i;
	xtXactID				curr_xn_id;
	register XTXactSegPtr 	seg = db->db_xn_idx;

	/* Find the highest transaction ID actually created... */
	curr_xn_id = seg->xs_last_xn_id;
	seg++;
	for (i=1; i<XT_XN_NO_OF_SEGMENTS; i++, seg++) {
		if (xt_xn_is_before(curr_xn_id, seg->xs_last_xn_id))
			curr_xn_id = seg->xs_last_xn_id;
	}
	return curr_xn_id;
}

xtPublic XTXactDataPtr xt_xn_add_old_xact(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr thread)
{
	register XTXactDataPtr	xact;
	register XTXactSegPtr 	seg;
	register XTXactDataPtr	*hash;

	(void) thread;
	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_WRITE_LOCK(&seg->xs_tab_lock, thread);
	hash = &seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];
	xact = *hash;
	while (xact) {
		if (xact->xd_start_xn_id == xn_id)
			goto done_ok;
		xact = xact->xd_next_xact;
	}

	if ((xact = seg->xs_free_list))
		seg->xs_free_list = xact->xd_next_xact;
	else {
		/* We have used up all the free transaction slots,
		 * the sweeper should work faster to free them
		 * up...
		 */
		db->db_sw_faster |= XT_SW_NO_MORE_XACT_SLOTS;
		if (!(xact = (XTXactDataPtr) xt_malloc_ns(sizeof(XTXactDataRec)))) {
			XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, TRUE);
			return NULL;
		}
	}

	xact->xd_next_xact = *hash;
	*hash = xact;

	xact->xd_start_xn_id = xn_id;
	xact->xd_end_xn_id = 0;
	xact->xd_end_time = 0;
	xact->xd_begin_log = 0;
	xact->xd_flags = 0;

	/* Get the largest transaction id. */
	if (xt_xn_is_before(seg->xs_last_xn_id, xn_id))
		seg->xs_last_xn_id = xn_id;

	done_ok:
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, TRUE);
#ifdef HIGH_X
	tot_alloced++;
	if (tot_alloced > high_alloced)
		high_alloced = tot_alloced;
#endif
	return xact;
}

static XTXactDataPtr xn_add_new_xact(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr thread)
{
	register XTXactDataPtr	xact;
	register XTXactSegPtr 	seg;
	register XTXactDataPtr	*hash;

	(void) thread;
	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_WRITE_LOCK(&seg->xs_tab_lock, thread);
	hash = &seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];

	if ((xact = seg->xs_free_list))
		seg->xs_free_list = xact->xd_next_xact;
	else {
		/* We have used up all the free transaction slots,
		 * the sweeper should work faster to free them
		 * up...
		 */
		db->db_sw_faster |= XT_SW_NO_MORE_XACT_SLOTS;
		if (!(xact = (XTXactDataPtr) xt_malloc_ns(sizeof(XTXactDataRec)))) {
			XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, TRUE);
			return NULL;
		}
	}

	xact->xd_next_xact = *hash;
	*hash = xact;

	xact->xd_thread_id = thread->t_id;
	xact->xd_start_xn_id = xn_id;
	xact->xd_end_xn_id = 0;
	xact->xd_end_time = 0;
	xact->xd_begin_log = 0;
	xact->xd_flags = 0;

	seg->xs_last_xn_id = xn_id;
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, TRUE);
#ifdef HIGH_X
	tot_alloced++;
	if (tot_alloced > high_alloced)
		high_alloced = tot_alloced;
#endif
	return xact;
}

static xtBool xn_get_xact_details(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr XT_UNUSED(thread), int *flags, xtXactID *start, xtWord4 *end, xtThreadID *thd_id)
{
	register XTXactSegPtr 	seg;
	register XTXactDataPtr	xact;
	xtBool					found = FALSE;

	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_READ_LOCK(&seg->xs_tab_lock, thread);
	xact = seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];
	while (xact) {
		if (xact->xd_start_xn_id == xn_id) {
			found = TRUE;
			if (flags)
				*flags = xact->xd_flags;
			if (start)
				*start = xact->xd_start_xn_id;
			if (end)
				*end = xact->xd_end_time;
			if (thd_id)
				*thd_id = xact->xd_thread_id;
			break;
		}
		xact = xact->xd_next_xact;
	}
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, FALSE);
	return found;
}

static xtBool xn_get_xact_pointer(XTDatabaseHPtr db, xtXactID xn_id, XTXactDataPtr *xact_ptr)
{
	register XTXactSegPtr 	seg;
	register XTXactDataPtr	xact;
	xtBool					found = FALSE;

	*xact_ptr = NULL;
	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_READ_LOCK(&seg->xs_tab_lock, thread);
	xact = seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];
	while (xact) {
		if (xact->xd_start_xn_id == xn_id) {
			found = TRUE;
			/* We only return pointers to transaction structures that are permanently
			 * allocated!
			 */
			if ((xtWord1 *) xact >= db->db_xn_data && (xtWord1 *) xact < db->db_xn_data_end)
				*xact_ptr = xact;
			break;
		}
		xact = xact->xd_next_xact;
	}
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, FALSE);
	return found;
}

static xtBool xn_get_xact_start(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr XT_UNUSED(thread), xtLogID *log_id, xtLogOffset *log_offset)
{
	register XTXactSegPtr 	seg;
	register XTXactDataPtr	xact;
	xtBool					found = FALSE;

	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_READ_LOCK(&seg->xs_tab_lock, thread);
	xact = seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];
	while (xact) {
		if (xact->xd_start_xn_id == xn_id) {
			found = TRUE;
			*log_id = xact->xd_begin_log;
			*log_offset = xact->xd_begin_offset;
			break;
		}
		xact = xact->xd_next_xact;
	}
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, FALSE);
	return found;
}

/* NOTE: this function may only be used by the sweeper or the recovery process. */
xtPublic XTXactDataPtr xt_xn_get_xact(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr XT_UNUSED(thread))
{
	register XTXactSegPtr 	seg;
	register XTXactDataPtr	xact;

	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_READ_LOCK(&seg->xs_tab_lock, thread);
	xact = seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];
	while (xact) {
		if (xact->xd_start_xn_id == xn_id)
			break;
		xact = xact->xd_next_xact;
	}
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, FALSE);
	return xact;
}

/*
 * Delete a transaction, return TRUE if the transaction
 * was found.
 */
xtPublic xtBool xt_xn_delete_xact(XTDatabaseHPtr db, xtXactID xn_id, XTThreadPtr thread)
{
	XTXactDataPtr	xact, pxact = NULL;
	XTXactSegPtr 	seg;

	(void) thread;
	seg = &db->db_xn_idx[xn_id & XT_XN_SEGMENT_MASK];
	XT_XACT_WRITE_LOCK(&seg->xs_tab_lock, thread);
	xact = seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE];
	while (xact) {
		if (xact->xd_start_xn_id == xn_id) {
			if (pxact)
				pxact->xd_next_xact = xact->xd_next_xact;
			else
				 seg->xs_table[(xn_id >> XT_XN_SEGMENT_SHIFTS) % XT_XN_HASH_TABLE_SIZE] = xact->xd_next_xact;
			xn_free_xact(db, seg, xact);
			XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, TRUE);
			return TRUE;
		}
		pxact = xact;
		xact = xact->xd_next_xact;
	}
	XT_XACT_UNLOCK(&seg->xs_tab_lock, thread, TRUE);
	return FALSE;
}

//#define DEBUG_RAM_LIST
#ifdef DEBUG_RAM_LIST

#define DEBUG_RAM_LIST_SIZE			80

int					check_ram_init_count = 0;
xt_rwlock_type		check_ram_lock;
xtXactID			check_ram_trns[DEBUG_RAM_LIST_SIZE];
int					check_ram_dummy;

static void check_ram_init(void)
{
	if (check_ram_init_count == 0)
		xt_init_rwlock(NULL, &check_ram_lock);
	check_ram_init_count++;
}

static void check_ram_free(void)
{
	check_ram_init_count--;
	if (check_ram_init_count == 0)
		xt_free_rwlock(&check_ram_lock);
}

static void check_ram_min_id(XTDatabaseHPtr db)
{
	int i;

	xt_slock_rwlock_ns(&check_ram_lock);
	for (i=0; i<DEBUG_RAM_LIST_SIZE; i++) {
		if (check_ram_trns[i] && xt_xn_is_before(check_ram_trns[i], db->db_xn_min_ram_id)) {
			/* This should never happen! */
			XTXactDataPtr x_ptr;

			check_ram_dummy = 0;
			for (i=0; i<DEBUG_RAM_LIST_SIZE; i++) {
				if (check_ram_trns[i]) {
					x_ptr = xt_xn_get_xact(db, check_ram_trns[i]);
					check_ram_dummy = 1;
				}
			}
			break;
		}
	}
	xt_unlock_rwlock_ns(&check_ram_lock);
}

static void check_ram_add(xtXactID xn_id)
{
	int i;
	
	xt_xlock_rwlock_ns(&check_ram_lock);
	for (i=0; i<DEBUG_RAM_LIST_SIZE; i++) {
		if (!check_ram_trns[i]) {
			check_ram_trns[i] = xn_id;
			xt_unlock_rwlock_ns(&check_ram_lock);
			return;
		}
	}
	xt_unlock_rwlock_ns(&check_ram_lock);
	printf("DEBUG --- List too small\n");
}

static void check_ram_del(xtXactID xn_id)
{
	int i;
	
	xt_xlock_rwlock_ns(&check_ram_lock);
	for (i=0; i<DEBUG_RAM_LIST_SIZE; i++) {
		if (check_ram_trns[i] == xn_id) {
			check_ram_trns[i] = 0;
			xt_unlock_rwlock_ns(&check_ram_lock);
			return;
		}
	}
	xt_unlock_rwlock_ns(&check_ram_lock);
}
#endif

/* ----------------------------------------------------------------------
 * Init and Exit
 */

xtPublic void xt_xn_init_db(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTXactDataPtr	xact;
	XTXactSegPtr	seg;

#ifdef DEBUG_RAM_LIST
	check_ram_init();
#endif
	xt_spinlock_init_with_autoname(self, &db->db_xn_id_lock);
	xt_spinlock_init_with_autoname(self, &db->db_xn_wait_spinlock);
	//xt_init_mutex_with_autoname(self, &db->db_xn_wait_lock);
	//xt_init_cond(self, &db->db_xn_wait_cond);
	xt_init_mutex_with_autoname(self, &db->db_sw_lock);
	xt_init_cond(self, &db->db_sw_cond);
	xt_init_mutex_with_autoname(self, &db->db_wr_lock);
	xt_init_cond(self, &db->db_wr_cond);

	/* Pre-allocate transaction data structures: */
	db->db_xn_data = (xtWord1 *) xt_malloc(self, sizeof(XTXactDataRec) * XT_XN_DATA_ALLOC_COUNT * XT_XN_NO_OF_SEGMENTS);
	db->db_xn_data_end = db->db_xn_data + sizeof(XTXactDataRec) * XT_XN_DATA_ALLOC_COUNT * XT_XN_NO_OF_SEGMENTS;
	xact = (XTXactDataPtr) db->db_xn_data;
	for (u_int i=0; i<XT_XN_NO_OF_SEGMENTS; i++) {
		seg = &db->db_xn_idx[i];
		XT_XACT_INIT_LOCK(self, &seg->xs_tab_lock);
		for (u_int j=0;  j<XT_XN_DATA_ALLOC_COUNT; j++) {
			xact->xd_next_xact = seg->xs_free_list;
			seg->xs_free_list = xact;
			xact++;
		}
	}

	/* Initialize the data logs: */
	db->db_datalogs.dlc_init(self, db); 

	/* Setup the transaction log: */
	db->db_xlog.xlog_setup(self, db, (off_t) xt_db_log_file_threshold, xt_db_transaction_buffer_size, xt_db_log_file_count);

	db->db_xn_end_time = 1;

	/* Initializing the restart file, also does
	 * recovery. This returns the log position after recovery.
	 *
	 * This is the log position where the writer thread will
	 * begin. The writer thread writes changes to the database that
	 * have been flushed to the log.
	 */
	xt_xres_init(self, db);

	/* Initialize the "last transaction in memory", by default
	 * this is the current transaction ID, which is the ID
	 * of the last transaction.
	 */
	for (u_int i=0; i<XT_XN_NO_OF_SEGMENTS; i++) {
		seg = &db->db_xn_idx[i];
		XT_XACT_INIT_LOCK(self, &seg->xs_tab_lock);
		seg->xs_last_xn_id = db->db_xn_curr_id;
	}

	/*
	 * The next transaction to clean is the lowest transaction
	 * in memory:
	 */
	db->db_xn_to_clean_id = db->db_xn_min_ram_id;

	/*
	 * No transactions are running, so the minimum transaction
	 * ID is the next one to run:
	 */
	db->db_xn_min_run_id = db->db_xn_curr_id + 1;

	db->db_xn_wait_for = xt_new_sortedlist(self, sizeof(XNWaitForRec), 100, 50, xn_compare_wait_for, db, xn_free_wait_for, FALSE, FALSE);
}

xtPublic void xt_xn_exit_db(XTThreadPtr self, XTDatabaseHPtr db)
{
#ifdef HIGH_X
	printf("=========> MOST TXs CURR ALLOC: %lu\n", tot_alloced);
	printf("=========> MOST TXs HIGH ALLOC: %lu\n", high_alloced);
	printf("=========> MAX TXs NOT CLEAN: %lu\n", not_clean_max);
	printf("=========> MAX TXs IN RAM: %lu\n", in_ram_max);
#endif

	xt_stop_sweeper(self, db);	// Should be done already!
	xt_stop_writer(self, db);	// Should be done already!

	xt_xres_exit(self, db);
	db->db_xlog.xlog_exit(self);

	db->db_datalogs.dlc_exit(self); 

	for (u_int i=0; i<XT_XN_NO_OF_SEGMENTS; i++) {
		XTXactSegPtr 	seg;

		seg = &db->db_xn_idx[i];
		for (u_int j=0; j<XT_XN_HASH_TABLE_SIZE; j++) {
			XTXactDataPtr	xact, nxact;
			
			xact = seg->xs_table[j];
			while (xact) {
				nxact = xact->xd_next_xact;
				xn_free_xact(db, seg, xact);
				xact = nxact;
			}
		}
		XT_XACT_FREE_LOCK(self, &seg->xs_tab_lock);
	}
	if (db->db_xn_wait_for) {
		xt_free_sortedlist(self, db->db_xn_wait_for);
		db->db_xn_wait_for = NULL;
	}
	if (db->db_xn_data) {
		xt_free(self, db->db_xn_data);
		db->db_xn_data = NULL;
		db->db_xn_data_end = NULL;
	}

	xt_free_cond(&db->db_wr_cond);
	xt_free_mutex(&db->db_wr_lock);
	xt_free_cond(&db->db_sw_cond);
	xt_free_mutex(&db->db_sw_lock);
	//xt_free_cond(&db->db_xn_wait_cond);
	//xt_free_mutex(&db->db_xn_wait_lock);
	xt_spinlock_free(self, &db->db_xn_wait_spinlock);
	xt_spinlock_free(self, &db->db_xn_id_lock);
#ifdef DEBUG_RAM_LIST
	check_ram_free();
#endif
}

xtPublic void xt_xn_init_thread(XTThreadPtr self, int what_for)
{
	ASSERT(self->st_database);

	if (!xt_init_row_lock_list(&self->st_lock_list))
		xt_throw(self);
	switch (what_for) {
		case XT_FOR_COMPACTOR:
			self->st_dlog_buf.dlb_init(self->st_database, xt_db_log_buffer_size);
			break;
		case XT_FOR_WRITER:
			/* The writer does not need a transaction buffer. */
			self->st_dlog_buf.dlb_init(self->st_database, 0);
			break;
		case XT_FOR_SWEEPER:
			self->st_dlog_buf.dlb_init(self->st_database, 0);
			break;
		case XT_FOR_USER:
			self->st_dlog_buf.dlb_init(self->st_database, xt_db_log_buffer_size);
			break;
	}
}

xtPublic void xt_xn_exit_thread(XTThreadPtr self)
{
	if (self->st_xact_data)
		xt_xn_rollback(self);
	self->st_dlog_buf.dlb_exit(self);
	xt_exit_row_lock_list(&self->st_lock_list);
}

/* ----------------------------------------------------------------------
 * Begin and End Transactions
 */

xtPublic xtBool xt_xn_begin(XTThreadPtr self)
{
	XTDatabaseHPtr	db = self->st_database;
	xtXactID		xn_id;

	ASSERT(!self->st_xact_data);

	xt_spinlock_lock(&db->db_xn_id_lock);
	xn_id = ++db->db_xn_curr_id;
	xt_spinlock_unlock(&db->db_xn_id_lock);

#ifdef HIGH_X
	if (xt_xn_is_before(not_clean_max, xn_id - db->db_xn_to_clean_id))
		not_clean_max = xn_id - db->db_xn_to_clean_id;
	if (xt_xn_is_before(in_ram_max, xn_id - db->db_xn_min_ram_id))
		in_ram_max = xn_id - db->db_xn_min_ram_id;
#endif
	/* {GAP-INC-ADD-XACT} This is the gap between incrementing the ID,
	 * and creating the transaction in memory.
	 * See xt_xn_get_curr_id().
	 */

	if (!(self->st_xact_data = xn_add_new_xact(db, xn_id, self)))
		return FAILED;
	self->st_xact_writer = FALSE;
	
	/* All transactions that committed before or at this time
	 * are this one are visible: */
	self->st_visible_time = db->db_xn_end_time;

#ifdef TRACE_TRANSACTION
	xt_ttracef(self, "BEGIN T%lu\n", (u_long) self->st_xact_data->xd_start_xn_id);
#endif
#ifdef XT_TRACK_CONNECTIONS
	xt_track_conn_info[self->t_id].ci_curr_xact_id = self->st_xact_data->xd_start_xn_id;
	xt_track_conn_info[self->t_id].ci_xact_start = xt_trace_clock();
#endif
	return OK;
}

static xtBool xn_end_xact(XTThreadPtr thread, u_int status)
{
	XTXactDataPtr	xact;
	xtBool			ok = TRUE;

	ASSERT_NS(thread->st_xact_data);
	if ((xact = thread->st_xact_data)) {
		XTDatabaseHPtr	db = thread->st_database;
		xtXactID		xn_id = xact->xd_start_xn_id;
		xtBool			writer;
		
		if ((writer = thread->st_xact_writer)) {
			/* The transaction wrote something: */
			XTXactEndEntryDRec	entry;
			xtWord4				sum;

			sum = XT_CHECKSUM4_XACT(xn_id) ^ XT_CHECKSUM4_XACT(0);
			entry.xe_status_1 = status;
			entry.xe_checksum_1 = XT_CHECKSUM_1(sum);
			XT_SET_DISK_4(entry.xe_xact_id_4, xn_id);
			XT_SET_DISK_4(entry.xe_not_used_4, 0);

#ifdef XT_IMPLEMENT_NO_ACTION
			/* This will check any resticts that have been delayed to the end of the statement. */
			if (thread->st_restrict_list.bl_count) {
				if (!xt_tab_restrict_rows(&thread->st_restrict_list, thread)) {
					ok = FALSE;
					status = XT_LOG_ENT_ABORT;
				}
			}
#endif

			/* Flush the data log: */
			if (!thread->st_dlog_buf.dlb_flush_log(TRUE, thread)) {
				ok = FALSE;
				status = XT_LOG_ENT_ABORT;
			}

			/* Write and flush the transaction log: */
			if (!xt_xlog_log_data(thread, sizeof(XTXactEndEntryDRec), (XTXactLogBufferDPtr) &entry, TRUE)) {
				ok = FALSE;
				status = XT_LOG_ENT_ABORT;
				/* Make sure this is done, if we failed to log
				 * the transction end!
				 */
				if (thread->st_xact_writer) {
					/* Adjust this in case of error, but don't forget
					 * to lock!
					 */
					xt_spinlock_lock(&db->db_xlog.xl_buffer_lock);
					db->db_xn_writer_count--;
					thread->st_xact_writer = FALSE;
					if (thread->st_xact_long_running) {
						db->db_xn_long_running_count--;
						thread->st_xact_long_running = FALSE;
					}
					xt_spinlock_unlock(&db->db_xlog.xl_buffer_lock);
				}
			}

			/* Setting this flag completes the transaction,
			 * Do this before we release the locks, because
			 * the unlocked transactions expect the
			 * transaction they are waiting for to be
			 * gone!
			 */
			xact->xd_end_time = ++db->db_xn_end_time;
			if (status == XT_LOG_ENT_COMMIT) {
				thread->st_statistics.st_commits++;
				xact->xd_flags |= (XT_XN_XAC_COMMITTED | XT_XN_XAC_ENDED);
			}
			else {
				thread->st_statistics.st_rollbacks++;
				xact->xd_flags |= XT_XN_XAC_ENDED;
			}

			/* {REMOVE-LOCKS} Drop locks is you have any: */
			thread->st_lock_list.xt_remove_all_locks(db, thread);

			/* Do this afterwards to make sure the sweeper
			 * does not cleanup transactions start cleaning up
			 * before any transactions that were waiting for
			 * this transaction have completed!
			 */
			xact->xd_end_xn_id = db->db_xn_curr_id;

			/* Now you can sweep! */
			xact->xd_flags |= XT_XN_XAC_SWEEP;
		}
		else {
			/* Read-only transaction can be removed, immediately */
			xact->xd_end_time = ++db->db_xn_end_time;
			xact->xd_flags |= (XT_XN_XAC_COMMITTED | XT_XN_XAC_ENDED);

			/* Drop locks is you have any: */
			thread->st_lock_list.xt_remove_all_locks(db, thread);

			xact->xd_end_xn_id = db->db_xn_curr_id;

			xact->xd_flags |= XT_XN_XAC_SWEEP;

			if (xt_xn_delete_xact(db, xn_id, thread)) {
				if (db->db_xn_min_ram_id == xn_id)
					db->db_xn_min_ram_id = xn_id+1;
			}
		}

#ifdef TRACE_TRANSACTION
		if (status == XT_LOG_ENT_COMMIT)
			xt_ttracef(thread, "COMMIT T%lu\n", (u_long) xn_id);
		else
			xt_ttracef(thread, "ABORT T%lu\n", (u_long) xn_id);
#endif

		if (db->db_xn_min_run_id == xn_id)
			db->db_xn_min_run_id = xn_id+1;

		thread->st_xact_data = NULL;

#ifdef XT_TRACK_CONNECTIONS
		xt_track_conn_info[thread->t_id].ci_prev_xact_id = xt_track_conn_info[thread->t_id].ci_curr_xact_id;
		xt_track_conn_info[thread->t_id].ci_prev_xact_time = xt_trace_clock() - xt_track_conn_info[thread->t_id].ci_xact_start;
		xt_track_conn_info[thread->t_id].ci_curr_xact_id = 0;
		xt_track_conn_info[thread->t_id].ci_xact_start = 0;
#endif

		xt_xn_wakeup_waiting_threads(thread);

		/* {WAKE-SW} Waking the sweeper
		 * is no longer unconditional.
		 * (see all comments to {WAKE-SW})
		 *
		 * We now wake the sweeper if it is
		 * supposed to work faster.
		 *
		 * There are now 2 cases:
		 * - We run out of transaction slots.
		 * - We encounter old index entries.
		 *
		 * The following test:
		 * runTest(INCREMENT_TEST, 16, INCREMENT_TEST_UPDATE_COUNT);
		 * has extreme problems with sweeping every 1/10s
		 * because a huge number of index entries accumulate
		 * that need to be cleaned.
		 *
		 * New code detects this case.
		 */
		if (db->db_sw_faster)
			xt_wakeup_sweeper(db);

		/* Don't get too far ahead of the sweeper! */
		if (writer) {
#ifdef XT_WAIT_FOR_CLEANUP
			xtXactID	wait_xn_id;
			
			/* This is the transaction that was committed 3 transactions ago: */
			wait_xn_id = thread->st_prev_xact[thread->st_last_xact];
			thread->st_prev_xact[thread->st_last_xact] = xn_id;
			/* This works because XT_MAX_XACT_BEHIND == 2! */
			ASSERT_NS((thread->st_last_xact + 1) % XT_MAX_XACT_BEHIND == thread->st_last_xact ^ 1);
			thread->st_last_xact ^= 1;
			while (xt_xn_is_before(db->db_xn_to_clean_id, wait_xn_id) && (db->db_sw_faster & XT_SW_TOO_FAR_BEHIND)) {
				xt_critical_wait();
			}
#else
			if ((db->db_sw_faster & XT_SW_TOO_FAR_BEHIND) != 0) {
				xtWord8	then = xt_trace_clock() + (xtWord8) 20000;

				for (;;) {
					xt_critical_wait();
					if (db->db_sw_faster & XT_SW_TOO_FAR_BEHIND)
						break;
					if (xt_trace_clock() >= then)
						break;
				}
			}
#endif
		}
	}
	return ok;
}

xtPublic xtBool xt_xn_commit(XTThreadPtr thread)
{
	return xn_end_xact(thread, XT_LOG_ENT_COMMIT);
}

xtPublic xtBool xt_xn_rollback(XTThreadPtr thread)
{
	return xn_end_xact(thread, XT_LOG_ENT_ABORT);
}

xtPublic xtBool xt_xn_log_tab_id(XTThreadPtr self, xtTableID tab_id)
{
	XTXactNewTabEntryDRec	entry;

	entry.xt_status_1 = XT_LOG_ENT_NEW_TAB;
	entry.xt_checksum_1 = XT_CHECKSUM_1(tab_id);
	XT_SET_DISK_4(entry.xt_tab_id_4, tab_id);
	return xt_xlog_log_data(self, sizeof(XTXactNewTabEntryDRec), (XTXactLogBufferDPtr) &entry, TRUE);
}

xtPublic int xt_xn_status(XTOpenTablePtr ot, xtXactID xn_id, xtRecordID XT_UNUSED(rec_id))
{
	register XTThreadPtr	self = ot->ot_thread;
	int						flags;
	xtWord4					end;

#ifdef DRIZZLED
	/* Conditional waste of time!
	 * Drizzle has strict warnings.
	 * I know this is not necessary!
	 */
	flags = 0;
	end = 0;
#endif
	if (xn_id == self->st_xact_data->xd_start_xn_id)
		return XT_XN_MY_UPDATE;
	if (xt_xn_is_before(xn_id, self->st_database->db_xn_min_ram_id) ||
		!xn_get_xact_details(self->st_database, xn_id, ot->ot_thread, &flags, NULL, &end, NULL)) {
		/* Not in RAM, rollback done: */
//*DBG*/xt_dump_xlogs(self->st_database, 0);
//*DBG*/xt_check_table(self, ot);
//*DBG*/xt_dump_trace();
		/* {XACT-NOT-IN-RAM}
		 * This should never happen (CHANGED see below)!
		 *
		 * Because if the transaction is no longer in RAM, then it has been
		 * cleaned up. So the record should be marked as clean, or not
		 * exist.
		 *
		 * After sweeping, we wait for all transactions to quit that were
		 * running at the time of cleanup before removing the transaction record.
		 * (see {XACT-NOT-IN-RAM})
		 *
		 * If this was not the case, then we could be here because:
		 * - The user transaction (T2) reads record x and notes that the record
		 * has not been cleaned (CLEAN bit not set).
		 *
		 * - The sweeper is busy sweeping the transaction (T1) that created
		 * record x.
		 * The SW sets the CLEAN bit on record x, and the schedules T1 for
		 * deletion.
		 *
		 * Now T1 should not be deleted before T2 quits. If it does happen
		 * then we land up here.
		 *
		 * THIS CAN NOW HAPPEN!
		 *
		 * First of all, a MYSTERY:
		 * This did happen, dispite the description above! The reason why
		 * is left as an exercise to the reader (really, I don't now why!)
		 *
		 * This has force me to add code to handle the situation. This
		 * is done by re-reading the record that is being checked by this
		 * function. After re-reading, the record should either be
		 * invalid (free) or clean (CLEAN bit set).
		 *
		 * If this is the case, then we will not run land up here
		 * again.
		 *
		 * Because we are only here because the record was valid but not
		 * clean (you can confirm this by looking at the code that
		 * calls this function).
		 */
		return XT_XN_REREAD;
	}
	if (!(flags & XT_XN_XAC_ENDED))
		/* Transaction not ended, may be visible. */
		return XT_XN_OTHER_UPDATE;
	/* Visible if the transaction was committed: */
	if (flags & XT_XN_XAC_COMMITTED) {
		if (!xt_xn_is_before(self->st_visible_time, end))  // was self->st_visible_time >= xact->xd_end_time
			return XT_XN_VISIBLE;
		return XT_XN_NOT_VISIBLE;
	}
	return XT_XN_ABORTED;
}

xtPublic xtWord8 xt_xn_bytes_to_sweep(XTDatabaseHPtr db, XTThreadPtr thread)
{
	xtXactID				xn_id;
	xtXactID				curr_xn_id;
	xtLogID					xn_log_id = 0;
	xtLogOffset				xn_log_offset = 0;
	xtLogID					x_log_id = 0;
	xtLogOffset				x_log_offset = 0;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtWord8					byte_count = 0;

	xn_id = db->db_xn_to_clean_id;
	curr_xn_id = xt_xn_get_curr_id(db);
	// Limit the number of transactions checked!
	for (int i=0; i<1000; i++) {
		if (xt_xn_is_before(curr_xn_id, xn_id))
			break;
		if (xn_get_xact_start(db, xn_id, thread, &x_log_id, &x_log_offset)) {
			if (xn_log_id) {
				if (xt_comp_log_pos(x_log_id, x_log_offset, xn_log_id, xn_log_offset) < 0) {
					xn_log_id = x_log_id;
					xn_log_offset = x_log_offset;
				}
			}
			else {
				xn_log_id = x_log_id;
				x_log_offset = x_log_offset;
			}
		}
		xn_id++;
	}
	if (!xn_log_id)
		return 0;

	/* Assume the logs have the threshold: */
	log_id = db->db_xlog.xl_write_log_id;
	log_offset = db->db_xlog.xl_write_log_offset;
	if (xn_log_id < log_id) {
		if (xn_log_offset < xt_db_log_file_threshold)
			byte_count = (size_t) (xt_db_log_file_threshold - xn_log_offset);
		xn_log_offset = 0;
		xn_log_id++;
	}
	while (xn_log_id < log_id) {
		byte_count += (size_t) xt_db_log_file_threshold;
		xn_log_id++;
	}
	if (xn_log_offset < log_offset)
		byte_count += (size_t) (log_offset - xn_log_offset);

	return byte_count;
}

/* ----------------------------------------------------------------------
 * S W E E P E R    P R O C E S S
 */

typedef struct XNSweeperState {
	XTDatabaseHPtr			ss_db;
	XTXactSeqReadRec		ss_seqread;
	XTDataBufferRec			ss_databuf;
	u_int					ss_call_cnt;
	XTBasicQueueRec			ss_to_free;
	xtBool					ss_flush_pending;
	XTOpenTablePtr			ss_ot;
} XNSweeperStateRec, *XNSweeperStatePtr;

static XTOpenTablePtr xn_sw_get_open_table(XTThreadPtr self, XNSweeperStatePtr ss, xtTableID tab_id, int *r)
{
	if (ss->ss_ot) {
		if (ss->ss_ot->ot_table->tab_id == tab_id)
			return ss->ss_ot;

		xt_db_return_table_to_pool(self, ss->ss_ot);
		ss->ss_ot = NULL;
	}

	if (!ss->ss_ot) {
		if (!(ss->ss_ot = xt_db_open_pool_table(self, ss->ss_db, tab_id, r, TRUE)))
			return NULL;
	}

	return ss->ss_ot;
}

static void xn_sw_close_open_table(XTThreadPtr self, XNSweeperStatePtr ss)
{
	if (ss->ss_ot) {
		xt_db_return_table_to_pool(self, ss->ss_ot);
		ss->ss_ot = NULL;
	}
}

/*
 * A thread can set a bit in db_sw_faster to make
 * the sweeper go faster.
 */
static void xn_sw_could_go_faster(XTThreadPtr self, XTDatabaseHPtr db)
{
	if (db->db_sw_faster) {
		if (!db->db_sw_fast) {
			xt_set_priority(self, xt_db_sweeper_priority+1);
			db->db_sw_fast = TRUE;
		}
	}
}

static void xn_sw_go_slower(XTThreadPtr self, XTDatabaseHPtr db)
{
	if (db->db_sw_fast) {
		xt_set_priority(self, xt_db_sweeper_priority);
		db->db_sw_fast = FALSE;
	}
	db->db_sw_faster = XT_SW_WORK_NORMAL;
}

/* Add a record to the "to free" queue. We note the current
 * transaction at the time this is done. The record will
 * only be freed once this transaction terminated, together
 * with all transactions that started before it! 
 *
 * The reason for this is that a sequential scan or some
 * other operation may read a committed record which is no longer
 * valid because it is no longer the latest variation (the first
 * variation reachable from the row pointer).
 *
 * In this case, the sweeper will free the variation.
 * If the variation is re-used and committed before
 * the sequential scan or read completes, and by some
 * fluke is used by the same record as previously,
 * the system will think the record is valid
 * again.
 *
 * Without re-reading the record the sequential
 * scan or other read will find it on the variation list, and
 * return the record data as if valid!
 *
 * ------------ 2008-01-03
 *
 * An example of this is:
 *
 * Assume we have 3 records.
 * The 3rd record is deleted, and committed.
 * Before cleanup can be performed
 * a sequential scan takes a copy of the records.
 *
 * Now assume a new insert is done before
 * the sequential scan gets to the 3rd record.
 *
 * The insert allocates the 3rd row and 3rd record
 * again.
 *
 * Now, when the sequential scan gets to the old copy of the 3rd record,
 * this is valid because the row points to this record again.
 *
 * HOWEVER! I have now changed the sequential scan so that it accesses
 * the records from the cache, without making a copy.
 *
 * This means that this problem cannot occur because the sequential scan
 * always reads the current data from the cache.
 *
 * There is also no race condition (although no lock is taken), because
 * the record is writen before the row (see here [(5)]).
 *
 * This means that the row does not point to the record before the
 * record has been modified.
 *
 * Once the record has been modified then the sequential scan will see
 * that the record belongs to a new transaction.
 *
 * If the row pointer was set before the record updated then a race
 * condition would exist when the sequential scan reads the record
 * after the insert has updated the row pointer but before it has
 * changed the record.
 *
 * AS A RESULT:
 *
 * I believe I can remove the delayed free record!
 *
 * This means I can combine the REMOVE and FREE operations.
 *
 * This is good because this takes care of the problem
 * that records are lost when:
 *
 * The server crashes when the delayed free list still has items on it.
 * AND
 * The transaction that freed the records has been cleaned, and this
 * fact has been committed to the log.
 *
 * So I have removed the delay here: [(6)]
 *
 * ------------ 2008-12-03
 *
 * This code to delay removal of records was finally removed (see above)
 */

/*
 * As above, but instead a transaction is added to the "to free" queue.
 *
 * It is important that transactions remain in memory until all
 * currently running transactions have ended. This is because
 * sequential and index scans have copies of old data.
 *
 * In the old data a record may not be indicated as cleaned. Such
 * a record is considered invalid if the transaction is not in RAM.
 *
 * GOTCHA:
 *
 * And this problem is demonstrated by the following example
 * which was derived from flush_table.test.
 *
 * Each handler command below is a separate transaction.
 * However the buffer is loaded by 'read first'.
 * Depending on when cleanup occurs, records can disappear
 * in some of the next commands.
 *
 * 2 solutions for the test. Use begin ... commit around
 * handler open ... close. Or use analyze table t1 before
 * open. analyze table waits for the sweeper to complete!
 *
 * create table dummy(table_id char(20) primary key);
 * let $1=100;
 * while ($1)
 * {
 *   drop table if exists t1;
 *   create table t1(table_id char(20) primary key);
 *   insert into t1 values ('Record-01');
 *   insert into t1 values ('Record-02');
 *   insert into t1 values ('Record-03');
 *   insert into t1 values ('Record-04');
 *   insert into t1 values ('Record-05');
 *   handler t1 open;
 *   handler t1 read first limit 1;
 *   handler t1 read next limit 1;
 *   handler t1 read next limit 1;
 *   handler t1 read next limit 1;
 *   handler t1 close;
 *   commit;
 *   dec $1;
 * }
 * 
 */
#ifdef MUST_DELAY_REMOVE
static void xn_sw_add_xact_to_free(XTThreadPtr self, XNSweeperStatePtr ss, xtXactID xn_id)
{
	XNSWToFreeItemRec free_item;

	if ((ss->ss_to_free.bq_front - ss->ss_to_free.bq_back) >= XT_TN_MAX_TO_FREE) {
		/* If the queue is full, try to free some items:
		 * We use the call count to avoid doing this every time,
		 * when the queue overflows!
		 */
		if ((ss->ss_call_cnt % XT_TN_MAX_TO_FREE_CHECK) == 0)
			/* GOTCHA: This call was not locking the sweeper,
			 * this could cause failure, of course:
			 */
			xn_sw_service_to_free(self, ss, TRUE);
		ss->ss_call_cnt++;
	}

	free_item.ri_wait_xn_id = ss->ss_db->db_xn_curr_id;
	free_item.ri_tab_id = 0;
	free_item.x.ri_xn_id = xn_id;

	xt_bq_add(self, &ss->ss_to_free, &free_item);
}
#endif

static void xt_sw_delete_variations(XTThreadPtr self, XNSweeperStatePtr ss, XTOpenTablePtr ot, xtRecordID rec_id, xtRowID row_id, xtXactID xn_id)
{
	xtRecordID prev_var_rec_id;

	while (rec_id) {
		switch (xt_tab_remove_record(ot, rec_id, ss->ss_databuf.db_data, &prev_var_rec_id, FALSE, row_id, xn_id)) {
			case XT_ERR:
				throw_();
				return;
			case TRUE:
				break;
		}
		rec_id = prev_var_rec_id;
	}
}

static void xt_sw_delete_variation(XTThreadPtr self, XNSweeperStatePtr ss, XTOpenTablePtr ot, xtRecordID rec_id, xtBool clean_delete, xtRowID row_id, xtXactID xn_id)
{
	xtRecordID prev_var_rec_id;

	switch (xt_tab_remove_record(ot, rec_id, ss->ss_databuf.db_data, &prev_var_rec_id, clean_delete, row_id, xn_id)) {
		case XT_ERR:
			throw_();
			return;
		case TRUE:
			break;
		case FALSE:
			break;
	}
}

/* Set rec_type to this value in order to force cleanup, without
 * a check.
 */
#define XN_FORCE_CLEANUP		XT_TAB_STATUS_FREED

/*
 * Read the record to be cleaned. Return TRUE if the cleanup has already been done.
 */
static xtBool xn_sw_cleanup_done(XTThreadPtr self, XTOpenTablePtr ot, xtRecordID rec_id, xtXactID xn_id, u_int rec_type, u_int stat_id, xtRowID row_id, XTTabRecHeadDPtr rec_head)
{
	if (!xt_tab_get_rec_data(ot, rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) rec_head))
		throw_();

	if (rec_type == XN_FORCE_CLEANUP) {
		if (XT_REC_IS_FREE(rec_head->tr_rec_type_1))
			return TRUE;
	}
	else {
		/* Transaction must match: */
		if (XT_GET_DISK_4(rec_head->tr_xact_id_4) != xn_id)
			return TRUE;

		/* Record header must match expected value from
		 * log or clean has been done, or is not required.
		 *
		 * For example, it is not required if a record
		 * has been overwritten in a transaction.
		 */
		if (rec_head->tr_rec_type_1 != rec_type ||
			rec_head->tr_stat_id_1 != stat_id)
			return TRUE;

		/* Row must match: */
		if (XT_GET_DISK_4(rec_head->tr_row_id_4) != row_id)
			return TRUE;
	}

	return FALSE;
}

static void xn_sw_clean_indices(XTThreadPtr XT_NDEBUG_UNUSED(self), XTOpenTablePtr ot, xtRecordID rec_id, xtRowID row_id, xtWord1 *rec_data, xtWord1 *rec_buffer)
{
	XTTableHPtr	tab = ot->ot_table;
	u_int		cols_req;
	XTIndexPtr	*ind;

	if (!tab->tab_dic.dic_key_count)
		return;

	cols_req = tab->tab_dic.dic_ind_cols_req;
	if (XT_REC_IS_FIXED(rec_data[0]))
		rec_buffer = rec_data + XT_REC_FIX_HEADER_SIZE;
	else {
		if (XT_REC_IS_VARIABLE(rec_data[0])) {
			if (!myxt_load_row(ot, rec_data + XT_REC_FIX_HEADER_SIZE, rec_buffer, cols_req))
				goto failed;
		}
		else if (XT_REC_IS_EXT_DLOG(rec_data[0])) {
			ASSERT(cols_req);
			if (cols_req && cols_req <= tab->tab_dic.dic_fix_col_count) {
				if (!myxt_load_row(ot, rec_data + XT_REC_EXT_HEADER_SIZE, rec_buffer, cols_req))
					goto failed;
			}
			else {
				if (rec_data != ot->ot_row_rbuffer)
					memcpy(ot->ot_row_rbuffer, rec_data, tab->tab_dic.dic_rec_size);
				if (!xt_tab_load_ext_data(ot, rec_id, rec_buffer, cols_req))
					goto failed;
			}
		}
		else
			/* This is possible, the record has already been cleaned up. */
			return;
	}

	ind = tab->tab_dic.dic_keys;
	for (u_int i=0; i<tab->tab_dic.dic_key_count; i++, ind++) {
		if (!xt_idx_update_row_id(ot, *ind, rec_id, row_id, rec_buffer))
			xt_log_and_clear_exception_ns();
	}
	return;
	
	failed:
	xt_log_and_clear_exception_ns();
}

/*
 * Return TRUE if the cleanup was done. FAILED if cleanup could not be done
 * because dictionary information is not available.
 */
static xtBool xn_sw_cleanup_variation(XTThreadPtr self, XNSweeperStatePtr ss, XTXactDataPtr xact, xtTableID tab_id, xtRecordID rec_id, u_int status, u_int rec_type, u_int stat_id, xtRowID row_id, xtWord1 *rec_buf)
{
	XTOpenTablePtr		ot;
	XTTableHPtr			tab;
	XTTabRecHeadDRec	rec_head;
	xtRecordID			after_rec_id;
	xtXactID			xn_id;
	int					r;

	if (!(ot = xn_sw_get_open_table(self, ss, tab_id, &r))) {
		/* The table no longer exists, consider cleanup done: */
		switch (r) {
			case XT_TAB_NOT_FOUND:
				break;
			case XT_TAB_NO_DICTIONARY:
			case XT_TAB_POOL_CLOSED:
				return FALSE;
		}
		return TRUE;
	}

	tab = ot->ot_table;

	/* Make sure the buffer is large enough! */
	xt_db_set_size(self, &ss->ss_databuf, (size_t) tab->tab_dic.dic_mysql_buf_size);

	xn_id = xact->xd_start_xn_id;
	if (xact->xd_flags & XT_XN_XAC_COMMITTED) {
		/* The transaction has been committed. Clean the record and
		 * remove variations no longer in use.
		 */
		switch (status) {
			case XT_LOG_ENT_REC_MODIFIED:
			case XT_LOG_ENT_UPDATE:
			case XT_LOG_ENT_UPDATE_FL:
			case XT_LOG_ENT_UPDATE_BG:
			case XT_LOG_ENT_UPDATE_FL_BG:
				if (xn_sw_cleanup_done(self, ot, rec_id, xn_id, rec_type, stat_id, row_id, &rec_head))
					goto done_ok;
				after_rec_id = XT_GET_DISK_4(rec_head.tr_prev_rec_id_4);
				xt_sw_delete_variations(self, ss, ot, after_rec_id, row_id, xn_id);
				rec_head.tr_rec_type_1 |= XT_TAB_STATUS_CLEANED_BIT;
				XT_SET_NULL_DISK_4(rec_head.tr_prev_rec_id_4);
				if (!xt_tab_put_log_op_rec_data(ot, XT_LOG_ENT_REC_CLEANED, 0, rec_id, offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE, (xtWord1 *) &rec_head))
					throw_();
				xn_sw_clean_indices(self, ot, rec_id, row_id, rec_buf, ss->ss_databuf.db_data);
				break;
			case XT_LOG_ENT_INSERT:
			case XT_LOG_ENT_INSERT_FL:
			case XT_LOG_ENT_INSERT_BG:
			case XT_LOG_ENT_INSERT_FL_BG: {
				/* POTENTIAL BUG 1:
				 *
				 * DROP TABLE IF EXISTS t1;
				 * CREATE TABLE t1 ( id int, name varchar(300)) engine=pbxt;
				 * 
				 * begin;
				 * insert t1(id, name) values(1, "aaa");
				 * update t1 set name=REPEAT('A', 300) where id = 1;
				 * commit;
				 * flush tables;
				 * select * from t1;
				 *
				 * Because the type of record changes, from VARIABLE to
				 * EXTENDED, the cleanup needs to take this into account.
				 *
				 * The input new status value which is written here
				 * depends on the first write to the record.
				 * However, the second write changes the record status.
				 *
				 * Previously we used a OR function to write the bit and
				 * return the byte value of the result.
				 *
				 * The write funtion now checks the record to be written
				 * to make sure it matches the record that needs to be
				 * cleaned. So OR'ing the bit is no longer required.
				 *
				 * POTENTIAL BUG 2:
				 *
				 * We have changed this to fix the following bug:
				 *
				 * T1 starts
				 * T2 starts
				 * T2 insert record 100 in row 50
				 * T2 commits
				 * T1 updates row 50 and adds record 101
				 *
				 * The sweeper does cleanup in order T1, T2, ...
				 *
				 * The sweeper cleans T1 by removing record 100 from the 
				 * row 50 variation list.
				 * This means that record 100 is free.
				 *
				 * The sweeper cleans T2 by marking record 100 as clean.
				 * !BUG! record 100 has already been freed!
				 *
				 * To avoid this we have to check a record before 
				 * cleaning (as we do above for update in xn_sw_cleanup_done())
				 * We check that the record is, in fact, the exact
				 * record that was inserted.
				 *
				 * This is now done be xt_tc_write_cond().
				 */
				xtOpSeqNo op_seq;

				rec_head.tr_rec_type_1 = rec_type | XT_TAB_STATUS_CLEANED_BIT;
				if(!tab->tab_recs.xt_tc_write_cond(self, ot->ot_rec_file, rec_id, rec_head.tr_rec_type_1, &op_seq, xn_id, row_id, stat_id, rec_type))
					/* this means record was not updated by xt_tc_write_bor and doesn't need to */
					break;
				if (!xt_xlog_modify_table(ot, XT_LOG_ENT_REC_CLEANED_1, op_seq, 0, rec_id, 1, &rec_head.tr_rec_type_1))
					throw_();
				xn_sw_clean_indices(self, ot, rec_id, row_id, rec_buf, ss->ss_databuf.db_data);
				break;
			}
			case XT_LOG_ENT_DELETE:
			case XT_LOG_ENT_DELETE_FL:
			case XT_LOG_ENT_DELETE_BG:
			case XT_LOG_ENT_DELETE_FL_BG:
				if (xn_sw_cleanup_done(self, ot, rec_id, xn_id, rec_type, stat_id, row_id, &rec_head))
					goto done_ok;
				after_rec_id = XT_GET_DISK_4(rec_head.tr_prev_rec_id_4);
				xt_sw_delete_variations(self, ss, ot, after_rec_id, row_id, xn_id);
				xt_sw_delete_variation(self, ss, ot, rec_id, TRUE, row_id, xn_id);
				if (row_id) {
					if (!xt_tab_free_row(ot, tab, row_id))
						throw_();
				}
				break;
		}
	}
	else {
		/* The transaction has been aborted. Remove the variation from the
		 * variation list. If this means the list is empty, then remove
		 * the record as well.
		 */
		xtRecordID			first_rec_id, next_rec_id, prev_rec_id;
		XTTabRecHeadDRec	prev_rec_head;

		if (xn_sw_cleanup_done(self, ot, rec_id, xn_id, rec_type, stat_id, row_id, &rec_head))
			goto done_ok;

		if (!row_id)
			row_id = XT_GET_DISK_4(rec_head.tr_row_id_4);
		after_rec_id = XT_GET_DISK_4(rec_head.tr_prev_rec_id_4);
		if (!row_id)
			goto unlink_done;

		/* Now remove the record from the variation list,
		 * (if it is still on the list).
		 */
		XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], self);

		/* Find the variation before the variation we wish to remove: */
		if (!(xt_tab_get_row(ot, row_id, &first_rec_id)))
			goto failed;
		prev_rec_id = 0;
		next_rec_id = first_rec_id;
		while (next_rec_id != rec_id) {
			if (!next_rec_id) {
				/* The record was not found in the list (we are done) */
				XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], self);
				goto unlink_done;
			}
			if (!xt_tab_get_rec_data(ot, next_rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &prev_rec_head)) {
				xt_log_and_clear_exception(self);
				break;
			}
			prev_rec_id = next_rec_id;
			next_rec_id = XT_GET_DISK_4(prev_rec_head.tr_prev_rec_id_4);
		}

		if (next_rec_id == rec_id) {
			/* The record was found on the list: */
			if (prev_rec_id) {
				/* Unlink the deleted variation:
				 * I have found the following sequence:
				 *
				 * 17933 in use  1906112
				 * 1906112 delete      xact=2901   row=17933 prev=2419240
				 * 2419240 delete      xact=2899   row=17933 prev=2153360
				 * 2153360 record-X C  xact=2599   row=17933 prev=0 Xlog=151 Xoff=16824 Xsiz=100
				 *
				 * Despite the following facts which should prevent chains from
				 * forming:
				 *
				 * --- Only one transaction can modify a row
				 * at any one time. So it is not possible for a new change
				 * to be linked onto an uncommitted change.
				 * 
				 * --- Transactions that modify the same row
				 * twice do not allocate a new record for each change.
				 *
				 * -- A change that has been
				 * rolled back will not be linked onto. Instead
				 * the new transaction will link to the last.
				 * Comitted record.
				 *
				 * So if the sweeper is slow in doing its job
				 * we can have the situation that a number of records
				 * can refer to the last committed record of the
				 * row.
				 *
				 * Only one will be reference by the row pointer.
				 *
				 * The other, will all have been rolled back.
				 * This occurs over here: [(4)]
				 */
				XT_SET_DISK_4(prev_rec_head.tr_prev_rec_id_4, after_rec_id);
				if (!xt_tab_put_log_op_rec_data(ot, XT_LOG_ENT_REC_UNLINKED, 0, prev_rec_id, offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE, (xtWord1 *) &prev_rec_head))
					goto failed;
			}
			else {
				/* Variation to be removed at the front of the list. */
				ASSERT(rec_id == first_rec_id);
				if (after_rec_id) {
					/* Unlink the deleted variation, from the front of the list: */
					if (!xt_tab_set_row(ot, XT_LOG_ENT_ROW_SET, row_id, after_rec_id))
						goto failed;
				}
				else {
					/* No more variations, remove the row: */
					if (!xt_tab_free_row(ot, tab, row_id))
						goto failed;
				}
			}
		}

		XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], self);

		/* Note: even when not found on the row list, the record must still
		 * be freed.
		 *
		 * There might be an exception to this, but there are very definite
		 * cases where this is required, for example when an unreferenced
		 * record is found and added to the clean up list xn_add_cu_record().
		 */

		unlink_done:
		/* Delete the extended record and index entries:
		 *
		 * NOTE! This must be done after we have release the row lock. Because
		 * a thread that does a duplicate check locks the index, and then
		 * check whether a row is valid, and can deadlock with
		 * code that locks a row, then an index!
		 *
		 * However, this should all be OK, because the variation has been removed from the
		 * row variation list at this stage, and now just need to be deleted.
		 */
		xt_sw_delete_variation(self, ss, ot, rec_id, FALSE, row_id, xn_id);
	}

	done_ok:
	return OK;

	failed:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], self);
	throw_();
	return FAILED;
}

/* Go through all updated records of a transaction and cleanup.
 * This means, of the transaction was aborted, then all the variations written
 * by the transaction must be removed.
 * If the transaction was committed then we remove older variations.
 * If a delete was committed this can lead to the row being removed.
 *
 * After a transaction has been cleaned it can be removed from RAM.
 * If this was the last transaction in a log, and the log has reached
 * threshold, and the log is no longer in exclusive use, then the log
 * can be deleted.
 *
 * This function returns OK if the transaction was cleaned up, FALSE
 * if a retry is required. Othersize an error is thrown.
 */
static xtBool xn_sw_cleanup_xact(XTThreadPtr self, XNSweeperStatePtr ss, XTXactDataPtr xact)
{
	XTDatabaseHPtr		db = ss->ss_db;
	XTXactLogBufferDPtr	record;
	xtTableID			tab_id;
	xtRecordID			rec_id;
	xtXactID			xn_id;
	xtRowID				row_id;

	if (!db->db_xlog.xlog_seq_start(&ss->ss_seqread, xact->xd_begin_log, xact->xd_begin_offset, FALSE))
		xt_throw(self);

	for (;;) {
		if (self->t_quit)
			return FAILED;

		xn_sw_could_go_faster(self, db);

		if (!db->db_xlog.xlog_seq_next(&ss->ss_seqread, &record, FALSE, self))
			xt_throw(self);
		if (!record) {
			/* Recovered transactions are considered cleaned when we
			 * reach the end of the transaction log.
			 * This is required, because transactions that do
			 * not have a commit (or rollback) record, because they were
			 * running when the server last went down, will otherwise not
			 * have the cleanup completed!!
			 */
			ASSERT(xact->xd_flags & XT_XN_XAC_RECOVERED);
			if (!(xact->xd_flags & XT_XN_XAC_RECOVERED))
				return FAILED;
			goto cleanup_done;
		}
		switch (record->xh.xh_status_1) {
			case XT_LOG_ENT_NEW_LOG:
				if (!db->db_xlog.xlog_seq_start(&ss->ss_seqread, XT_GET_DISK_4(record->xl.xl_log_id_4), 0, FALSE))
					xt_throw(self);
				break;
			case XT_LOG_ENT_COMMIT:
			case XT_LOG_ENT_ABORT:
				xn_id = XT_GET_DISK_4(record->xe.xe_xact_id_4);
				if (xn_id == xact->xd_start_xn_id)
					goto cleanup_done;
				break;
			case XT_LOG_ENT_REC_MODIFIED:
			case XT_LOG_ENT_UPDATE:
			case XT_LOG_ENT_INSERT:
			case XT_LOG_ENT_DELETE:
			case XT_LOG_ENT_UPDATE_BG:
			case XT_LOG_ENT_INSERT_BG:
			case XT_LOG_ENT_DELETE_BG:
				xn_id = XT_GET_DISK_4(record->xu.xu_xact_id_4);
				if (xn_id != xact->xd_start_xn_id)
					break;
				tab_id = XT_GET_DISK_4(record->xu.xu_tab_id_4);
				rec_id = XT_GET_DISK_4(record->xu.xu_rec_id_4);
				row_id = XT_GET_DISK_4(record->xu.xu_row_id_4);
				if (!xn_sw_cleanup_variation(self, ss, xact, tab_id, rec_id, record->xu.xu_status_1, record->xu.xu_rec_type_1, record->xu.xu_stat_id_1, row_id, &record->xu.xu_rec_type_1))
					return FAILED;
				break;
			case XT_LOG_ENT_UPDATE_FL:
			case XT_LOG_ENT_INSERT_FL:
			case XT_LOG_ENT_DELETE_FL:
			case XT_LOG_ENT_UPDATE_FL_BG:
			case XT_LOG_ENT_INSERT_FL_BG:
			case XT_LOG_ENT_DELETE_FL_BG:
				xn_id = XT_GET_DISK_4(record->xf.xf_xact_id_4);
				if (xn_id != xact->xd_start_xn_id)
					break;
				tab_id = XT_GET_DISK_4(record->xf.xf_tab_id_4);
				rec_id = XT_GET_DISK_4(record->xf.xf_rec_id_4);
				row_id = XT_GET_DISK_4(record->xf.xf_row_id_4);
				if (!xn_sw_cleanup_variation(self, ss, xact, tab_id, rec_id, record->xf.xf_status_1, record->xf.xf_rec_type_1, record->xf.xf_stat_id_1, row_id, &record->xf.xf_rec_type_1))
					return FAILED;
				break;
			default:
				break;
		}
	}

	cleanup_done:
	/* Write the log to indicate the transaction has been cleaned: */
	XTXactCleanupEntryDRec cu;

	cu.xc_status_1 = XT_LOG_ENT_CLEANUP;
	cu.xc_checksum_1 = XT_CHECKSUM_1(XT_CHECKSUM4_XACT(xact->xd_start_xn_id));
	XT_SET_DISK_4(cu.xc_xact_id_4, xact->xd_start_xn_id);

	if (!xt_xlog_log_data(self, sizeof(XTXactCleanupEntryDRec), (XTXactLogBufferDPtr) &cu, FALSE))
		return FAILED;

	ss->ss_flush_pending = TRUE;

	xact->xd_flags |= XT_XN_XAC_CLEANED;
	ASSERT(db->db_xn_to_clean_id == xact->xd_start_xn_id);
#ifdef MUST_DELAY_REMOVE
	xn_sw_add_xact_to_free(self, ss, xact->xd_start_xn_id);
#else
	xn_id = xact->xd_start_xn_id;
	if (xt_xn_delete_xact(db, xn_id, self)) {
		/* Recalculate the minimum memory transaction: */
		ASSERT(!xt_xn_is_before(xn_id, db->db_xn_min_ram_id));
		
		if (db->db_xn_min_ram_id == xn_id) {
			db->db_xn_min_ram_id = xn_id+1;
		}
		else {
			xtXactID xn_curr_xn_id = xt_xn_get_curr_id(db);

			while (!xt_xn_is_before(xn_curr_xn_id, db->db_xn_min_ram_id)) { // was db->db_xn_min_ram_id <= xn_curr_xn_id
				/* db_xn_min_ram_id may be changed, by some other process! */
				xn_id = db->db_xn_min_ram_id;
				if (xn_get_xact_details(db, xn_id, self, NULL, NULL, NULL, NULL))
					break;
				db->db_xn_min_ram_id = xn_id+1;
			}
		}
	}
#endif

	return OK;
}

static void xn_free_sw_state(XTThreadPtr self, XNSweeperStatePtr ss)
{
	xn_sw_close_open_table(self, ss);
	if (ss->ss_db)
		ss->ss_db->db_xlog.xlog_seq_exit(&ss->ss_seqread);
	xt_db_set_size(self, &ss->ss_databuf, 0);
	xt_bq_set_size(self, &ss->ss_to_free, 0);
}

static void xn_sw_main(XTThreadPtr self)
{
	XTDatabaseHPtr		db = self->st_database;
	XNSweeperStatePtr	ss;
	XTXactDataPtr		xact, xact2;
	time_t				idle_start = 0;
	xtXactID			curr_id;

	xt_set_priority(self, xt_db_sweeper_priority);

	alloczr_(ss, xn_free_sw_state, sizeof(XNSweeperStateRec), XNSweeperStatePtr);
	ss->ss_db = db;

	if (!db->db_xlog.xlog_seq_init(&ss->ss_seqread, xt_db_log_buffer_size, FALSE))
		xt_throw(self);

	ss->ss_to_free.bq_item_size = sizeof(XNSWToFreeItemRec);
	ss->ss_to_free.bq_max_waste = XT_TN_MAX_TO_FREE_WASTE;
	ss->ss_to_free.bq_item_inc = XT_TN_MAX_TO_FREE_INC;
	ss->ss_call_cnt = 0;
	ss->ss_flush_pending = FALSE;

	while (!self->t_quit) {
		while (!self->t_quit) {
			/* We are just about to check the condition for sleeping,
			 * so if the condition for sleeping holds, then we wil
			 * exit the loop and sleep.
			 *
			 * We will then sleep if nobody sets the flag before we
			 * actually do sleep!
			 */
			curr_id = xt_xn_get_curr_id(db);
			if (xt_xn_is_before(curr_id, db->db_xn_to_clean_id)) {
				db->db_sw_faster &= ~XT_SW_TOO_FAR_BEHIND;
				break;
			}
			/* {TUNING} How far to we allow the sweeper to get behind?
			 * The higher this is, the higher burst performance can
			 * be. But too high and the sweeper falls out of reading the
			 * transaction log cache, and also starts to spread
			 * changes around in index and data blocks that are no
			 * longer hot.
			 */
			if (curr_id - db->db_xn_to_clean_id > 250)
				db->db_sw_faster |= XT_SW_TOO_FAR_BEHIND;
			else
				db->db_sw_faster &= ~XT_SW_TOO_FAR_BEHIND;
			xn_sw_could_go_faster(self, db);
			idle_start = 0;

			if ((xact = xt_xn_get_xact(db, db->db_xn_to_clean_id, self))) {
				xtXactID xn_id;

				if (!(xact->xd_flags & XT_XN_XAC_SWEEP))
					/* Transaction has not yet ending, and ready to sweep. */
					goto sleep;

				/* Check if we can cleanup the transaction.
				 * We do this by checking to see if there is any running
				 * transaction which start before the end of this transaction.
				 */
				xn_id = xact->xd_start_xn_id;
				while (xt_xn_is_before(xn_id, xact->xd_end_xn_id)) {
					xn_id++;
					if ((xact2 = xt_xn_get_xact(db, xn_id, self))) {
						if (!(xact2->xd_flags & XT_XN_XAC_ENDED)) {
							/* A transaction was started before the end of
							 * the transaction we wish to sweep, and this
							 * transaction has not committed, the we have to
							 * wait.
							 */
							db->db_stat_sweep_waits++;
							goto sleep;
						}
					}
				}
				
				/* Can cleanup the transaction, and move to the next. */
				if (xact->xd_flags & XT_XN_XAC_LOGGED) {
#ifdef TRACE_SWEEPER_ACTIVITY
					printf("SWEEPER: cleanup %d\n", (int) xact->xd_start_xn_id);
#endif
					if (!xn_sw_cleanup_xact(self, ss, xact)) {
						/* We failed to clean (try again later)... */
#ifdef TRACE_SWEEPER_ACTIVITY
						printf("SWEEPER: cleanup retry...\n", (int) xact->xd_start_xn_id);
#endif
						goto sleep;
					}
#ifdef TRACE_SWEEPER_ACTIVITY
					printf("SWEEPER: cleanup DONE\n", (int) xact->xd_start_xn_id);
#endif
				}
				else {
					/* This was a read-only transaction, it is safe to
					 * just remove the transaction structure from memory.
					 * (should not be necessary because RO transactions
					 * do this themselves):
					 */
					if (xt_xn_delete_xact(db, db->db_xn_to_clean_id, self)) {
						if (db->db_xn_min_ram_id == db->db_xn_to_clean_id)
							db->db_xn_min_ram_id = db->db_xn_to_clean_id+1;
					}
				}
			}
			
			/* Move on to clean the next: */
			db->db_xn_to_clean_id++;
		}

		sleep:			

		xn_sw_close_open_table(self, ss);

		xn_sw_go_slower(self, db);

		/* Shrink the free list, if it is empty, and larger then
		 * the default:
		 */
		if (ss->ss_to_free.bq_size > XT_TN_MAX_TO_FREE) {
			if (ss->ss_to_free.bq_front == 0 && ss->ss_to_free.bq_back == 0)
				xt_bq_set_size(self, &ss->ss_to_free, XT_TN_MAX_TO_FREE);
		}

		/* Windows: close the log file that we have open for reading, if we
		 * read past the end of the log on the last transaction.
		 * This makes sure that the log is closed when the checkpointer
		 * tries to remove or rename it!!
		 */
		if (ss->ss_seqread.xseq_log_file) {
			if (ss->ss_seqread.xseq_rec_log_id != ss->ss_seqread.xseq_log_id)
				db->db_xlog.xlog_seq_close(&ss->ss_seqread);
		}

		if (ss->ss_flush_pending) {
			/* Flush pending means we have written something to the log.
			 *
			 * if so we flush the log so that the writer will also do
			 * its work!
			 *
			 * This will lead to the freeer continuing if it is waiting.
			 */

			time_t now = time(NULL);
			if (idle_start) {
				/* By default, we wait for 2 seconds idle time, the
				 * we flush the log.
				 */
				if (now >= idle_start + 2) {
					if (!xt_xlog_flush_log(self))
						xt_throw(self);
					ss->ss_flush_pending = FALSE;
				}
			}
			else
				idle_start = now;
		}

		/* {WAKE-SW} Waking up the sweeper is very expensive!
		 * Cost is 3% of execution time on the test:
		 * runTest(SMALL_SELECT_TEST, 2, 100000)
		 *
		 * On the other hand, polling every 1/10 second
		 * is cheap, because the check for transactions
		 * ready for cleanup is very quick.
		 *
		 * So this is the prefered method.
		 */
		xn_sw_wait_for_xact(self, db, 10);
	}

	if (ss->ss_flush_pending) {
		xt_xlog_flush_log(self);
		ss->ss_flush_pending = FALSE;
	}

	freer_(); // xn_free_sw_state(ss)
}

static void *xn_sw_run_thread(XTThreadPtr self)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) self->t_data;
	int				count;
	void			*mysql_thread;

	mysql_thread = myxt_create_thread();

	while (!self->t_quit) {
		try_(a) {
			/*
			 * The garbage collector requires that the database
			 * is in use because.
			 */
			xt_use_database(self, db, XT_FOR_SWEEPER);

			/* This action is both safe and required:
			 *
			 * safe: releasing the database is safe because as
			 * long as this thread is running the database
			 * reference is valid, and this reference cannot
			 * be the only one to the database because
			 * otherwize this thread would not be running.
			 *
			 * required: releasing the database is necessary
			 * otherwise we cannot close the database
			 * correctly because we only shutdown this
			 * thread when the database is closed and we
			 * only close the database when all references
			 * are removed.
			 */
			xt_heap_release(self, self->st_database);

			xn_sw_main(self);
		}
		catch_(a) {
			/* This error is "normal"! */
			if (self->t_exception.e_xt_err != XT_ERR_NO_DICTIONARY &&
				!(self->t_exception.e_xt_err == XT_SIGNAL_CAUGHT &&
				self->t_exception.e_sys_err == SIGTERM))
				xt_log_and_clear_exception(self);
		}
		cont_(a);

		/* Avoid releasing the database (done above) */
		self->st_database = NULL;
		xt_unuse_database(self, self);

		/* After an exception, pause before trying again... */
		/* Number of seconds */
#ifdef DEBUG
		count = 10;
#else
		count = 2*60;
#endif
		db->db_sw_idle = XT_THREAD_INERR;
		while (!self->t_quit && count > 0) {
			sleep(1);
			count--;
		}
		db->db_sw_idle = XT_THREAD_BUSY;
	}

	myxt_destroy_thread(mysql_thread, TRUE);
	return NULL;
}

static void xn_sw_free_thread(XTThreadPtr self, void *data)
{
	XTDatabaseHPtr db = (XTDatabaseHPtr) data;

	if (db->db_sw_thread) {
		xt_lock_mutex(self, &db->db_sw_lock);
		pushr_(xt_unlock_mutex, &db->db_sw_lock);
		db->db_sw_thread = NULL;
		freer_(); // xt_unlock_mutex(&db->db_sw_lock)
	}
}

/* Wait for a transaction to quit: */
static void xn_sw_wait_for_xact(XTThreadPtr self, XTDatabaseHPtr db, u_int hsecs)
{
	xt_lock_mutex(self, &db->db_sw_lock);
	pushr_(xt_unlock_mutex, &db->db_sw_lock);
	db->db_sw_idle = XT_THREAD_IDLE;
	if (!self->t_quit && !db->db_sw_faster)
		xt_timed_wait_cond(self, &db->db_sw_cond, &db->db_sw_lock, hsecs * 10);
	db->db_sw_idle = XT_THREAD_BUSY;
	db->db_sw_check_count++;
	freer_(); // xt_unlock_mutex(&db->db_sw_lock)
}

xtPublic void xt_start_sweeper(XTThreadPtr self, XTDatabaseHPtr db)
{
	char name[PATH_MAX];

	sprintf(name, "SW-%s", xt_last_directory_of_path(db->db_main_path));
	xt_remove_dir_char(name);
	db->db_sw_thread = xt_create_daemon(self, name);
	xt_set_thread_data(db->db_sw_thread, db, xn_sw_free_thread);
	xt_run_thread(self, db->db_sw_thread, xn_sw_run_thread);
}

xtPublic void xt_wait_for_sweeper(XTThreadPtr self, XTDatabaseHPtr db, int abort_time)
{
	time_t	then, now;
	xtBool	message = FALSE;

	if (db->db_sw_thread) {
		then = time(NULL);
		/* Changed xt_xn_get_curr_id(db) to db->db_xn_curr_id,
		 * This should work because we are not concerned about the difference
		 * between xt_xn_get_curr_id(db) and db->db_xn_curr_id,
		 * Which is just a matter of when transactions we can expect ot find
		 * in memory (see {GAP-INC-ADD-XACT})
		 */
		while (!xt_xn_is_before(db->db_xn_curr_id, db->db_xn_to_clean_id)) { // was db->db_xn_to_clean_id <= xt_xn_get_curr_id(db)
			xt_lock_mutex(self, &db->db_sw_lock);
			pushr_(xt_unlock_mutex, &db->db_sw_lock);
			xt_wakeup_sweeper(db);
			freer_(); // xt_unlock_mutex(&db->db_sw_lock)
			xt_sleep_milli_second(10);
			now = time(NULL);
			if (abort_time && now >= then + abort_time) {
				xt_logf(XT_NT_INFO, "Aborting wait for '%s' sweeper\n", db->db_name);
				message = FALSE;
				break;
			}
			if (now >= then + 2) {
				if (!message) {
					message = TRUE;
					xt_logf(XT_NT_INFO, "Waiting for '%s' sweeper...\n", db->db_name);
				}
			}
		}

		if (message)
			xt_logf(XT_NT_INFO, "Sweeper '%s' done.\n", db->db_name);
	}
}

xtPublic void xt_stop_sweeper(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTThreadPtr thr_sw;

	if (db->db_sw_thread) {
		xt_lock_mutex(self, &db->db_sw_lock);
		pushr_(xt_unlock_mutex, &db->db_sw_lock);

		/* This pointer is safe as long as you have the transaction lock. */
		if ((thr_sw = db->db_sw_thread)) {
			xtThreadID tid = thr_sw->t_id;

			/* Make sure the thread quits when woken up. */
			xt_terminate_thread(self, thr_sw);

			xt_wakeup_sweeper(db);
	
			freer_(); // xt_unlock_mutex(&db->db_sw_lock)

			/*
			 * GOTCHA: This is a wierd thing but the SIGTERM directed
			 * at a particular thread (in this case the sweeper) was
			 * being caught by a different thread and killing the server
			 * sometimes. Disconcerting.
			 * (this may only be a problem on Mac OS X)
			xt_kill_thread(thread);
			 */
			xt_wait_for_thread(tid, FALSE);
	
			/* PMC - This should not be necessary to set the signal here, but in the
			 * debugger the handler is not called!!?
			thr_sw->t_delayed_signal = SIGTERM;
			xt_kill_thread(thread);
			 */
			db->db_sw_thread = NULL;
		}
		else
			freer_(); // xt_unlock_mutex(&db->db_sw_lock)
	}
}

xtPublic void xt_wakeup_sweeper(XTDatabaseHPtr db)
{
	/* This flag makes the gap for the race condition
	 * very small.
	 *
	 * However, this posibility still remains because
	 * we do not lock the mutex db_sw_lock here.
	 *
	 * The reason is that it is too expensive.
	 *
	 * In the event that the wakeup is missed the sleeper
	 * wait will timeout eventually.
	 */
	if (db->db_sw_idle) {
		if (!xt_broadcast_cond_ns(&db->db_sw_cond))
			xt_log_and_clear_exception_ns();
	}
}
