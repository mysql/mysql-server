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

#include "sync0policy.h"

#ifndef ib_mutex_h
#define ib_mutex_h

/** POSIX mutex. */
template <template <typename> class Policy = DefaultPolicy>
struct POSIXMutex {

	typedef Policy<POSIXMutex> MutexPolicy;

	POSIXMutex() UNIV_NOTHROW
	{
		int	ret;

		ret = pthread_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST);
		ut_a(ret == 0);
		ut_d(m_locked = false);
	}

	~POSIXMutex() UNIV_NOTHROW
	{
		int	ret;

		ut_ad(!m_locked);
		ret = pthread_mutex_destroy(&m_mutex);
		ut_a(ret == 0);
	}

	/** Release the muytex. */
	void exit() UNIV_NOTHROW
	{
		int	ret;

		ut_ad(m_locked);
		ut_d(m_locked = false);

		// FIXME: Do we check for EINTR?
		ret = pthread_mutex_unlock(&m_mutex);
		ut_a(ret == 0);
	}

	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
		int	ret;

		ret = pthread_mutex_lock(&m_mutex);

		ut_a(ret == 0);
		ut_ad(!m_locked);
		ut_d(m_locked = true);
	}

	/** @return true if locking succeeded */
	bool try_lock(const char* name, ulint line) UNIV_NOTHROW
	{
		bool	locked = pthread_mutex_trylock(&m_mutex) == 0;
#ifdef UNIV_DEBUG
		if (locked) {
			ut_ad(!m_locked);
			ut_d(m_locked = locked);
		}
#endif /* UNIV_DEBUG */

		return(locked);
	}

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(m_locked && m_policy.is_owned());
	}
#endif /* UNIV_DEBUG */

private:
#ifdef UNIV_DEBUG
	bool			m_locked;
#endif /* UNIV_DEBUG */

	pthread_mutex_t		m_mutex;

public:
	MutexPolicy		m_policy;
};

#ifdef HAVE_IB_LINUX_FUTEX

#include <linux/futex.h>
#include <sys/syscall.h>

/** Mutex implementation that used the Linux futex. */
template <template <typename> class Policy = DefaultPolicy>
struct Futex {

	typedef Policy<Futex> MutexPolicy;

	Futex() UNIV_NOTHROW
		:
		m_lock_word(MUTEX_STATE_UNLOCKED)
	{
		/* Check that lock_word is aligned. */
		ut_ad(!((ulint) &m_lock_word % sizeof(ulint)));
	}

	~Futex()
	{
		ut_a(m_lock_word == MUTEX_STATE_UNLOCKED);
	}

	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
		lock_word_t	lock = MutexPolicy::trylock_poll(*this);

		/* If there were no waiters when this lock tried
		to acquire the mutex then set the waiters flag now. */

		if (lock != MUTEX_STATE_UNLOCKED) {

			/* When this thread set the waiters flag it is
			possible that the mutex had already been released
			by then. In this case the thread can assume it
			was granted the mutex. */

			if (lock == MUTEX_STATE_LOCKED && set_waiters()) {
				return;
			}

			wait();
		}
	}

	/** Release the mutex. */
	void exit() UNIV_NOTHROW
	{
		/* If there are threads waiting then we have to wake
		them up. Reset the lock state to unlocked so that waiting
		threads can test for success. */

		if (m_lock_word == MUTEX_STATE_WAITERS) {

			m_lock_word = MUTEX_STATE_UNLOCKED;

		} else if (unlock() == MUTEX_STATE_LOCKED) {
			/* No threads waiting, no need to signal a wakeup. */
			return;
		}

		signal();
	}

	/** Try and lock the mutex.
	@return the old state of the mutex */
	lock_word_t trylock() UNIV_NOTHROW
	{	
		return(CAS(&m_lock_word,
			    MUTEX_STATE_UNLOCKED, MUTEX_STATE_LOCKED));
	}

	/** Try and lock the mutex.
	@return true if successful */
	bool try_lock() UNIV_NOTHROW
	{
		return(trylock() == MUTEX_STATE_UNLOCKED);
	}

	/** @return true if mutex is unlocked */
	bool is_locked() const UNIV_NOTHROW
	{
		return(state() != MUTEX_STATE_UNLOCKED);
	}

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(is_locked() && m_policy.is_owned());
	}
#endif /* UNIV_DEBUG */

private:
	/**
	@return the lock state. */
	lock_word_t state() const UNIV_NOTHROW
	{
		/** Try and force a memory read. */
		const volatile void*	p = &m_lock_word;

		return(*(lock_word_t*) p);
	}

	/** Release the mutex.
	@return the new state of the mutex */
	lock_word_t unlock() UNIV_NOTHROW
	{
		return(TAS(&m_lock_word, MUTEX_STATE_UNLOCKED));
	}

	/** Note that there are threads waiting and need to be woken up.
	@return true if state was MUTEX_STATE_UNLOCKED (ie. granted) */
	bool set_waiters() UNIV_NOTHROW
	{
		return(TAS(&m_lock_word, MUTEX_STATE_WAITERS)
		       == MUTEX_STATE_UNLOCKED);
	}

	/** Set the waiters flag, only if the mutex is locked
	@return true if succesful. */
	bool try_set_waiters() UNIV_NOTHROW
	{
		return(CAS(&m_lock_word,
			   MUTEX_STATE_LOCKED, MUTEX_STATE_WAITERS)
		       == MUTEX_STATE_LOCKED);
	}

	/** Wait if the lock is contended. */
	void wait() UNIV_NOTHROW
	{
		do {
			/* Use FUTEX_WAIT_PRIVATE because our mutexes are
			not shared between processes. */

			syscall(SYS_futex, &m_lock_word,
				FUTEX_WAIT_PRIVATE, MUTEX_STATE_WAITERS,
				0, 0, 0);

		// FIXME: Do we care about the return value?

		} while (!set_waiters());
	}

	/** Wakeup a waiting thread */
	void signal() UNIV_NOTHROW
	{
		/* Use FUTEX_WAIT_PRIVATE because our mutexes are
		not shared between processes. */

		syscall(SYS_futex, &m_lock_word,
			FUTEX_WAKE_PRIVATE, MUTEX_STATE_LOCKED,
			0, 0, 0);

		// FIXME: Do we care about the return value?
	}

private:
	volatile lock_word_t	m_lock_word;

public:
	MutexPolicy		m_policy;
};

#endif /* HAVE_IB_LINUX_FUTEX */

template <template <typename> class Policy = DefaultPolicy>
struct TTASMutex {

	typedef Policy<TTASMutex> MutexPolicy;

	TTASMutex() UNIV_NOTHROW
		:
		m_lock_word(MUTEX_STATE_UNLOCKED)
	{
		/* Check that lock_word is aligned. */
		ut_ad(!((ulint) &m_lock_word % sizeof(ulint)));
	}
 
	~TTASMutex()
	{
		ut_ad(m_lock_word == MUTEX_STATE_UNLOCKED);
	}
 
 	/** Try and lock the mutex. Note: POSIX returns 0 on success.
	@return true on success */
	bool try_lock() UNIV_NOTHROW
	{
		return(CAS(&m_lock_word,
			   MUTEX_STATE_UNLOCKED, MUTEX_STATE_LOCKED)
		       == MUTEX_STATE_UNLOCKED);

	}
 
 	/** Release the mutex. */
	void exit() UNIV_NOTHROW
	{
#ifdef UNIV_DEBUG
		switch (state()) {
		case MUTEX_STATE_WAITERS:
			/* It is a spin lock, can't be any waiters. */
			ut_error;

		case MUTEX_STATE_UNLOCKED:
			/* This is an invalid state. */
			ut_error;

		case MUTEX_STATE_LOCKED:
			break;
		}

		lock_word_t	lock =
#endif /* UNIV_DEBUG */

		TAS(&m_lock_word, MUTEX_STATE_UNLOCKED);

		ut_ad(lock == MUTEX_STATE_LOCKED);
	}

 	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
		if (!try_lock()) {
			busy_wait();
		}
	}

	/**
	@return the lock state. */
	lock_word_t state() const UNIV_NOTHROW
	{
		/** Try and force a memory read. */
		const volatile void*	p = &m_lock_word;

		return(*(lock_word_t*) p);
	}

	/** @return true if locked by some thread */
	bool is_locked() const UNIV_NOTHROW
	{
		return(state() != MUTEX_STATE_UNLOCKED);
	}

#ifdef UNIV_DEBUG
	/** @return true if the calling thread owns the mutex. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(is_locked() && m_policy.is_owned());
	}
#endif /* UNIV_DEBUG */

private:
	/**
	Spin and try to acquire the lock. */
	void busy_wait() UNIV_NOTHROW
	{
		/* The Test, Test again And Set (TTAS) loop. */

		for (;;) {
			MutexPolicy::test_poll(*this);

			/* Test again and set */
			switch (CAS(&m_lock_word,
				    MUTEX_STATE_UNLOCKED, MUTEX_STATE_LOCKED)) {

			case MUTEX_STATE_LOCKED:
				break;

			case MUTEX_STATE_UNLOCKED:
				return;

			case MUTEX_STATE_WAITERS:
				/* This is a spin only mutex. */
				ut_error;
			}

			os_thread_yield();
		}
	}

private:
	// Disable copying
	TTASMutex(const TTASMutex&);
	TTASMutex& operator=(const TTASMutex&);

	/** lock_word is the target of the atomic test-and-set instruction
	when atomic operations are enabled. */
	volatile lock_word_t	m_lock_word;

public:
	/** Policy data */
	MutexPolicy		m_policy;
};

template <template <typename> class Policy = DefaultPolicy>
struct TTASWaitMutex {

	typedef Policy<TTASWaitMutex> MutexPolicy;

	TTASWaitMutex() UNIV_NOTHROW
		:
		m_event(),
		m_waiters(),
		m_lock_word(MUTEX_STATE_UNLOCKED)
	{
		/* Check that lock_word is aligned. */
		ut_ad(!((ulint) &m_lock_word % sizeof(ulint)));

		m_event = os_event_create(NULL);
	}
 
	~TTASWaitMutex()
	{
		ut_ad(m_lock_word == MUTEX_STATE_UNLOCKED);

		os_event_free(m_event);
		m_event = 0;
	}
 
 	/** Try and lock the mutex. Note: POSIX returns 0 on success.
	@return true on success */
	bool try_lock() UNIV_NOTHROW
	{
		return(tas_lock());
	}
 
 	/** Release the mutex. */
	void exit() UNIV_NOTHROW
	{
		/* A problem: we assume that mutex_reset_lock word
		is a memory barrier, that is when we read the waiters
		field next, the read must be serialized in memory
		after the reset. A speculative processor might
		perform the read first, which could leave a waiting
		thread hanging indefinitely.

		Our current solution call every second
		sync_arr_wake_threads_if_sema_free()
		to wake up possible hanging threads if they are missed
		in mutex_signal_object. */

		tas_unlock();

		if (*waiters() != 0) {
			signal();
		}
	}

 	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
		/* Note that we do not peek at the value of m_lock_word
		before trying the atomic test_and_set; we could peek,
		and possibly save time. */

		if (!try_lock()) {
			spin_and_wait(filename, line);
		}
	}

	/**
	@return the lock state. */
	lock_word_t state() const UNIV_NOTHROW
	{
		/** Try and force a memory read. */
		const volatile void*	p = &m_lock_word;

		return(*(lock_word_t*) p);
	}

	/** The event that the mutex will wait in sync0arr.cc 
	@return even instance */
	os_event_t event() UNIV_NOTHROW
	{
		return(m_event);
	}

	/** @return true if locked by some thread */
	bool is_locked() const UNIV_NOTHROW
	{
		return(state() != MUTEX_STATE_UNLOCKED);
	}

#ifdef UNIV_DEBUG
	/** @return true if the calling thread owns the mutex. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(is_locked() && m_policy.is_owned());
	}
#endif /* UNIV_DEBUG */

private:
	/**
	Try and acquire the mutex by spinning.
	@param n_spins - spin count
	@param max spins - maximum number of spins
	@param max delay - random delay upper bound
	@return current spin count */

	ulint spin(ulint n_spins, ulint max_spins, ulint max_delay) UNIV_NOTHROW
	{
		/* Spin waiting for the lock word to become zero. Note
		that we do not have to assume that the read access to
		the lock word is atomic, as the actual locking is always
		committed with atomic test-and-set. In reality, however,
		all processors probably have an atomic read of a memory word. */

		ulint	n = n_spins / max_spins;

		while (!(n_spins++ % max_spins)) {

			if (m_lock_word == MUTEX_STATE_UNLOCKED) {
				break;
			}

			ulint	m = n * ut_rnd_interval(0, max_delay);

			for (ulint i = 0; i < m; ++i) {
				UT_RELAX_CPU();
			}
		}

		return(n_spins);
	}

	/**
	Wait in the sync array.
	@return true if the mutex acquisition was successful. */
	bool wait(const char* file_name, ulint line) UNIV_NOTHROW;

	/**
	Reserves a mutex for the current thread. If the mutex is reserved,
	the function spins a preset time (controlled by srv_n_spin_wait_rounds),
       	waiting for the mutex before suspending the thread. */
	void spin_and_wait(const char* file_name, ulint line) UNIV_NOTHROW
	{
		ulint	n_spins = 0;
		ulint	max_spins = srv_n_spin_wait_rounds;
		ulint	max_delay = srv_spin_wait_delay;

		for (;;) {

			n_spins = spin(n_spins, max_spins, max_delay);

			if (try_lock()) {
				break;
			}

			os_thread_yield();

			if (wait(file_name, line)) {
				break;
			}
		}
	}

	/**
	@return the value of the m_waiters flag */
	volatile ulint* waiters() UNIV_NOTHROW
	{
		/* Declared volatile in the hope that the value is
		read from memory */

	        volatile ulint*   ptr;

		ptr = &m_waiters;

		/* Here we assume that the read of a single
		word from memory is atomic */

		return(ptr);
	}

	/* Note that there are threads waiting on the mutex */
	void set_waiters() UNIV_NOTHROW
	{
		*waiters() = 1;
	}

	/* Note that there are no threads waiting on the mutex */
	void clear_waiters() UNIV_NOTHROW
	{
		*waiters() = 0;
	}

	/**
	Try and acquire the lock using TestAndSet.
	@return	true if lock succeeded */
	bool tas_lock() UNIV_NOTHROW
	{
		return(os_atomic_test_and_set_ulint(
				&m_lock_word, MUTEX_STATE_LOCKED)
			== MUTEX_STATE_UNLOCKED);
	}

	/** In theory __sync_lock_release should be used to release the lock.
	Unfortunately, it does not work properly alone. The workaround is
	that more conservative __sync_lock_test_and_set is used instead. */
	void tas_unlock() UNIV_NOTHROW
	{
		os_atomic_test_and_set_ulint(
			&m_lock_word, MUTEX_STATE_UNLOCKED);
	}

	/** Wakeup any waiting thread(s). */
	void signal() UNIV_NOTHROW;

private:
	// Disable copying
	TTASWaitMutex(const TTASWaitMutex&);
	TTASWaitMutex& operator=(const TTASWaitMutex&);

	/** Used by sync0arr.cc for the wait queue */
	os_event_t		m_event;

	/** Set to 0 or 1. 1 if there are (or may be) threads waiting
	in the global wait array for this mutex to be released. */
	ulint   		m_waiters;

	/** lock_word is the target of the atomic test-and-set instruction
	when atomic operations are enabled. */
	volatile lock_word_t	m_lock_word;

public:
	/** Policy data */

	MutexPolicy		m_policy;
};

/** Mutex interface for all policy mutexes. */
template <typename MutexImpl>
struct PolicyMutex
{
	typedef MutexImpl MutexType;
	typedef typename MutexImpl::MutexPolicy Policy;

	PolicyMutex() UNIV_NOTHROW : m_impl() { }
	~PolicyMutex() { }

	/** Release the mutex. */
	void exit() UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		pfs_exit(); 
#endif /* UNIV_PFS_MUTEX */

		m_impl.exit();

		m_impl.m_policy.release(m_impl);
	}

	/** Acquire the mutex.
	@name - filename where locked
	@line - line number where locked */
	void enter(const char* name = 0, ulint line = 0) UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		PSI_mutex_locker* locker = pfs_begin(name, line);
#endif /* UNIV_PFS_MUTEX */

		m_impl.m_policy.enter(m_impl);

		m_impl.enter(name, line);

		m_impl.m_policy.locked(m_impl);

#ifdef UNIV_PFS_MUTEX
		pfs_end(locker, 0); 
#endif /* UNIV_PFS_MUTEX */
	}

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int trylock(const char* name = 0, ulint line = 0) UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		PSI_mutex_locker* locker = pfs_begin(name, line);
#endif /* UNIV_PFS_MUTEX */

		m_impl.m_policy.enter(m_impl);

		int ret = !m_impl.try_lock();

		if (ret == 0) {
			m_impl.m_policy.locked(m_impl);
		}

#ifdef UNIV_PFS_MUTEX
		pfs_end(locker, ret); 
#endif /* UNIV_PFS_MUTEX */

		return(ret);
	}

#ifdef UNIV_DEBUG

	/** @return true if the thread owns the mutex. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(m_impl.is_owned());
	}
#endif /* UNIV_DEBUG */

	/**
	Initialise the mutex.

	@param name - mutex name
	@param filename - file where created
	@param line - line number in file where created */
	void init(const char* name, const char* filename, ulint line)
		UNIV_NOTHROW;

	void destroy() UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		pfs_del();
#endif /* UNIV_PFS_MUTEX */
	}	

private:
#ifdef UNIV_PFS_MUTEX

	/** Performance schema monitoring.
	@param name - file name where locked
	@param line - line number in file where locked */
	PSI_mutex_locker* pfs_begin(const char* name, ulint line) UNIV_NOTHROW
	{
		if (m_ptr != 0) {
			PSI_mutex_locker_state	state;

			return(PSI_MUTEX_CALL(start_mutex_wait)(
					&state, m_ptr,
					PSI_MUTEX_LOCK, name, line));
		}

		return(0);
	}

	/** Performance schema monitoring
	@param ret - 0 for success and 1 for failure */
	void pfs_end(PSI_mutex_locker* locker, int ret) UNIV_NOTHROW
	{
		if (locker != 0) {
			PSI_MUTEX_CALL(end_mutex_wait)(locker, ret);
		}
	}

	/** Performance schema monitoring - register mutex release */
	void pfs_exit()
	{
		if (m_ptr != 0) {
			PSI_MUTEX_CALL(unlock_mutex)(m_ptr);
		}
	}

	/** Performance schema monitoring - register mutex
	@param key - Performance Schema key.
	@param ptr - pointer to memory */
	void pfs_add(mysql_pfs_key_t key) UNIV_NOTHROW
	{
		ut_ad(m_ptr == 0);
		m_ptr = PSI_MUTEX_CALL(init_mutex)(key, &m_impl);
	}

	/** Performance schema monitoring - deregister */
	void pfs_del()
	{
		if (m_ptr != 0) {
			PSI_MUTEX_CALL(destroy_mutex)(m_ptr);
			m_ptr = 0;
		}
	}

	/** The performance schema instrumentation hook. */
	PSI_mutex*		m_ptr;
#endif /* UNIV_PFS_MUTEX */

	/** The mutex implementation */
	MutexImpl		m_impl;
};

#ifndef UNIV_DEBUG

# ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<Futex<DefaultPolicy> >  FutexMutex;
# endif /* HAVE_IB_LINUX_FUTEX */

typedef PolicyMutex<POSIXMutex<DefaultPolicy> > SysMutex;
typedef PolicyMutex<TTASMutex<DefaultPolicy> > SpinMutex;
typedef PolicyMutex<TTASWaitMutex<TrackPolicy> > Mutex;

#else

# ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<Futex<DebugPolicy> > FutexMutex;
# endif /* HAVE_IB_LINUX_FUTEX */

typedef PolicyMutex<POSIXMutex<DebugPolicy> > SysMutex;
typedef PolicyMutex<TTASMutex<DebugPolicy> > SpinMutex;
typedef PolicyMutex<TTASWaitMutex<DebugPolicy> > Mutex;

#endif /* !UNIV_DEBUG */

typedef Mutex ib_mutex_t;

#include "ib_mutex.ic"

#endif /* ib_mutex_h */
