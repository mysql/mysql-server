/* Copyright (c) 2009 PrimeBase Technologies GmbH
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
 * 2009-01-20	Vladimir Kolesnikov
 *
 * H&G2JCtL
 */

#include "xt_config.h"
#include "locklist_xt.h"

#ifdef XT_THREAD_LOCK_INFO
#include "pthread_xt.h"
#include "thread_xt.h"
#include "trace_xt.h"

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTSpinLock *lock)
{
	ptr->li_spin_lock = lock;
	ptr->li_lock_type = XTThreadLockInfo::SPIN_LOCK;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTRWMutex *lock)
{
	ptr->li_rw_mutex  = lock;
	ptr->li_lock_type = XTThreadLockInfo::RW_MUTEX;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTFastLock *lock)
{
	ptr->li_fast_lock  = lock;
	ptr->li_lock_type = XTThreadLockInfo::FAST_LOCK;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, xt_mutex_struct *lock)
{
	ptr->li_mutex     = lock;
	ptr->li_lock_type = XTThreadLockInfo::MUTEX;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, xt_rwlock_struct *lock)
{
	ptr->li_rwlock    = lock;
	ptr->li_lock_type = XTThreadLockInfo::RW_LOCK;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTXSMutexLock *lock)
{
	ptr->li_fast_rwlock = lock;
	ptr->li_lock_type   = XTThreadLockInfo::FAST_RW_LOCK;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTSpinXSLock *lock)
{
	ptr->li_spin_rwlock = lock;
	ptr->li_lock_type   = XTThreadLockInfo::SPIN_RW_LOCK;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTAtomicRWLock *lock)
{
	ptr->li_atomic_rwlock = lock;
	ptr->li_lock_type   = XTThreadLockInfo::ATOMIC_RW_LOCK;
}

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTSkewRWLock *lock)
{
	ptr->li_skew_rwlock = lock;
	ptr->li_lock_type   = XTThreadLockInfo::SKEW_RW_LOCK;
}

void xt_thread_lock_info_free(XTThreadLockInfoPtr ptr)
{
	/* TODO: check to see if it's present in a thread's list */
}

void xt_thread_lock_info_add_owner (XTThreadLockInfoPtr ptr)
{
	XTThread *self = xt_get_self();

	if (!self)
		return;

	if (self->st_thread_lock_count < XT_THREAD_LOCK_INFO_MAX_COUNT) {
		self->st_thread_lock_list[self->st_thread_lock_count] = ptr;
		self->st_thread_lock_count++;
	}
}

void xt_thread_lock_info_release_owner (XTThreadLockInfoPtr ptr)
{
	XTThread *self = xt_get_self();

	if (!self)
		return;

	for (int i = self->st_thread_lock_count - 1; i >= 0; i--) {
		if (self->st_thread_lock_list[i] == ptr) {
			self->st_thread_lock_count--;
			memcpy(self->st_thread_lock_list + i, 
				self->st_thread_lock_list + i + 1, 
				(self->st_thread_lock_count - i)*sizeof(XTThreadLockInfoPtr));
			self->st_thread_lock_list[self->st_thread_lock_count] = NULL;
			break;
		}
	}
}

void xt_trace_thread_locks(XTThread *self)
{
	if (!self)
		return;

	xt_ttracef(self, "thread lock list (first in list added first): ");

	if (!self->st_thread_lock_count) {
		xt_trace(" <empty>\n");
		return;
	}

	xt_trace("\n");

	int count = min(self->st_thread_lock_count, XT_THREAD_LOCK_INFO_MAX_COUNT);

	for(int i = 0; i < count; i++) {

		const char *lock_type = NULL;
		const char *lock_name = NULL;

		XTThreadLockInfoPtr li = self->st_thread_lock_list[i];

		switch(li->li_lock_type) {
			case XTThreadLockInfo::SPIN_LOCK:
				lock_type = "XTSpinLock";
				lock_name = li->li_spin_lock->spl_name;
				break;
			case XTThreadLockInfo::RW_MUTEX:
				lock_type = "XTRWMutex";
				lock_name = li->li_rw_mutex->xs_name;
				break;
			case XTThreadLockInfo::MUTEX:
				lock_type = "xt_mutex_struct";
#ifdef XT_WIN
				lock_name = li->li_mutex->mt_name;
#else
				lock_name = li->li_mutex->mu_name;
#endif
				break;
			case XTThreadLockInfo::RW_LOCK:
				lock_type = "xt_rwlock_struct";
				lock_name = li->li_rwlock->rw_name;
				break;
			case XTThreadLockInfo::FAST_LOCK:
				lock_type = "XTFastLock";
				lock_name = li->li_fast_lock->fal_name;
				break;
			case XTThreadLockInfo::FAST_RW_LOCK:
				lock_type = "XTXSMutexLock";
				lock_name = li->li_fast_rwlock->xsm_name;
				break;
			case XTThreadLockInfo::SPIN_RW_LOCK:
				lock_type = "XTSpinRWLock";
				lock_name = li->li_spin_rwlock->sxs_name;
				break;
			case XTThreadLockInfo::ATOMIC_RW_LOCK:
				lock_type = "XTAtomicRWLock";
				lock_name = li->li_atomic_rwlock->arw_name;
				break;
		}

		xt_ttracef(self, "  #lock#%d: type: %s name: %s \n", count, lock_type, lock_name);
	}
}

#endif

