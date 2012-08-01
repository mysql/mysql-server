/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file include/ib_mutex.h
Policy based mutexes.

Created 2012-03-24 Sunny Bains.
***********************************************************************/

#ifndef ib_mutex_h
#define ib_mutex_h

#include "univ.i"
#include "ut0rnd.h"
#include "sync0sync.h"

extern ulong	srv_spin_wait_delay;
extern ulong	srv_n_spin_wait_rounds;

#ifdef UNIV_PFS_MUTEX
/** InnoDB default policy */
struct CheckedPolicy {
	CheckedPolicy(ulint level, mysql_pfs_key_t key)
		:
		m_level(level),
		m_key(key) { }
	ulint			m_level;	/*!< Mutex ordering */
	mysql_pfs_key_t		m_key;		/*!< Performance schema key */
};
#else
/** InnoDB default policy */
struct CheckedPolicy {
	CheckedPolicy(ulint level)
		:
		m_level(level) { }
	ulint			m_level;	/*!< Mutex ordering */
};
#endif /* UNIV_PFS_MUTEX */

/** POSIX mutex. */
class POSIXMutex {
public:
	POSIXMutex(const CheckedPolicy& policy) UNIV_NOTHROW {
		int	ret;

		ret = pthread_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST);
		ut_a(ret == 0);
	}

	~POSIXMutex() UNIV_NOTHROW
	{
		int	ret;

		ret = pthread_mutex_destroy(&m_mutex);
		ut_a(ret == 0);
	}

	/** Release the muytex. */
	void exit() UNIV_NOTHROW
	{
		int	ret;

		ret = pthread_mutex_unlock(&m_mutex);

		ut_a(ret == 0);
	}

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW
	{
		int	ret;

		ret = pthread_mutex_lock(&m_mutex);

		ut_a(ret == 0);
	}

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int try_lock() UNIV_NOTHROW
	{
		return(pthread_mutex_trylock(&m_mutex));
	}

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	// FIXME:
	//bool is_owned() UNIV_NOTHROW { ut_error; return(false); }
#endif /* UNIV_DEBUG */

private:
	pthread_mutex_t			m_mutex;
};

/** InnoDB default mutex. */
class ClassicMutex {
public:
	ClassicMutex(const CheckedPolicy& policy) UNIV_NOTHROW {
		mutex_create(policy.m_key, &m_mutex, policy.m_level);
	}

	~ClassicMutex() { mutex_free(&m_mutex); }

	/** Release the muytex. */
	void exit() UNIV_NOTHROW { mutex_exit(&m_mutex); }

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW { mutex_enter(&m_mutex); }

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int try_lock() UNIV_NOTHROW { return(mutex_enter_nowait(&m_mutex)); }

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { return(mutex_own(&m_mutex)); }
#endif /* UNIV_DEBUG */

private:
	ib_mutex_t		m_mutex;
};

#ifdef HAVE_IB_LINUX_FUTEX

#define cmpxchg(p, o, n)	__sync_val_compare_and_swap(p, o, n)
#define xchg(p, v)		__sync_lock_test_and_set(p, v)

#include <linux/futex.h>
#include <sys/syscall.h>

/** Mutex that doesnot wait on an event and only spins. */
class Futex {
public:
	Futex(const CheckedPolicy& policy) UNIV_NOTHROW
	{
		m_mutex.lock_word = 0;
	}

	~Futex() { ut_a(m_mutex.lock_word == 0); }

	/** Release the muytex. */
	void exit() UNIV_NOTHROW
	{
		/* Unlock, and if not contended then exit. */
		if (m_mutex.lock_word == 2) {
			m_mutex.lock_word = 0;
		} else if (xchg(&m_mutex.lock_word, 0) == 1) {
			return;
		}

		/* Spin and hope someone takes the lock */
		for (ulint i = 0; i <= SYNC_SPIN_ROUNDS * 2; i++) {

			/* Need to set to state 2 because there
			may be waiters */

			if (m_mutex.lock_word
		            && cmpxchg(&m_mutex.lock_word, 1, 2)) {
				return;
			}

			UT_RELAX_CPU();
		}
	
		/* We need to wake someone up */
		sys_futex(
			&m_mutex.lock_word, FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
	}

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW
	{
		lock_word_t	c;
	
		/* Spin and try to take lock */
		for (ulint i = 0; i <= SYNC_SPIN_ROUNDS; i++) {

			c = cmpxchg(&m_mutex.lock_word, 0, 1);

			if (!c) {
				return;
			}
		
			UT_RELAX_CPU();
		}

		/* The lock is now contended */
		if (c == 1) {
			c = xchg(&m_mutex.lock_word, 2);
		}

		while (c) {
			/* Wait in the kernel */
			sys_futex(
				&m_mutex.lock_word,
				FUTEX_WAIT_PRIVATE, 2, 0, 0, 0);

			c = xchg(&m_mutex.lock_word, 2);
		}
	}

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int try_lock() UNIV_NOTHROW
	{
		/* Try to take the lock, if is currently unlocked */
		if (!cmpxchg(&m_mutex.lock_word, 0, 1)) {
			return(0);
		}

		return(EBUSY);
	}

#ifdef UNIV_DEBUG
	// FIXME:
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { ut_error; return(false); }
#endif /* UNIV_DEBUG */

private:
	static int sys_futex(
		volatile void*	addr1,
		int		op,
		int		val1,
		struct timespec*timeout,
		void*		addr2,
		int		val3)
	{
		return(syscall(
			SYS_futex, addr1, op, val1, timeout, addr2, val3));
	}

private:
	/** The mutex implementation */
	spin_mutex_t		m_mutex;
};

#endif /* HAVE_IB_LINUX_FUTEX */

/** Mutex that doesnot wait on an event and only spins. */
class SpinOnlyMutex {
public:
	SpinOnlyMutex(const CheckedPolicy& policy) UNIV_NOTHROW {
		mutex_create(policy.m_key, &m_mutex, policy.m_level);
	}
 
	~SpinOnlyMutex() { mutex_free(&m_mutex); }
 
 	/** Release the muytex. */
	void exit() UNIV_NOTHROW { mutex_exit(&m_mutex); }
 
 	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW { mutex_enter_spinonly(&m_mutex); }
 
 	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int try_lock() UNIV_NOTHROW { return(mutex_enter_nowait(&m_mutex)); }
 
#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { return(mutex_own(&m_mutex)); }
#endif /* UNIV_DEBUG */
private:
 	/** The mutex implementation */
	ib_mutex_t		m_mutex;
};

/** Mutex interface for all policy mutexes. */
template <typename MutexImpl, typename Policy>
class PolicyMutex {
public:
	PolicyMutex(const Policy& policy) UNIV_NOTHROW : m_impl(policy) { }
	~PolicyMutex() { }

	/** Release the mutex. */
	void exit() UNIV_NOTHROW { m_impl.exit(); }

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW { m_impl.enter(); }

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int trylock() UNIV_NOTHROW { return(m_impl.try_lock()); }

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { return(m_impl.is_owned()); }
#endif /* UNIV_DEBUG */

private:
	/** The mutex implementation */
	MutexImpl	m_impl;
};

typedef PolicyMutex<ClassicMutex, CheckedPolicy>  Mutex;
typedef PolicyMutex<POSIXMutex, CheckedPolicy>  SysMutex;
typedef PolicyMutex<SpinOnlyMutex, CheckedPolicy>  SpinMutex;

#ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<Futex, CheckedPolicy>  FutexMutex;
#endif /* HAVE_IB_LINUX_FUTEX */

#endif /* ib_mutex_h */
