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

extern ulong	srv_spin_wait_delay;
extern ulong	srv_n_spin_wait_rounds;

/** Default mutex policy. Should be as simple as possible so that
it doesn't occupy any space. No vptrs etc. */
template <typename Mutex>
struct DefaultPolicy {

	/** Poll waiting for mutex to be unlocked */
	static void poll(const Mutex& mutex) UNIV_NOTHROW
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

	void init(const char*, const char*, ulint) UNIV_NOTHROW { }
};

/** For observing Mutex events. */
template <typename Mutex>
struct Observer {
	typedef Mutex Latch;

	virtual void enter(const Mutex&) UNIV_NOTHROW = 0;
	virtual void locked(const Mutex&) UNIV_NOTHROW = 0;
	virtual void release(const Mutex&) UNIV_NOTHROW = 0;
};

/** Track policy. */
template <typename Mutex>
struct TrackPolicy : public Observer<Mutex> {

	TrackPolicy()
		:
		m_name(0),
		m_cline(ULINT_UNDEFINED),
		m_cfile_name(0) { }

	void init(const char* name, const char* filename, ulint line)
		UNIV_NOTHROW
	{
		m_name = name;
		m_cline = line;
		m_cfile_name = filename;
	}

	static void poll(const Mutex& mutex) UNIV_NOTHROW
	{
		DefaultPolicy<Mutex>::poll(mutex);
	}

	virtual void enter(const Mutex&) UNIV_NOTHROW { /* No op */}

	virtual void locked(const Mutex&) UNIV_NOTHROW { /* No op */}

	virtual void release(const Mutex&) UNIV_NOTHROW { /* No op */}

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

	DebugPolicy()
		:
		m_thread_id(os_thread_id_t(ULINT_UNDEFINED)),
		m_magic_n(MUTEX_MAGIC_N)
	{
	}

	~DebugPolicy()
	{
		ut_ad(m_thread_id == os_thread_id_t(ULINT_UNDEFINED));
	}

	/* @return true if thread owns the mutex */
	bool is_owned() const UNIV_NOTHROW
	{
		return(os_thread_eq(m_thread_id, os_thread_get_curr_id()));
	}

	void init(const char* name, const char* filename, ulint line)
		UNIV_NOTHROW;

	virtual void enter(const Mutex&) UNIV_NOTHROW;

	virtual void locked(const Mutex&) UNIV_NOTHROW;

	virtual void release(const Mutex&) UNIV_NOTHROW;

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
	latch_t			m_latch;
};

#endif /* UNIV_DEBUG */

#ifndef UNIV_NONINL
#include "sync0policy.ic"
#endif /* UNIV_NOINL */

#endif /* sync0policy_h */
