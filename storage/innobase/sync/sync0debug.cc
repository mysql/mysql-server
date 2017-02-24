/*****************************************************************************

Copyright (c) 2014, 2017, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
@file sync/sync0debug.cc
Debug checks for latches.

Created 2012-08-21 Sunny Bains
*******************************************************/

#include "sync0sync.h"
#include "sync0debug.h"

#include "ut0new.h"
#include "srv0start.h"

#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#ifdef UNIV_DEBUG

my_bool		srv_sync_debug;

/** The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be
acquired in addition to the mutex protecting the lock. */
static ib_mutex_t		rw_lock_debug_mutex;

/** If deadlock detection does not get immediately the mutex,
it may wait for this event */
static os_event_t		rw_lock_debug_event;

/** This is set to true, if there may be waiters for the event */
static bool			rw_lock_debug_waiters;

/** The latch held by a thread */
struct Latched {

	/** Constructor */
	Latched() : m_latch(), m_level(SYNC_UNKNOWN) { }

	/** Constructor
	@param[in]	latch		Latch instance
	@param[in]	level		Level of latch held */
	Latched(const latch_t*	latch,
		latch_level_t	level)
		:
		m_latch(latch),
		m_level(level)
	{
		/* No op */
	}

	/** @return the latch level */
	latch_level_t get_level() const
	{
		return(m_level);
	}

	/** Check if the rhs latch and level match
	@param[in]	rhs		instance to compare with
	@return true on match */
	bool operator==(const Latched& rhs) const
	{
		return(m_latch == rhs.m_latch && m_level == rhs.m_level);
	}

	/** The latch instance */
	const latch_t*		m_latch;

	/** The latch level. For buffer blocks we can pass a separate latch
	level to check against, see buf_block_dbg_add_level() */
	latch_level_t		m_level;
};

/** Thread specific latches. This is ordered on level in descending order. */
typedef std::vector<Latched, ut_allocator<Latched> > Latches;

/** The deadlock detector. */
struct LatchDebug {

	/** Debug mutex for control structures, should not be tracked
	by this module. */
	typedef OSMutex Mutex;

	/** Comparator for the ThreadMap. */
	struct os_thread_id_less
		: public std::binary_function<
		  os_thread_id_t,
		  os_thread_id_t,
		  bool>
	{
		/** @return true if lhs < rhs */
		bool operator()(
			const os_thread_id_t& lhs,
			const os_thread_id_t& rhs) const
			UNIV_NOTHROW
		{
			return(os_thread_pf(lhs) < os_thread_pf(rhs));
		}
	};

	/** For tracking a thread's latches. */
	typedef std::map<
		os_thread_id_t,
		Latches*,
		os_thread_id_less,
		ut_allocator<std::pair<const os_thread_id_t, Latches*> > >
		ThreadMap;

	/** Constructor */
	LatchDebug()
		UNIV_NOTHROW;

	/** Destructor */
	~LatchDebug()
		UNIV_NOTHROW
	{
		m_mutex.destroy();
	}

	/** Create a new instance if one doesn't exist else return
	the existing one.
	@param[in]	add		add an empty entry if one is not
					found (default no)
	@return	pointer to a thread's acquired latches. */
	Latches* thread_latches(bool add = false)
		UNIV_NOTHROW;

	/** Check that all the latches already owned by a thread have a lower
	level than limit.
	@param[in]	latches		the thread's existing (acquired) latches
	@param[in]	limit		to check against
	@return latched if there is one with a level <= limit . */
	const Latched* less(
		const Latches*	latches,
		latch_level_t	limit) const
		UNIV_NOTHROW;

	/** Checks if the level value exists in the thread's acquired latches.
	@param[in]	latches		the thread's existing (acquired) latches
	@param[in]	level		to lookup
	@return	latch if found or 0 */
	const latch_t* find(
		const Latches*	Latches,
		latch_level_t	level) const
		UNIV_NOTHROW;

	/**
	Checks if the level value exists in the thread's acquired latches.
	@param[in]	level		to lookup
	@return	latch if found or 0 */
	const latch_t* find(latch_level_t level)
		UNIV_NOTHROW;

	/** Report error and abort.
	@param[in]	latches		thread's existing latches
	@param[in]	latched		The existing latch causing the
					invariant to fail
	@param[in]	level		The new level request that breaks
					the order */
	void crash(
		const Latches*	latches,
		const Latched*	latched,
		latch_level_t	level) const
		UNIV_NOTHROW;

	/** Do a basic ordering check.
	@param[in]	latches		thread's existing latches
	@param[in]	requested_level	Level requested by latch
	@param[in]	level		declared ulint so that we can
					do level - 1. The level of the
					latch that the thread is trying
					to acquire
	@return true if passes, else crash with error message. */
	bool basic_check(
		const Latches*	latches,
		latch_level_t	requested_level,
		ulint		level) const
		UNIV_NOTHROW;

	/** Adds a latch and its level in the thread level array. Allocates
	the memory for the array if called for the first time for this
	OS thread.  Makes the checks against other latch levels stored
	in the array for this thread.

	@param[in]	latch	latch that the thread wants to acqire.
	@param[in]	level	latch level to check against */
	void lock_validate(
		const latch_t*	latch,
		latch_level_t	level)
		UNIV_NOTHROW
	{
		/* Ignore diagnostic latches, starting with '.' */

		if (*latch->get_name() != '.'
		    && latch->get_level() != SYNC_LEVEL_VARYING) {

			ut_ad(level != SYNC_LEVEL_VARYING);

			Latches*	latches = check_order(latch, level);

			ut_a(latches->empty()
			     || level == SYNC_LEVEL_VARYING
			     || level == SYNC_NO_ORDER_CHECK
			     || latches->back().get_level()
			     == SYNC_NO_ORDER_CHECK
			     || latches->back().m_latch->get_level()
			     == SYNC_LEVEL_VARYING
			     || latches->back().get_level() >= level);
		}
	}

	/** Adds a latch and its level in the thread level array. Allocates
	the memory for the array if called for the first time for this
	OS thread.  Makes the checks against other latch levels stored
	in the array for this thread.

	@param[in]	latch	latch that the thread wants to acqire.
	@param[in]	level	latch level to check against */
	void lock_granted(
		const latch_t*	latch,
		latch_level_t	level)
		UNIV_NOTHROW
	{
		/* Ignore diagnostic latches, starting with '.' */

		if (*latch->get_name() != '.'
		    && latch->get_level() != SYNC_LEVEL_VARYING) {

			Latches*	latches = thread_latches(true);

			latches->push_back(Latched(latch, level));
		}
	}

	/** For recursive X rw-locks.
	@param[in]	latch		The RW-Lock to relock  */
	void relock(const latch_t* latch)
		UNIV_NOTHROW
	{
		ut_a(latch->m_rw_lock);

		latch_level_t	level = latch->get_level();

		/* Ignore diagnostic latches, starting with '.' */

		if (*latch->get_name() != '.'
		    && latch->get_level() != SYNC_LEVEL_VARYING) {

			Latches*	latches = thread_latches(true);

			Latches::iterator	it = std::find(
				latches->begin(), latches->end(),
				Latched(latch, level));

			ut_a(latches->empty()
			     || level == SYNC_LEVEL_VARYING
			     || level == SYNC_NO_ORDER_CHECK
			     || latches->back().m_latch->get_level()
			     == SYNC_LEVEL_VARYING
			     || latches->back().m_latch->get_level()
			     == SYNC_NO_ORDER_CHECK
			     || latches->back().get_level() >= level
			     || it != latches->end());

			if (it == latches->end()) {
				latches->push_back(Latched(latch, level));
			} else {
				latches->insert(it, Latched(latch, level));
			}
		}
	}

	/** Iterate over a thread's latches.
	@param[in,out]	functor		The callback
	@return true if the functor returns true. */
	bool for_each(sync_check_functor_t& functor)
		UNIV_NOTHROW
	{
		const Latches*	latches = thread_latches();

		if (latches == 0) {
			return(functor.result());
		}

		Latches::const_iterator	end = latches->end();

		for (Latches::const_iterator it = latches->begin();
		     it != end;
		     ++it) {

			if (functor(it->m_level)) {
				break;
			}
		}

		return(functor.result());
	}

	/** Removes a latch from the thread level array if it is found there.
	@param[in]	latch		The latch that was released
	@return true if found in the array; it is not an error if the latch is
	not found, as we presently are not able to determine the level for
	every latch reservation the program does */
	void unlock(const latch_t* latch) UNIV_NOTHROW;

	/** Get the level name
	@param[in]	level		The level ID to lookup
	@return level name */
	const std::string& get_level_name(latch_level_t level) const
		UNIV_NOTHROW
	{
		Levels::const_iterator	it = m_levels.find(level);

		ut_ad(it != m_levels.end());

		return(it->second);
	}

	/** Initialise the debug data structures */
	static void init()
		UNIV_NOTHROW;

	/** Shutdown the latch debug checking */
	static void shutdown()
		UNIV_NOTHROW;

	/** @return the singleton instance */
	static LatchDebug* instance()
		UNIV_NOTHROW
	{
		return(s_instance);
	}

	/** Create the singleton instance */
	static void create_instance()
		UNIV_NOTHROW
	{
		ut_ad(s_instance == NULL);

		s_instance = UT_NEW_NOKEY(LatchDebug());
	}

private:
	/** Disable copying */
	LatchDebug(const LatchDebug&);
	LatchDebug& operator=(const LatchDebug&);

	/** Adds a latch and its level in the thread level array. Allocates
	the memory for the array if called first time for this OS thread.
	Makes the checks against other latch levels stored in the array
	for this thread.

	@param[in]	latch	 pointer to a mutex or an rw-lock
	@param[in]	level	level in the latching order
	@return the thread's latches */
	Latches* check_order(
		const latch_t*	latch,
		latch_level_t	level)
		UNIV_NOTHROW;

	/** Print the latches acquired by a thread
	@param[in]	latches		Latches acquired by a thread */
	void print_latches(const Latches* latches) const
		UNIV_NOTHROW;

	/** Special handling for the RTR mutexes. We need to add proper
	levels for them if possible.
	@param[in]	latch		Latch to check
	@return true if it is a an _RTR_ mutex */
	bool is_rtr_mutex(const latch_t* latch) const
		UNIV_NOTHROW
	{
		return(latch->get_id() == LATCH_ID_RTR_ACTIVE_MUTEX
		       || latch->get_id() == LATCH_ID_RTR_PATH_MUTEX
		       || latch->get_id() == LATCH_ID_RTR_MATCH_MUTEX
		       || latch->get_id() == LATCH_ID_RTR_SSN_MUTEX);
	}

private:
	/** Comparator for the Levels . */
	struct latch_level_less
		: public std::binary_function<
		  latch_level_t,
		  latch_level_t,
		  bool>
	{
		/** @return true if lhs < rhs */
		bool operator()(
			const latch_level_t& lhs,
			const latch_level_t& rhs) const
			UNIV_NOTHROW
		{
			return(lhs < rhs);
		}
	};

	typedef std::map<
		latch_level_t,
		std::string,
		latch_level_less,
		ut_allocator<std::pair<const latch_level_t, std::string> > >
		Levels;

	/** Mutex protecting the deadlock detector data structures. */
	Mutex			m_mutex;

	/** Thread specific data. Protected by m_mutex. */
	ThreadMap		m_threads;

	/** Mapping from latche level to its string representation. */
	Levels			m_levels;

	/** The singleton instance. Must be created in single threaded mode. */
	static LatchDebug*	s_instance;

public:
	/** For checking whether this module has been initialised or not. */
	static bool		s_initialized;
};

/** The latch order checking infra-structure */
LatchDebug* LatchDebug::s_instance = NULL;
bool LatchDebug::s_initialized = false;

#define LEVEL_MAP_INSERT(T)						\
do {									\
	std::pair<Levels::iterator, bool>	result =		\
		m_levels.insert(Levels::value_type(T, #T));		\
	ut_ad(result.second);						\
} while(0)

/** Setup the mapping from level ID to level name mapping */
LatchDebug::LatchDebug()
{
	m_mutex.init();

	LEVEL_MAP_INSERT(SYNC_UNKNOWN);
	LEVEL_MAP_INSERT(SYNC_MUTEX);
	LEVEL_MAP_INSERT(RW_LOCK_SX);
	LEVEL_MAP_INSERT(RW_LOCK_X_WAIT);
	LEVEL_MAP_INSERT(RW_LOCK_S);
	LEVEL_MAP_INSERT(RW_LOCK_X);
	LEVEL_MAP_INSERT(RW_LOCK_NOT_LOCKED);
	LEVEL_MAP_INSERT(SYNC_MONITOR_MUTEX);
	LEVEL_MAP_INSERT(SYNC_ANY_LATCH);
	LEVEL_MAP_INSERT(SYNC_DOUBLEWRITE);
	LEVEL_MAP_INSERT(SYNC_BUF_FLUSH_LIST);
	LEVEL_MAP_INSERT(SYNC_BUF_BLOCK);
	LEVEL_MAP_INSERT(SYNC_BUF_PAGE_HASH);
	LEVEL_MAP_INSERT(SYNC_BUF_POOL);
	LEVEL_MAP_INSERT(SYNC_POOL);
	LEVEL_MAP_INSERT(SYNC_POOL_MANAGER);
	LEVEL_MAP_INSERT(SYNC_SEARCH_SYS);
	LEVEL_MAP_INSERT(SYNC_WORK_QUEUE);
	LEVEL_MAP_INSERT(SYNC_FTS_TOKENIZE);
	LEVEL_MAP_INSERT(SYNC_FTS_OPTIMIZE);
	LEVEL_MAP_INSERT(SYNC_FTS_BG_THREADS);
	LEVEL_MAP_INSERT(SYNC_FTS_CACHE_INIT);
	LEVEL_MAP_INSERT(SYNC_RECV);
	LEVEL_MAP_INSERT(SYNC_LOG_FLUSH_ORDER);
	LEVEL_MAP_INSERT(SYNC_LOG);
	LEVEL_MAP_INSERT(SYNC_LOG_WRITE);
	LEVEL_MAP_INSERT(SYNC_PAGE_CLEANER);
	LEVEL_MAP_INSERT(SYNC_PURGE_QUEUE);
	LEVEL_MAP_INSERT(SYNC_TRX_SYS_HEADER);
	LEVEL_MAP_INSERT(SYNC_REC_LOCK);
	LEVEL_MAP_INSERT(SYNC_THREADS);
	LEVEL_MAP_INSERT(SYNC_TRX);
	LEVEL_MAP_INSERT(SYNC_TRX_SYS);
	LEVEL_MAP_INSERT(SYNC_LOCK_SYS);
	LEVEL_MAP_INSERT(SYNC_LOCK_WAIT_SYS);
	LEVEL_MAP_INSERT(SYNC_INDEX_ONLINE_LOG);
	LEVEL_MAP_INSERT(SYNC_IBUF_BITMAP);
	LEVEL_MAP_INSERT(SYNC_IBUF_BITMAP_MUTEX);
	LEVEL_MAP_INSERT(SYNC_IBUF_TREE_NODE);
	LEVEL_MAP_INSERT(SYNC_IBUF_TREE_NODE_NEW);
	LEVEL_MAP_INSERT(SYNC_IBUF_INDEX_TREE);
	LEVEL_MAP_INSERT(SYNC_IBUF_MUTEX);
	LEVEL_MAP_INSERT(SYNC_FSP_PAGE);
	LEVEL_MAP_INSERT(SYNC_FSP);
	LEVEL_MAP_INSERT(SYNC_EXTERN_STORAGE);
	LEVEL_MAP_INSERT(SYNC_TRX_UNDO_PAGE);
	LEVEL_MAP_INSERT(SYNC_RSEG_HEADER);
	LEVEL_MAP_INSERT(SYNC_RSEG_HEADER_NEW);
	LEVEL_MAP_INSERT(SYNC_NOREDO_RSEG);
	LEVEL_MAP_INSERT(SYNC_REDO_RSEG);
	LEVEL_MAP_INSERT(SYNC_TRX_UNDO);
	LEVEL_MAP_INSERT(SYNC_PURGE_LATCH);
	LEVEL_MAP_INSERT(SYNC_TREE_NODE);
	LEVEL_MAP_INSERT(SYNC_TREE_NODE_FROM_HASH);
	LEVEL_MAP_INSERT(SYNC_TREE_NODE_NEW);
	LEVEL_MAP_INSERT(SYNC_INDEX_TREE);
	LEVEL_MAP_INSERT(SYNC_IBUF_PESS_INSERT_MUTEX);
	LEVEL_MAP_INSERT(SYNC_IBUF_HEADER);
	LEVEL_MAP_INSERT(SYNC_DICT_HEADER);
	LEVEL_MAP_INSERT(SYNC_STATS_AUTO_RECALC);
	LEVEL_MAP_INSERT(SYNC_DICT_AUTOINC_MUTEX);
	LEVEL_MAP_INSERT(SYNC_DICT);
	LEVEL_MAP_INSERT(SYNC_FTS_CACHE);
	LEVEL_MAP_INSERT(SYNC_DICT_OPERATION);
	LEVEL_MAP_INSERT(SYNC_FILE_FORMAT_TAG);
	LEVEL_MAP_INSERT(SYNC_TRX_I_S_LAST_READ);
	LEVEL_MAP_INSERT(SYNC_TRX_I_S_RWLOCK);
	LEVEL_MAP_INSERT(SYNC_RECV_WRITER);
	LEVEL_MAP_INSERT(SYNC_LEVEL_VARYING);
	LEVEL_MAP_INSERT(SYNC_NO_ORDER_CHECK);

	/* Enum count starts from 0 */
	ut_ad(m_levels.size() == SYNC_LEVEL_MAX + 1);
}

/** Print the latches acquired by a thread
@param[in]	latches		Latches acquired by a thread */
void
LatchDebug::print_latches(const Latches* latches) const
	UNIV_NOTHROW
{
	ib::error() << "Latches already owned by this thread: ";

	Latches::const_iterator	end = latches->end();

	for (Latches::const_iterator it = latches->begin();
	     it != end;
	     ++it) {

		ib::error()
			<< sync_latch_get_name(it->m_latch->get_id())
			<< " -> "
			<< it->m_level << " "
			<< "(" << get_level_name(it->m_level) << ")";
	}
}

/** Report error and abort
@param[in]	latches		thread's existing latches
@param[in]	latched		The existing latch causing the invariant to fail
@param[in]	level		The new level request that breaks the order */
void
LatchDebug::crash(
	const Latches*	latches,
	const Latched*	latched,
	latch_level_t	level) const
	UNIV_NOTHROW
{
	const latch_t*		latch = latched->m_latch;
	const std::string&	in_level_name = get_level_name(level);

	const std::string&	latch_level_name =
		get_level_name(latched->m_level);

	ib::error()
		<< "Thread " << os_thread_pf(os_thread_get_curr_id())
		<< " already owns a latch "
		<< sync_latch_get_name(latch->m_id) << " at level"
		<< " " << latched->m_level << " (" << latch_level_name
		<< " ), which is at a lower/same level than the"
		<< " requested latch: "
		<< level << " (" << in_level_name << "). "
		<< latch->to_string();

	print_latches(latches);

	ut_error;
}

/** Check that all the latches already owned by a thread have a lower
level than limit.
@param[in]	latches		the thread's existing (acquired) latches
@param[in]	limit		to check against
@return latched info if there is one with a level <= limit . */
const Latched*
LatchDebug::less(
	const Latches*	latches,
	latch_level_t	limit) const
	UNIV_NOTHROW
{
	Latches::const_iterator	end = latches->end();

	for (Latches::const_iterator it = latches->begin(); it != end; ++it) {

		if (it->m_level <= limit) {
			return(&(*it));
		}
	}

	return(NULL);
}

/** Do a basic ordering check.
@param[in]	latches		thread's existing latches
@param[in]	requested_level	Level requested by latch
@param[in]	in_level	declared ulint so that we can do level - 1.
				The level of the latch that the thread is
				trying to acquire
@return true if passes, else crash with error message. */
bool
LatchDebug::basic_check(
	const Latches*	latches,
	latch_level_t	requested_level,
	ulint		in_level) const
	UNIV_NOTHROW
{
	latch_level_t	level = latch_level_t(in_level);

	ut_ad(level < SYNC_LEVEL_MAX);

	const Latched*	latched = less(latches, level);

	if (latched != NULL) {
		crash(latches, latched, requested_level);
		return(false);
	}

	return(true);
}

/** Create a new instance if one doesn't exist else return the existing one.
@param[in]	add		add an empty entry if one is not found
				(default no)
@return	pointer to a thread's acquired latches. */
Latches*
LatchDebug::thread_latches(bool add)
	UNIV_NOTHROW
{
	m_mutex.enter();

	os_thread_id_t		thread_id = os_thread_get_curr_id();
	ThreadMap::iterator	lb = m_threads.lower_bound(thread_id);

	if (lb != m_threads.end()
	    && !(m_threads.key_comp()(thread_id, lb->first))) {

		Latches*	latches = lb->second;

		m_mutex.exit();

		return(latches);

	} else if (!add) {

		m_mutex.exit();

		return(NULL);

	} else {
		typedef ThreadMap::value_type value_type;

		Latches*	latches = UT_NEW_NOKEY(Latches());

		ut_a(latches != NULL);

		latches->reserve(32);

		m_threads.insert(lb, value_type(thread_id, latches));

		m_mutex.exit();

		return(latches);
	}
}

/** Checks if the level value exists in the thread's acquired latches.
@param[in]	levels		the thread's existing (acquired) latches
@param[in]	level		to lookup
@return	latch if found or 0 */
const latch_t*
LatchDebug::find(
	const Latches*	latches,
	latch_level_t	level) const UNIV_NOTHROW
{
	Latches::const_iterator	end = latches->end();

	for (Latches::const_iterator it = latches->begin(); it != end; ++it) {

		if (it->m_level == level) {

			return(it->m_latch);
		}
	}

	return(0);
}

/** Checks if the level value exists in the thread's acquired latches.
@param[in]	 level		The level to lookup
@return	latch if found or NULL */
const latch_t*
LatchDebug::find(latch_level_t level)
	UNIV_NOTHROW
{
	return(find(thread_latches(), level));
}

/**
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread.
@param[in]	latch	pointer to a mutex or an rw-lock
@param[in]	level	level in the latching order
@return the thread's latches */
Latches*
LatchDebug::check_order(
	const latch_t*	latch,
	latch_level_t	level)
	UNIV_NOTHROW
{
	ut_ad(latch->get_level() != SYNC_LEVEL_VARYING);

	Latches*	latches = thread_latches(true);

	/* NOTE that there is a problem with _NODE and _LEAF levels: if the
	B-tree height changes, then a leaf can change to an internal node
	or the other way around. We do not know at present if this can cause
	unnecessary assertion failures below. */

	switch (level) {
	case SYNC_NO_ORDER_CHECK:
	case SYNC_EXTERN_STORAGE:
	case SYNC_TREE_NODE_FROM_HASH:
		/* Do no order checking */
		break;

	case SYNC_TRX_SYS_HEADER:

		if (srv_is_being_started) {
			/* This is violated during trx_sys_create_rsegs()
			when creating additional rollback segments when
			upgrading in innobase_start_or_create_for_mysql(). */
			break;
		}

		/* Fall through */

	case SYNC_MONITOR_MUTEX:
	case SYNC_RECV:
	case SYNC_FTS_BG_THREADS:
	case SYNC_WORK_QUEUE:
	case SYNC_FTS_TOKENIZE:
	case SYNC_FTS_OPTIMIZE:
	case SYNC_FTS_CACHE:
	case SYNC_FTS_CACHE_INIT:
	case SYNC_PAGE_CLEANER:
	case SYNC_LOG:
	case SYNC_LOG_WRITE:
	case SYNC_LOG_FLUSH_ORDER:
	case SYNC_FILE_FORMAT_TAG:
	case SYNC_DOUBLEWRITE:
	case SYNC_SEARCH_SYS:
	case SYNC_THREADS:
	case SYNC_LOCK_SYS:
	case SYNC_LOCK_WAIT_SYS:
	case SYNC_TRX_SYS:
	case SYNC_IBUF_BITMAP_MUTEX:
	case SYNC_REDO_RSEG:
	case SYNC_NOREDO_RSEG:
	case SYNC_TRX_UNDO:
	case SYNC_PURGE_LATCH:
	case SYNC_PURGE_QUEUE:
	case SYNC_DICT_AUTOINC_MUTEX:
	case SYNC_DICT_OPERATION:
	case SYNC_DICT_HEADER:
	case SYNC_TRX_I_S_RWLOCK:
	case SYNC_TRX_I_S_LAST_READ:
	case SYNC_IBUF_MUTEX:
	case SYNC_INDEX_ONLINE_LOG:
	case SYNC_STATS_AUTO_RECALC:
	case SYNC_POOL:
	case SYNC_POOL_MANAGER:
	case SYNC_RECV_WRITER:

		basic_check(latches, level, level);
		break;

	case SYNC_ANY_LATCH:

		/* Temporary workaround for LATCH_ID_RTR_*_MUTEX */
		if (is_rtr_mutex(latch)) {

			const Latched*	latched = less(latches, level);

			if (latched == NULL
			    || (latched != NULL
				&& is_rtr_mutex(latched->m_latch))) {

				/* No violation */
				break;

			}

			crash(latches, latched, level);

		} else {
			basic_check(latches, level, level);
		}

		break;

	case SYNC_TRX:

		/* Either the thread must own the lock_sys->mutex, or
		it is allowed to own only ONE trx_t::mutex. */

		if (less(latches, level) != NULL) {
			basic_check(latches, level, level - 1);
			ut_a(find(latches, SYNC_LOCK_SYS) != 0);
		}
		break;

	case SYNC_BUF_FLUSH_LIST:
	case SYNC_BUF_POOL:

		/* We can have multiple mutexes of this type therefore we
		can only check whether the greater than condition holds. */

		basic_check(latches, level, level - 1);
		break;

	case SYNC_BUF_PAGE_HASH:

		/* Multiple page_hash locks are only allowed during
		buf_validate and that is where buf_pool mutex is already
		held. */

		/* Fall through */

	case SYNC_BUF_BLOCK:

		/* Either the thread must own the (buffer pool) buf_pool->mutex
		or it is allowed to latch only ONE of (buffer block)
		block->mutex or buf_pool->zip_mutex. */

		if (less(latches, level) != NULL) {
			basic_check(latches, level, level - 1);
			ut_a(find(latches, SYNC_BUF_POOL) != 0);
		}
		break;

	case SYNC_REC_LOCK:

		if (find(latches, SYNC_LOCK_SYS) != 0) {
			basic_check(latches, level, SYNC_REC_LOCK - 1);
		} else {
			basic_check(latches, level, SYNC_REC_LOCK);
		}
		break;

	case SYNC_IBUF_BITMAP:

		/* Either the thread must own the master mutex to all
		the bitmap pages, or it is allowed to latch only ONE
		bitmap page. */

		if (find(latches, SYNC_IBUF_BITMAP_MUTEX) != 0) {

			basic_check(latches, level, SYNC_IBUF_BITMAP - 1);

		} else if (!srv_is_being_started) {

			/* This is violated during trx_sys_create_rsegs()
			when creating additional rollback segments during
			upgrade. */

			basic_check(latches, level, SYNC_IBUF_BITMAP);
		}
		break;

	case SYNC_FSP_PAGE:
		ut_a(find(latches, SYNC_FSP) != 0);
		break;

	case SYNC_FSP:

		ut_a(find(latches, SYNC_FSP) != 0
		     || basic_check(latches, level, SYNC_FSP));
		break;

	case SYNC_TRX_UNDO_PAGE:

		/* Purge is allowed to read in as many UNDO pages as it likes.
		The purge thread can read the UNDO pages without any covering
		mutex. */

		ut_a(find(latches, SYNC_TRX_UNDO) != 0
		     || find(latches, SYNC_REDO_RSEG) != 0
		     || find(latches, SYNC_NOREDO_RSEG) != 0
		     || basic_check(latches, level, level - 1));
		break;

	case SYNC_RSEG_HEADER:

		ut_a(find(latches, SYNC_REDO_RSEG) != 0
		     || find(latches, SYNC_NOREDO_RSEG) != 0);
		break;

	case SYNC_RSEG_HEADER_NEW:

		ut_a(find(latches, SYNC_FSP_PAGE) != 0);
		break;

	case SYNC_TREE_NODE:

		{
			const latch_t*	fsp_latch;

			fsp_latch = find(latches, SYNC_FSP);

			ut_a((fsp_latch != NULL
			      && fsp_latch->is_temp_fsp())
			     || find(latches, SYNC_INDEX_TREE) != 0
			     || find(latches, SYNC_DICT_OPERATION)
			     || basic_check(latches,
					    level, SYNC_TREE_NODE - 1));
		}

		break;

	case SYNC_TREE_NODE_NEW:

		ut_a(find(latches, SYNC_FSP_PAGE) != 0);
		break;

	case SYNC_INDEX_TREE:

		basic_check(latches, level, SYNC_TREE_NODE - 1);
		break;

	case SYNC_IBUF_TREE_NODE:

		ut_a(find(latches, SYNC_IBUF_INDEX_TREE) != 0
		     || basic_check(latches, level, SYNC_IBUF_TREE_NODE - 1));
		break;

	case SYNC_IBUF_TREE_NODE_NEW:

		/* ibuf_add_free_page() allocates new pages for the change
		buffer while only holding the tablespace x-latch. These
		pre-allocated new pages may only be used while holding
		ibuf_mutex, in btr_page_alloc_for_ibuf(). */

		ut_a(find(latches, SYNC_IBUF_MUTEX) != 0
		     || find(latches, SYNC_FSP) != 0);
		break;

	case SYNC_IBUF_INDEX_TREE:

		if (find(latches, SYNC_FSP) != 0) {
			basic_check(latches, level, level - 1);
		} else {
			basic_check(latches, level, SYNC_IBUF_TREE_NODE - 1);
		}
		break;

	case SYNC_IBUF_PESS_INSERT_MUTEX:

		basic_check(latches, level, SYNC_FSP - 1);
		ut_a(find(latches, SYNC_IBUF_MUTEX) == 0);
		break;

	case SYNC_IBUF_HEADER:

		basic_check(latches, level, SYNC_FSP - 1);
		ut_a(find(latches, SYNC_IBUF_MUTEX) == NULL);
		ut_a(find(latches, SYNC_IBUF_PESS_INSERT_MUTEX) == NULL);
		break;

	case SYNC_DICT:
		basic_check(latches, level, SYNC_DICT);
		break;

	case SYNC_MUTEX:
	case SYNC_UNKNOWN:
	case SYNC_LEVEL_VARYING:
	case RW_LOCK_X:
	case RW_LOCK_X_WAIT:
	case RW_LOCK_S:
	case RW_LOCK_SX:
	case RW_LOCK_NOT_LOCKED:
		/* These levels should never be set for a latch. */
		ut_error;
		break;
	}

	return(latches);
}

/** Removes a latch from the thread level array if it is found there.
@param[in]	latch		that was released/unlocked
@param[in]	level		level of the latch
@return true if found in the array; it is not an error if the latch is
not found, as we presently are not able to determine the level for
every latch reservation the program does */
void
LatchDebug::unlock(const latch_t* latch)
	UNIV_NOTHROW
{
	if (latch->get_level() == SYNC_LEVEL_VARYING) {
		// We don't have varying level mutexes
		ut_ad(latch->m_rw_lock);
	}

	Latches*	latches;

	if (*latch->get_name() == '.') {

		/* Ignore diagnostic latches, starting with '.' */

	} else if ((latches = thread_latches()) != NULL) {

		Latches::reverse_iterator	rend = latches->rend();

		for (Latches::reverse_iterator it = latches->rbegin();
		     it != rend;
		     ++it) {

			if (it->m_latch != latch) {

				continue;
			}

			Latches::iterator	i = it.base();

			latches->erase(--i);

			/* If this thread doesn't own any more
			latches remove from the map.

			FIXME: Perhaps use the master thread
			to do purge. Or, do it from close connection.
			This could be expensive. */

			if (latches->empty()) {

				m_mutex.enter();

				os_thread_id_t	thread_id;

				thread_id = os_thread_get_curr_id();

				m_threads.erase(thread_id);

				m_mutex.exit();

				UT_DELETE(latches);
			}

			return;
		}

		if (latch->get_level() != SYNC_LEVEL_VARYING) {
			ib::error()
				<< "Couldn't find latch "
				<< sync_latch_get_name(latch->get_id());

			print_latches(latches);

			/** Must find the latch. */
			ut_error;
		}
	}
}

/** Get the latch id from a latch name.
@param[in]	name	Latch name
@return latch id if found else LATCH_ID_NONE. */
latch_id_t
sync_latch_get_id(const char* name)
{
	LatchMetaData::const_iterator	end = latch_meta.end();

	/* Linear scan should be OK, this should be extremely rare. */

	for (LatchMetaData::const_iterator it = latch_meta.begin();
	     it != end;
	     ++it) {

		if (*it == NULL || (*it)->get_id() == LATCH_ID_NONE) {

			continue;

		} else if (strcmp((*it)->get_name(), name) == 0) {

			return((*it)->get_id());
		}
	}

	return(LATCH_ID_NONE);
}

/** Get the latch name from a sync level
@param[in]	level		Latch level to lookup
@return NULL if not found. */
const char*
sync_latch_get_name(latch_level_t level)
{
	LatchMetaData::const_iterator	end = latch_meta.end();

	/* Linear scan should be OK, this should be extremely rare. */

	for (LatchMetaData::const_iterator it = latch_meta.begin();
	     it != end;
	     ++it) {

		if (*it == NULL || (*it)->get_id() == LATCH_ID_NONE) {

			continue;

		} else if ((*it)->get_level() == level) {

			return((*it)->get_name());
		}
	}

	return(0);
}

/** Check if it is OK to acquire the latch.
@param[in]	latch	latch type */
void
sync_check_lock_validate(const latch_t* latch)
{
	if (LatchDebug::instance() != NULL) {
		LatchDebug::instance()->lock_validate(
			latch, latch->get_level());
	}
}

/** Note that the lock has been granted
@param[in]	latch	latch type */
void
sync_check_lock_granted(const latch_t* latch)
{
	if (LatchDebug::instance() != NULL) {
		LatchDebug::instance()->lock_granted(latch, latch->get_level());
	}
}

/** Check if it is OK to acquire the latch.
@param[in]	latch	latch type
@param[in]	level	Latch level */
void
sync_check_lock(
	const latch_t*	latch,
	latch_level_t	level)
{
	if (LatchDebug::instance() != NULL) {

		ut_ad(latch->get_level() == SYNC_LEVEL_VARYING);
		ut_ad(latch->get_id() == LATCH_ID_BUF_BLOCK_LOCK);

		LatchDebug::instance()->lock_validate(latch, level);
		LatchDebug::instance()->lock_granted(latch, level);
	}
}

/** Check if it is OK to re-acquire the lock.
@param[in]	latch		RW-LOCK to relock (recursive X locks) */
void
sync_check_relock(const latch_t* latch)
{
	if (LatchDebug::instance() != NULL) {
		LatchDebug::instance()->relock(latch);
	}
}

/** Removes a latch from the thread level array if it is found there.
@param[in]	latch		The latch to unlock */
void
sync_check_unlock(const latch_t* latch)
{
	if (LatchDebug::instance() != NULL) {
		LatchDebug::instance()->unlock(latch);
	}
}

/** Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@param[in]	level		to find
@return	a matching latch, or NULL if not found */
const latch_t*
sync_check_find(latch_level_t level)
{
	if (LatchDebug::instance() != NULL) {
		return(LatchDebug::instance()->find(level));
	}

	return(NULL);
}

/** Iterate over the thread's latches.
@param[in,out]	functor		called for each element.
@return false if the sync debug hasn't been initialised
@return the value returned by the functor */
bool
sync_check_iterate(sync_check_functor_t& functor)
{
	if (LatchDebug::instance() != NULL) {
		return(LatchDebug::instance()->for_each(functor));
	}

	return(false);
}

/** Enable sync order checking.

Note: We don't enforce any synchronisation checks. The caller must ensure
that no races can occur */
void
sync_check_enable()
{
	if (!srv_sync_debug) {

		return;
	}

	/* We should always call this before we create threads. */

	LatchDebug::create_instance();
}

/** Initialise the debug data structures */
void
LatchDebug::init()
	UNIV_NOTHROW
{
	ut_a(rw_lock_debug_event == NULL);

	mutex_create(LATCH_ID_RW_LOCK_DEBUG, &rw_lock_debug_mutex);

	rw_lock_debug_event = os_event_create("rw_lock_debug_event");

	rw_lock_debug_waiters = FALSE;
}

/** Shutdown the latch debug checking

Note: We don't enforce any synchronisation checks. The caller must ensure
that no races can occur */
void
LatchDebug::shutdown()
	UNIV_NOTHROW
{
	ut_a(rw_lock_debug_event != NULL);

	os_event_destroy(rw_lock_debug_event);

	rw_lock_debug_event = NULL;

	mutex_free(&rw_lock_debug_mutex);

	if (instance() == NULL) {

		return;
	}

	ut_a(s_initialized);

	s_initialized = false;

	UT_DELETE(s_instance);

	LatchDebug::s_instance = NULL;
}

/** Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
void
rw_lock_debug_mutex_enter()
{
	for (;;) {

		if (0 == mutex_enter_nowait(&rw_lock_debug_mutex)) {
			return;
		}

		os_event_reset(rw_lock_debug_event);

		rw_lock_debug_waiters = TRUE;

		if (0 == mutex_enter_nowait(&rw_lock_debug_mutex)) {
			return;
		}

		os_event_wait(rw_lock_debug_event);
	}
}

/** Releases the debug mutex. */
void
rw_lock_debug_mutex_exit()
{
	mutex_exit(&rw_lock_debug_mutex);

	if (rw_lock_debug_waiters) {
		rw_lock_debug_waiters = FALSE;
		os_event_set(rw_lock_debug_event);
	}
}
#endif /* UNIV_DEBUG */

/* Meta data for all the InnoDB latches. If the latch is not in recorded
here then it will be be considered for deadlock checks.  */
LatchMetaData	latch_meta;

/** Load the latch meta data. */
static
void
sync_latch_meta_init()
	UNIV_NOTHROW
{
	latch_meta.resize(LATCH_ID_MAX);

	/* The latches should be ordered on latch_id_t. So that we can
	index directly into the vector to update and fetch meta-data. */

	LATCH_ADD_MUTEX(AUTOINC, SYNC_DICT_AUTOINC_MUTEX, autoinc_mutex_key);

#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
	LATCH_ADD_MUTEX(BUF_BLOCK_MUTEX, SYNC_BUF_BLOCK,
			buffer_block_mutex_key);
#else
	LATCH_ADD_MUTEX(BUF_BLOCK_MUTEX, SYNC_BUF_BLOCK, PFS_NOT_INSTRUMENTED);
#endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */

	LATCH_ADD_MUTEX(BUF_POOL, SYNC_BUF_POOL, buf_pool_mutex_key);

	LATCH_ADD_MUTEX(BUF_POOL_ZIP, SYNC_BUF_BLOCK, buf_pool_zip_mutex_key);

	LATCH_ADD_MUTEX(CACHE_LAST_READ, SYNC_TRX_I_S_LAST_READ,
			cache_last_read_mutex_key);

	LATCH_ADD_MUTEX(DICT_FOREIGN_ERR, SYNC_NO_ORDER_CHECK,
			dict_foreign_err_mutex_key);

	LATCH_ADD_MUTEX(DICT_SYS, SYNC_DICT, dict_sys_mutex_key);

	LATCH_ADD_MUTEX(FILE_FORMAT_MAX, SYNC_FILE_FORMAT_TAG,
			file_format_max_mutex_key);

	LATCH_ADD_MUTEX(FIL_SYSTEM, SYNC_ANY_LATCH, fil_system_mutex_key);

	LATCH_ADD_MUTEX(FLUSH_LIST, SYNC_BUF_FLUSH_LIST, flush_list_mutex_key);

	LATCH_ADD_MUTEX(FTS_BG_THREADS, SYNC_FTS_BG_THREADS,
			fts_bg_threads_mutex_key);

	LATCH_ADD_MUTEX(FTS_DELETE, SYNC_FTS_OPTIMIZE, fts_delete_mutex_key);

	LATCH_ADD_MUTEX(FTS_OPTIMIZE, SYNC_FTS_OPTIMIZE,
			fts_optimize_mutex_key);

	LATCH_ADD_MUTEX(FTS_DOC_ID, SYNC_FTS_OPTIMIZE, fts_doc_id_mutex_key);

	LATCH_ADD_MUTEX(FTS_PLL_TOKENIZE, SYNC_FTS_TOKENIZE,
			fts_pll_tokenize_mutex_key);

	LATCH_ADD_MUTEX(HASH_TABLE_MUTEX, SYNC_BUF_PAGE_HASH,
			hash_table_mutex_key);

	LATCH_ADD_MUTEX(IBUF_BITMAP, SYNC_IBUF_BITMAP_MUTEX,
			ibuf_bitmap_mutex_key);

	LATCH_ADD_MUTEX(IBUF, SYNC_IBUF_MUTEX, ibuf_mutex_key);

	LATCH_ADD_MUTEX(IBUF_PESSIMISTIC_INSERT, SYNC_IBUF_PESS_INSERT_MUTEX,
			ibuf_pessimistic_insert_mutex_key);

	LATCH_ADD_MUTEX(LOG_SYS, SYNC_LOG, log_sys_mutex_key);

	LATCH_ADD_MUTEX(LOG_WRITE, SYNC_LOG_WRITE, log_sys_write_mutex_key);

	LATCH_ADD_MUTEX(LOG_FLUSH_ORDER, SYNC_LOG_FLUSH_ORDER,
			log_flush_order_mutex_key);

	LATCH_ADD_MUTEX(MUTEX_LIST, SYNC_NO_ORDER_CHECK, mutex_list_mutex_key);

	LATCH_ADD_MUTEX(PAGE_CLEANER, SYNC_PAGE_CLEANER,
			page_cleaner_mutex_key);

	LATCH_ADD_MUTEX(PURGE_SYS_PQ, SYNC_PURGE_QUEUE,
			purge_sys_pq_mutex_key);

	LATCH_ADD_MUTEX(RECALC_POOL, SYNC_STATS_AUTO_RECALC,
			recalc_pool_mutex_key);

	LATCH_ADD_MUTEX(RECV_SYS, SYNC_RECV, recv_sys_mutex_key);

	LATCH_ADD_MUTEX(RECV_WRITER, SYNC_RECV_WRITER, recv_writer_mutex_key);

	LATCH_ADD_MUTEX(REDO_RSEG, SYNC_REDO_RSEG, redo_rseg_mutex_key);

	LATCH_ADD_MUTEX(NOREDO_RSEG, SYNC_NOREDO_RSEG, noredo_rseg_mutex_key);

#ifdef UNIV_DEBUG
	/* Mutex names starting with '.' are not tracked. They are assumed
	to be diagnostic mutexes used in debugging. */
	latch_meta[LATCH_ID_RW_LOCK_DEBUG] =
		LATCH_ADD_MUTEX(RW_LOCK_DEBUG,
			SYNC_NO_ORDER_CHECK,
			rw_lock_debug_mutex_key);
#endif /* UNIV_DEBUG */

	LATCH_ADD_MUTEX(RTR_SSN_MUTEX, SYNC_ANY_LATCH, rtr_ssn_mutex_key);

	LATCH_ADD_MUTEX(RTR_ACTIVE_MUTEX, SYNC_ANY_LATCH,
			rtr_active_mutex_key);

	LATCH_ADD_MUTEX(RTR_MATCH_MUTEX, SYNC_ANY_LATCH, rtr_match_mutex_key);

	LATCH_ADD_MUTEX(RTR_PATH_MUTEX, SYNC_ANY_LATCH, rtr_path_mutex_key);

	LATCH_ADD_MUTEX(RW_LOCK_LIST, SYNC_NO_ORDER_CHECK,
			rw_lock_list_mutex_key);

	LATCH_ADD_MUTEX(RW_LOCK_MUTEX, SYNC_NO_ORDER_CHECK, rw_lock_mutex_key);

	LATCH_ADD_MUTEX(SRV_DICT_TMPFILE, SYNC_DICT_OPERATION,
			srv_dict_tmpfile_mutex_key);

	LATCH_ADD_MUTEX(SRV_INNODB_MONITOR, SYNC_NO_ORDER_CHECK,
			srv_innodb_monitor_mutex_key);

	LATCH_ADD_MUTEX(SRV_MISC_TMPFILE, SYNC_ANY_LATCH,
			srv_misc_tmpfile_mutex_key);

	LATCH_ADD_MUTEX(SRV_MONITOR_FILE, SYNC_NO_ORDER_CHECK,
			srv_monitor_file_mutex_key);

#ifdef UNIV_DEBUG
	LATCH_ADD_MUTEX(SYNC_THREAD, SYNC_NO_ORDER_CHECK,
			sync_thread_mutex_key);
#else
	LATCH_ADD_MUTEX(SYNC_THREAD, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
#endif /* UNIV_DEBUG */

	LATCH_ADD_MUTEX(BUF_DBLWR, SYNC_DOUBLEWRITE, buf_dblwr_mutex_key);

	LATCH_ADD_MUTEX(TRX_UNDO, SYNC_TRX_UNDO, trx_undo_mutex_key);

	LATCH_ADD_MUTEX(TRX_POOL, SYNC_POOL, trx_pool_mutex_key);

	LATCH_ADD_MUTEX(TRX_POOL_MANAGER, SYNC_POOL_MANAGER,
			trx_pool_manager_mutex_key);

	LATCH_ADD_MUTEX(TRX, SYNC_TRX, trx_mutex_key);

	LATCH_ADD_MUTEX(LOCK_SYS, SYNC_LOCK_SYS, lock_mutex_key);

	LATCH_ADD_MUTEX(LOCK_SYS_WAIT, SYNC_LOCK_WAIT_SYS,
			lock_wait_mutex_key);

	LATCH_ADD_MUTEX(TRX_SYS, SYNC_TRX_SYS, trx_sys_mutex_key);

	LATCH_ADD_MUTEX(SRV_SYS, SYNC_THREADS, srv_sys_mutex_key);

	LATCH_ADD_MUTEX(SRV_SYS_TASKS, SYNC_ANY_LATCH, srv_threads_mutex_key);

	LATCH_ADD_MUTEX(PAGE_ZIP_STAT_PER_INDEX, SYNC_ANY_LATCH,
			page_zip_stat_per_index_mutex_key);

#ifndef PFS_SKIP_EVENT_MUTEX
	LATCH_ADD_MUTEX(EVENT_MANAGER, SYNC_NO_ORDER_CHECK,
			event_manager_mutex_key);
	LATCH_ADD_MUTEX(EVENT_MUTEX, SYNC_NO_ORDER_CHECK, event_mutex_key);
#else
	LATCH_ADD_MUTEX(EVENT_MANAGER, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);
	LATCH_ADD_MUTEX(EVENT_MUTEX, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);
#endif /* !PFS_SKIP_EVENT_MUTEX */

	LATCH_ADD_MUTEX(SYNC_ARRAY_MUTEX, SYNC_NO_ORDER_CHECK,
			sync_array_mutex_key);

	LATCH_ADD_MUTEX(THREAD_MUTEX, SYNC_NO_ORDER_CHECK, thread_mutex_key);

	LATCH_ADD_MUTEX(ZIP_PAD_MUTEX, SYNC_NO_ORDER_CHECK, zip_pad_mutex_key);

	LATCH_ADD_MUTEX(OS_AIO_READ_MUTEX, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);

	LATCH_ADD_MUTEX(OS_AIO_WRITE_MUTEX, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);

	LATCH_ADD_MUTEX(OS_AIO_LOG_MUTEX, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);

	LATCH_ADD_MUTEX(OS_AIO_IBUF_MUTEX, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);

	LATCH_ADD_MUTEX(OS_AIO_SYNC_MUTEX, SYNC_NO_ORDER_CHECK,
			PFS_NOT_INSTRUMENTED);

	LATCH_ADD_MUTEX(ROW_DROP_LIST, SYNC_NO_ORDER_CHECK,
			row_drop_list_mutex_key);

	LATCH_ADD_RWLOCK(INDEX_ONLINE_LOG, SYNC_INDEX_ONLINE_LOG,
			index_online_log_key);

	LATCH_ADD_MUTEX(WORK_QUEUE, SYNC_WORK_QUEUE, PFS_NOT_INSTRUMENTED);

	// Add the RW locks
	LATCH_ADD_RWLOCK(BTR_SEARCH, SYNC_SEARCH_SYS, btr_search_latch_key);

#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
	LATCH_ADD_RWLOCK(BUF_BLOCK_LOCK, SYNC_LEVEL_VARYING,
			 buf_block_lock_key);
#else
	LATCH_ADD_RWLOCK(BUF_BLOCK_LOCK, SYNC_LEVEL_VARYING,
			 PFS_NOT_INSTRUMENTED);
#endif /* PFS_SKIP_BUFFER_MUTEX_RWLOCK */

#ifdef UNIV_DEBUG
	LATCH_ADD_RWLOCK(BUF_BLOCK_DEBUG, SYNC_NO_ORDER_CHECK,
			 buf_block_debug_latch_key);
#else
	LATCH_ADD_RWLOCK(BUF_BLOCK_DEBUG, SYNC_NO_ORDER_CHECK,
			 PFS_NOT_INSTRUMENTED);
#endif /* UNIV_DEBUG */

	LATCH_ADD_RWLOCK(DICT_OPERATION, SYNC_DICT, dict_operation_lock_key);

	LATCH_ADD_RWLOCK(CHECKPOINT, SYNC_NO_ORDER_CHECK, checkpoint_lock_key);

	LATCH_ADD_RWLOCK(FIL_SPACE, SYNC_FSP, fil_space_latch_key);

	LATCH_ADD_RWLOCK(FTS_CACHE, SYNC_FTS_CACHE, fts_cache_rw_lock_key);

	LATCH_ADD_RWLOCK(FTS_CACHE_INIT, SYNC_FTS_CACHE_INIT,
			 fts_cache_init_rw_lock_key);

	LATCH_ADD_RWLOCK(TRX_I_S_CACHE, SYNC_TRX_I_S_RWLOCK,
			 trx_i_s_cache_lock_key);

	LATCH_ADD_RWLOCK(TRX_PURGE, SYNC_PURGE_LATCH, trx_purge_latch_key);

	LATCH_ADD_RWLOCK(IBUF_INDEX_TREE, SYNC_IBUF_INDEX_TREE,
			 index_tree_rw_lock_key);

	LATCH_ADD_RWLOCK(INDEX_TREE, SYNC_INDEX_TREE, index_tree_rw_lock_key);

	LATCH_ADD_RWLOCK(DICT_TABLE_STATS, SYNC_INDEX_TREE,
			 dict_table_stats_key);

	LATCH_ADD_RWLOCK(HASH_TABLE_RW_LOCK, SYNC_BUF_PAGE_HASH,
			 hash_table_locks_key);

	LATCH_ADD_RWLOCK(SYNC_DEBUG_MUTEX, SYNC_NO_ORDER_CHECK,
			 PFS_NOT_INSTRUMENTED);

	LATCH_ADD_MUTEX(MASTER_KEY_ID_MUTEX, SYNC_NO_ORDER_CHECK,
			master_key_id_mutex_key);

	latch_id_t	id = LATCH_ID_NONE;

	/* The array should be ordered on latch ID.We need to
	index directly into it from the mutex policy to update
	the counters and access the meta-data. */

	for (LatchMetaData::iterator it = latch_meta.begin();
	     it != latch_meta.end();
	     ++it) {

		const latch_meta_t*	meta = *it;

		/* Skip blank entries */
		if (meta == NULL || meta->get_id() == LATCH_ID_NONE) {
			continue;
		}

		ut_a(id < meta->get_id());

		id = meta->get_id();
	}
}

/** Destroy the latch meta data */
static
void
sync_latch_meta_destroy()
{
	for (LatchMetaData::iterator it = latch_meta.begin();
	     it != latch_meta.end();
	     ++it) {

		UT_DELETE(*it);
	}

	latch_meta.clear();
}

/** Track mutex file creation name and line number. This is to avoid storing
{ const char* name; uint16_t line; } in every instance. This results in the
sizeof(Mutex) > 64. We use a lookup table to store it separately. Fetching
the values is very rare, only required for diagnostic purposes. And, we
don't create/destroy mutexes that frequently. */
struct CreateTracker {

	/** Constructor */
	CreateTracker()
		UNIV_NOTHROW
	{
		m_mutex.init();
	}

	/** Destructor */
	~CreateTracker()
		UNIV_NOTHROW
	{
		ut_d(m_files.empty());

		m_mutex.destroy();
	}

	/** Register where the latch was created
	@param[in]	ptr		Latch instance
	@param[in]	filename	Where created
	@param[in]	line		Line number in filename */
	void register_latch(
		const void*	ptr,
		const char*	filename,
		uint16_t	line)
		UNIV_NOTHROW
	{
		m_mutex.enter();

		Files::iterator	lb = m_files.lower_bound(ptr);

		ut_ad(lb == m_files.end()
		      || m_files.key_comp()(ptr, lb->first));

		typedef Files::value_type value_type;

		m_files.insert(lb, value_type(ptr, File(filename, line)));

		m_mutex.exit();
	}

	/** Deregister a latch - when it is destroyed
	@param[in]	ptr		Latch instance being destroyed */
	void deregister_latch(const void* ptr)
		UNIV_NOTHROW
	{
		m_mutex.enter();

		Files::iterator	lb = m_files.lower_bound(ptr);

		ut_ad(lb != m_files.end()
		      && !(m_files.key_comp()(ptr, lb->first)));

		m_files.erase(lb);

		m_mutex.exit();
	}

	/** Get the create string, format is "name:line"
	@param[in]	ptr		Latch instance
	@return the create string or "" if not found */
	std::string get(const void* ptr)
		UNIV_NOTHROW
	{
		m_mutex.enter();

		std::string	created;

		Files::iterator	lb = m_files.lower_bound(ptr);

		if (lb != m_files.end()
		    && !(m_files.key_comp()(ptr, lb->first))) {

			std::ostringstream	msg;

			msg << lb->second.m_name << ":" << lb->second.m_line;

			created = msg.str();
		}

		m_mutex.exit();

		return(created);
	}

private:
	/** For tracking the filename and line number */
	struct File {

		/** Constructor */
		File() UNIV_NOTHROW : m_name(), m_line() { }

		/** Constructor
		@param[in]	name		Filename where created
		@param[in]	line		Line number where created */
		File(const char*  name, uint16_t line)
			UNIV_NOTHROW
			:
			m_name(sync_basename(name)),
			m_line(line)
		{
			/* No op */
		}

		/** Filename where created */
		std::string		m_name;

		/** Line number where created */
		uint16_t		m_line;
	};

	/** Map the mutex instance to where it was created */
	typedef std::map<
		const void*,
		File,
		std::less<const void*>,
		ut_allocator<std::pair<const void* const, File> > >
		Files;

	typedef OSMutex	Mutex;

	/** Mutex protecting m_files */
	Mutex			m_mutex;

	/** Track the latch creation */
	Files			m_files;
};

/** Track latch creation location. For reducing the size of the latches */
static CreateTracker*	create_tracker;

/** Register a latch, called when it is created
@param[in]	ptr		Latch instance that was created
@param[in]	filename	Filename where it was created
@param[in]	line		Line number in filename */
void
sync_file_created_register(
	const void*	ptr,
	const char*	filename,
	uint16_t	line)
{
	create_tracker->register_latch(ptr, filename, line);
}

/** Deregister a latch, called when it is destroyed
@param[in]	ptr		Latch to be destroyed */
void
sync_file_created_deregister(const void* ptr)
{
	create_tracker->deregister_latch(ptr);
}

/** Get the string where the file was created. Its format is "name:line"
@param[in]	ptr		Latch instance
@return created information or "" if can't be found */
std::string
sync_file_created_get(const void* ptr)
{
	return(create_tracker->get(ptr));
}

/** Initializes the synchronization data structures. */
void
sync_check_init()
{
	ut_ad(!LatchDebug::s_initialized);
	ut_d(LatchDebug::s_initialized = true);

	/** For collecting latch statistic - SHOW ... MUTEX */
	mutex_monitor = UT_NEW_NOKEY(MutexMonitor());

	/** For trcking mutex creation location */
	create_tracker = UT_NEW_NOKEY(CreateTracker());

	sync_latch_meta_init();

	/* Init the rw-lock & mutex list and create the mutex to protect it. */

	UT_LIST_INIT(rw_lock_list, &rw_lock_t::list);

	mutex_create(LATCH_ID_RW_LOCK_LIST, &rw_lock_list_mutex);

	ut_d(LatchDebug::init());

	sync_array_init(OS_THREAD_MAX_N);
}

/** Frees the resources in InnoDB's own synchronization data structures. Use
os_sync_free() after calling this. */
void
sync_check_close()
{
	ut_d(LatchDebug::shutdown());

	mutex_free(&rw_lock_list_mutex);

	sync_array_close();

	UT_DELETE(mutex_monitor);

	mutex_monitor = NULL;

	UT_DELETE(create_tracker);

	create_tracker = NULL;

	sync_latch_meta_destroy();
}

