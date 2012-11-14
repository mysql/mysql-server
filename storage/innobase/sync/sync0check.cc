/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file sync/sync0check.cc
Debug checks for latches.

Created 2012-08-21 Sunny Bains
*******************************************************/

#include "sync0check.h"

#ifdef UNIV_NONINL
#include "sync0check.ic"
#endif

#include "sync0rw.h"
#include "sync0mutex.h"
#include "sync0sync.h"

// FIXME: Get rid of this
#include "srv0start.h"

#include "ha_prototypes.h"

#include <map>
#include <vector>
#include <string>

extern	ulint	srv_max_n_threads;

// FIXME: Add duplicate checking when initialising the map.
#ifdef UNIV_PFS_MUTEX
#define LATCH_ADD(m, n, l, k)					\
	do { (*m)[n] = latch_meta_t((n), (l), (#l), (k)); } while (0)
#else
#define LATCH_ADD(m, n, l, k)					\
	do { (*m)[n] = latch_meta_t((n), (l), (#l)); } while (0)
#endif /* UNIV_PFS_MUTEX */

# ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_PFS_MUTEX */

static bool sync_check_initialised = false;

typedef TTASMutex<TrackPolicy> SyncMutex;

typedef std::vector<const latch_t*> Latches;

/** Latch meta-data */
struct latch_meta_t {

#ifdef UNIV_PFS_MUTEX
	/**
	@param name - mutex name
	@pram level - the latch level
	@para level_name - text representation of level
	@param pfs_key - performance schema key */
	latch_meta_t(
		const char*	name,
		latch_level_t	level,
		const char*	level_name,
		mysql_pfs_key_t pfs_key)
		:
		m_name(name),
		m_level(level),
		m_pfs_key(pfs_key),
		m_level_name(level_name) { }
#else /* UNIV_PFS_MUTEX */
	/**
	@param name - mutex name
	@pram level - the latch level
	@para level_name - text representation of level */
	latch_meta_t(
		const char*	name,
		latch_level_t	level,
		const char*	level_name)
		:
		m_name(name),
		m_level(level),
       		m_level_name(level_name) { }

#endif /* UNIV_PFS_MUTEX */

	latch_meta_t()
		:
		m_name(0),
		m_level(SYNC_UNKNOWN),
#ifdef UNIV_PFS_MUTEX
		m_pfs_key(PFS_NOT_INSTRUMENTED),
#endif /* UNIV_PFS_MUTEX */
		m_level_name(0) { }

	/** Latch name */
	const char*		m_name;

	/** Latch ordering level. */
	latch_level_t		m_level;

#ifdef UNIV_PFS_MUTEX
	/** PFS key */
	mysql_pfs_key_t		m_pfs_key;
#endif /* UNIV_PFS_MUTEX */

	/** Level textual representation. */
	const char*		m_level_name;

	/** The latches created for a level. */
	Latches			m_latches;
};


typedef std::map<std::string, latch_meta_t> LatchMap;

/** Mapping from latch name to latch meta data. */
static LatchMap* SrvLatches;

/** The deadlock detector. */
struct SyncCheck {

	/** Comparator for the ThreadMap. */
	struct os_thread_id_less
		: public std::binary_function<
		  os_thread_id_t,
		  os_thread_id_t,
		  bool>
	{
		bool operator()(
			const os_thread_id_t& t1,
			const os_thread_id_t& t2) const
		{
			return(os_thread_pf(t1) < os_thread_pf(t2));
		}
	};

	/** For tracking a thread's latches. */
	typedef std::map<os_thread_id_t, Latches*, os_thread_id_less> ThreadMap;

	SyncCheck()
	{
		new (&m_mutex) SyncMutex();
	}

	~SyncCheck()
	{
		m_mutex.destroy();
	}

	/**
	Create a new instance if one doesn't exist else return the existing one.
	@param add - add an empty entry if one is not found (default no)
	@return	pointer to a thread's acquired latches. */
	Latches* thread_latches(bool add = false) UNIV_NOTHROW;

	// FIXME: These two can be combined using STL functions.

	/**
	Check that all the latches already owned by a thread have a lower
	level than limit.
	@param levels - the thread's existing (acquired) latches
	@param limit - to check against 
	@return latch if there is one with a level <= limit . */
	const latch_t* less(
		const Latches*	latches,
		latch_level_t	limit) const UNIV_NOTHROW;

	/**
	Checks if the level value exists in the thread's acquired latches.
	@param levels - the thread's existing (acquired) latches
	@param level - to lookup
	@return	latch if found or 0 */
	const latch_t* find(
		const Latches*	Latches,
		latch_level_t	level) const UNIV_NOTHROW;

	/**
	Checks if the level value exists in the thread's acquired latches.
	@param level - to lookup
	@return	latch if found or 0 */
	const latch_t* find(latch_level_t level) UNIV_NOTHROW;

	/** Report error and abort. */
	void crash(const latch_t* latch, latch_level_t level)
		const UNIV_NOTHROW;

	/** Do a basic ordering check.
	@param latches - thread's existing latches
	@param level - declared ulint so that we can do level - 1
	@return true if passes, else crash with error message. */
	bool basic_check(const Latches* latches, ulint level)
		const UNIV_NOTHROW;

	/**
	Adds a latch and its level in the thread level array. Allocates
	the memory for the array if called first time for this OS thread.
	Makes the checks against other latch levels stored in the array
	for this thread.

	@param latch - latch that the thread wants to acqire. */
	void lock(const latch_t* latch) UNIV_NOTHROW
	{
		if (!m_enabled || latch->m_level == SYNC_LEVEL_VARYING) {
			return;
		}

		check_order(latch)->push_back(latch);
	}

	/**
	For recursive X rw-locks. */
	void relock(const latch_t* latch) UNIV_NOTHROW
	{
		if (!m_enabled || latch->m_level == SYNC_LEVEL_VARYING) {
			return;
		}

		Latches*	latches = thread_latches(true);

		/* If it is a relock() then it can't be empty. */
		ut_a(!latches->empty());

		latches->push_back(latch);
	}

	/**
	Iterate over a thread's latches.
	@return true if the functor returns true. */
	bool for_each(sync_check_functor_t& functor) UNIV_NOTHROW
	{
		mutex_enter(&m_mutex);

		const Latches*		latches = thread_latches();

		mutex_exit(&m_mutex);

		if (latches == 0) {
			return(functor.result());
		}

		Latches::const_iterator	end = latches->end();

		for (Latches::const_iterator it = latches->begin();
		     it != end;
		     ++it) {

			if (functor(*(*it))) {
				break;
			}
		}

		return(functor.result());
	}

	/**
	Removes a latch from the thread level array if it is found there.
	@return true if found in the array; it is not an error if the latch is
	not found, as we presently are not able to determine the level for
	every latch reservation the program does */
	void unlock(const latch_t* latch) UNIV_NOTHROW;

	/** Enable checking */
	void enable() UNIV_NOTHROW
	{
		ut_a(!m_enabled);
		m_enabled = true;
	}

private:
	SyncCheck(const SyncCheck&);
	SyncCheck& operator=(const SyncCheck&);

	/**
	Adds a latch and its level in the thread level array. Allocates
	the memory for the array if called first time for this OS thread.
	Makes the checks against other latch levels stored in the array
	for this thread.

	@param latch - latch that the thread wants to acqire. */
	Latches* check_order(const latch_t* latch) UNIV_NOTHROW;

public:
	/** Mutex protecting the deadlock detector data structures. */
	SyncMutex		m_mutex;

	/** Latching order checks start when this is set true */
	bool			m_enabled;

	/** This variable is set to TRUE when sync_init is called */
	bool			m_initialised;

	/** Thread specific data. */
	ThreadMap		m_threads;
};

static SyncCheck syncCheck;

/** Report error and abort. */
void
SyncCheck::crash(const latch_t* latch, latch_level_t level)
	const UNIV_NOTHROW
{
	//mutex_own(&m_mutex);

	ib_logf(IB_LOG_LEVEL_ERROR,
		"Thread already owns a latch (\"%s\" : %lu), "
		"with a lower level than (\"%s\" : %lu).",
		sync_latch_get_name(latch->m_level),
		(ulint) latch->m_level,
		sync_latch_get_name(level), (ulint) level);

		latch->print(stderr);

	ut_error;
}

/** Do a basic ordering check.
@return true if passes, else crash with error message. */
bool
SyncCheck::basic_check(const Latches* latches, ulint lvl)
	const UNIV_NOTHROW
{
	latch_level_t	level = latch_level_t(lvl);

	//ut_ad(mutex_own(&m_mutex));

	const latch_t*	latch = less(latches, level);

	if (latch != 0) {
		crash(latch, level);
		return(false);
	}

	return(true);
}

/**
Create a new instance if one doesn't exist else return the existing one.
@return	pointer to a thread's acquired latches. */
Latches*
SyncCheck::thread_latches(bool add) UNIV_NOTHROW
{
	//ut_ad(mutex_own(&m_mutex));

	os_thread_id_t	thread_id = os_thread_get_curr_id();

	ThreadMap::iterator lb = m_threads.lower_bound(thread_id);

	if (lb != m_threads.end()
	    && !(m_threads.key_comp()(thread_id, lb->first))) {

		return(lb->second);

	} else if (!add) {

		return(0);

	} else {
		typedef ThreadMap::value_type Item;

		Latches*	latches = new(std::nothrow) Latches();

		ut_a(latches != 0);

		latches->reserve(32);

		return(m_threads.insert(lb, Item(thread_id, latches))->second);
	}
}

/**
Check that all the latches already owned by a thread have a lower
level than limit.
@param levels - the thread's existing (acquired) latches
@param limit - to check against 
@return latch if there is one with a level <= limit . */
const latch_t*
SyncCheck::less(
	const Latches*	latches,
	latch_level_t	limit) const UNIV_NOTHROW
{
	//ut_ad(mutex_own(&m_mutex));

	Latches::const_iterator	end = latches->end();

	for (Latches::const_iterator it = latches->begin(); it != end; ++it) {

		if ((*it)->m_level <= limit) {
			return(*it);
		}
	}

	return(0);
}

/**
Checks if the level value exists in the thread's acquired latches.
@param levels - the thread's existing (acquired) latches
@param level - to lookup
@return	latch if found or 0 */
const latch_t*
SyncCheck::find(
	const Latches*	latches,
	latch_level_t	level) const UNIV_NOTHROW
{
	//ut_ad(mutex_own(&m_mutex));

	Latches::const_iterator	end = latches->end();

	for (Latches::const_iterator it = latches->begin(); it != end; ++it) {

		if ((*it)->m_level == level) {
			return(*it);
		}
	}

	return(0);
}

/**
Checks if the level value exists in the thread's acquired latches.
@param level - to lookup
@return	latch if found or 0 */
const latch_t*
SyncCheck::find(latch_level_t level) UNIV_NOTHROW
{
	ut_ad(m_enabled);
	//ut_ad(mutex_own(&m_mutex));

	return(find(thread_latches(), level));
}

/**
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread.

@param latch - pointer to a mutex or an rw-lock
@param level - level in the latching order 
@return the thread's latches */
Latches*
SyncCheck::check_order(const latch_t* latch)
{
	ut_ad(m_enabled && latch->m_level != SYNC_LEVEL_VARYING);

	mutex_enter(&m_mutex);

	Latches*	latches = thread_latches(true);

	/* NOTE that there is a problem with _NODE and _LEAF levels: if the
	B-tree height changes, then a leaf can change to an internal node
	or the other way around. We do not know at present if this can cause
	unnecessary assertion failures below. */

	switch (latch->m_level) {
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

	case SYNC_MEM_POOL:
	case SYNC_MEM_HASH:
	case SYNC_RECV:
	case SYNC_FTS_BG_THREADS:
	case SYNC_WORK_QUEUE:
	case SYNC_FTS_OPTIMIZE:
	case SYNC_FTS_CACHE:
	case SYNC_FTS_CACHE_INIT:
	case SYNC_LOG:
	case SYNC_LOG_FLUSH_ORDER:
	case SYNC_ANY_LATCH:
	case SYNC_FILE_FORMAT_TAG:
	case SYNC_DOUBLEWRITE:
	case SYNC_SEARCH_SYS:
	case SYNC_THREADS:
	case SYNC_LOCK_SYS:
	case SYNC_LOCK_WAIT_SYS:
	case SYNC_TRX_SYS:
	case SYNC_IBUF_BITMAP_MUTEX:
	case SYNC_RSEG:
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

		basic_check(latches, latch->m_level);
		break;

	case SYNC_TRX:

		/* Either the thread must own the lock_sys->mutex, or
		it is allowed to own only ONE trx_t::mutex. */

		if (less(latches, latch->m_level) != 0) {
			basic_check(latches, latch->m_level - 1);
			ut_a(find(latches, SYNC_LOCK_SYS) != 0);
		}
		break;

	case SYNC_BUF_FLUSH_LIST:
	case SYNC_BUF_POOL:

		/* We can have multiple mutexes of this type therefore we
		can only check whether the greater than condition holds. */

		basic_check(latches, latch->m_level - 1);
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

		if (less(latches, latch->m_level) != 0) {
			basic_check(latches, latch->m_level - 1);
			ut_a(find(latches, SYNC_BUF_POOL) != 0);
		}
		break;

	case SYNC_REC_LOCK:

		if (find(latches, SYNC_LOCK_SYS) != 0) {
			basic_check(latches, SYNC_REC_LOCK - 1);
		} else {
			basic_check(latches, SYNC_REC_LOCK);
		}
		break;

	case SYNC_IBUF_BITMAP:

		/* Either the thread must own the master mutex to all
		the bitmap pages, or it is allowed to latch only ONE
		bitmap page. */

		if (find(latches, SYNC_IBUF_BITMAP_MUTEX) != 0) {

			basic_check(latches, SYNC_IBUF_BITMAP - 1);

		} else if (!srv_is_being_started) {

			/* This is violated during trx_sys_create_rsegs()
			when creating additional rollback segments during
			upgrade. */

			basic_check(latches, SYNC_IBUF_BITMAP);
		}
		break;

	case SYNC_FSP_PAGE:
		ut_a(find(latches, SYNC_FSP) != 0);
		break;

	case SYNC_FSP:

		ut_a(find(latches, SYNC_FSP) != 0
		     || basic_check(latches, SYNC_FSP));
		break;

	case SYNC_TRX_UNDO_PAGE:

		/* Purge is allowed to read in as many UNDO pages as it likes.
		The purge thread can read the UNDO pages without any covering
		mutex. */

		ut_a(find(latches, SYNC_TRX_UNDO) != 0
		     || find(latches, SYNC_RSEG) != 0
		     || basic_check(latches, latch->m_level - 1));
		break;

	case SYNC_RSEG_HEADER:

		ut_a(find(latches, SYNC_RSEG) != 0);
		break;

	case SYNC_RSEG_HEADER_NEW:

		ut_a(find(latches, SYNC_FSP_PAGE) != 0);
		break;

	case SYNC_TREE_NODE:

		ut_a(find(latches, SYNC_INDEX_TREE) != 0
		     || find(latches, SYNC_DICT_OPERATION)
		     || basic_check(latches, SYNC_TREE_NODE - 1));
		break;

	case SYNC_TREE_NODE_NEW:

		ut_a(find(latches, SYNC_FSP_PAGE) != 0);
		break;

	case SYNC_INDEX_TREE:
		basic_check(latches, SYNC_TREE_NODE - 1);
		break;

	case SYNC_IBUF_TREE_NODE:

		ut_a(find(latches, SYNC_IBUF_INDEX_TREE) != 0
		     || basic_check(latches, SYNC_IBUF_TREE_NODE - 1));
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
			basic_check(latches, latch->m_level - 1);
		} else {
			basic_check(latches, SYNC_IBUF_TREE_NODE - 1);
		}
		break;

	case SYNC_IBUF_PESS_INSERT_MUTEX:

		basic_check(latches, SYNC_FSP - 1);
		ut_a(find(latches, SYNC_IBUF_MUTEX) != 0);
		break;

	case SYNC_IBUF_HEADER:

		basic_check(latches, SYNC_FSP - 1);
		ut_a(find(latches, SYNC_IBUF_MUTEX) != 0);
		ut_a(find(latches, SYNC_IBUF_PESS_INSERT_MUTEX) != 0);
		break;

	case SYNC_DICT:
		basic_check(latches, SYNC_DICT);
		break;

	case SYNC_MUTEX:
	case SYNC_UNKNOWN:
	case SYNC_LEVEL_VARYING:
	case RW_LOCK_X:
	case RW_LOCK_X_WAIT:
	case RW_LOCK_S:
	case RW_LOCK_NOT_LOCKED:
	case SYNC_USER_TRX_LOCK:
		/* These levels should never be set for a latch. */
		ut_error;
		break;
	}

	mutex_exit(&m_mutex);

	return(latches);
}

/*
Removes a latch from the thread level array if it is found there.
@param latch - that was released/unlocked
@param level - level of the latch
@return true if found in the array; it is not an error if the latch is
not found, as we presently are not able to determine the level for
every latch reservation the program does */
void
SyncCheck::unlock(const latch_t* latch)
{
	if (!m_enabled) {
		return;
	} else if (latch->m_level == SYNC_LEVEL_VARYING) {
		// We don't have varying level mutexes
		ut_ad(latch->m_rw_lock);
	} else {

		Latches*	latches = thread_latches();

		ut_ad(latches != 0);

		Latches::iterator	end = latches->end();

		for (Latches::iterator it = latches->begin(); it != end; ++it) {
			if ((*it)->m_level == latch->m_level) {

				latches->erase(it);

				/* If this thread doesn't own any more
				latches remove from the map.

				FIXME: This could be inefficient. Perhaps use
				the master thread to do purge. */

				if (latches->empty()) {

					mutex_enter(&m_mutex);

					os_thread_id_t	thread_id;

					thread_id = os_thread_get_curr_id();

					m_threads.erase(thread_id);

					mutex_exit(&m_mutex);

					delete latches;
				}

				return;
			}
		}

		/** Must find the latch. */
		ut_error;
	}
}

/******************************************************************//**
Load the latch meta data. */
static
void
sync_latch_meta_init()
/*==================*/
{
	// First add the mutexes
	LATCH_ADD(SrvLatches, "autoinc",
		  SYNC_DICT_AUTOINC_MUTEX, autoinc_mutex_key);

	LATCH_ADD(SrvLatches, "buf_block_mutex",
		  SYNC_BUF_BLOCK, buffer_block_mutex_key);

	LATCH_ADD(SrvLatches, "buf_pool",
		  SYNC_BUF_POOL, buf_pool_mutex_key);

	LATCH_ADD(SrvLatches, "buf_pool_zip",
		  SYNC_BUF_BLOCK, buf_pool_zip_mutex_key);

	LATCH_ADD(SrvLatches, "cache_last_read",
		  SYNC_TRX_I_S_LAST_READ, cache_last_read_mutex_key);

	LATCH_ADD(SrvLatches, "dict_foreign_err",
		  SYNC_NO_ORDER_CHECK, dict_foreign_err_mutex_key);

	LATCH_ADD(SrvLatches, "dict_sys",
		  SYNC_DICT, dict_sys_mutex_key);

	LATCH_ADD(SrvLatches, "file_format_max",
		  SYNC_FILE_FORMAT_TAG, file_format_max_mutex_key);

	LATCH_ADD(SrvLatches, "fil_system",
		  SYNC_ANY_LATCH, fil_system_mutex_key);

	LATCH_ADD(SrvLatches, "flush_list",
		  SYNC_BUF_FLUSH_LIST, flush_list_mutex_key);

	LATCH_ADD(SrvLatches, "fts_bg_threads",
		  SYNC_FTS_BG_THREADS, fts_bg_threads_mutex_key);

	LATCH_ADD(SrvLatches, "fts_delete",
		  SYNC_FTS_OPTIMIZE , fts_delete_mutex_key);

	LATCH_ADD(SrvLatches, "fts_optimize",
		  SYNC_FTS_OPTIMIZE, fts_optimize_mutex_key);

	LATCH_ADD(SrvLatches, "fts_doc_id",
		  SYNC_FTS_OPTIMIZE, fts_doc_id_mutex_key);

	LATCH_ADD(SrvLatches, "hash_table_mutex",
		  SYNC_BUF_PAGE_HASH, hash_table_mutex_key);

	LATCH_ADD(SrvLatches, "ibuf_bitmap",
		  SYNC_IBUF_BITMAP_MUTEX, ibuf_bitmap_mutex_key);

	LATCH_ADD(SrvLatches, "ibuf",
		  SYNC_IBUF_MUTEX, ibuf_mutex_key);

	LATCH_ADD(SrvLatches, "ibuf_pessimistic_insert",
		  SYNC_IBUF_PESS_INSERT_MUTEX,
		  ibuf_pessimistic_insert_mutex_key);

	LATCH_ADD(SrvLatches, "log_sys",
		  SYNC_LOG, log_sys_mutex_key);

	LATCH_ADD(SrvLatches, "log_flush_order",
		  SYNC_LOG_FLUSH_ORDER, log_flush_order_mutex_key);

#ifndef HAVE_ATOMIC_BUILTINS
	LATCH_ADD(SrvLatches, "server",
		  SYNC_THREADS, server_mutex_key);
#endif /* !HAVE_ATOMIC_BUILTINS */

#ifdef UNIV_MEM_DEBUG
	LATCH_ADD(SrvLatches, "mem_hash",
		  SYNC_MEM_HASH, mem_hash_mutex_key);
#endif /* UNIV_MEM_DEBUG */

	LATCH_ADD(SrvLatches, "mem_pool",
		  SYNC_MEM_POOL, mem_pool_mutex_key);

	LATCH_ADD(SrvLatches, "purge_sys_bh",
		  SYNC_PURGE_QUEUE, purge_sys_bh_mutex_key);

	LATCH_ADD(SrvLatches, "recalc_pool",
		  SYNC_STATS_AUTO_RECALC, recalc_pool_mutex_key);

	LATCH_ADD(SrvLatches, "recv_sys",
		  SYNC_RECV, recv_sys_mutex_key);

	LATCH_ADD(SrvLatches, "recv_writer",
		  SYNC_LEVEL_VARYING, recv_writer_mutex_key);

	LATCH_ADD(SrvLatches, "rseg",
		  SYNC_RSEG, rseg_mutex_key);

#ifdef UNIV_SYNC_DEBUG
	LATCH_ADD(SrvLatches, "rw_lock_debug",
		  SYNC_NO_ORDER_CHECK, rw_lock_debug_mutex_key);
#endif /* UNIV_SYNC_DEBUG */

	LATCH_ADD(SrvLatches, "rw_lock_list",
		  SYNC_NO_ORDER_CHECK, rw_lock_list_mutex_key);

	LATCH_ADD(SrvLatches, "rw_lock_mutex",
		  SYNC_NO_ORDER_CHECK, rw_lock_mutex_key);

	LATCH_ADD(SrvLatches, "srv_dict_tmpfile",
		  SYNC_DICT_OPERATION, srv_dict_tmpfile_mutex_key);

	LATCH_ADD(SrvLatches, "srv_innodb_monitor",
		  SYNC_NO_ORDER_CHECK, srv_innodb_monitor_mutex_key);

	LATCH_ADD(SrvLatches, "srv_misc_tmpfile",
		  SYNC_ANY_LATCH, srv_misc_tmpfile_mutex_key);

	LATCH_ADD(SrvLatches, "srv_monitor_file",
		  SYNC_NO_ORDER_CHECK, srv_monitor_file_mutex_key);

#ifdef UNIV_SYNC_DEBUG
	LATCH_ADD(SrvLatches, "sync_thread",
		  SYNC_NO_ORDER_CHECK, sync_thread_mutex_key);
#endif /* UNIV_SYNC_DEBUG */

	LATCH_ADD(SrvLatches, "buf_dblwr",
		  SYNC_DOUBLEWRITE, buf_dblwr_mutex_key);

	LATCH_ADD(SrvLatches, "trx_undo",
		  SYNC_TRX_UNDO, trx_undo_mutex_key);

	LATCH_ADD(SrvLatches, "trx",
		  SYNC_TRX, trx_mutex_key);

	LATCH_ADD(SrvLatches, "lock_sys",
		  SYNC_LOCK_SYS, lock_sys_mutex_key);

	LATCH_ADD(SrvLatches, "lock_sys_wait",
		  SYNC_LOCK_WAIT_SYS, lock_sys_wait_mutex_key);

	LATCH_ADD(SrvLatches, "trx_sys",
		  SYNC_TRX_SYS, trx_sys_mutex_key);

	LATCH_ADD(SrvLatches, "srv_sys",
		  SYNC_THREADS, srv_sys_mutex_key);

	LATCH_ADD(SrvLatches, "srv_sys_tasks",
		  SYNC_ANY_LATCH, srv_sys_tasks_mutex_key);

	LATCH_ADD(SrvLatches, "page_zip_stat_per_index",
		  SYNC_ANY_LATCH, page_zip_stat_per_index_mutex_key);

#ifndef HAVE_ATOMIC_BUILTINS
	LATCH_ADD(SrvLatches, "srv_conc",
		  SYNC_NO_ORDER_CHECK, srv_conc_mutex_key);
#endif /* !HAVE_ATOMIC_BUILTINS */

#ifndef HAVE_ATOMIC_BUILTINS_64
	LATCH_ADD(SrvLatches, "monitor",
		  SYNC_ANY_LATCH, monitor_mutex_key);
#endif /* !HAVE_ATOMIC_BUILTINS_64 */

#ifndef PFS_SKIP_EVENT_MUTEX
	LATCH_ADD(SrvLatches, "event_manager",
		  SYNC_NO_ORDER_CHECK, event_manager_mutex_key);
#else
	LATCH_ADD(SrvLatches, "event_manager",
		  SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
#endif /* !PFS_SKIP_EVENT_MUTEX */

	LATCH_ADD(SrvLatches, "event_mutex",
		  SYNC_NO_ORDER_CHECK, event_mutex_key);

	LATCH_ADD(SrvLatches, "sync_array_mutex",
		  SYNC_NO_ORDER_CHECK, sync_array_mutex_key);

	LATCH_ADD(SrvLatches, "ut_list_mutex",
		  SYNC_NO_ORDER_CHECK, ut_list_mutex_key);

	LATCH_ADD(SrvLatches, "thread_mutex",
		  SYNC_NO_ORDER_CHECK, thread_mutex_key);

	LATCH_ADD(SrvLatches, "zip_pad_mutex",
		  SYNC_NO_ORDER_CHECK, zip_pad_mutex_key);

	LATCH_ADD(SrvLatches, "os_file_seek_mutex",
		  SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

#if !defined(HAVE_ATOMIC_BUILTINS) || UNIV_WORD_SIZE < 8
	LATCH_ADD(SrvLatches, "os_file_count_mutex",
		  SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
#endif /* !HAVE_ATOMIC_BUILTINS || UNIV_WORD_SIZE < 8 */

	LATCH_ADD(SrvLatches, "test_mutex",
		  SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

	LATCH_ADD(SrvLatches, "os_aio_mutex",
		  SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

	LATCH_ADD(SrvLatches, "row_drop_list",
		  SYNC_NO_ORDER_CHECK, row_drop_list_mutex_key);

	LATCH_ADD(SrvLatches, "index_online_log",
		  SYNC_INDEX_ONLINE_LOG, index_online_log_key);

	LATCH_ADD(SrvLatches, "work_queue",
		  SYNC_WORK_QUEUE, PFS_NOT_INSTRUMENTED);

	// Add the RW locks
#ifdef UNIV_LOG_ARCHIVE
	LATCH_ADD(SrvLatches, "archive",
		  SYNC_NO_ORDER_CHECK, archive_lock_key);
#endif /* UNIV_LOG_ARCHIVE */

	LATCH_ADD(SrvLatches, "btr_search",
		  SYNC_SEARCH_SYS, btr_search_latch_key);

	LATCH_ADD(SrvLatches, "buf_block_lock",
		  SYNC_LEVEL_VARYING, buf_block_lock_key);

#ifdef UNIV_SYNC_DEBUG
	LATCH_ADD(SrvLatches, "buf_block_debug",
		  SYNC_NO_ORDER_CHECK, buf_block_debug_latch_key);
#endif /* UNIV_SYNC_DEBUG */

	LATCH_ADD(SrvLatches, "dict_operation",
		  SYNC_DICT, dict_operation_lock_key);

	LATCH_ADD(SrvLatches, "checkpoint",
		  SYNC_NO_ORDER_CHECK, checkpoint_lock_key);

	LATCH_ADD(SrvLatches, "fil_space",
		  SYNC_FSP, fil_space_latch_key);

	LATCH_ADD(SrvLatches, "fts_cache",
		  SYNC_FTS_CACHE, fts_cache_rw_lock_key);

	LATCH_ADD(SrvLatches, "fts_cache_init",
		  SYNC_FTS_CACHE_INIT, fts_cache_init_rw_lock_key);

	LATCH_ADD(SrvLatches, "trx_i_s_cache",
		  SYNC_TRX_I_S_RWLOCK, trx_i_s_cache_lock_key);

	LATCH_ADD(SrvLatches, "trx_purge",
		  SYNC_PURGE_LATCH, trx_purge_latch_key);

	LATCH_ADD(SrvLatches, "ibuf_index_tree",
		  SYNC_IBUF_INDEX_TREE, index_tree_rw_lock_key);

	LATCH_ADD(SrvLatches, "index_tree",
		  SYNC_INDEX_TREE, index_tree_rw_lock_key);

	LATCH_ADD(SrvLatches, "dict_table_stats",
		  SYNC_INDEX_TREE, dict_table_stats_latch_key);

	LATCH_ADD(SrvLatches, "hash_table_rw_lock",
		  SYNC_BUF_PAGE_HASH, hash_table_rw_lock_key);
}

/**
Initializes the synchronization data structures. */
UNIV_INTERN
void
sync_check_init()
{
	ut_a(!sync_check_initialised);

	sync_check_initialised = true;

	ut_a(SrvLatches == 0);
	SrvLatches = new(std::nothrow) LatchMap();

	sync_latch_meta_init();

	/* Init the mutex list and create the mutex to protect it. */

	//mutex_create("mutex_list", &mutex_list_mutex);

	/* Init the rw-lock list and create the mutex to protect it. */

	UT_LIST_INIT(rw_lock_list);

	mutex_create("rw_lock_list", &rw_lock_list_mutex);

#ifdef UNIV_SYNC_DEBUG
	mutex_create("rw_lock_debug", &rw_lock_debug_mutex);
	rw_lock_debug_event = os_event_create("rw_lock_debug_event");
	rw_lock_debug_waiters = FALSE;
#endif /* UNIV_SYNC_DEBUG */

	sync_array_init(OS_THREAD_MAX_N);
}

/**
Frees the resources in InnoDB's own synchronization data structures. Use
os_sync_free() after calling this. */
UNIV_INTERN
void
sync_check_close()
{
	ut_a(sync_check_initialised);

	sync_check_initialised = false;

	delete SrvLatches;
	SrvLatches = 0;

#ifdef UNIV_SYNC_DEBUG
	mutex_free(&rw_lock_debug_mutex);
	os_event_destroy(rw_lock_debug_event);
	rw_lock_debug_event = 0;
#endif /* UNIV_SYNC_DEBUG */

	mutex_free(&rw_lock_list_mutex);

	sync_array_close();
}

/**
Get the sync level for a latch name.
@return SYNC_UNKNOWN - if not found. */
UNIV_INTERN
latch_level_t
sync_latch_get_level(
	const char*	name)			/*!< in: Latch name */
{
	LatchMap::iterator it  = SrvLatches->find(name);

	ut_a(it != SrvLatches->end());

	return(it->second.m_level);
}

/**
Get the latch name from a sync level.
@return 0 if not found. */
UNIV_INTERN
const char*
sync_latch_get_name(
	latch_level_t	level)			/*!< in: Latch level */
{
	LatchMap::const_iterator	end = SrvLatches->end();

	// Linear scan should be OK, this should be extremely rare.
	for (LatchMap::const_iterator it = SrvLatches->begin();
	     it != end;
	     ++it) {

		if (it->second.m_level == level) {
			return(it->first.c_str());
		}
	}

	return(0);
}

#ifdef UNIV_PFS_MUTEX
/**
Get the sync level for a latch name.
@return SYNC_UNKNOWN - if not found. */
UNIV_INTERN
mysql_pfs_key_t
sync_latch_get_pfs_key(
	const char*	name)			/*!< Latch name */
{
	LatchMap::iterator it  = SrvLatches->find(name);

	/* Must find th the PFS key, even if it is not instrumented. */
	ut_a(it != SrvLatches->end());

	return(it->second.m_pfs_key);
}
#endif /* UNIV_PFS_MUTEX */

/**
Check if it is OK to acquire the latch. 
@param latch - latch type */
UNIV_INTERN
void
sync_check_lock(const latch_t* latch)
{
	syncCheck.lock(latch);
}

/**
Check if it is OK to acquire the latch. 
@param latch - latch type
@param level - latch order */
UNIV_INTERN
void
sync_check_lock(const latch_t* latch, latch_level_t level)
{
	syncCheck.lock(latch);
}

/**
Check if it is OK to re-acquire the lock. */
UNIV_INTERN
void
sync_check_relock(const latch_t* latch)
{
	syncCheck.relock(latch);
}

/**
Removes a latch from the thread level array if it is found there.
@param latch - to unlock */
UNIV_INTERN
void
sync_check_unlock(const latch_t* latch)
{
	syncCheck.unlock(latch);
}

/**
Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@param level - to find
@return	a matching latch, or NULL if not found */
UNIV_INTERN
const latch_t*
sync_check_find(latch_level_t level)
{
	return(syncCheck.find(level));
}

/**
Iterate over the thread's latches.
@param functor - called for each element. */
UNIV_INTERN
bool
sync_check_iterate(sync_check_functor_t& functor)
{
	return(syncCheck.for_each(functor));
}

/**
Enable sync order checking. */
UNIV_INTERN
void
sync_check_enable()
{
}
