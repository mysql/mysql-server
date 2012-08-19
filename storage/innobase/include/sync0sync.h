/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.
Copyright (c) 2012, Facebook Inc.

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
@file include/sync0sync.h
Mutex, the basic synchronization primitive

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0sync_h
#define sync0sync_h

#include "univ.i"
#include "ut0lst.h"
#include "ut0mem.h"
#include "os0thread.h"
#include "os0sync.h"
#include "sync0types.h"
#include "sync0arr.h"

#if defined UNIV_PFS_MUTEX || defined UNIV_PFS_RWLOCK

/* By default, buffer mutexes and rwlocks will be excluded from
instrumentation due to their large number of instances. */
# define PFS_SKIP_BUFFER_MUTEX_RWLOCK

/* By default, event->mutex will also be excluded from instrumentation */
# define PFS_SKIP_EVENT_MUTEX

#endif /* UNIV_PFS_MUTEX || UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
/* Key defines to register InnoDB mutexes with performance schema */
extern mysql_pfs_key_t	autoinc_mutex_key;
extern mysql_pfs_key_t	buffer_block_mutex_key;
extern mysql_pfs_key_t	buf_pool_mutex_key;
extern mysql_pfs_key_t	buf_pool_zip_mutex_key;
extern mysql_pfs_key_t	cache_last_read_mutex_key;
extern mysql_pfs_key_t	dict_foreign_err_mutex_key;
extern mysql_pfs_key_t	dict_sys_mutex_key;
extern mysql_pfs_key_t	file_format_max_mutex_key;
extern mysql_pfs_key_t	fil_system_mutex_key;
extern mysql_pfs_key_t	flush_list_mutex_key;
extern mysql_pfs_key_t	fts_bg_threads_mutex_key;
extern mysql_pfs_key_t	fts_delete_mutex_key;
extern mysql_pfs_key_t	fts_optimize_mutex_key;
extern mysql_pfs_key_t	fts_doc_id_mutex_key;
extern mysql_pfs_key_t	hash_table_mutex_key;
extern mysql_pfs_key_t	ibuf_bitmap_mutex_key;
extern mysql_pfs_key_t	ibuf_mutex_key;
extern mysql_pfs_key_t	ibuf_pessimistic_insert_mutex_key;
extern mysql_pfs_key_t	log_sys_mutex_key;
extern mysql_pfs_key_t	log_flush_order_mutex_key;
# ifndef HAVE_ATOMIC_BUILTINS
extern mysql_pfs_key_t	server_mutex_key;
# endif /* !HAVE_ATOMIC_BUILTINS */
# ifdef UNIV_MEM_DEBUG
extern mysql_pfs_key_t	mem_hash_mutex_key;
# endif /* UNIV_MEM_DEBUG */
extern mysql_pfs_key_t	mem_pool_mutex_key;
extern mysql_pfs_key_t	mutex_list_mutex_key;
extern mysql_pfs_key_t	purge_sys_bh_mutex_key;
extern mysql_pfs_key_t	recv_sys_mutex_key;
extern mysql_pfs_key_t	rseg_mutex_key;
# ifdef UNIV_SYNC_DEBUG
extern mysql_pfs_key_t	rw_lock_debug_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
extern mysql_pfs_key_t	rw_lock_list_mutex_key;
extern mysql_pfs_key_t	rw_lock_mutex_key;
extern mysql_pfs_key_t	srv_dict_tmpfile_mutex_key;
extern mysql_pfs_key_t	srv_innodb_monitor_mutex_key;
extern mysql_pfs_key_t	srv_misc_tmpfile_mutex_key;
extern mysql_pfs_key_t	srv_threads_mutex_key;
extern mysql_pfs_key_t	srv_monitor_file_mutex_key;
# ifdef UNIV_SYNC_DEBUG
extern mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
extern mysql_pfs_key_t	buf_dblwr_mutex_key;
extern mysql_pfs_key_t	trx_undo_mutex_key;
extern mysql_pfs_key_t	trx_mutex_key;
extern mysql_pfs_key_t	lock_sys_mutex_key;
extern mysql_pfs_key_t	lock_sys_wait_mutex_key;
extern mysql_pfs_key_t	trx_sys_mutex_key;
extern mysql_pfs_key_t	srv_sys_mutex_key;
extern mysql_pfs_key_t	srv_sys_tasks_mutex_key;
#ifndef HAVE_ATOMIC_BUILTINS
extern mysql_pfs_key_t	srv_conc_mutex_key;
#endif /* !HAVE_ATOMIC_BUILTINS */
#ifndef HAVE_ATOMIC_BUILTINS_64
extern mysql_pfs_key_t	monitor_mutex_key;
#endif /* !HAVE_ATOMIC_BUILTINS_64 */
extern mysql_pfs_key_t	event_os_mutex_key;
extern mysql_pfs_key_t	ut_list_mutex_key;
extern mysql_pfs_key_t	os_mutex_key;
extern mysql_pfs_key_t  zip_pad_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/******************************************************************//**
Initializes the synchronization data structures. */
UNIV_INTERN
void
sync_init();
/*=======*/

/******************************************************************//**
Frees the resources in synchronization data structures. */
UNIV_INTERN
void
sync_close();
/*========*/

/*******************************************************************//**
Prints wait info of the sync system. */
UNIV_INTERN
void
sync_print_wait_info(
/*=================*/
	FILE*	file);			/*!< in: file where to print */

/*******************************************************************//**
Prints info of the sync system. */
UNIV_INTERN
void
sync_print(
/*=======*/
	FILE*	file);			/*!< in: file where to print */

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread. */
UNIV_INTERN
void
sync_thread_add_level(
/*==================*/
	void*	latch,			/*!< in: pointer to a mutex or
					an rw-lock */
	ulint	level,			/*!< in: level in the latching
					order; if SYNC_LEVEL_VARYING, nothing
					is done */
	ibool	relock)			/*!< in: TRUE if re-entering
					an x-lock */
	__attribute__((nonnull));

/******************************************************************//**
Removes a latch from the thread level array if it is found there.
@return TRUE if found in the array; it is no error if the latch is
not found, as we presently are not able to determine the level for
every latch reservation the program does */
UNIV_INTERN
ibool
sync_thread_reset_level(
/*====================*/
	void*	latch);			/*!< in: pointer to a mutex or
					an rw-lock */

/******************************************************************//**
Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@return	a matching latch, or NULL if not found */
UNIV_INTERN
void*
sync_thread_levels_contains(
/*========================*/
	ulint	level);			/*!< in: latching order level
					(SYNC_DICT, ...)*/

/******************************************************************//**
Checks that the level array for the current thread is empty.
@return	a latch, or NULL if empty except the exceptions specified below */
UNIV_INTERN
void*
sync_thread_levels_nonempty_gen(
/*============================*/
	ibool	dict_mutex_allowed)	/*!< in: TRUE if dictionary mutex is
					allowed to be owned by the thread */
	__attribute__((warn_unused_result));

/******************************************************************//**
Checks if the level array for the current thread is empty,
except for data dictionary latches. */
#define sync_thread_levels_empty_except_dict()		\
	(!sync_thread_levels_nonempty_gen(TRUE))

/******************************************************************//**
Checks if the level array for the current thread is empty,
except for the btr_search_latch.
@return	a latch, or NULL if empty except the exceptions specified below */
UNIV_INTERN
void*
sync_thread_levels_nonempty_trx(
/*============================*/
	ibool	has_search_latch)	/*!< in: TRUE if and only if the thread
					is supposed to hold btr_search_latch */
	__attribute__((warn_unused_result));

#endif /* UNIV_SYNC_DEBUG */

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

/* Latching order levels. If you modify these, you have to also update
sync_thread_add_level(). */

/* User transaction locks are higher than any of the latch levels below:
no latches are allowed when a thread goes to wait for a normal table
or row lock! */
#define SYNC_USER_TRX_LOCK	9999
#define SYNC_NO_ORDER_CHECK	3000	/* this can be used to suppress
					latching order checking */
#define	SYNC_LEVEL_VARYING	2000	/* Level is varying. Only used with
					buffer pool page locks, which do not
					have a fixed level, but instead have
					their level set after the page is
					locked; see e.g.
					ibuf_bitmap_get_map_page(). */
#define SYNC_TRX_I_S_RWLOCK	1910	/* Used for
					trx_i_s_cache_t::rw_lock */
#define SYNC_TRX_I_S_LAST_READ	1900	/* Used for
					trx_i_s_cache_t::last_read_mutex */
#define SYNC_FILE_FORMAT_TAG	1200	/* Used to serialize access to the
					file format tag */
#define	SYNC_DICT_OPERATION	1010	/* table create, drop, etc. reserve
					this in X-mode; implicit or backround
					operations purge, rollback, foreign
					key checks reserve this in S-mode */
#define SYNC_FTS_CACHE		1005	/* FTS cache rwlock */
#define SYNC_DICT		1000
#define SYNC_DICT_AUTOINC_MUTEX	999
#define SYNC_STATS_AUTO_RECALC	997
#define SYNC_DICT_HEADER	995
#define SYNC_IBUF_HEADER	914
#define SYNC_IBUF_PESS_INSERT_MUTEX 912
/*-------------------------------*/
#define	SYNC_INDEX_TREE		900
#define SYNC_TREE_NODE_NEW	892
#define SYNC_TREE_NODE_FROM_HASH 891
#define SYNC_TREE_NODE		890
#define	SYNC_PURGE_LATCH	800
#define	SYNC_TRX_UNDO		700
#define SYNC_RSEG		600
#define SYNC_RSEG_HEADER_NEW	591
#define SYNC_RSEG_HEADER	590
#define SYNC_TRX_UNDO_PAGE	570
#define SYNC_EXTERN_STORAGE	500
#define	SYNC_FSP		400
#define	SYNC_FSP_PAGE		395
/*------------------------------------- Change buffer headers */
#define SYNC_IBUF_MUTEX		370	/* ibuf_mutex */
/*------------------------------------- Change buffer tree */
#define SYNC_IBUF_INDEX_TREE	360
#define SYNC_IBUF_TREE_NODE_NEW	359
#define SYNC_IBUF_TREE_NODE	358
#define	SYNC_IBUF_BITMAP_MUTEX	351
#define	SYNC_IBUF_BITMAP	350
/*------------------------------------- Change log for online create index */
#define SYNC_INDEX_ONLINE_LOG	340
/*------------------------------------- MySQL query cache mutex */
/*------------------------------------- MySQL binlog mutex */
/*-------------------------------*/
#define SYNC_LOCK_WAIT_SYS	300
#define SYNC_LOCK_SYS		299
#define SYNC_TRX_SYS		298
#define SYNC_TRX		297
#define SYNC_THREADS		295
#define SYNC_REC_LOCK		294
#define SYNC_TRX_SYS_HEADER	290
#define	SYNC_PURGE_QUEUE	200
#define SYNC_LOG		170
#define SYNC_LOG_FLUSH_ORDER	147
#define SYNC_RECV		168
#define SYNC_FTS_CACHE_INIT	166	/* Used for FTS cache initialization */
#define SYNC_FTS_BG_THREADS	165
#define SYNC_FTS_OPTIMIZE       164     // FIXME: is this correct number, test
#define	SYNC_WORK_QUEUE		162
#define	SYNC_SEARCH_SYS		160	/* NOTE that if we have a memory
					heap that can be extended to the
					buffer pool, its logical level is
					SYNC_SEARCH_SYS, as memory allocation
					can call routines there! Otherwise
					the level is SYNC_MEM_HASH. */
#define	SYNC_BUF_POOL		150	/* Buffer pool mutex */
#define	SYNC_BUF_PAGE_HASH	149	/* buf_pool->page_hash rw_lock */
#define	SYNC_BUF_BLOCK		146	/* Block mutex */
#define	SYNC_BUF_FLUSH_LIST	145	/* Buffer flush list mutex */
#define SYNC_DOUBLEWRITE	140
#define	SYNC_ANY_LATCH		135
#define	SYNC_MEM_HASH		131
#define	SYNC_MEM_POOL		130

/* Codes used to designate lock operations */
#define RW_LOCK_NOT_LOCKED	350
#define RW_LOCK_EX		351
#define RW_LOCK_EXCLUSIVE	351
#define RW_LOCK_SHARED		352
#define RW_LOCK_WAIT_EX		353
#define SYNC_MUTEX		354

/** Constant determining how long spin wait is continued before suspending
the thread. A value 600 rounds on a 1995 100 MHz Pentium seems to correspond
to 20 microseconds. */

#define	SYNC_SPIN_ROUNDS	srv_n_spin_wait_rounds

/** The number of mutex_exit calls. Intended for performance monitoring. */
extern	ib_int64_t	mutex_exit_count;

#ifdef UNIV_SYNC_DEBUG
/** Latching order checks start when this is set true */
extern bool		sync_order_checks_on;
#endif /* UNIV_SYNC_DEBUG */

/** This variable is set to true when sync_init is called */
extern bool		sync_initialized;

/** Global list of database mutexes (not OS mutexes) created. */
typedef UT_LIST_BASE_NODE_T(ib_mutex_t)  ut_list_base_node_t;

/** Global list of database mutexes (not OS mutexes) created. */
extern ut_list_base_node_t  mutex_list;

/** Mutex protecting the mutex_list variable */
extern ib_mutex_t mutex_list_mutex;

#ifndef HAVE_ATOMIC_BUILTINS
/**********************************************************//**
Function that uses a mutex to decrement a variable atomically */
UNIV_INLINE
void
os_atomic_dec_ulint_func(
/*=====================*/
	ib_mutex_t*		mutex,		/*!< in: mutex guarding the
						decrement */
	volatile ulint*		var,		/*!< in/out: variable to
						decrement */
	ulint			delta);		/*!< in: delta to decrement */

/**********************************************************//**
Function that uses a mutex to increment a variable atomically */
UNIV_INLINE
void
os_atomic_inc_ulint_func(
/*=====================*/
	ib_mutex_t*		mutex,		/*!< in: mutex guarding the
						increment */
	volatile ulint*		var,		/*!< in/out: variable to
						increment */
	ulint			delta);		/*!< in: delta to increment */
#endif /* !HAVE_ATOMIC_BUILTINS */

#endif /* !sync0sync_h */
