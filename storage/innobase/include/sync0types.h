/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/sync0types.h
Global types for sync

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0types_h
#define sync0types_h

#ifdef HAVE_WINDOWS_ATOMICS
typedef LONG	lock_word_t;	/*!< On Windows, InterlockedExchange operates
				on LONG variable */
#elif defined(HAVE_IB_LINUX_FUTEX)
typedef int	lock_word_t;
#else
typedef ulint	lock_word_t;
#endif /* HAVE_WINDOWS_ATOMICS */

#ifdef __WIN__
/** Native mutex */
typedef CRITICAL_SECTION	sys_mutex_t;
#else
/** Native mutex */
typedef pthread_mutex_t		sys_mutex_t;
#endif /* __WIN__ */

/** The new (C++11) syntax allows the following and we should use it when it
is available on platforms that we support.

	enum class mutex_state_t : lock_word_t { ... };
*/

/** Mutex states. */
enum mute_state_t {
	/** Mutex is free */
	MUTEX_STATE_UNLOCKED = 0,

	/** Mutex is acquired by some thread. */
	MUTEX_STATE_LOCKED = 1,

	/** Mutex is contended and there are threads waiting on the lock. */
	MUTEX_STATE_WAITERS = 2
};

/*
		LATCHING ORDER WITHIN THE DATABASE
		==================================

The mutex or latch in the central memory object, for instance, a rollback
segment object, must be acquired before acquiring the latch or latches to
the corresponding file data structure. In the latching order below, these
file page object latches are placed immediately below the corresponding
central memory object latch or mutex.

Synchronization object			Notes
----------------------			-----

Dictionary mutex			If we have a pointer to a dictionary
|					object, e.g., a table, it can be
|					accessed without reserving the
|					dictionary mutex. We must have a
|					reservation, a memoryfix, to the
|					appropriate table object in this case,
|					and the table must be explicitly
|					released later.
V
Dictionary header
|
V
Secondary index tree latch		The tree latch protects also all
|					the B-tree non-leaf pages. These
V					can be read with the page only
Secondary index non-leaf		bufferfixed to save CPU time,
|					no s-latch is needed on the page.
|					Modification of a page requires an
|					x-latch on the page, however. If a
|					thread owns an x-latch to the tree,
|					it is allowed to latch non-leaf pages
|					even after it has acquired the fsp
|					latch.
V
Secondary index leaf			The latch on the secondary index leaf
|					can be kept while accessing the
|					clustered index, to save CPU time.
V
Clustered index tree latch		To increase concurrency, the tree
|					latch is usually released when the
|					leaf page latch has been acquired.
V
Clustered index non-leaf
|
V
Clustered index leaf
|
V
Transaction system header
|
V
Transaction undo mutex			The undo log entry must be written
|					before any index page is modified.
|					Transaction undo mutex is for the undo
|					logs the analogue of the tree latch
|					for a B-tree. If a thread has the
|					trx undo mutex reserved, it is allowed
|					to latch the undo log pages in any
|					order, and also after it has acquired
|					the fsp latch.
V
Rollback segment mutex			The rollback segment mutex must be
|					reserved, if, e.g., a new page must
|					be added to an undo log. The rollback
|					segment and the undo logs in its
|					history list can be seen as an
|					analogue of a B-tree, and the latches
|					reserved similarly, using a version of
|					lock-coupling. If an undo log must be
|					extended by a page when inserting an
|					undo log record, this corresponds to
|					a pessimistic insert in a B-tree.
V
Rollback segment header
|
V
Purge system latch
|
V
Undo log pages				If a thread owns the trx undo mutex,
|					or for a log in the history list, the
|					rseg mutex, it is allowed to latch
|					undo log pages in any order, and even
|					after it has acquired the fsp latch.
|					If a thread does not have the
|					appropriate mutex, it is allowed to
|					latch only a single undo log page in
|					a mini-transaction.
V
File space management latch		If a mini-transaction must allocate
|					several file pages, it can do that,
|					because it keeps the x-latch to the
|					file space management in its memo.
V
File system pages
|
V
lock_sys_wait_mutex			Mutex protecting lock timeout data
|
V
lock_sys_mutex				Mutex protecting lock_sys_t
|
V
trx_sys->mutex				Mutex protecting trx_sys_t
|
V
Threads mutex				Background thread scheduling mutex
|
V
query_thr_mutex				Mutex protecting query threads
|
V
trx_mutex				Mutex protecting trx_t fields
|
V
Search system mutex
|
V
Buffer pool mutex
|
V
Log mutex
|
Any other latch
|
V
Memory pool mutex */

/** Latching order levels. If you modify these, you have to also update
sync_thread_add_level(). */

enum latch_level_t {
	SYNC_UNKNOWN = 0,

	SYNC_MUTEX = 1,

	SYNC_POOL,
	SYNC_POOL_MANAGER,

	RW_LOCK_X_WAIT,
	RW_LOCK_S,
	RW_LOCK_X,
	RW_LOCK_NOT_LOCKED,

	SYNC_MEM_POOL,
	SYNC_MEM_HASH,
	SYNC_ANY_LATCH,
	SYNC_DOUBLEWRITE,
	SYNC_BUF_FLUSH_LIST,
	SYNC_BUF_BLOCK,
	SYNC_BUF_PAGE_HASH,
	SYNC_BUF_POOL,

	SYNC_SEARCH_SYS,
	SYNC_WORK_QUEUE,
	SYNC_FTS_OPTIMIZE,
	SYNC_FTS_BG_THREADS,
	SYNC_FTS_CACHE_INIT,
	SYNC_RECV,
	SYNC_LOG_FLUSH_ORDER,
	SYNC_LOG,
	SYNC_PURGE_QUEUE,
	SYNC_TRX_SYS_HEADER,
	SYNC_REC_LOCK,
	SYNC_THREADS,
	SYNC_TRX,
	SYNC_TRX_SYS,
	SYNC_LOCK_SYS,
	SYNC_LOCK_WAIT_SYS,

	SYNC_INDEX_ONLINE_LOG,
	
	SYNC_IBUF_BITMAP,
	SYNC_IBUF_BITMAP_MUTEX,
	SYNC_IBUF_TREE_NODE,
	SYNC_IBUF_TREE_NODE_NEW,
	SYNC_IBUF_INDEX_TREE,
	
	SYNC_IBUF_MUTEX,
	
	SYNC_FSP_PAGE,
	SYNC_FSP,
	SYNC_EXTERN_STORAGE,
	SYNC_TRX_UNDO_PAGE,
	SYNC_RSEG_HEADER,
	SYNC_RSEG_HEADER_NEW,
	SYNC_RSEG,
	SYNC_TRX_UNDO,
	SYNC_PURGE_LATCH,
	SYNC_TREE_NODE,
	SYNC_TREE_NODE_FROM_HASH,
	SYNC_TREE_NODE_NEW,
	SYNC_INDEX_TREE,
	
	SYNC_IBUF_PESS_INSERT_MUTEX,
	SYNC_IBUF_HEADER,
	SYNC_DICT_HEADER,
	SYNC_STATS_AUTO_RECALC,
	SYNC_DICT_AUTOINC_MUTEX,
	SYNC_DICT,
	SYNC_FTS_CACHE,
	
	SYNC_DICT_OPERATION,
	
	SYNC_FILE_FORMAT_TAG,
	
	SYNC_TRX_I_S_LAST_READ,
	
	SYNC_TRX_I_S_RWLOCK,

	/** Level is varying. Only used with buffer pool page locks, which
	do not have a fixed level, but instead have their level set after
	the page is locked; see e.g.  ibuf_bitmap_get_map_page(). */

	SYNC_LEVEL_VARYING,

	/** This can be used to suppress order checking. */
	SYNC_NO_ORDER_CHECK,


	/** User transaction locks are higher than any of the latch levels
	below: no latches are allowed when a thread goes to wait for a
	normal table or row lock! */

	SYNC_USER_TRX_LOCK,
};

/* Forward declraration. */
//struct Writer;

/** All (ordered) latches, used in debugging, must derive from this class. */
struct latch_t {
	latch_t(latch_level_t level = SYNC_UNKNOWN)
		:
		m_level(level),
       		m_rw_lock(false) { }

	virtual ~latch_t() { }

	bool is_rw_lock() const
	{
		return(m_rw_lock);
	}

	virtual void print(FILE* stream) const = 0;

	/** The order or level of the latch */
	latch_level_t	m_level;

	/* true if it is a rw-lock */
	bool		m_rw_lock;
};

/** Subclass this to iterate over a thread's latches. */
struct sync_check_functor_t {
	virtual ~sync_check_functor_t() { }
	virtual bool operator()(const latch_t&) = 0;
	virtual bool result() const = 0;
};

/** Functor to check whether the calling thread owns the btr search mutex. */
struct btrsea_sync_check : public sync_check_functor_t {

	btrsea_sync_check(bool has_search_latch = true)
		:
		m_result(false),
		m_has_search_latch(has_search_latch) { }

	virtual ~btrsea_sync_check() { }

	virtual bool operator()(const latch_t& latch)
	{
		// FIXME: This condition doesn't look right
		if (!m_has_search_latch || latch.m_level != SYNC_SEARCH_SYS) {
			return(m_result = true);
		}

		return(false);
	}

	virtual bool result() const
	{
		return(m_result);
	}

	bool		m_result;
	bool		m_has_search_latch;
};

/** Functor to check for dictionay latching constraints. */
struct dict_sync_check : public sync_check_functor_t {

	dict_sync_check(bool dict_mutex_allowed = true)
		:
		m_result(false),
		m_dict_mutex_allowed(dict_mutex_allowed) { }

	virtual ~dict_sync_check() { }

	virtual bool operator()(const latch_t& latch)
	{
		if ((!m_dict_mutex_allowed
		     || (latch.m_level != SYNC_DICT
			 && latch.m_level != SYNC_DICT_OPERATION
			 && latch.m_level != SYNC_FTS_CACHE))) {

			return(m_result = true);
		}

		return(false);
	}

	virtual bool result() const
	{
		return(m_result);
	}

	bool		m_result;
	bool		m_dict_mutex_allowed;
};

#endif /* sync0types.h */
