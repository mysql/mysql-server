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
@file include/sync0policy.h
Policies for mutexes.

Created 2012-08-21 Sunny Bains.
***********************************************************************/

#ifndef sync0policy_h
#define sync0policy_h

#include "univ.i"
#include "ut0rnd.h"
#include "sync0types.h"

extern ulong	srv_spin_wait_delay;
extern ulong	srv_n_spin_wait_rounds;
extern ulong 	srv_force_recovery_crash;

/** Default mutex policy. Should be as simple as possible so that
it doesn't occupy any space. No vptrs etc. */
template <typename Mutex>
struct DefaultPolicy {

	DefaultPolicy(bool track=false) { }

	/** Poll waiting for mutex to be unlocked.
	@return value of lock word before locking. */
	static lock_word_t trylock_poll(Mutex& mutex) UNIV_NOTHROW
	{
		lock_word_t	lock;
		ulint		delay;

		delay = ut_rnd_interval(0, srv_spin_wait_delay) * 10;

		for (ulint i = 0; i <= srv_n_spin_wait_rounds; ++i) {

			if ((lock = mutex.trylock()) == MUTEX_STATE_UNLOCKED) {

				return(lock);

			} else if (srv_spin_wait_delay > 0) {

				for (ulint j = 0; j < delay; ++j) {
					UT_RELAX_CPU();
				}
			}
		}

		return(lock);
	}

	/** Poll waiting for mutex to be unlocked */
	static void test_poll(Mutex& mutex) UNIV_NOTHROW
	{
		ulint	delay = ut_rnd_interval(0, srv_spin_wait_delay) * 10;

		for (ulint i = 0; i <= srv_n_spin_wait_rounds; ++i) {

			if (!mutex.is_locked()) {

				break;

			} else if (srv_spin_wait_delay > 0) {

				for (ulint j = 0; j < delay; ++j) {
					UT_RELAX_CPU();
				}
			}
		}
	}

	void enter(const Mutex&) UNIV_NOTHROW { }
	void locked(const Mutex&) UNIV_NOTHROW { }
	void release(const Mutex&) UNIV_NOTHROW { }

	void init(
		const Mutex&,
		const char*, const char*, ulint) UNIV_NOTHROW { }

	void destroy() UNIV_NOTHROW {}
};

/** Track policy. */
template <typename Mutex>
struct TrackPolicy {

	/** Default constructor. */
	TrackPolicy()
		:
		m_name(),
		m_cline(ULINT_UNDEFINED),
		m_cfile_name() { }

	/** Called when the mutex is "created". Note: Not from the constructor
	but when the mutex is initialised. */
	void init(const Mutex&	mutex,
		  const char*	name,
		  const char*	filename,
		  ulint line) UNIV_NOTHROW
	{
		m_name = name;
		m_cline = line;
		m_cfile_name = filename;
	}

	/** Called when the mutex is destroyed. */
	void destroy() UNIV_NOTHROW { }

	/** The mutex wants to do a trlock poll. */
	static lock_word_t trylock_poll(Mutex& mutex) UNIV_NOTHROW
	{
		return(DefaultPolicy<Mutex>::trylock_poll(mutex));
	}

	/** The mutes wants to spin. */
	static void test_poll(Mutex& mutex) UNIV_NOTHROW
	{
		DefaultPolicy<Mutex>::test_poll(mutex);
	}

	void enter(const Mutex&) UNIV_NOTHROW { }
	void locked(const Mutex&) UNIV_NOTHROW { }
	void release(const Mutex&) UNIV_NOTHROW { }

	void print(FILE* stream) const;

	/** Name of the mutex */
	const char*		m_name;

	/** Line where created */
	ulint			m_cline;

	/** File name where mutex created */
	const char*		m_cfile_name;
};

#ifdef UNIV_DEBUG

# define MUTEX_MAGIC_N 979585UL

/** Default debug policy. */
template <typename Mutex>
struct DebugPolicy : public TrackPolicy<Mutex> {

	struct DebugLatch : public latch_t {

		DebugLatch() : m_mutex(0) { } 

		virtual void print(FILE* stream) const
		{
			m_mutex->m_policy.print(stream);
		}

		const Mutex*	m_mutex;
	};

	/** Default constructor. */
	DebugPolicy(bool track = true)
		:
		m_track(track),
		m_thread_id(),
		m_magic_n() UNIV_NOTHROW
	{
		/* No op */
	}

	/** Destructor */
	~DebugPolicy() UNIV_NOTHROW
	{
		// FIXME: This invariant doesn't hold if we exit
		// without invoking the shutdown code.
		//ut_a(m_magic_n == 0 || srv_force_recovery_crash);
		//ut_ad(m_thread_id == 0 || srv_force_recovery_crash);
	}

	/** Mutex is being destroyed. */
	void destroy() UNIV_NOTHROW
	{
		ut_ad(m_thread_id == os_thread_id_t(ULINT_UNDEFINED));

		m_magic_n = 0;
		m_thread_id = 0;

		TrackPolicy<Mutex>::destroy();
	}

	/* @return true if thread owns the mutex */
	bool is_owned() const UNIV_NOTHROW
	{
		return(os_thread_eq(m_thread_id, os_thread_get_curr_id()));
	}

	virtual void init(
		const Mutex&	mutex,
		const char*	name,
		const char*	filename,
		ulint		line) UNIV_NOTHROW;

	virtual void enter(const Mutex&) UNIV_NOTHROW;

	virtual void locked(const Mutex&) UNIV_NOTHROW;

	virtual void release(const Mutex&) UNIV_NOTHROW;

	void print(FILE* stream) const
	{
		TrackPolicy<Mutex>::print(stream);

		if (os_thread_pf(m_thread_id) != ULINT_UNDEFINED) {

			fprintf(stream,
				"Locked mutex: "
				"addr %p thread %ld file %s line %ld",
				(void*) m_latch.m_mutex,
				os_thread_pf(m_thread_id),
				m_file_name,
				m_line);
		} else {
			fprintf(stream,"Not locked");
		}

		fprintf(stream, "\n");
	}

	/** true if the lock/unlock should be tracked */
	bool			m_track;

	/** Owning thread id, or ULINT_UNDEFINED. NOTE: os_thread_id can be
	any type even a pointer. */
	os_thread_id_t		m_thread_id;

       	/** File where the mutex was locked */
	const char*		m_file_name;

	/** Line where the mutex was locked */
	ulint			m_line;

	/** Magic number to check for memory corruption. */
	ulint			m_magic_n;

	/** Latching information required by the latch ordering checks. */
	DebugLatch		m_latch;
};

#endif /* UNIV_DEBUG */

#ifndef UNIV_NONINL
#include "sync0policy.ic"
#endif /* UNIV_NOINL */

#endif /* sync0policy_h */
