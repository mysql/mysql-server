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

/**************************************************//**
@file include/ut0counter.h

Counter utility class

Created 2012/04/12 by Sunny Bains
*******************************************************/

#ifndef UT0COUNTER_H
#define UT0COUNTER_H

#include "univ.i"
#include <string.h>
#include "os0thread.h"

/** CPU cache line size */
#define CACHE_LINE_SIZE		64

/** Default number of slots to use in ib_counter_t */
#define IB_N_SLOTS		64

/** Get the offset into the counter array. */
template <typename Type, int N>
struct generic_indexer_t {
	/** Default constructor/destructor should be OK. */

        /** @return offset within m_counter */
        size_t offset(size_t index) const UNIV_NOTHROW {
                return(((index % N) + 1) * (CACHE_LINE_SIZE / sizeof(Type)));
        }
};

#ifdef HAVE_SCHED_GETCPU
#include <utmpx.h>
/** Use the cpu id to index into the counter array. If it fails then
use the thread id. */
template <typename Type, int N>
struct get_sched_indexer_t : public generic_indexer_t<Type, N> {
	/** Default constructor/destructor should be OK. */

	/* @return result from sched_getcpu(), the thread id if it fails. */
	size_t get_rnd_index() const UNIV_NOTHROW {

		size_t	cpu = sched_getcpu();
		if (cpu == -1) {
			cpu = (lint) os_thread_get_curr_id();
		}

		return(cpu);
	}
};
#endif /* HAVE_SCHED_GETCPU */

/** Use the thread id to index into the counter array. */
template <typename Type, int N>
struct thread_id_indexer_t : public generic_indexer_t<Type, N> {
	/** Default constructor/destructor should are OK. */

	/* @return a random number, currently we use the thread id. Where
	thread id is represented as a pointer, it may not work as
	effectively. */
	size_t get_rnd_index() const UNIV_NOTHROW {
		return((lint) os_thread_get_curr_id());
	}
};

/** For counters wher N=1 */
template <typename Type, int N=1>
struct single_indexer_t {
	/** Default constructor/destructor should are OK. */

        /** @return offset within m_counter */
        size_t offset(size_t index) const UNIV_NOTHROW {
		ut_ad(N == 1);
                return((CACHE_LINE_SIZE / sizeof(Type)));
        }

	/* @return 1 */
	size_t get_rnd_index() const UNIV_NOTHROW {
		ut_ad(N == 1);
		return(1);
	}
};

/** Class for using fuzzy counters. The counter is not protected by any
mutex and the results are not guaranteed to be 100% accurate but close
enough. Creates an array of counters and separates each element by the
CACHE_LINE_SIZE bytes */
template <
	typename Type,
	int N = IB_N_SLOTS,
	template<typename, int> class Indexer = thread_id_indexer_t>
class ib_counter_t {
public:
	ib_counter_t() { memset(m_counter, 0x0, sizeof(m_counter)); }

	~ib_counter_t()
	{
		ut_ad(validate());
	}

	bool validate() UNIV_NOTHROW {
#ifdef UNIV_DEBUG
		size_t	n = (CACHE_LINE_SIZE / sizeof(Type));

		/* Check that we aren't writing outside our defined bounds. */
		for (size_t i = 0; i < UT_ARR_SIZE(m_counter); i += n) {
			for (size_t j = 1; j < n - 1; ++j) {
				ut_ad(m_counter[i + j] == 0);
			}
		}
#endif /* UNIV_DEBUG */
		return(true);
	}

	/** If you can't use a good index id. Increment by 1. */
	void inc() UNIV_NOTHROW { add(1); }

	/** If you can't use a good index id.
	* @param n  - is the amount to increment */
	void add(Type n) UNIV_NOTHROW {
		size_t	i = m_policy.offset(m_policy.get_rnd_index());

		ut_ad(i < UT_ARR_SIZE(m_counter));

		m_counter[i] += n;
	}

	/** Use this if you can use a unique indentifier, saves a
	call to get_rnd_index().
	@param i - index into a slot
	@param n - amount to increment */
	void add(size_t index, Type n) UNIV_NOTHROW {
		size_t	i = m_policy.offset(index);

		ut_ad(i < UT_ARR_SIZE(m_counter));

		m_counter[i] += n;
	}

	/** If you can't use a good index id. Decrement by 1. */
	void dec() UNIV_NOTHROW { sub(1); }

	/** If you can't use a good index id.
	* @param - n is the amount to decrement */
	void sub(Type n) UNIV_NOTHROW {
		size_t	i = m_policy.offset(m_policy.get_rnd_index());

		ut_ad(i < UT_ARR_SIZE(m_counter));

		m_counter[i] -= n;
	}

	/** Use this if you can use a unique indentifier, saves a
	call to get_rnd_index().
	@param i - index into a slot
	@param n - amount to decrement */
	void sub(size_t index, Type n) UNIV_NOTHROW {
		size_t	i = m_policy.offset(index);

		ut_ad(i < UT_ARR_SIZE(m_counter));

		m_counter[i] -= n;
	}

	/* @return total value - not 100% accurate, since it is not atomic. */
	operator Type() const UNIV_NOTHROW {
		Type	total = 0;

		for (size_t i = 0; i < N; ++i) {
			total += m_counter[m_policy.offset(i)];
		}

		return(total);
	}

private:
	/** Indexer into the array */
	Indexer<Type, N>m_policy;

        /** Slot 0 is unused. */
	Type		m_counter[(N + 1) * (CACHE_LINE_SIZE / sizeof(Type))];
};

#endif /* UT0COUNTER_H */
