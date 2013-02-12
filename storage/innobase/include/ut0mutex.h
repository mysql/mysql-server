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
@file include/ut0mutex.h
Policy based mutexes.

Created 2012-03-24 Sunny Bains.
***********************************************************************/

#include "sync0policy.h"

#ifndef ut0mutex_h
#define ut0mutex_h

/** OS event mutex. We can't track the locking because this mutex is used
by the events code. The mutex can be released by the condition variable
and therefore we lose the tracking information. */
template <template <typename> class Policy = DefaultPolicy>
struct OSBasicMutex {

	typedef Policy<OSBasicMutex> MutexPolicy;

	OSBasicMutex(bool track = false) 
		:
		m_policy(track) UNIV_NOTHROW
	{
		ut_d(m_freed = true);
	}

	~OSBasicMutex() UNIV_NOTHROW
	{
		// FIXME: This invariant doesn't hold if we exit
		// without invoking the shutdown code.
		//ut_ad((m_freed);
	}

	/** Required for os_event_t */
	operator sys_mutex_t*() UNIV_NOTHROW
	{
		return(&m_mutex);
	}

	/** Initialise the mutex. */
	void init(
		const char*	name,
		const char*	filename,
		ulint		line) UNIV_NOTHROW
	{
		ut_ad(m_freed);
#ifdef __WIN__
		InitializeCriticalSection((LPCRITICAL_SECTION) &m_mutex);
#else
		{
			int	ret;

			ret = pthread_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST);
			ut_a(ret == 0);
		}
#endif /* __WIN__ */
		ut_d(m_freed = false);

		m_policy.init(*this, name, filename, line);
	}
	
	/** Destroy the mutex */
	void destroy() UNIV_NOTHROW
	{
                ut_ad(!m_freed);
#ifdef __WIN__
		DeleteCriticalSection((LPCRITICAL_SECTION) &m_mutex);
#else
		int	ret;

		ret = pthread_mutex_destroy(&m_mutex);

		if (ret != 0) {

			fprintf(stderr,
				" InnoDB: Return value %lu when calling "
				"pthread_mutex_destroy(). Byte contents of the "
				"pthread mutex at %p:",
				(ulint) ret, (void*) &m_mutex);

			ut_print_buf(stderr, &m_mutex, sizeof(m_mutex));
			putc('\n', stderr);
		}
#endif /* __WIN__ */

		m_policy.destroy();

		ut_d(m_freed = true);
	}

	/** Release the muytex. */
	void exit() UNIV_NOTHROW
	{
		ut_ad(!m_freed);
#ifdef __WIN__
		LeaveCriticalSection(&m_mutex);
#else
		// FIXME: Do we check for EINTR?
		int	ret = pthread_mutex_unlock(&m_mutex);
		ut_a(ret == 0);
#endif /* __WIN__ */
	}

	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
                ut_ad(!m_freed);
#ifdef __WIN__
		EnterCriticalSection((LPCRITICAL_SECTION) &m_mutex);
#else
		int	ret = pthread_mutex_lock(&m_mutex);
		ut_a(ret == 0);
#endif /* __WIN__ */
	}

	/** @return true if locking succeeded */
	bool try_lock() UNIV_NOTHROW
	{
                ut_ad(!m_freed);
#ifdef __WIN__
		return(TryEnterCriticalSection(&m_mutex) != 0);
#else
		/* NOTE that the MySQL my_pthread.h redefines
		pthread_mutex_trylock so that it returns 0 on
		success. In the operating system libraries, HP-UX-10.20
		follows the old Posix 1003.4a Draft 4 and returns 1
		on success (but MySQL remaps that to 0), while Linux,
		FreeBSD, Solaris, AIX, Tru64 Unix, HP-UX-11.0 return
		0 on success. */

		return(pthread_mutex_trylock(&m_mutex) == 0);
#endif /* __WIN__ */
	}

#ifdef UNIV_DEBUG
	/** @return true if some thread owns the mutex. Because these are
        used by os_event_t we cannot track the ownership reliably. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(m_policy.is_owned());
	}
#endif /* UNIV_DEBUG */

private:
#ifdef UNIV_DEBUG
        /** true if the mutex has been freed/destroyed. */
	bool			m_freed;

#endif /* UNIV_DEBUG */

	sys_mutex_t		m_mutex;

public:
	MutexPolicy		m_policy;
};

/** OS debug mutex. */
template <template <typename> class Policy = DefaultPolicy>
struct OSTrackedMutex : public OSBasicMutex<Policy> {

	typedef Policy<OSTrackedMutex> MutexPolicy;

	OSTrackedMutex() 
		:
		OSBasicMutex<Policy>(true) UNIV_NOTHROW
	{
		ut_d(m_locked = false);
	}

	~OSTrackedMutex() UNIV_NOTHROW
	{
		ut_ad(!m_locked);
	}

	/** Initialise the mutex. */
	void init(
		const char*	name,
		const char*	filename,
		ulint		line) UNIV_NOTHROW
	{
		ut_ad(!m_locked);
                OSBasicMutex<Policy>::init(name, filename, line);
	}
	
	/** Destroy the mutex */
	void destroy() UNIV_NOTHROW
	{
		ut_ad(!m_locked);
                OSBasicMutex<Policy>::destroy();
	}

	/** Release the muytex. */
	void exit() UNIV_NOTHROW
	{
		ut_ad(m_locked);
		ut_d(m_locked = false);

                OSBasicMutex<Policy>::exit();
	}

	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
                OSBasicMutex<Policy>::enter(filename, line);

		ut_ad(!m_locked);
		ut_d(m_locked = true);
	}

	/** @return true if locking succeeded */
	bool try_lock() UNIV_NOTHROW
	{
		bool	locked = OSBasicMutex<Policy>::try_lock();

		if (locked) {
			ut_ad(!m_locked);
			ut_d(m_locked = locked);
		}

		return(locked);
	}

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() const UNIV_NOTHROW
	{
		return(m_locked && OSBasicMutex<Policy>::is_owned());
	}
#endif /* UNIV_DEBUG */

private:
#ifdef UNIV_DEBUG
        /** true if the mutex has been locked. */
	bool			m_locked;
#endif /* UNIV_DEBUG */
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

	/** Initialise the mutex. */
	void init(
		const char*	name,
		const char*	filename,
		ulint		line) UNIV_NOTHROW
	{
		ut_a(m_lock_word == MUTEX_STATE_UNLOCKED);
		m_policy.init(*this, name, filename, line);
	}

	/** Destroy the mutex. */
	void destroy() UNIV_NOTHROW
	{
		/* The destructor can be called at shutdown. */
		ut_a(m_lock_word == MUTEX_STATE_UNLOCKED);
		m_policy.destroy();
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
 
	/** Initialise the mutex. */
	void init(
		const char*	name,
		const char*	filename,
		ulint		line) UNIV_NOTHROW
	{
		ut_ad(m_lock_word == MUTEX_STATE_UNLOCKED);
		m_policy.init(*this, name, filename, line);
	}

	/** Destroy the mutex. */
	void destroy() UNIV_NOTHROW
	{
		/* The destructor can be called at shutdown. */
		ut_ad(m_lock_word == MUTEX_STATE_UNLOCKED);
		m_policy.destroy();
	}

	/**
	Try and acquire the lock using TestAndSet.
	@return	true if lock succeeded */
	bool tas_lock() UNIV_NOTHROW
	{
		return(TAS(&m_lock_word, MUTEX_STATE_LOCKED)
			== MUTEX_STATE_UNLOCKED);
	}

	/** In theory __sync_lock_release should be used to release the lock.
	Unfortunately, it does not work properly alone. The workaround is
	that more conservative __sync_lock_test_and_set is used instead. */
	void tas_unlock() UNIV_NOTHROW
	{
#ifdef UNIV_DEBUG
		ut_ad(state() == MUTEX_STATE_LOCKED);

		lock_word_t	lock =
#endif /* UNIV_DEBUG */

		TAS(&m_lock_word, MUTEX_STATE_UNLOCKED);

		ut_ad(lock == MUTEX_STATE_LOCKED);
	}

 	/** Try and lock the mutex.
	@return true on success */
	bool try_lock() UNIV_NOTHROW
	{
		return(tas_lock());
	}
 
 	/** Release the mutex. */
	void exit() UNIV_NOTHROW
	{
		tas_unlock();
	}

 	/** Acquire the mutex. */
	void enter(const char* filename, ulint line) UNIV_NOTHROW
	{
		if (!try_lock()) {
			busy_wait();
		}
	}

	/** @return the lock state. */
	lock_word_t state() const UNIV_NOTHROW
	{
		/** Try and force a memory read. */
		const volatile void*	p = &m_lock_word;

		return(*(lock_word_t*) p);
	}

	/** @return true if locked by some thread */
	bool is_locked() const UNIV_NOTHROW
	{
		return(m_lock_word != MUTEX_STATE_UNLOCKED);
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

		do {
			ulint	i;

			for (i  = 0;
			     !is_locked() && i < srv_n_spin_wait_rounds;
			     ++i) {

				if (srv_spin_wait_delay) {
					ut_delay(ut_rnd_interval(
						 0, srv_spin_wait_delay));
				}
			}

			if (i == srv_n_spin_wait_rounds) {
				os_thread_yield();
			}

		} while (!try_lock());
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
	}
 
	~TTASWaitMutex()
	{
		// FIXME: This invariant doesn't hold if we exit
		// without invoking the shutdown code.
		//ut_ad(m_event == 0 || srv_force_recovery_crash);
		ut_ad(m_lock_word == MUTEX_STATE_UNLOCKED);
	}
 
	/** Initialise the mutex. */
	void init(
		const char*	name,
		const char*	filename,
		ulint		line) UNIV_NOTHROW
	{
		ut_a(m_event == 0);
		ut_a(m_lock_word == MUTEX_STATE_UNLOCKED);

		m_event = os_event_create(name);

		m_policy.init(*this, name, filename, line);
	}

	/**
	This is the real desctructor. This mutex can be created in BSS and
	its desctructor will be called on exit(). We can't call
	os_event_destroy() at that stage. */
	void destroy()
	{
		ut_ad(m_lock_word == MUTEX_STATE_UNLOCKED);

		/* We have to free the event before InnoDB shuts down. */
		os_event_destroy(m_event);
		m_event = 0;

		m_policy.destroy();
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
		return(m_lock_word != MUTEX_STATE_UNLOCKED);
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
	Spin and wait for the mutex to become free.
	@param i - spin start index
	@return number of spins */
	ulint spin(ulint i) const UNIV_NOTHROW
	{
		ut_ad(i >= 0 && i < srv_n_spin_wait_rounds);

		/* Spin waiting for the lock word to become zero. Note
		that we do not have to assume that the read access to
		the lock word is atomic, as the actual locking is always
		committed with atomic test-and-set. In reality, however,
		all processors probably have an atomic read of a memory word. */

		while (is_locked() && i < srv_n_spin_wait_rounds) {

			if (srv_spin_wait_delay) {

				ut_delay(ut_rnd_interval(
						0, srv_spin_wait_delay));
			}

			++i;
		}

		return(i);
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

		for (;;) {

			n_spins = spin(n_spins);

			if (n_spins < srv_n_spin_wait_rounds) {

				if (try_lock()) {
					return;
				} else {
					continue;
				}
			}

			os_thread_yield();

			if (wait(file_name, line)) {
				break;
			}

			n_spins = 0;
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

	/** Note that there are threads waiting on the mutex */
	void set_waiters() UNIV_NOTHROW
	{
		*waiters() = 1;
	}

	/** Note that there are no threads waiting on the mutex */
	void clear_waiters() UNIV_NOTHROW
	{
		*waiters() = 0;
	}

	/**
	Try and acquire the lock using TestAndSet.
	@return	true if lock succeeded */
	bool tas_lock() UNIV_NOTHROW
	{
		return(TAS(&m_lock_word, MUTEX_STATE_LOCKED)
			== MUTEX_STATE_UNLOCKED);
	}

	/** In theory __sync_lock_release should be used to release the lock.
	Unfortunately, it does not work properly alone. The workaround is
	that more conservative __sync_lock_test_and_set is used instead. */
	void tas_unlock() UNIV_NOTHROW
	{
		TAS(&m_lock_word, MUTEX_STATE_UNLOCKED);
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

	PolicyMutex() UNIV_NOTHROW : m_impl()
	{
#ifdef UNIV_PFS_MUTEX
		m_ptr = 0;
#endif /* UNIV_PFS_MUTEX */
	}

	~PolicyMutex() { }

	/** Release the mutex. */
	void exit() UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		pfs_exit(); 
#endif /* UNIV_PFS_MUTEX */

		m_impl.m_policy.release(m_impl);

		m_impl.exit();
	}

	/** Acquire the mutex.
	@name - filename where locked
	@line - line number where locked */
	void enter(const char* name = 0, ulint line = 0) UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		/* Note: locker is really an alias for state. That's why
		it has to be in the same scope during pfs_end(). */

		PSI_mutex_locker_state	state;
		PSI_mutex_locker* locker = pfs_begin(&state, name, line);
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
		/* Note: locker is really an alias for state. That's why
		it has to be in the same scope during pfs_end(). */

		PSI_mutex_locker_state	state;
		PSI_mutex_locker* locker = pfs_begin(&state, name, line);
#endif /* UNIV_PFS_MUTEX */

		m_impl.m_policy.enter(m_impl);

		int ret = !m_impl.try_lock();

		if (ret == 0) {
			m_impl.m_policy.locked(m_impl);
		}

#ifdef UNIV_PFS_MUTEX
		pfs_end(locker, 0); 
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
		UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		extern mysql_pfs_key_t sync_latch_get_pfs_key(const char*);

		pfs_add(sync_latch_get_pfs_key(name));
#endif /* UNIV_PFS_MUTEX */

		m_impl.init(name, filename, line);
	}

	/** Free resources (if any) */
	void destroy() UNIV_NOTHROW
	{
#ifdef UNIV_PFS_MUTEX
		pfs_del();
#endif /* UNIV_PFS_MUTEX */
		m_impl.destroy();
	}	

	/** Required for os_event_t */
	operator sys_mutex_t*() UNIV_NOTHROW
	{
		return(m_impl.operator sys_mutex_t*());
	}

#ifdef UNIV_PFS_MUTEX
	/** Performance schema monitoring - register mutex with PFS.

	Note: This is public only because we want to get around an issue
	with registering a subset of buffer pool pages with PFS when
	PFS_GROUP_BUFFER_SYNC is defined. Therefore this has to then
	be called by external code (see buf0buf.cc).

	@param key - Performance Schema key. */
	void pfs_add(mysql_pfs_key_t key) UNIV_NOTHROW
	{
		ut_ad(m_ptr == 0);
		m_ptr = PSI_MUTEX_CALL(init_mutex)(key, this);
	}

private:

	/** Performance schema monitoring.
	@param state - PFS locker state
	@param name - file name where locked
	@param line - line number in file where locked */
	PSI_mutex_locker* pfs_begin(
		PSI_mutex_locker_state*	state,
		const char*		name,
		ulint			line) UNIV_NOTHROW
	{
		if (m_ptr != 0) {
			return(PSI_MUTEX_CALL(start_mutex_wait)(
					state, m_ptr,
					PSI_MUTEX_LOCK, name, (uint) line));
		}

		return(0);
	}

	/** Performance schema monitoring
	@param locker - PFS identifier
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

private:
	/** The mutex implementation */
	MutexImpl		m_impl;
};

#ifndef UNIV_DEBUG

# ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<Futex<DefaultPolicy> >  FutexMutex;
# endif /* HAVE_IB_LINUX_FUTEX */

typedef PolicyMutex<TTASWaitMutex<TrackPolicy> > Mutex;
typedef PolicyMutex<TTASMutex<DefaultPolicy> > SpinMutex;
typedef PolicyMutex<OSTrackedMutex<DefaultPolicy> > SysMutex;
typedef PolicyMutex<OSBasicMutex<DefaultPolicy> > EventMutex;

#else /* !UNIV_DEBUG */

# ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<Futex<DebugPolicy> > FutexMutex;
# endif /* HAVE_IB_LINUX_FUTEX */

typedef PolicyMutex<TTASWaitMutex<DebugPolicy> > Mutex;
typedef PolicyMutex<TTASMutex<DebugPolicy> > SpinMutex;
typedef PolicyMutex<OSTrackedMutex<DebugPolicy> > SysMutex;
typedef PolicyMutex<OSBasicMutex<DebugPolicy> > EventMutex;

#endif /* !UNIV_DEBUG */

typedef Mutex ib_mutex_t;

#include "ut0mutex.ic"

#endif /* ut0mutex_h */
