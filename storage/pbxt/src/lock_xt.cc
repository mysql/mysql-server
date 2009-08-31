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
 * 2008-01-24	Paul McCullagh
 *
 * Row lock functions.
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <stdio.h>

#include "lock_xt.h"
#include "thread_xt.h"
#include "table_xt.h"
#include "xaction_xt.h"
#include "database_xt.h"
#include "trace_xt.h"

#ifdef DEBUG
//#define XT_TRACE_LOCKS
//#define CHECK_ROWLOCK_GROUP_CONSISTENCY
#endif

/*
 * This function should never be called. It indicates a link
 * error!
 */
xtPublic void xt_log_atomic_error_and_abort(c_char *func, c_char *file, u_int line)
{
	xt_logf(NULL, func, file, line, XT_LOG_ERROR, "%s", "Atomic operations not supported\n");
	abort();
}

/*
 * -----------------------------------------------------------------------
 * ROW LOCKS, LIST BASED
 */
#ifdef XT_USE_LIST_BASED_ROW_LOCKS

#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
/* 
 * Requires a spin-lock on group->lg_lock!
 */
static void check_rowlock_group(XTLockGroupPtr group)
{
	XTThreadPtr self = xt_get_self();

	char *crash = NULL;

	if (group->lg_lock.spl_locker != self)
		*crash = 1;

	if (group->lg_list_in_use > group->lg_list_size)
		*crash = 1;

	xtRowID prev_row = 0;
	XTLockItemPtr item = group->lg_list;

	for (int i = 0; i < group->lg_list_in_use; i++, item++) {

		if (!item->li_thread_id)
			*crash = 1;

		if(!xt_thr_array[item->li_thread_id]->st_xact_data)
			*crash = 1;

		if(item->li_count > XT_TEMP_LOCK_BYTES)
			*crash = 1;

		// rows per thread must obey the row_id > prev_row_id + prev_count*group_size rule
		if (prev_row >= item->li_row_id)
			*crash = 1;

		// calculate the new prev. row
		if (item->li_count < XT_TEMP_LOCK_BYTES)
			prev_row = item->li_row_id + (item->li_count - 1) * XT_ROW_LOCK_GROUP_COUNT;
		else
			prev_row = item->li_row_id;
	}
}
#endif

static int xlock_cmp_row_ids(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtRowID			row_id = *((xtTableID *) a);
	XTLockItemPtr	item = (XTLockItemPtr) b;

	if (row_id < item->li_row_id)
		return -1;
	if (row_id > item->li_row_id)
		return 1;
	return 0;
}

void XTRowLockList::xt_remove_all_locks(struct XTDatabase *, XTThreadPtr thread)
{
#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "remove all locks\n");
#endif
	if (!bl_count)
		return;

	xtThreadID			thd_id;
	XTPermRowLockPtr	plock;
#ifndef XT_USE_TABLE_REF
	XTOpenTablePtr		pot = NULL;
#endif

	thd_id = thread->t_id;
	plock = (XTPermRowLockPtr) bl_data;
	for (u_int i=0; i<bl_count; i++) {
#ifdef XT_USE_TABLE_REF
		XTTableHPtr		tab = plock->pr_table;
#else
		if (!xt_db_open_pool_table_ns(&pot, db, plock->pr_tab_id)) {
			/* Should not happen, but just in case, we just don't
			 * remove the lock. We will probably end up with a deadlock
			 * somewhere.
			 */
			xt_log_and_clear_exception_ns();
		}
		else {
#endif
			for (int j=0; j<XT_ROW_LOCK_GROUP_COUNT; j++) {
				if (plock->pr_group[j]) {
					/* Go through group j and compact. */
#ifndef XT_USE_TABLE_REF
					XTTableHPtr		tab = pot->ot_table;
#endif
					XTLockGroupPtr	group;
					XTLockItemPtr	copy;
					XTLockItemPtr	item;
					int				new_count;

					group = &tab->tab_locks.rl_groups[j];
					xt_spinlock_lock(&group->lg_lock);
					copy = group->lg_list;
					item = group->lg_list;
					new_count = 0;
					for (size_t k=0; k<group->lg_list_in_use; k++) {
						if (item->li_thread_id != thd_id) {
							if (copy != item) {
								copy->li_row_id = item->li_row_id;
								copy->li_count = item->li_count;
								copy->li_thread_id = item->li_thread_id;
							}
							new_count++;
							copy++;
						}
#ifdef XT_TRACE_LOCKS
						else {
							if (item->li_count == XT_TEMP_LOCK_BYTES)
								xt_ttracef(xt_get_self(), "remove group %d lock row_id=%d TEMP\n", j, (int) item->li_row_id);
							else
								xt_ttracef(xt_get_self(), "remove group %d locks row_id=%d (%d)\n", j, (int) item->li_row_id, (int) item->li_count);
						}
#endif
						item++;
					}
					group->lg_list_in_use = new_count;
#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
					check_rowlock_group(group);
#endif
					if (group->lg_wait_queue)
						tab->tab_locks.rl_grant_locks(group, thread);

					xt_spinlock_unlock(&group->lg_lock);
					
					xt_xn_wakeup_thread_list(thread);
				}
			}
#ifdef XT_USE_TABLE_REF
			xt_heap_release(NULL, plock->pr_table);
#else
			xt_db_return_table_to_pool_ns(pot);
		}
#endif
		plock++;
	}
	bl_count = 0;
}

#ifdef DEBUG_LOCK_QUEUE
int *dummy_ptr = 0;

void XTRowLocks::rl_check(XTLockWaitPtr no_lw)
{
	XTLockGroupPtr	group;
	XTLockWaitPtr	lw, lw_prev;

	for (int i=0; i<XT_ROW_LOCK_GROUP_COUNT; i++) {
		group = &rl_groups[i];
		xt_spinlock_lock(&group->lg_lock);

		lw = group->lg_wait_queue;
		lw_prev = NULL;
		while (lw) {
			if (lw == no_lw)
				*dummy_ptr = 1;
			if (lw->lw_prev != lw_prev)
				*dummy_ptr = 2;
			lw_prev = lw;
			lw = lw->lw_next;
		}
		xt_spinlock_unlock(&group->lg_lock);
	}
}
#endif

xtBool XTRowLocks::rl_lock_row(XTLockGroupPtr group, XTLockWaitPtr lw, XTRowLockListPtr, int *result)
{
	XTLockItemPtr	item;
	size_t			index;
	xtRowID			row_id = lw->lw_row_id;

#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
	check_rowlock_group(group);
#endif
	if (group->lg_list_size == group->lg_list_in_use) {
		if (!xt_realloc_ns((void **) &group->lg_list, (group->lg_list_size + 2) * sizeof(XTLockItemRec)))
			return FAILED;
		group->lg_list_size += 2;
	}
	item = (XTLockItemPtr) xt_bsearch(NULL, &row_id, group->lg_list, group->lg_list_in_use, sizeof(XTLockItemRec), &index, NULL, xlock_cmp_row_ids);
	
	/* There's no item with this ID, but there could be an item with a range that covers this row */
	if (!item && group->lg_list_in_use) {
		if (index > 0) {
			int count;
	
			item = group->lg_list + index - 1;

			count = item->li_count;
			if (item->li_count == XT_TEMP_LOCK_BYTES)
				count = 1;

			if (row_id >= item->li_row_id + count * XT_ROW_LOCK_GROUP_COUNT)
				item = NULL;
		}
	}
	
	if (item) {
		/* Item already exists. */
		if (item->li_thread_id == lw->lw_thread->t_id) {
			/* Already have a permanent lock: */
			*result = XT_NO_LOCK;
			lw->lw_curr_lock = XT_NO_LOCK;
			return OK;
		}
		/* {REMOVE-LOCKS}
		 * This must be valid because a thread must remove
		 * the locks before it frees its st_xact_data structure,
		 * xt_thr_array entry must also be valid, because
		 * transaction must be ended before the thread is
		 * killed.
		 */
		*result = item->li_count == XT_TEMP_LOCK_BYTES ? XT_TEMP_LOCK : XT_PERM_LOCK;
		lw->lw_xn_id = xt_thr_array[item->li_thread_id]->st_xact_data->xd_start_xn_id;
		lw->lw_curr_lock = *result;
		return OK;
	}

	/* Add the lock: */
	XT_MEMMOVE(group->lg_list, &group->lg_list[index+1], 
		&group->lg_list[index], (group->lg_list_in_use - index) * sizeof(XTLockItemRec));
	group->lg_list[index].li_row_id = row_id;
	group->lg_list[index].li_count = XT_TEMP_LOCK_BYTES;
	group->lg_list[index].li_thread_id = lw->lw_thread->t_id;
	group->lg_list_in_use++;

#ifdef XT_TRACE_LOCKS
	xt_ttracef(ot->ot_thread, "set temp lock row=%d setby=%s\n", (int) row_id, xt_get_self()->t_name);
#endif
#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
	check_rowlock_group(group);
#endif
	*result = XT_NO_LOCK;
	lw->lw_ot->ot_temp_row_lock = row_id;
	lw->lw_curr_lock = XT_NO_LOCK;
	return OK;
}

void XTRowLocks::rl_grant_locks(XTLockGroupPtr group, XTThreadPtr thread)
{
	XTLockWaitPtr	lw, lw_next, lw_prev;
	int				result;
	xtThreadID		lw_thd_id;

	thread->st_thread_list_count = 0;
	lw = group->lg_wait_queue;
	while (lw) {
		lw_next = lw->lw_next;
		lw_prev = lw->lw_prev;
		lw_thd_id = lw->lw_thread->t_id;
		/* NOTE: after lw_curr_lock is changed, lw may no longer be referenced
		 * by this function!!!
		 */
		if (!rl_lock_row(group, lw, &lw->lw_thread->st_lock_list, &result)) {
			/* We transfer the error to the other thread! */
			XTThreadPtr self = xt_get_self();

			result = XT_LOCK_ERR;
			memcpy(&lw->lw_thread->t_exception, &self->t_exception, sizeof(XTExceptionRec));
			lw->lw_curr_lock = XT_LOCK_ERR;
		}
		if (result == XT_NO_LOCK || result == XT_LOCK_ERR) {
			/* Remove from the wait queue: */
			if (lw_next)
				lw_next->lw_prev = lw_prev;
			if (lw_prev)
				lw_prev->lw_next = lw_next;
			if (group->lg_wait_queue == lw)
				group->lg_wait_queue = lw_next;
			if (group->lg_wait_queue_end == lw)
				group->lg_wait_queue_end = lw_prev;
			if (result == XT_NO_LOCK) {
				/* Add to the thread list: */
				if (thread->st_thread_list_count == thread->st_thread_list_size) {
					if (!xt_realloc_ns((void **) &thread->st_thread_list, (thread->st_thread_list_size+1) * sizeof(xtThreadID))) {
						xt_xn_wakeup_thread(lw_thd_id);
						goto done;
					}
					thread->st_thread_list_size++;
				}
				thread->st_thread_list[thread->st_thread_list_count] = lw_thd_id;
				thread->st_thread_list_count++;
				done:;
			}
		}
		lw = lw_next;
	}
}

void XTRowLocks::xt_cancel_temp_lock(XTLockWaitPtr lw)
{
	XTLockGroupPtr	group;

	group = &rl_groups[lw->lw_row_id % XT_ROW_LOCK_GROUP_COUNT];
	xt_spinlock_lock(&group->lg_lock);
	if (lw->lw_curr_lock == XT_TEMP_LOCK || lw->lw_curr_lock == XT_PERM_LOCK) {
		/* In case of XT_LOCK_ERR or XT_NO_LOCK, the lw structure will
		 * no longer be on the wait queue.
		 */
		XTLockWaitPtr	lw_next, lw_prev;

		lw_next = lw->lw_next;
		lw_prev = lw->lw_prev;

		/* Remove from the wait queue: */
		if (lw_next)
			lw_next->lw_prev = lw_prev;
		if (lw_prev)
			lw_prev->lw_next = lw_next;
		if (group->lg_wait_queue == lw)
			group->lg_wait_queue = lw_next;
		if (group->lg_wait_queue_end == lw)
			group->lg_wait_queue_end = lw_prev;
	}
	xt_spinlock_unlock(&group->lg_lock);
}

//#define QUEUE_ORDER_FIFO

/* Try to lock a row.
 * This function returns:
 * XT_NO_LOCK on success.
 * XT_TEMP_LOCK if there is a temporary lock on the row.
 * XT_PERM_LOCK if there is a permanent lock in the row.
 * XT_FAILED an error occured.
 *
 * If there is a lock on this row, the transaction ID of the
 * locker is also returned.
 *
 * The caller must wait if the row is locked. If the lock is
 * permanent, then the caller must wait for the transaction to
 * terminate. If the lock is temporary, then the caller must
 * wait for the transaction to signal that the lock has been
 * released.
 */
xtBool XTRowLocks::xt_set_temp_lock(XTOpenTablePtr ot, XTLockWaitPtr lw, XTRowLockListPtr lock_list)
{
	XTLockGroupPtr	group;
	int				result;

	if (ot->ot_temp_row_lock) {
		/* Check if we don't already have this temp lock: */
		if (ot->ot_temp_row_lock == lw->lw_row_id) {
			lw->lw_curr_lock = XT_NO_LOCK;
			return OK;
		}

		xt_make_lock_permanent(ot, lock_list);
	}

	/* Add a temporary lock. */
	group = &rl_groups[lw->lw_row_id % XT_ROW_LOCK_GROUP_COUNT];
	xt_spinlock_lock(&group->lg_lock);

	if (!rl_lock_row(group, lw, lock_list, &result)) {
		xt_spinlock_unlock(&group->lg_lock);
		return FAILED;
	}

	if (result != XT_NO_LOCK) {
		/* Add the thread to the end of the thread queue: */
#ifdef QUEUE_ORDER_FIFO
		if (group->lg_wait_queue_end) {
			group->lg_wait_queue_end->lw_next = lw;
			lw->lw_prev = group->lg_wait_queue_end;
		}
		else {
			group->lg_wait_queue = lw;
			lw->lw_prev = NULL;
		}
		lw->lw_next = NULL;
		group->lg_wait_queue_end = lw;
#else
		XTLockWaitPtr	pos = group->lg_wait_queue_end;
		xtXactID		xn_id = ot->ot_thread->st_xact_data->xd_start_xn_id;
		
		while (pos) {
			if (pos->lw_thread->st_xact_data->xd_start_xn_id < xn_id)
				break;
			pos = pos->lw_prev;
		}
		if (pos) {
			lw->lw_prev = pos;
			lw->lw_next = pos->lw_next;
			if (pos->lw_next)
				pos->lw_next->lw_prev = lw;
			else
				group->lg_wait_queue_end = lw;
			pos->lw_next = lw;
		}
		else {
			/* Front of the queue: */
			lw->lw_prev = NULL;
			lw->lw_next = group->lg_wait_queue;
			if (group->lg_wait_queue)
				group->lg_wait_queue->lw_prev = lw;
			else
				group->lg_wait_queue_end = lw;
			group->lg_wait_queue = lw;
		}
#endif
	}

	xt_spinlock_unlock(&group->lg_lock);
	return OK;
}

/*
 * Remove a temporary lock.
 * 
 * If updated is set to TRUE this means that the row was update.
 * This means that any thread waiting on the temporary lock will
 * also have to wait for the transaction to quit before
 * continuing.
 *
 * If the thread were to continue it would just hang again because
 * it will discover that the transaction has updated the row.
 *
 * So the 'updated' flag is an optimisation which prevents the
 * thread from making an unncessary retry.
 */
void XTRowLocks::xt_remove_temp_lock(XTOpenTablePtr ot, xtBool updated)
{
	xtRowID			row_id;
	XTLockGroupPtr	group;
	XTLockItemPtr	item;
	size_t			index;
	xtBool			lock_granted = FALSE;
	xtThreadID		locking_thread_id = 0;

	if (!(row_id = ot->ot_temp_row_lock))
		return;

	group = &rl_groups[row_id % XT_ROW_LOCK_GROUP_COUNT];
	xt_spinlock_lock(&group->lg_lock);
#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
	check_rowlock_group(group);
#endif

#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "remove temp lock %d\n", (int) row_id);
#endif
	item = (XTLockItemPtr) xt_bsearch(NULL, &row_id, group->lg_list, group->lg_list_in_use, sizeof(XTLockItemRec), &index, NULL, xlock_cmp_row_ids);
	if (item) {
		/* Item exists. */
		if (item->li_thread_id == ot->ot_thread->t_id &&
			item->li_count == XT_TEMP_LOCK_BYTES) {
			XTLockWaitPtr	lw;

			/* First check if there is some thread waiting to take over this lock: */
			lw = group->lg_wait_queue;
			while (lw) {
				if (lw->lw_row_id == row_id) {
					lock_granted = TRUE;
					break;
				}
				lw = lw->lw_next;
			}

			if (lock_granted) {
				/* Grant the lock just released... */
				XTLockWaitPtr	lw_next, lw_prev;
				xtXactID		locking_xact_id;

				/* Store this info, lw will soon be untouchable! */
				lw_next = lw->lw_next;
				lw_prev = lw->lw_prev;
				locking_xact_id = lw->lw_thread->st_xact_data->xd_start_xn_id;
				locking_thread_id = lw->lw_thread->t_id;

				/* Lock has moved from one thread to the next.
				 * change the thread holding this lock:
				 */
				item->li_thread_id = locking_thread_id;

				/* Remove from the wait queue: */
				if (lw_next)
					lw_next->lw_prev = lw_prev;
				if (lw_prev)
					lw_prev->lw_next = lw_next;
				if (group->lg_wait_queue == lw)
					group->lg_wait_queue = lw_next;
				if (group->lg_wait_queue_end == lw)
					group->lg_wait_queue_end = lw_prev;

				/* If the thread that release the lock updated the
				 * row then we will have to wait for the transaction
				 * to terminate:
				 */
				if (updated) {
					lw->lw_row_updated = TRUE;
					lw->lw_updating_xn_id = ot->ot_thread->st_xact_data->xd_start_xn_id;
				}

				/* The thread has the lock now: */
				lw->lw_ot->ot_temp_row_lock = row_id;
				lw->lw_curr_lock = XT_NO_LOCK;

				/* Everyone after this that is waiting for the same lock is
				 * now waiting for a different transaction:
				 */
				lw = lw_next;
				while (lw) {
					if (lw->lw_row_id == row_id) {
						lw->lw_xn_id = locking_xact_id;
						lw->lw_curr_lock = XT_TEMP_LOCK;
					}
					lw = lw->lw_next;
				}
			}
			else {
				/* Remove the lock: */
				XT_MEMMOVE(group->lg_list, &group->lg_list[index], 
					&group->lg_list[index+1], (group->lg_list_in_use - index - 1) * sizeof(XTLockItemRec));
				group->lg_list_in_use--;
			}
		}
	}
#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
	check_rowlock_group(group);
#endif
	xt_spinlock_unlock(&group->lg_lock);

	ot->ot_temp_row_lock = 0;
	if (lock_granted)
		xt_xn_wakeup_thread(locking_thread_id);
}

xtBool XTRowLocks::xt_make_lock_permanent(XTOpenTablePtr ot, XTRowLockListPtr lock_list)
{
	xtRowID			row_id;
	XTLockGroupPtr	group;
	XTLockItemPtr	item;
	size_t			index;

	if (!(row_id = ot->ot_temp_row_lock))
		return OK;

#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "make lock perm %d\n", (int) ot->ot_temp_row_lock);
#endif

	/* Add to the lock list: */
	XTPermRowLockPtr locks = (XTPermRowLockPtr) lock_list->bl_data;
	for (unsigned i=0; i<lock_list->bl_count; i++) {
#ifdef XT_USE_TABLE_REF
		if (locks->pr_table == ot->ot_table) {
#else
		if (locks->pr_tab_id == ot->ot_table->tab_id) {
#endif
			locks->pr_group[row_id % XT_ROW_LOCK_GROUP_COUNT] = 1;
			goto done;
		}
		locks++;
	}

	/* Add new to lock list: */
	{
		XTPermRowLockRec perm_lock;
		
#ifdef XT_USE_TABLE_REF
		perm_lock.pr_table = ot->ot_table;
		xt_heap_reference(NULL, perm_lock.pr_table);
#else
		perm_lock.pr_tab_id = ot->ot_table->tab_id;
#endif
		memset(perm_lock.pr_group, 0, XT_ROW_LOCK_GROUP_COUNT);
		perm_lock.pr_group[row_id % XT_ROW_LOCK_GROUP_COUNT] = 1;
		if (!xt_bl_append(NULL, lock_list, &perm_lock)) {
			xt_remove_temp_lock(ot, FALSE);
			return FAILED;
		}
	}

	done:
	group = &rl_groups[row_id % XT_ROW_LOCK_GROUP_COUNT];
	xt_spinlock_lock(&group->lg_lock);

	item = (XTLockItemPtr) xt_bsearch(NULL, &row_id, group->lg_list, group->lg_list_in_use, sizeof(XTLockItemRec), &index, NULL, xlock_cmp_row_ids);
	ASSERT_NS(item);
#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
	check_rowlock_group(group);
#endif
	if (item) {
		/* Lock exists (it should!). */
		if (item->li_thread_id == ot->ot_thread->t_id &&
			item->li_count == XT_TEMP_LOCK_BYTES) {
			if (index > 0 &&
				group->lg_list[index-1].li_thread_id == ot->ot_thread->t_id &&
				group->lg_list[index-1].li_count < XT_TEMP_LOCK_BYTES-2 &&
				group->lg_list[index-1].li_row_id == row_id - (XT_ROW_LOCK_GROUP_COUNT * group->lg_list[index-1].li_count)) {
				group->lg_list[index-1].li_count++;
				/* Combine with the left: */
				if (index + 1 < group->lg_list_in_use &&
					group->lg_list[index+1].li_thread_id == ot->ot_thread->t_id &&
					group->lg_list[index+1].li_count != XT_TEMP_LOCK_BYTES &&
					group->lg_list[index+1].li_row_id == row_id + XT_ROW_LOCK_GROUP_COUNT) {
					/* And combine with the right */
					u_int left = group->lg_list[index-1].li_count + group->lg_list[index+1].li_count;
					u_int right;

					if (left > XT_TEMP_LOCK_BYTES-1) {
						right = left - (XT_TEMP_LOCK_BYTES-1);
						left = XT_TEMP_LOCK_BYTES-1;
					}
					else
						right = 0;

					group->lg_list[index-1].li_count = left;
					if (right) {
						/* There is something left over on the right: */
						group->lg_list[index+1].li_count = right;
						group->lg_list[index+1].li_row_id = group->lg_list[index-1].li_row_id + left * XT_ROW_LOCK_GROUP_COUNT;
						XT_MEMMOVE(group->lg_list, &group->lg_list[index], 
							&group->lg_list[index+1], (group->lg_list_in_use - index - 1) * sizeof(XTLockItemRec));
						group->lg_list_in_use--;
					}
					else {
						XT_MEMMOVE(group->lg_list, &group->lg_list[index], 
							&group->lg_list[index+2], (group->lg_list_in_use - index - 2) * sizeof(XTLockItemRec));
						group->lg_list_in_use -= 2;
					}
				}
				else {
					XT_MEMMOVE(group->lg_list, &group->lg_list[index], 
						&group->lg_list[index+1], (group->lg_list_in_use - index - 1) * sizeof(XTLockItemRec));
					group->lg_list_in_use--;
				}
			}
			else if (index + 1 < group->lg_list_in_use &&
					group->lg_list[index+1].li_thread_id == ot->ot_thread->t_id &&
					group->lg_list[index+1].li_count < XT_TEMP_LOCK_BYTES-2 &&
					group->lg_list[index+1].li_row_id == row_id + XT_ROW_LOCK_GROUP_COUNT) {
				/* Combine with the right: */
				group->lg_list[index+1].li_count++;
				group->lg_list[index+1].li_row_id = row_id;
				XT_MEMMOVE(group->lg_list, &group->lg_list[index], 
					&group->lg_list[index+1], (group->lg_list_in_use - index - 1) * sizeof(XTLockItemRec));
				group->lg_list_in_use--;
			}
			else
				group->lg_list[index].li_count = 1;
		}
	}
#ifdef CHECK_ROWLOCK_GROUP_CONSISTENCY
	check_rowlock_group(group);
#endif
	xt_spinlock_unlock(&group->lg_lock);

	ot->ot_temp_row_lock = 0;
	return OK;
}

xtBool xt_init_row_locks(XTRowLocksPtr rl)
{
	for (int i=0; i<XT_ROW_LOCK_GROUP_COUNT; i++) {
		xt_spinlock_init_with_autoname(NULL, &rl->rl_groups[i].lg_lock);
		rl->rl_groups[i].lg_wait_queue = NULL;
		rl->rl_groups[i].lg_list_size = 0;
		rl->rl_groups[i].lg_list_in_use = 0;
		rl->rl_groups[i].lg_list = NULL;
	}
	return OK;
}

void xt_exit_row_locks(XTRowLocksPtr rl)
{
	for (int i=0; i<XT_ROW_LOCK_GROUP_COUNT; i++) {
		xt_spinlock_free(NULL, &rl->rl_groups[i].lg_lock);
		rl->rl_groups[i].lg_wait_queue = NULL;
		rl->rl_groups[i].lg_list_size = 0;
		rl->rl_groups[i].lg_list_in_use = 0;
		if (rl->rl_groups[i].lg_list) {
			xt_free_ns(rl->rl_groups[i].lg_list);
			rl->rl_groups[i].lg_list = NULL;
		}
	}
}

/*
 * -----------------------------------------------------------------------
 * ROW LOCKS, HASH BASED
 */
#else // XT_USE_LIST_BASED_ROW_LOCKS

void XTRowLockList::old_xt_remove_all_locks(struct XTDatabase *db, xtThreadID thd_id)
{
#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "remove all locks\n");
#endif
	if (!bl_count)
		return;

	int					pgroup;
	xtTableID			ptab_id;
	XTPermRowLockPtr	plock;
	XTOpenTablePtr		pot = NULL;

	plock = (XTPermRowLockPtr) &bl_data[bl_count * bl_item_size];
	for (u_int i=0; i<bl_count; i++) {
		plock--;
		pgroup = plock->pr_group;
		ptab_id = plock->pr_tab_id;
		if (pot) {
			if (pot->ot_table->tab_id == ptab_id)
				goto remove_lock;
			xt_db_return_table_to_pool_ns(pot);
			pot = NULL;
		}

		if (!xt_db_open_pool_table_ns(&pot, db, ptab_id)) {
			/* Should not happen, but just in case, we just don't
			 * remove the lock. We will probably end up with a deadlock
			 * somewhere.
			 */
			xt_log_and_clear_exception_ns();
			goto skip_remove_lock;
		}
		if (!pot)
			/* Can happen of the table has been dropped: */
			goto skip_remove_lock;

		remove_lock:
#ifdef XT_TRACE_LOCKS
		xt_ttracef(xt_get_self(), "remove lock group=%d\n", pgroup);
#endif
		pot->ot_table->tab_locks.tab_row_locks[pgroup] = NULL;
		pot->ot_table->tab_locks.tab_lock_perm[pgroup] = 0;
		skip_remove_lock:;
	}
	bl_count = 0;

	if (pot)
		xt_db_return_table_to_pool_ns(pot);
}

/* Try to lock a row.
 * This function returns:
 * XT_NO_LOCK on success.
 * XT_TEMP_LOCK if there is a temporary lock on the row.
 * XT_PERM_LOCK if there is a permanent lock in the row.
 *
 * If there is a lock on this row, the transaction ID of the
 * locker is also returned.
 *
 * The caller must wait if the row is locked. If the lock is
 * permanent, then the caller must wait for the transaction to
 * terminate. If the lock is temporary, then the caller must
 * wait for the transaction to signal that the lock has been
 * released.
 */
int XTRowLocks::old_xt_set_temp_lock(XTOpenTablePtr ot, xtRowID row, xtXactID *xn_id, XTRowLockListPtr lock_list)
{
	int				group;
	XTXactDataPtr	xact, my_xact;

	if (ot->ot_temp_row_lock) {
		/* Check if we don't already have this temp lock: */
		if (ot->ot_temp_row_lock == row) {
			gl->lw_curr_lock = XT_NO_LOCK;
			return XT_NO_LOCK;
		}

		xt_make_lock_permanent(ot, lock_list);
	}

	my_xact = ot->ot_thread->st_xact_data;
	group = row % XT_ROW_LOCK_COUNT;
	if ((xact = tab_row_locks[group])) {
		if (xact == my_xact)
			return XT_NO_LOCK;
		*xn_id = xact->xd_start_xn_id;
		return tab_lock_perm[group] ? XT_PERM_LOCK : XT_TEMP_LOCK;
	}

	tab_row_locks[row % XT_ROW_LOCK_COUNT] = my_xact;

#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "set temp lock %d group=%d for %s\n", (int) row, (int) row % XT_ROW_LOCK_COUNT, ot->ot_thread->t_name);
#endif
	ot->ot_temp_row_lock = row;
	return XT_NO_LOCK;
}

/* Just check if there is a lock on the row.
 * This function returns:
 * XT_NO_LOCK if there is no lock.
 * XT_TEMP_LOCK if there is a temporary lock on the row.
 * XT_PERM_LOCK if a lock is a permanent lock in the row.
 */
int XTRowLocks::old_xt_is_locked(struct XTOpenTable *ot, xtRowID row, xtXactID *xn_id)
{
	int				group;
	XTXactDataPtr	xact;

	group = row % XT_ROW_LOCK_COUNT;
	if ((xact = tab_row_locks[group])) {
		if (xact == ot->ot_thread->st_xact_data)
			return XT_NO_LOCK;
		*xn_id = xact->xd_start_xn_id;
		if (tab_lock_perm[group])
			return XT_PERM_LOCK;
		return XT_TEMP_LOCK;
	}
	return XT_NO_LOCK;
}

void XTRowLocks::old_xt_remove_temp_lock(XTOpenTablePtr ot)
{
	int				group;
	XTXactDataPtr	xact, my_xact;

	if (!ot->ot_temp_row_lock)
		return;

	my_xact = ot->ot_thread->st_xact_data;
	group = ot->ot_temp_row_lock % XT_ROW_LOCK_COUNT;
#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "remove temp lock %d group=%d\n", (int) ot->ot_temp_row_lock, (int) ot->ot_temp_row_lock % XT_ROW_LOCK_COUNT);
#endif
	ot->ot_temp_row_lock = 0;
	if ((xact = tab_row_locks[group])) {
		if (xact == my_xact)
			tab_row_locks[group] = NULL;
	}

	if (ot->ot_table->tab_db->db_xn_wait_count)
		xt_xn_wakeup_transactions(ot->ot_table->tab_db, ot->ot_thread);
}

xtBool XTRowLocks::old_xt_make_lock_permanent(XTOpenTablePtr ot, XTRowLockListPtr lock_list)
{
	int group;

	if (!ot->ot_temp_row_lock)
		return OK;

#ifdef XT_TRACE_LOCKS
	xt_ttracef(xt_get_self(), "make lock perm %d group=%d\n", (int) ot->ot_temp_row_lock, (int) ot->ot_temp_row_lock % XT_ROW_LOCK_COUNT);
#endif
	/* Check if the lock is already permanent: */
	group = ot->ot_temp_row_lock % XT_ROW_LOCK_COUNT;
	if (!tab_lock_perm[group]) {
		XTPermRowLockRec plock;

		plock.pr_tab_id = ot->ot_table->tab_id;
		plock.pr_group = group;
		if (!xt_bl_append(NULL, lock_list, &plock)) {
			xt_remove_temp_lock(ot);
			return FAILED;
		}
		tab_lock_perm[group] = 1;
	}

	ot->ot_temp_row_lock = 0;
	return OK;
}

/* Release this lock, and all locks gained after this lock
 * on this table.
 *
 * The locks are only released temporarily. The will be regained
 * below using regain locks.
 *
 * Returns:
 * XT_NO_LOCK if no lock is released.
 * XT_PERM_LOCK if a lock is released.
 *
 * Note that only permanent locks can be released in this way.
 * So if the thread has a temporary lock, it will first be made
 * permanent.
 *
 * {RELEASING-LOCKS}
 * The idea of the releasing locks comes from the fact that each
 * lock, locks a group of records.
 * So if T1 has a lock (e.g. when doing SELECT FOR UPDATE),
 * and then encounters an updated record x
 * from T2, and it must wait for T2, it firsts releases the
 * lock, just in case T2 tries to gain a lock on another
 * record y in the same group, which will cause it to
 * wait on T1.
 *
 * However, there are several problems with releasing
 * locks.
 * - It can cause a "live-lock", where another transation
 * keeps getting in before.
 * - It may not solve the problem in all cases because
 * the SELECT FOR UPDATE has locked other record groups
 * before it encountered record x.
 * - Further problems occur when locks are granted by
 * callback:
 * T1 waits for T2, because it has a lock on record x
 * T2 releases the lock because it must wait for T3
 * T1 is granted the lock (but does not know about this yet)
 * T2 tries to regain lock (after T3 quits) and
 * must wait for T1 - DEADLOCK
 *
 * In general, it does not make sense to release locks
 * when it can be granted again by a callback.
 *
 * TODO: 2 possible solutions:
 * - Do not lock groups, lock rows.
 *   UPDATE INTENSION ROW LOCK
 * - Use multiple lock types:
 *   UPDATE INTENSION LOCK (required first)
 *   SHARED UPDATE LOCK (used by INSERT or DELETE)
 *   EXCLUSIVE UPDATE LOCK (used by SELECT FOR UPDATE)
 *
 * Temporary solution. Do not release any locks.
int XTRowLocks::xt_release_locks(struct XTOpenTable *ot, xtRowID row, XTRowLockListPtr lock_list)
 */ 

/*
 * Regain a lock previously held. This function regains locks
 * released by xt_release_locks().
 *
 * It will return lock_type and xn_id if the row is locked, and therefore
 * regain cannot continue. In this case, the caller must wait.
 * It returns XT_NO_LOCK if there are no more locks to be regained.
 *
 * Locks are always regained in the order in which they were originally
 * taken.
xtBool XTRowLocks::xt_regain_locks(struct XTOpenTable *ot, int *lock_type, xtXactID *xn_id, XTRowLockListPtr lock_list)
 */

xtBool old_xt_init_row_locks(XTRowLocksPtr rl)
{
	memset(rl->tab_lock_perm, 0, XT_ROW_LOCK_COUNT);
	memset(rl->tab_row_locks, 0, XT_ROW_LOCK_COUNT * sizeof(XTXactDataPtr));
	return OK;
}

void old_xt_exit_row_locks(XTRowLocksPtr XT_UNUSED(rl))
{
}

#endif // XT_USE_LIST_BASED_ROW_LOCKS

xtPublic xtBool xt_init_row_lock_list(XTRowLockListPtr lock_list)
{
	lock_list->bl_item_size = sizeof(XTPermRowLockRec);
	lock_list->bl_size = 0;
	lock_list->bl_count = 0;
	lock_list->bl_data = NULL;
	return OK;
}

xtPublic void xt_exit_row_lock_list(XTRowLockListPtr lock_list)
{
	xt_bl_set_size(NULL, lock_list, 0);
}

/*
 * -----------------------------------------------------------------------
 * SPECIAL EXCLUSIVE/SHARED (XS) LOCK
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_rwmutex_init(struct XTThread *self, XTRWMutexPtr xsl, const char *n)
#else
xtPublic void xt_rwmutex_init(XTThreadPtr self, XTRWMutexPtr xsl)
#endif
{
#ifdef DEBUG
	xsl->xs_lock_thread = 0;
	xsl->xs_inited = 12345;
#endif
	xt_init_mutex_with_autoname(self, &xsl->xs_lock);
	xt_init_cond(self, &xsl->xs_cond);
	xt_atomic_set4(&xsl->xs_state, 0);
	xsl->xs_xlocker = 0;
	/* Must be aligned! */
	ASSERT(xt_thr_maximum_threads == xt_align_size(xt_thr_maximum_threads, XT_XS_LOCK_ALIGN));
	xsl->x.xs_rlock = (xtWord1 *) xt_calloc(self, xt_thr_maximum_threads);
#ifdef XT_THREAD_LOCK_INFO
	xsl->xs_name = n;
	xt_thread_lock_info_init(&xsl->xs_lock_info, xsl);
#endif
}

xtPublic void xt_rwmutex_free(XTThreadPtr self, XTRWMutexPtr xsl)
{
#ifdef DEBUG
	ASSERT(!xsl->xs_lock_thread);
	ASSERT(xsl->xs_inited == 12345);
	xsl->xs_inited = 0;
#endif
	if (xsl->x.xs_rlock)
		xt_free(self, (void *) xsl->x.xs_rlock);
	xt_free_mutex(&xsl->xs_lock);
	xt_free_cond(&xsl->xs_cond);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&xsl->xs_lock_info);
#endif
}

xtPublic xtBool xt_rwmutex_xlock(XTRWMutexPtr xsl, xtThreadID thd_id)
{
#ifdef DEBUG
	ASSERT_NS(xsl->xs_inited == 12345);
#endif
	ASSERT_NS(xt_get_self()->t_id == thd_id);
	xt_lock_mutex_ns(&xsl->xs_lock);
	ASSERT_NS(xsl->x.xs_rlock[thd_id] == XT_NO_LOCK);
	
	/* Wait for exclusive locker: */
	while (xsl->xs_xlocker) {
		if (!xt_timed_wait_cond_ns(&xsl->xs_cond, &xsl->xs_lock, 10000)) {
			xt_unlock_mutex_ns(&xsl->xs_lock);
			return FAILED;
		}
	}

	/* I am the locker (set state before locker!): */
	xt_atomic_set4(&xsl->xs_state, 0);
	xsl->xs_xlocker = thd_id;

	/* Wait for all the read lockers: */
	while (xsl->xs_state < xt_thr_current_max_threads) {
		while (xsl->x.xs_rlock[xsl->xs_state]) {
			/* {RACE-WR_MUTEX}
			 * Just in case of this, we keep the wait time down!
			 */
			if (!xt_timed_wait_cond_ns(&xsl->xs_cond, &xsl->xs_lock, 10)) {
				xt_atomic_set4(&xsl->xs_state, 0);
				xsl->xs_xlocker = 0;
				xt_unlock_mutex_ns(&xsl->xs_lock);
				return FAILED;
			}
		}
		/* State can be incremented in parallel by a reader
		 * thread!
		 */
		xt_atomic_set4(&xsl->xs_state, xsl->xs_state + 1);
	}

	/* I have waited for all: */
	xt_atomic_set4(&xsl->xs_state, xt_thr_maximum_threads);

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&xsl->xs_lock_info);
#endif

	return OK;
}

xtPublic xtBool xt_rwmutex_slock(XTRWMutexPtr xsl, xtThreadID thd_id)
{
#ifdef DEBUG
	ASSERT_NS(xsl->xs_inited == 12345);
#endif
	ASSERT_NS(xt_get_self()->t_id == thd_id);

	xt_atomic_inc1(&xsl->x.xs_rlock[thd_id]);

	if (xsl->x.xs_rlock[thd_id] > 1)
		return OK;

	/* Check if there could be an X locker: */
	if (xsl->xs_xlocker) {
		/* There is an X locker.
		 * If xs_state < thd_id then the X locker will wait for me.
		 * So I should not wait!
		 */
		if (xsl->xs_state >= thd_id) {
			/* If xsl->xs_state >= thd_id, then the locker has already
			 * checked me, and I will have to wait.
			 *
			 * Otherwise, xs_state <= thd_id, which means the
			 * X locker has not checked me, and will still wait for me (or 
			 * is already waiting for me). In this case, I will have to
			 * take the mutex to make sure exactly how far he
			 * is with the checking.
			 */
			xt_lock_mutex_ns(&xsl->xs_lock);
			while (xsl->xs_state > thd_id && xsl->xs_xlocker) {
				if (!xt_timed_wait_cond_ns(&xsl->xs_cond, &xsl->xs_lock, 10000)) {
					xt_unlock_mutex_ns(&xsl->xs_lock);
					xsl->x.xs_rlock[thd_id]--;
					return FAILED;
				}
			}
			xt_unlock_mutex_ns(&xsl->xs_lock);
		}
	}

	/* There is no exclusive locker, so we have the read lock: */
	ASSERT_NS(xsl->xs_state != xt_thr_maximum_threads);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&xsl->xs_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_rwmutex_unlock(XTRWMutexPtr xsl, xtThreadID thd_id)
{
#ifdef DEBUG
	ASSERT_NS(xsl->xs_inited == 12345);
#endif
	ASSERT_NS(xt_get_self()->t_id == thd_id);
	if (xsl->xs_xlocker == thd_id) {
		/* I have an X lock. */
		ASSERT_NS(xsl->x.xs_rlock[thd_id] == XT_NO_LOCK);
		ASSERT_NS(xsl->xs_state == xt_thr_maximum_threads);
		xt_atomic_set4(&xsl->xs_state, 0);
		xsl->xs_xlocker = 0;
		xt_unlock_mutex_ns(&xsl->xs_lock);
		/* Wake up any other X or shared lockers: */
		if (!xt_broadcast_cond_ns(&xsl->xs_cond))
			return FAILED;
	}
	else {
		/* I have a shared lock: */
		ASSERT_NS(xsl->x.xs_rlock[thd_id] > 0);
		ASSERT_NS(xsl->xs_state != xt_thr_maximum_threads); /* TODO: PMC - HOW can this fail?! - but it does? */
		if (xsl->x.xs_rlock[thd_id] > 1)
			xsl->x.xs_rlock[thd_id]--;
		else {
			/* {RACE-WR_MUTEX}.
			 * A BUG FIX:
			 *
			 * Previously I was checking "xsl->xs_xlocker" after,
			 * descrementing the READ lock.
			 *
			 * This resulted in a race condition that could cause the
			 * unlocking reader to hang in xt_lock_mutex_ns().
			 * This was because the X locker, grabbed the mutex (xs_lock)
			 * but did not wait for the reader.
			 *
			 * The result was that the reader had to wait in UNLOCK
			 * until the X locker did an unlock!
			 *
			 * This only became obvious when it caused a deadlock (because
			 * the reader was waiting for the locker, which it should not
			 * have been, of course).
			 */
			if (xsl->xs_xlocker) {
				xt_lock_mutex_ns(&xsl->xs_lock);
				if (xsl->xs_xlocker && xsl->xs_state == thd_id) {
					/* If the X locker is waiting for me,
					 * then allow him to continue. 
					 */
					if (!xt_broadcast_cond_ns(&xsl->xs_cond)) {
						xt_unlock_mutex_ns(&xsl->xs_lock);
						return FAILED;
					}
				}
				xt_atomic_dec1(&xsl->x.xs_rlock[thd_id]);
				xt_unlock_mutex_ns(&xsl->xs_lock);
			}
			else
				/* {RACE-WR_MUTEX}
				 * There is a race condition between the check above, and the
				 * the decrement here.
				 *
				 * However, if I check xsl->xs_xlocker afterwards, and then
				 * try to get the lock xs_lock, I could hand for the duration
				 * of the X lock.
				 */
				xt_atomic_dec1(&xsl->x.xs_rlock[thd_id]);
		}
	}
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&xsl->xs_lock_info);
#endif
	return OK;
}

/*
 * -----------------------------------------------------------------------
 * SPIN LOCK
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_spinlock_init(XTThreadPtr self, XTSpinLockPtr spl, const char *n)
#else
xtPublic void xt_spinlock_init(XTThreadPtr self, XTSpinLockPtr spl)
#endif
{
	(void) self;
	spl->spl_lock = 0;
#ifdef XT_NO_ATOMICS
	xt_init_mutex(self, &spl->spl_mutex);
#endif
#ifdef DEBUG
	spl->spl_locker = 0;
#endif
#ifdef XT_THREAD_LOCK_INFO
	spl->spl_name = n;
	xt_thread_lock_info_init(&spl->spl_lock_info, spl);
#endif
}

xtPublic void xt_spinlock_free(XTThreadPtr XT_UNUSED(self), XTSpinLockPtr spl)
{
	(void) spl;
#ifdef XT_NO_ATOMICS
	xt_free_mutex(&spl->spl_mutex);
#endif
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&spl->spl_lock_info);
#endif
}

xtPublic xtBool xt_spinlock_spin(XTSpinLockPtr spl)
{
	volatile xtWord4	*lck = &spl->spl_lock;

	for (;;) {
		for (int i=0; i<10; i++) {
			/* Check the lock variable: */
			if (!*lck) {
				/* Try to get the lock: */
				if (!xt_spinlock_set(spl))
					goto done_ok;
			}
		}

		/* Go to "sleep" */
		xt_critical_wait();
	}

	done_ok:
	return OK;
}

#ifdef DEBUG
xtPublic void xt_spinlock_set_thread(XTSpinLockPtr spl)
{
	spl->spl_locker = xt_get_self();
}
#endif

/*
 * -----------------------------------------------------------------------
 * FAST LOCK
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_fastlock_init(XTThreadPtr self, XTFastLockPtr fal, const char *n)
#else
xtPublic void xt_fastlock_init(XTThreadPtr self, XTFastLockPtr fal)
#endif
{
	xt_spinlock_init_with_autoname(self, &fal->fal_spinlock);
	xt_spinlock_init_with_autoname(self, &fal->fal_wait_lock);
	for (u_int i=0; i<XT_FAST_LOCK_MAX_WAIT; i++)
		fal->fal_wait_list[i] = NULL;
	fal->fal_wait_count = 0;
	fal->fal_wait_wakeup = 0;
	fal->fal_wait_alloc = 0;
#ifdef XT_THREAD_LOCK_INFO
	fal->fal_name = n;
	xt_thread_lock_info_init(&fal->fal_lock_info, fal);
#endif
}

xtPublic void xt_fastlock_free(XTThreadPtr self, XTFastLockPtr fal)
{
	xt_spinlock_free(self, &fal->fal_spinlock);
	xt_spinlock_free(self, &fal->fal_wait_lock);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&fal->fal_lock_info);
#endif
}

xtPublic xtBool xt_fastlock_spin(XTFastLockPtr fal, XTThreadPtr thread)
{
	volatile xtWord4	*lck = &fal->fal_spinlock.spl_lock;

	do {
		for (int i=0; i<10; i++) {
			/* Check the lock variable: */
			if (!*lck) {
				/* Try to get the lock: */
				if (!xt_spinlock_set(&fal->fal_spinlock)) {
					fal->fal_locker = thread;
					return OK;
				}
			}
		}

		for (int i=0; i<10; i++) {
			xt_critical_wait();
			if (!*lck) {
				/* Try to get the lock: */
				if (!xt_spinlock_set(&fal->fal_spinlock)) {
					fal->fal_locker = thread;
					return OK;
				}
			}
		}

		/* Wait for a wakeup */
		xt_spinlock_lock(&fal->fal_wait_lock);
		if (fal->fal_wait_count == XT_FAST_LOCK_MAX_WAIT) {
			xt_register_ulxterr(XT_REG_CONTEXT, XT_ERR_TOO_MANY_WAITERS, (u_long) XT_FAST_LOCK_MAX_WAIT+1);
			xt_spinlock_unlock(&fal->fal_wait_lock);
			return FAILED;
		}
		while (fal->fal_wait_list[fal->fal_wait_alloc])
			fal->fal_wait_alloc = (fal->fal_wait_alloc + 1) % XT_FAST_LOCK_MAX_WAIT;
		fal->fal_wait_list[fal->fal_wait_alloc] = thread;
		fal->fal_wait_alloc = (fal->fal_wait_alloc + 1) % XT_FAST_LOCK_MAX_WAIT;
		fal->fal_wait_count++;
		xt_lock_thread(thread);
		xt_spinlock_unlock(&fal->fal_wait_lock);
		if (!xt_wait_thread(thread)) {
			xt_unlock_thread(thread);
			if (fal->fal_locker == thread)
				xt_fastlock_unlock(fal, thread);
			return FAILED;
		}
		xt_unlock_thread(thread);
	} while (fal->fal_locker != thread);
	return OK;
}

/* Wake up one of the waiters. */
xtPublic void xt_fastlock_wakeup(XTFastLockPtr fal)
{
	xt_spinlock_lock(&fal->fal_wait_lock);
	if (fal->fal_wait_count) {
		XTThreadPtr thread;

		/* Find a waiting thread, and give it the exclusive lock: */
		while (!fal->fal_wait_list[fal->fal_wait_wakeup])
			fal->fal_wait_wakeup = (fal->fal_wait_wakeup + 1) % XT_FAST_LOCK_MAX_WAIT;
		thread = fal->fal_wait_list[fal->fal_wait_wakeup];
		fal->fal_wait_list[fal->fal_wait_wakeup] = NULL;
		fal->fal_wait_wakeup = (fal->fal_wait_wakeup + 1) % XT_FAST_LOCK_MAX_WAIT;
		fal->fal_wait_count--;
		fal->fal_locker = thread;

		xt_lock_thread(thread);
		xt_spinlock_unlock(&fal->fal_wait_lock);
		xt_signal_thread(thread);
		xt_unlock_thread(thread);
	}
	else {
		xt_spinlock_unlock(&fal->fal_wait_lock);
		fal->fal_locker = NULL;
		xt_spinlock_reset(&fal->fal_spinlock);
	}
}

/*
 * -----------------------------------------------------------------------
 * READ/WRITE SPIN LOCK
 *
 * An extremely genius very fast read/write lock based on atomics!
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_spinxslock_init(struct XTThread *XT_UNUSED(self), XTSpinXSLockPtr sxs, const char *name)
#else
xtPublic void xt_spinxslock_init(struct XTThread *XT_UNUSED(self), XTSpinXSLockPtr sxs)
#endif
{
	sxs->sxs_xlocked = 0;
	sxs->sxs_rlock_count = 0;
	sxs->sxs_wait_count = 0;
#ifdef DEBUG
	sxs->sxs_locker = 0;
#endif
#ifdef XT_THREAD_LOCK_INFO
	sxs->sxs_name = name;
	xt_thread_lock_info_init(&sxs->sxs_lock_info, sxs);
#endif
}

xtPublic void xt_spinxslock_free(struct XTThread *XT_UNUSED(self), XTSpinXSLockPtr sxs)
{
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&sxs->sxs_lock_info);
#else
	(void) sxs;
#endif
}

xtPublic xtBool xt_spinxslock_xlock(XTSpinXSLockPtr sxs, xtThreadID XT_NDEBUG_UNUSED(thd_id))
{
	register xtWord2 set;

	/* Wait for exclusive locker: */
	for (;;) {
		set = xt_atomic_tas2(&sxs->sxs_xlocked, 1);
		if (!set)
			break;
		xt_yield();
	}

#ifdef DEBUG
	sxs->sxs_locker = thd_id;
#endif

	/* Wait for all the reader to wait! */
	while (sxs->sxs_wait_count < sxs->sxs_rlock_count)
		xt_yield();

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&sxs->sxs_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_spinxslock_slock(XTSpinXSLockPtr sxs)
{
	xt_atomic_inc2(&sxs->sxs_rlock_count);

	/* Check if there could be an X locker: */
	if (sxs->sxs_xlocked) {
		/* I am waiting... */
		xt_atomic_inc2(&sxs->sxs_wait_count);
		while (sxs->sxs_xlocked)
			xt_yield();
		xt_atomic_dec2(&sxs->sxs_wait_count);
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&sxs->sxs_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_spinxslock_unlock(XTSpinXSLockPtr sxs, xtBool xlocked)
{
	if (xlocked) {
#ifdef DEBUG
		sxs->sxs_locker = 0;
#endif
		sxs->sxs_xlocked = 0;
	}
	else
		xt_atomic_dec2(&sxs->sxs_rlock_count);

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&sxs->sxs_lock_info);
#endif
	return OK;
}

/*
 * -----------------------------------------------------------------------
 * FAST READ/WRITE LOCK (BASED ON FAST MUTEX)
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_xsmutex_init(struct XTThread *self, XTXSMutexLockPtr xsm, const char *name)
#else
xtPublic void xt_xsmutex_init(struct XTThread *self, XTXSMutexLockPtr xsm)
#endif
{
	xt_init_mutex_with_autoname(self, &xsm->xsm_lock);
	xt_init_cond(self, &xsm->xsm_cond);
	xt_init_cond(self, &xsm->xsm_cond_2);
	xsm->xsm_xlocker = 0;
	xsm->xsm_rlock_count = 0;
	xsm->xsm_wait_count = 0;
#ifdef DEBUG
	xsm->xsm_locker = 0;
#endif
#ifdef XT_THREAD_LOCK_INFO
	xsm->xsm_name = name;
	xt_thread_lock_info_init(&xsm->xsm_lock_info, xsm);
#endif
}

xtPublic void xt_xsmutex_free(struct XTThread *XT_UNUSED(self), XTXSMutexLockPtr xsm)
{
	xt_free_mutex(&xsm->xsm_lock);
	xt_free_cond(&xsm->xsm_cond);
	xt_free_cond(&xsm->xsm_cond_2);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&xsm->xsm_lock_info);
#endif
}

xtPublic xtBool xt_xsmutex_xlock(XTXSMutexLockPtr xsm, xtThreadID thd_id)
{
	xt_lock_mutex_ns(&xsm->xsm_lock);

	/* Wait for exclusive locker: */
	while (xsm->xsm_xlocker) {
		if (!xt_timed_wait_cond_ns(&xsm->xsm_cond, &xsm->xsm_lock, 10000)) {
			xt_unlock_mutex_ns(&xsm->xsm_lock);
			return FAILED;
		}
	}

	/* GOTCHA: You would think this is not necessary...
	 * But is does not always work, if a normal insert is used.
	 * The reason is, I guess, on MMP the assignment is not
	 * always immediately visible to other processors, because they
	 * have old versions of this variable in there cache.
	 *
	 * But this is required, because the locking mechanism is based
	 * on:
	 * Locker: sets xlocker, tests rlock_count
	 * Reader: incs rlock_count, tests xlocker
	 *
	 * The test, in both cases, may not read stale values.
	 * volatile does not help, because this just turns compiler
	 * optimisations off.
	 */
	xt_atomic_set4(&xsm->xsm_xlocker, thd_id);

	/* Wait for all the reader to wait! */
	while (xsm->xsm_wait_count < xsm->xsm_rlock_count) {
		/* {RACE-WR_MUTEX} Here as well: */
		if (!xt_timed_wait_cond_ns(&xsm->xsm_cond, &xsm->xsm_lock, 100)) {
			xsm->xsm_xlocker = 0;
			xt_unlock_mutex_ns(&xsm->xsm_lock);
			return FAILED;
		}
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&xsm->xsm_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_xsmutex_slock(XTXSMutexLockPtr xsm, xtThreadID XT_UNUSED(thd_id))
{
	xt_atomic_inc2(&xsm->xsm_rlock_count);

	/* Check if there could be an X locker: */
	if (xsm->xsm_xlocker) {
		/* I am waiting... */
		xt_lock_mutex_ns(&xsm->xsm_lock);
		xsm->xsm_wait_count++;
		/* Wake up the xlocker: */
		if (xsm->xsm_xlocker && xsm->xsm_wait_count == xsm->xsm_rlock_count) {
			if (!xt_broadcast_cond_ns(&xsm->xsm_cond)) {
				xsm->xsm_wait_count--;
				xt_unlock_mutex_ns(&xsm->xsm_lock);
				return FAILED;
			}
		}
		while (xsm->xsm_xlocker) {
			if (!xt_timed_wait_cond_ns(&xsm->xsm_cond_2, &xsm->xsm_lock, 10000)) {
				xsm->xsm_wait_count--;
				xt_unlock_mutex_ns(&xsm->xsm_lock);
				return FAILED;
			}
		}
		xsm->xsm_wait_count--;
		xt_unlock_mutex_ns(&xsm->xsm_lock);
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&xsm->xsm_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_xsmutex_unlock(XTXSMutexLockPtr xsm, xtThreadID thd_id)
{
	if (xsm->xsm_xlocker == thd_id) {
		xsm->xsm_xlocker = 0;
		if (xsm->xsm_wait_count) {
			if (!xt_broadcast_cond_ns(&xsm->xsm_cond_2)) {
				xt_unlock_mutex_ns(&xsm->xsm_lock);
				return FAILED;
			}
		}
		else {
			/* Wake up any other X or shared lockers: */
			if (!xt_broadcast_cond_ns(&xsm->xsm_cond)) {
				xt_unlock_mutex_ns(&xsm->xsm_lock);
				return FAILED;
			}
		}
		xt_unlock_mutex_ns(&xsm->xsm_lock);
	}
	else {
		/* Taking the advice from {RACE-WR_MUTEX} I do the decrement
		 * after I have a lock!
		 */
		if (xsm->xsm_xlocker) {
			xt_lock_mutex_ns(&xsm->xsm_lock);
			xt_atomic_dec2(&xsm->xsm_rlock_count);
			if (xsm->xsm_xlocker && xsm->xsm_wait_count == xsm->xsm_rlock_count) {
				/* If the X locker is waiting for me,
				 * then allow him to continue. 
				 */
				if (!xt_broadcast_cond_ns(&xsm->xsm_cond)) {
					xt_unlock_mutex_ns(&xsm->xsm_lock);
					return FAILED;
				}
			}
			xt_unlock_mutex_ns(&xsm->xsm_lock);
		}
		else
			xt_atomic_dec2(&xsm->xsm_rlock_count);
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&xsm->xsm_lock_info);
#endif
	return OK;
}

/*
 * -----------------------------------------------------------------------
 * ATOMIC READ/WRITE LOCK (BASED ON ATOMIC OPERATIONS)
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_atomicrwlock_init(struct XTThread *XT_UNUSED(self), XTAtomicRWLockPtr arw, const char *n)
#else
xtPublic void xt_atomicrwlock_init(struct XTThread *XT_UNUSED(self), XTAtomicRWLockPtr arw)
#endif
{
	arw->arw_reader_count = 0;
	arw->arw_xlock_set = 0;
#ifdef XT_THREAD_LOCK_INFO
	arw->arw_name = n;
	xt_thread_lock_info_init(&arw->arw_lock_info, arw);
#endif
}

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_atomicrwlock_free(struct XTThread *, XTAtomicRWLockPtr arw)
#else
xtPublic void xt_atomicrwlock_free(struct XTThread *, XTAtomicRWLockPtr XT_UNUSED(arw))
#endif
{
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&arw->arw_lock_info);
#endif
}

xtPublic xtBool xt_atomicrwlock_xlock(XTAtomicRWLockPtr arw, xtThreadID XT_NDEBUG_UNUSED(thr_id))
{
	register xtWord2 set;

	/* First get an exclusive lock: */
	for (;;) {
		set = xt_atomic_tas2(&arw->arw_xlock_set, 1);
		if (!set)
			break;
		xt_yield();
	}

	/* Wait for the remaining readers: */
	while (arw->arw_reader_count)
		xt_yield();

#ifdef DEBUG
	arw->arw_locker = thr_id;
#endif

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&arw->arw_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_atomicrwlock_slock(XTAtomicRWLockPtr arw)
{
	register xtWord2 set;

	/* First get an exclusive lock: */
	for (;;) {
		set = xt_atomic_tas2(&arw->arw_xlock_set, 1);
		if (!set)
			break;
		xt_yield();
	}

	/* Add a reader: */
	xt_atomic_inc2(&arw->arw_reader_count);

	/* Release the xlock: */
	arw->arw_xlock_set = 0;

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&arw->arw_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_atomicrwlock_unlock(XTAtomicRWLockPtr arw, xtBool xlocked)
{
	if (xlocked) {
#ifdef DEBUG
		arw->arw_locker = 0;
#endif
		arw->arw_xlock_set = 0;
	}
	else
		xt_atomic_dec2(&arw->arw_reader_count);

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&arw->arw_lock_info);
#endif

	return OK;
}

/*
 * -----------------------------------------------------------------------
 * "SKEW" ATOMITC READ/WRITE LOCK (BASED ON ATOMIC OPERATIONS)
 *
 * This lock type favors writers. It only works if the proportion of readers
 * to writer is high.
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_skewrwlock_init(struct XTThread *XT_UNUSED(self), XTSkewRWLockPtr srw, const char *n)
#else
xtPublic void xt_skewrwlock_init(struct XTThread *XT_UNUSED(self), XTSkewRWLockPtr srw)
#endif
{
	srw->srw_reader_count = 0;
	srw->srw_xlock_set = 0;
#ifdef XT_THREAD_LOCK_INFO
	srw->srw_name = n;
	xt_thread_lock_info_init(&srw->srw_lock_info, srw);
#endif
}

#ifdef XT_THREAD_LOCK_INFO
xtPublic void xt_skewrwlock_free(struct XTThread *, XTSkewRWLockPtr srw)
#else
xtPublic void xt_skewrwlock_free(struct XTThread *, XTSkewRWLockPtr XT_UNUSED(srw))
#endif
{
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&srw->srw_lock_info);
#endif
}

xtPublic xtBool xt_skewrwlock_xlock(XTSkewRWLockPtr srw, xtThreadID XT_NDEBUG_UNUSED(thr_id))
{
	register xtWord2 set;

	/* First get an exclusive lock: */
	for (;;) {
		set = xt_atomic_tas2(&srw->srw_xlock_set, 1);
		if (!set)
			break;
		xt_yield();
	}

	/* Wait for the remaining readers: */
	while (srw->srw_reader_count)
		xt_yield();

#ifdef DEBUG
	srw->srw_locker = thr_id;
#endif

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&srw->srw_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_skewrwlock_slock(XTSkewRWLockPtr srw)
{
	/* Wait for an exclusive lock: */
	retry:
	for (;;) {
		if (!srw->srw_xlock_set)
			break;
		xt_yield();
	}

	/* Add a reader: */
	xt_atomic_inc2(&srw->srw_reader_count);

	/* Check for xlock again: */
	if (srw->srw_xlock_set) {
		xt_atomic_dec2(&srw->srw_reader_count);
		goto retry;
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&srw->srw_lock_info);
#endif
	return OK;
}

xtPublic xtBool xt_skewrwlock_unlock(XTSkewRWLockPtr srw, xtBool xlocked)
{
	if (xlocked)
		srw->srw_xlock_set = 0;
	else
		xt_atomic_dec2(&srw->srw_reader_count);

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&srw->srw_lock_info);
#endif
#ifdef DEBUG
	srw->srw_locker = 0;
#endif

	return OK;
}

/*
 * -----------------------------------------------------------------------
 * UNIT TESTS
 */

#define JOB_MEMCPY			1
#define JOB_SLEEP			2
#define JOB_PRINT			3
#define JOB_INCREMENT		4
#define JOB_SNOOZE			5
#define JOB_DOUBLE_INC		6

#define LOCK_PTHREAD_RW		1
#define LOCK_PTHREAD_MUTEX	2
#define LOCK_RWMUTEX		3
#define LOCK_SPINLOCK		4
#define LOCK_FASTLOCK		5
#define LOCK_SPINXSLOCK		6
#define LOCK_XSMUTEX		7
#define LOCK_ATOMICRWLOCK	8
#define LOCK_SKEWRWLOCK		9

typedef struct XSLockTest {
	u_int			xs_interations;
	xtBool			xs_which_lock;
	xtBool			xs_which_job;
	xtBool			xs_debug_print;
	XTRWMutexRec	xs_lock;
	xt_rwlock_type	xs_plock;
	XTSpinLockRec	xs_spinlock;
	xt_mutex_type	xs_mutex;
	XTFastLockRec	xs_fastlock;
	XTSpinXSLockRec	xs_spinrwlock;
	XTXSMutexRec	xs_fastrwlock;
	XTAtomicRWLockRec xs_atomicrwlock;
	XTSkewRWLockRec xs_skewrwlock;
	int				xs_progress;
	xtWord4			xs_inc;
} XSLockTestRec, *XSLockTestPtr;

static void lck_free_thread_data(XTThreadPtr XT_UNUSED(self), void *XT_UNUSED(data))
{
}

static void lck_do_job(XTThreadPtr self, int job, XSLockTestPtr data, xtBool reader)
{
	char b1[2048], b2[2048];

	switch (job) {
		case JOB_MEMCPY:
			memcpy(b1, b2, 2048);
			data->xs_inc++;
			break;
		case JOB_SLEEP:
			xt_sleep_milli_second(1);
			data->xs_inc++;
			break;
		case JOB_PRINT:
			printf("- %s got lock\n", self->t_name);
			xt_sleep_milli_second(10);
			data->xs_inc++;
			break;
		case JOB_INCREMENT:
			data->xs_inc++;
			break;
		case JOB_SNOOZE:
			xt_sleep_milli_second(10);
			data->xs_inc++;
			break;
		case JOB_DOUBLE_INC:
			if (reader) {
				if ((data->xs_inc & 1) != 0)
					printf("Noooo!\n");
			}
			else {
				data->xs_inc++;
				data->xs_inc++;
			}
			break;
	}
}

#if 0
static void *lck_run_dumper(XTThreadPtr self)
{
	int state = 0;

	while (state != 1) {
		sleep(1);
		if (state == 2) {
			xt_dump_trace();
			state = 0;
		}
	}
}
#endif

static void *lck_run_reader(XTThreadPtr self)
{
	XSLockTestRec	*data = (XSLockTestRec *) self->t_data;

	if (data->xs_debug_print)
		printf("- %s start\n", self->t_name);
	for (u_int i=0; i<data->xs_interations; i++) {
		if (data->xs_progress && ((i+1) % data->xs_progress) == 0)
			printf("- %s %d\n", self->t_name, i+1);
		if (data->xs_which_lock == LOCK_PTHREAD_RW) {
			xt_slock_rwlock_ns(&data->xs_plock);
			lck_do_job(self, data->xs_which_job, data, TRUE);
			xt_unlock_rwlock_ns(&data->xs_plock);
		}
		else if (data->xs_which_lock == LOCK_RWMUTEX) {
			xt_rwmutex_slock(&data->xs_lock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, TRUE);
			xt_rwmutex_unlock(&data->xs_lock, self->t_id);
		}
		else if (data->xs_which_lock == LOCK_SPINXSLOCK) {
			xt_spinxslock_slock(&data->xs_spinrwlock);
			lck_do_job(self, data->xs_which_job, data, TRUE);
			xt_spinxslock_unlock(&data->xs_spinrwlock, FALSE);
		}
		else if (data->xs_which_lock == LOCK_XSMUTEX) {
			xt_xsmutex_slock(&data->xs_fastrwlock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, TRUE);
			xt_xsmutex_unlock(&data->xs_fastrwlock, self->t_id);
		}
		else if (data->xs_which_lock == LOCK_ATOMICRWLOCK) {
			xt_atomicrwlock_slock(&data->xs_atomicrwlock);
			lck_do_job(self, data->xs_which_job, data, TRUE);
			xt_atomicrwlock_unlock(&data->xs_atomicrwlock, FALSE);
		}
		else if (data->xs_which_lock == LOCK_SKEWRWLOCK) {
			xt_skewrwlock_slock(&data->xs_skewrwlock);
			lck_do_job(self, data->xs_which_job, data, TRUE);
			xt_skewrwlock_unlock(&data->xs_skewrwlock, FALSE);
		}
		else
			ASSERT(FALSE);
	}
	if (data->xs_debug_print)
		printf("- %s stop\n", self->t_name);
	return NULL;
}

static void *lck_run_writer(XTThreadPtr self)
{
	XSLockTestRec	*data = (XSLockTestRec *) self->t_data;

	if (data->xs_debug_print)
		printf("- %s start\n", self->t_name);
	for (u_int i=0; i<data->xs_interations; i++) {
		if (data->xs_progress && ((i+1) % data->xs_progress) == 0)
			printf("- %s %d\n", self->t_name, i+1);
		if (data->xs_which_lock == LOCK_PTHREAD_RW) {
			xt_xlock_rwlock_ns(&data->xs_plock);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_unlock_rwlock_ns(&data->xs_plock);
		}
		else if (data->xs_which_lock == LOCK_RWMUTEX) {
			xt_rwmutex_xlock(&data->xs_lock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_rwmutex_unlock(&data->xs_lock, self->t_id);
		}
		else if (data->xs_which_lock == LOCK_SPINXSLOCK) {
			xt_spinxslock_xlock(&data->xs_spinrwlock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_spinxslock_unlock(&data->xs_spinrwlock, TRUE);
		}
		else if (data->xs_which_lock == LOCK_XSMUTEX) {
			xt_xsmutex_xlock(&data->xs_fastrwlock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_xsmutex_unlock(&data->xs_fastrwlock, self->t_id);
		}
		else if (data->xs_which_lock == LOCK_ATOMICRWLOCK) {
			xt_atomicrwlock_xlock(&data->xs_atomicrwlock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_atomicrwlock_unlock(&data->xs_atomicrwlock, TRUE);
		}
		else if (data->xs_which_lock == LOCK_SKEWRWLOCK) {
			xt_skewrwlock_xlock(&data->xs_skewrwlock, self->t_id);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_skewrwlock_unlock(&data->xs_skewrwlock, TRUE);
		}
		else
			ASSERT(FALSE);
	}
	if (data->xs_debug_print)
		printf("- %s stop\n", self->t_name);
	return NULL;
}

static void lck_print_test(XSLockTestRec *data)
{
	switch (data->xs_which_lock) {
		case LOCK_PTHREAD_RW:
			printf("pthread read/write");
			break;
		case LOCK_PTHREAD_MUTEX:
			printf("pthread mutex");
			break;
		case LOCK_RWMUTEX:
			printf("fast read/write mutex");
			break;
		case LOCK_SPINLOCK:
			printf("spin mutex");
			break;
		case LOCK_FASTLOCK:
			printf("fast mutex");
			break;
		case LOCK_SPINXSLOCK:
			printf("spin read/write lock");
			break;
		case LOCK_XSMUTEX:
			printf("fast x/s mutex");
			break;
		case LOCK_ATOMICRWLOCK:
			printf("atomic read/write lock");
			break;
		case LOCK_SKEWRWLOCK:
			printf("skew read/write lock");
			break;
	}

	switch (data->xs_which_job) {
		case JOB_MEMCPY:
			printf(" MEMCPY 2K");
			break;
		case JOB_SLEEP:
			printf(" SLEEP 1/1000s");
			break;
		case JOB_PRINT:
			printf(" PRINT DEBUG");
			break;
		case JOB_INCREMENT:
			printf(" INCREMENT");
			break;
		case JOB_SNOOZE:
			printf(" SLEEP 1/100s");
			break;
	}
	
	printf(" %d interations", data->xs_interations);
}

static void *lck_run_mutex_locker(XTThreadPtr self)
{
	XSLockTestRec *data = (XSLockTestRec *) self->t_data;

	if (data->xs_debug_print)
		printf("- %s start\n", self->t_name);
	for (u_int i=0; i<data->xs_interations; i++) {
		if (data->xs_progress && ((i+1) % data->xs_progress) == 0)
			printf("- %s %d\n", self->t_name, i+1);
		if (data->xs_which_lock == LOCK_PTHREAD_MUTEX) {
			xt_lock_mutex_ns(&data->xs_mutex);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_unlock_mutex_ns(&data->xs_mutex);
		}
		else if (data->xs_which_lock == LOCK_SPINLOCK) {
			xt_spinlock_lock(&data->xs_spinlock);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_spinlock_unlock(&data->xs_spinlock);
		}
		else if (data->xs_which_lock == LOCK_FASTLOCK) {
			xt_fastlock_lock(&data->xs_fastlock, self);
			lck_do_job(self, data->xs_which_job, data, FALSE);
			xt_fastlock_unlock(&data->xs_fastlock, self);
		}
		else
			ASSERT(FALSE);
	}
	if (data->xs_debug_print)
		printf("- %s stop\n", self->t_name);
	return NULL;
}

typedef struct LockThread {
	xtThreadID		id;
	XTThreadPtr		ptr;
} LockThreadRec, *LockThreadPtr;

static void lck_reader_writer_test(XTThreadPtr self, XSLockTestRec *data, int reader_cnt, int writer_cnt)
{
	xtWord8			start;
	LockThreadPtr	threads;
	int				thread_cnt = reader_cnt + writer_cnt;
	char			buffer[40];

	//XTThreadPtr dumper = xt_create_daemon(self, "DUMPER");
	//xt_run_thread(self, dumper, lck_run_dumper);

	printf("READ/WRITE TEST: ");
	lck_print_test(data);
	printf(", %d readers, %d writers\n", reader_cnt, writer_cnt);
	threads = (LockThreadPtr) xt_malloc(self, thread_cnt * sizeof(LockThreadRec));

	for (int i=0; i<thread_cnt; i++) {
		sprintf(buffer, "%s%d", i < reader_cnt ? "READER-" : "WRITER-", i+1);
		threads[i].ptr = xt_create_daemon(self, buffer);
		threads[i].id = threads[i].ptr->t_id;
		xt_set_thread_data(threads[i].ptr, data, lck_free_thread_data);
	}

	start = xt_trace_clock();
	for (int i=0; i<reader_cnt; i++)
		xt_run_thread(self, threads[i].ptr, lck_run_reader);
	for (int i=reader_cnt; i<thread_cnt; i++)
		xt_run_thread(self, threads[i].ptr, lck_run_writer);

	for (int i=0; i<thread_cnt; i++)
		xt_wait_for_thread(threads[i].id, TRUE);
	printf("----- %d reader, %d writer time=%s\n", reader_cnt, writer_cnt, xt_trace_clock_diff(buffer, start));

	xt_free(self, threads);
	printf("TEST RESULT = %d\n", data->xs_inc);

	//xt_wait_for_thread(dumper, TRUE);
}

static void lck_mutex_lock_test(XTThreadPtr self, XSLockTestRec *data, int thread_cnt)
{
	xtWord8			start;
	LockThreadPtr	threads;
	char			buffer[40];

	printf("LOCK MUTEX TEST: ");
	lck_print_test(data);
	printf(", %d threads\n", thread_cnt);
	threads = (LockThreadPtr) xt_malloc(self, thread_cnt * sizeof(LockThreadRec));

	for (int i=0; i<thread_cnt; i++) {
		sprintf(buffer, "THREAD%d", i+1);
		threads[i].ptr = xt_create_daemon(self, buffer);
		threads[i].id = threads[i].ptr->t_id;
		xt_set_thread_data(threads[i].ptr, data, lck_free_thread_data);
	}

	start = xt_trace_clock();
	for (int i=0; i<thread_cnt; i++)
		xt_run_thread(self, threads[i].ptr, lck_run_mutex_locker);

	for (int i=0; i<thread_cnt; i++)
		xt_wait_for_thread(threads[i].id, TRUE);
	printf("----- %d threads time=%s\n", thread_cnt, xt_trace_clock_diff(buffer, start));

	xt_free(self, threads);
	printf("TEST RESULT = %d\n", data->xs_inc);
}

xtPublic void xt_unit_test_read_write_locks(XTThreadPtr self)
{
	XSLockTestRec	data;

	memset(&data, 0, sizeof(data));

	printf("TEST: xt_unit_test_read_write_locks\n");
	printf("size of XTXSMutexRec = %d\n", (int) sizeof(XTXSMutexRec));
	printf("size of pthread_cond_t = %d\n", (int) sizeof(pthread_cond_t));
	printf("size of pthread_mutex_t = %d\n", (int) sizeof(pthread_mutex_t));
	xt_rwmutex_init_with_autoname(self, &data.xs_lock);
	xt_init_rwlock_with_autoname(self, &data.xs_plock);
	xt_spinxslock_init_with_autoname(self, &data.xs_spinrwlock);
	xt_xsmutex_init_with_autoname(self, &data.xs_fastrwlock);
	xt_atomicrwlock_init_with_autoname(self, &data.xs_atomicrwlock);
	xt_skewrwlock_init_with_autoname(self, &data.xs_skewrwlock);

	/**
	data.xs_interations = 10;
	data.xs_which_lock = LOCK_RWMUTEX; // LOCK_PTHREAD_RW, LOCK_RWMUTEX, LOCK_SPINXSLOCK, LOCK_XSMUTEX
	data.xs_which_job = JOB_PRINT;
	data.xs_debug_print = TRUE;
	data.xs_progress = 0;
	lck_reader_writer_test(self, &data, 4, 0);
	lck_reader_writer_test(self, &data, 0, 2);
	lck_reader_writer_test(self, &data, 1, 1);
	lck_reader_writer_test(self, &data, 4, 2);
	**/

	/**
	data.xs_interations = 4000;
	data.xs_which_lock = LOCK_RWMUTEX; // LOCK_PTHREAD_RW, LOCK_RWMUTEX, LOCK_SPINXSLOCK, LOCK_XSMUTEX
	data.xs_which_job = JOB_SLEEP;
	data.xs_debug_print = TRUE;
	data.xs_progress = 200;
	lck_reader_writer_test(self, &data, 4, 0);
	lck_reader_writer_test(self, &data, 0, 2);
	lck_reader_writer_test(self, &data, 1, 1);
	lck_reader_writer_test(self, &data, 4, 2);
	**/

	// LOCK_PTHREAD_RW, LOCK_RWMUTEX, LOCK_SPINXSLOCK, LOCK_XSMUTEX, LOCK_ATOMICRWLOCK, LOCK_SKEWRWLOCK
	/**/
	data.xs_interations = 100000;
	data.xs_which_lock = LOCK_XSMUTEX;
	data.xs_which_job = JOB_DOUBLE_INC; // JOB_INCREMENT, JOB_DOUBLE_INC
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	lck_reader_writer_test(self, &data, 10, 0);
	data.xs_which_lock = LOCK_XSMUTEX;
	lck_reader_writer_test(self, &data, 10, 0);
	//lck_reader_writer_test(self, &data, 0, 5);
	//lck_reader_writer_test(self, &data, 10, 0);
	//lck_reader_writer_test(self, &data, 10, 5);
	/**/

	/**/
	data.xs_interations = 10000;
	data.xs_which_lock = LOCK_XSMUTEX;
	data.xs_which_job = JOB_MEMCPY;
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	lck_reader_writer_test(self, &data, 10, 0);
	data.xs_which_lock = LOCK_XSMUTEX;
	lck_reader_writer_test(self, &data, 10, 0);
	//lck_reader_writer_test(self, &data, 0, 5);
	//lck_reader_writer_test(self, &data, 10, 0);
	//lck_reader_writer_test(self, &data, 10, 5);
	/**/

	/**/
	data.xs_interations = 1000;
	data.xs_which_lock = LOCK_XSMUTEX;
	data.xs_which_job = JOB_SLEEP; // JOB_SLEEP, JOB_SNOOZE
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	lck_reader_writer_test(self, &data, 10, 0);
	data.xs_which_lock = LOCK_XSMUTEX;
	lck_reader_writer_test(self, &data, 10, 0);
	/**/

	xt_rwmutex_free(self, &data.xs_lock);
	xt_free_rwlock(&data.xs_plock);
	xt_spinxslock_free(self, &data.xs_spinrwlock);
	xt_xsmutex_free(self, &data.xs_fastrwlock);
	xt_atomicrwlock_free(self, &data.xs_atomicrwlock);
	xt_skewrwlock_free(self, &data.xs_skewrwlock);
}

xtPublic void xt_unit_test_mutex_locks(XTThreadPtr self)
{
	XSLockTestRec	data;

	memset(&data, 0, sizeof(data));

	printf("TEST: xt_unit_test_mutex_locks\n");
	xt_spinlock_init_with_autoname(self, &data.xs_spinlock);
	xt_fastlock_init_with_autoname(self, &data.xs_fastlock);
	xt_init_mutex_with_autoname(self, &data.xs_mutex);

	/**/
	data.xs_interations = 10;
	data.xs_which_lock = LOCK_SPINLOCK; // LOCK_SPINLOCK, LOCK_PTHREAD_MUTEX, LOCK_FASTLOCK
	data.xs_which_job = JOB_PRINT;
	data.xs_debug_print = TRUE;
	data.xs_progress = 0;
	data.xs_inc = 0;
	lck_mutex_lock_test(self, &data, 2);
	/**/

	/**/
	data.xs_interations = 100000;
	data.xs_which_lock = LOCK_SPINLOCK; // LOCK_SPINLOCK, LOCK_PTHREAD_MUTEX, LOCK_FASTLOCK
	data.xs_which_job = JOB_INCREMENT;
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	data.xs_inc = 0;
	lck_mutex_lock_test(self, &data, 10);
	/**/

	/**/
	data.xs_interations = 10000;
	data.xs_which_lock = LOCK_SPINLOCK; // LOCK_SPINLOCK, LOCK_PTHREAD_MUTEX, LOCK_FASTLOCK
	data.xs_which_job = JOB_MEMCPY;
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	data.xs_inc = 0;
	lck_mutex_lock_test(self, &data, 10);
	/**/

	/**/
	data.xs_interations = 1000;
	data.xs_which_lock = LOCK_FASTLOCK; // LOCK_SPINLOCK, LOCK_PTHREAD_MUTEX, LOCK_FASTLOCK
	data.xs_which_job = JOB_SLEEP;
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	data.xs_inc = 0;
	lck_mutex_lock_test(self, &data, 10);
	/**/

	/**/
	data.xs_interations = 100;
	data.xs_which_lock = LOCK_FASTLOCK; // LOCK_SPINLOCK, LOCK_PTHREAD_MUTEX, LOCK_FASTLOCK
	data.xs_which_job = JOB_SNOOZE;
	data.xs_debug_print = FALSE;
	data.xs_progress = 0;
	data.xs_inc = 0;
	lck_mutex_lock_test(self, &data, 10);
	/**/

	xt_spinlock_free(self, &data.xs_spinlock);
	xt_fastlock_free(self, &data.xs_fastlock);
	xt_free_mutex(&data.xs_mutex);
}

xtPublic void xt_unit_test_create_threads(XTThreadPtr self)
{
	XTThreadPtr		threads[10];

	printf("TEST: xt_unit_test_create_threads\n");
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* Create some threads: */
	threads[0] = xt_create_daemon(self, "test0");
	printf("thread = %d\n", threads[0]->t_id);
	threads[1] = xt_create_daemon(self, "test1");
	printf("thread = %d\n", threads[1]->t_id);
	threads[2] = xt_create_daemon(self, "test2");
	printf("thread = %d\n", threads[2]->t_id);
	threads[3] = xt_create_daemon(self, "test3");
	printf("thread = %d\n", threads[3]->t_id);
	threads[4] = xt_create_daemon(self, "test4");
	printf("thread = %d\n", threads[4]->t_id);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* Max stays the same: */
	xt_free_thread(threads[3]);
	xt_free_thread(threads[2]);
	xt_free_thread(threads[1]);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* Fill in the gaps: */
	threads[1] = xt_create_daemon(self, "test1");
	printf("thread = %d\n", threads[1]->t_id);
	threads[2] = xt_create_daemon(self, "test2");
	printf("thread = %d\n", threads[2]->t_id);
	threads[3] = xt_create_daemon(self, "test3");
	printf("thread = %d\n", threads[3]->t_id);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* And add one: */
	threads[5] = xt_create_daemon(self, "test5");
	printf("thread = %d\n", threads[5]->t_id);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* Max stays the same: */
	xt_free_thread(threads[3]);
	xt_free_thread(threads[2]);
	xt_free_thread(threads[1]);
	xt_free_thread(threads[4]);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* Recalculate the max: */
	xt_free_thread(threads[5]);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	/* Fill in the gaps: */
	threads[1] = xt_create_daemon(self, "test1");
	printf("thread = %d\n", threads[1]->t_id);
	threads[2] = xt_create_daemon(self, "test2");
	printf("thread = %d\n", threads[2]->t_id);
	threads[3] = xt_create_daemon(self, "test3");
	printf("thread = %d\n", threads[3]->t_id);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);

	xt_free_thread(threads[3]);
	xt_free_thread(threads[2]);
	xt_free_thread(threads[1]);
	xt_free_thread(threads[0]);
	printf("current max threads = %d, in use = %d\n", xt_thr_current_max_threads, xt_thr_current_thread_count);
}

#ifdef UNUSED_CODE
int XTRowLocks::xt_release_locks(struct XTOpenTable *ot, xtRowID row, XTRowLockListPtr lock_list)
{
	if (ot->ot_temp_row_lock)
		xt_make_lock_permanent(ot, lock_list);

	if (!lock_list->bl_count)
		return XT_NO_LOCK;

	int					group, pgroup;
	XTXactDataPtr		xact;
	xtTableID			tab_id, ptab_id;
	XTPermRowLockPtr	plock;
	XTOpenTablePtr		pot = NULL;
	XTRowLocksPtr		row_locks;

	/* Do I have the lock? */
	group = row % XT_ROW_LOCK_COUNT;
	if (!(xact = tab_row_locks[group]))
		/* There is no lock: */
		return XT_NO_LOCK;

	if (xact != ot->ot_thread->st_xact_data)
		/* There is a lock but it does not belong to me! */
		return XT_NO_LOCK;

	tab_id = ot->ot_table->tab_id;
	plock = (XTPermRowLockPtr) &lock_list->bl_data[lock_list->bl_count * lock_list->bl_item_size];
	lock_list->rll_release_point = lock_list->bl_count;
	for (u_int i=0; i<lock_list->bl_count; i++) {
		plock--;

		pgroup = plock->pr_group;
		ptab_id = plock->pr_tab_id;

		if (ptab_id == tab_id)
			row_locks = this;
		else {
			if (pot) {
				if (pot->ot_table->tab_id == ptab_id)
					goto remove_lock;
				xt_db_return_table_to_pool_ns(pot);
				pot = NULL;
			}

			if (!xt_db_open_pool_table_ns(&pot, ot->ot_table->tab_db, tab_id)) {
				/* Should not happen, but just in case, we just don't
				 * remove the lock. We will probably end up with a deadlock
				 * somewhere.
				 */
				xt_log_and_clear_exception_ns();
				goto skip_remove_lock;
			}
			if (!pot)
				/* Can happen of the table has been dropped: */
				goto skip_remove_lock;

			remove_lock:
			row_locks = &pot->ot_table->tab_locks;
		}

#ifdef XT_TRACE_LOCKS
		xt_ttracef(xt_get_self(), "release lock group=%d\n", pgroup);
#endif
		row_locks->tab_row_locks[pgroup] = NULL;
		row_locks->tab_lock_perm[pgroup] = 0;
		skip_remove_lock:;

		lock_list->rll_release_point--;
		if (tab_id == ptab_id && group == pgroup)
			break;
	}

	if (pot) 
		xt_db_return_table_to_pool_ns(pot);
	return XT_PERM_LOCK;
}

xtBool XTRowLocks::xt_regain_locks(struct XTOpenTable *ot, int *lock_type, xtXactID *xn_id, XTRowLockListPtr lock_list)
{
	int					group;
	XTXactDataPtr		xact, my_xact;
	XTPermRowLockPtr	plock;
	xtTableID			tab_id;
	XTOpenTablePtr		pot = NULL;
	XTRowLocksPtr		row_locks = NULL;
	XTTableHPtr			tab = NULL;

	for (u_int i=lock_list->rll_release_point; i<lock_list->bl_count; i++) {
		plock = (XTPermRowLockPtr) &lock_list->bl_data[i * lock_list->bl_item_size];

		my_xact = ot->ot_thread->st_xact_data;
		group = plock->pr_group;
		tab_id = plock->pr_tab_id;

		if (tab_id == ot->ot_table->tab_id) {
			row_locks = this;
			tab = ot->ot_table;
		}
		else {
			if (pot) {
				if (tab_id == pot->ot_table->tab_id)
					goto gain_lock;
				xt_db_return_table_to_pool_ns(pot);
				pot = NULL;
			}

			if (!xt_db_open_pool_table_ns(&pot, ot->ot_table->tab_db, tab_id))
				return FAILED;
			if (!pot)
				goto no_gain_lock;
			
			gain_lock:
			tab = pot->ot_table;
			row_locks = &tab->tab_locks;
			no_gain_lock:;
		}

#ifdef XT_TRACE_LOCKS
		xt_ttracef(xt_get_self(), "regain lock group=%d\n", group);
#endif
		XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[group % XT_ROW_RWLOCKS], ot->ot_thread);
		if ((xact = row_locks->tab_row_locks[group])) {
			if (xact != my_xact) {
				*xn_id = xact->xd_start_xn_id;
				*lock_type = row_locks->tab_lock_perm[group] ? XT_PERM_LOCK : XT_TEMP_LOCK;
				goto done;
			}
		}
		else
			row_locks->tab_row_locks[group] = my_xact;
		row_locks->tab_lock_perm[group] = 1;
		XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[group % XT_ROW_RWLOCKS], ot->ot_thread);
		lock_list->rll_release_point++;
	}
	*lock_type = XT_NO_LOCK;
	return OK;

	done:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[group % XT_ROW_RWLOCKS], ot->ot_thread);
	return OK;
}

#endif
