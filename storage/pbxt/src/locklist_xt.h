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

#ifndef __xt_locklist_h__
#define __xt_locklist_h__

#ifdef DEBUG
#define XT_THREAD_LOCK_INFO
#ifndef XT_WIN
/* We need DEBUG_LOCKING in order to enable pthread function wrappers */
#define DEBUG_LOCKING
#endif
#endif

#include "xt_defs.h"

struct XTThread;
struct XTSpinLock;
struct XTRWMutex;
struct xt_mutex_struct;
struct xt_rwlock_struct;
struct XTFastLock;
struct XTXSMutexLock;
struct XTSpinXSLock;
struct XTAtomicRWLock;
struct XTSkewRWLock;

#ifdef XT_THREAD_LOCK_INFO

#define XT_THREAD_LOCK_INFO_MAX_COUNT 50

#ifdef XT_WIN
#define LOCKLIST_ARG_SUFFIX(name) #name " in " __FUNCTION__ "() at " __FILE__ ":" QUOTE(__LINE__)
#else
#define LOCKLIST_ARG_SUFFIX(name) #name " in " QUOTE(__PRETTY_FUNCTION__) "() at " QUOTE(__FILE__) ":" QUOTE(__LINE__)
#endif

/*
 * An instance of XTThreadLockInfo class keeps information about a lock kept by a thread.
 * There's a list of XTThreadLockInfo instances per thread. An instance can be included
 * into several thread lists in case of shared locks.
 */
typedef struct XTThreadLockInfo {

	enum LockType { SPIN_LOCK, RW_MUTEX, MUTEX, RW_LOCK, FAST_LOCK, FAST_RW_LOCK, SPIN_RW_LOCK, ATOMIC_RW_LOCK, SKEW_RW_LOCK };

	LockType		  li_lock_type;

	union {
		XTSpinLock       *li_spin_lock;	  // SPIN_LOCK
		XTRWMutex        *li_rw_mutex;	  // RW_MUTEX
		XTFastLock		 *li_fast_lock;   // FAST_LOCK
		XTXSMutexLock	 *li_fast_rwlock; // FAST_RW_LOCK
		XTSpinXSLock	 *li_spin_rwlock; // SPIN_RW_LOCK
		XTAtomicRWLock	 *li_atomic_rwlock; // ATOMIC_RW_LOCK
		xt_mutex_struct  *li_mutex;		  // MUTEX
		xt_rwlock_struct *li_rwlock;	  // RW_LOCK
		XTSkewRWLock	 *li_skew_rwlock;	// SKEW_RW_LOCK
	};
} 
XTThreadLockInfoRec, *XTThreadLockInfoPtr;

void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTSpinLock *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTRWMutex *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTFastLock *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTXSMutexLock *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTSpinXSLock *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTAtomicRWLock *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, xt_mutex_struct *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, xt_rwlock_struct *lock);
void xt_thread_lock_info_init(XTThreadLockInfoPtr ptr, XTSkewRWLock *lock);
void xt_thread_lock_info_free(XTThreadLockInfoPtr ptr);

void xt_thread_lock_info_add_owner (XTThreadLockInfoPtr ptr);
void xt_thread_lock_info_release_owner (XTThreadLockInfoPtr ptr);

void xt_trace_thread_locks(XTThread *self);

#endif // XT_THREAD_LOCK_INFO
#endif // __xt_locklist_h__
