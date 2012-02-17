/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.

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
#include "sync0types.h"
#include "ut0lst.h"
#include "ut0mem.h"
#include "os0thread.h"
#include "os0sync.h"
#include "sync0arr.h"

#if  defined(UNIV_DEBUG) && !defined(UNIV_HOTBACKUP)
extern my_bool	timed_mutexes;
#endif /* UNIV_DEBUG && !UNIV_HOTBACKUP */

#ifdef HAVE_WINDOWS_ATOMICS
typedef LONG lock_word_t;	/*!< On Windows, InterlockedExchange operates
				on LONG variable */
#else
typedef byte lock_word_t;
#endif

#if defined UNIV_PFS_MUTEX || defined UNIV_PFS_RWLOCK
/* There are mutexes/rwlocks that we want to exclude from
instrumentation even if their corresponding performance schema
define is set. And this PFS_NOT_INSTRUMENTED is used
as the key value to dentify those objects that would
be excluded from instrumentation. */
# define PFS_NOT_INSTRUMENTED		ULINT32_UNDEFINED

# define PFS_IS_INSTRUMENTED(key)	((key) != PFS_NOT_INSTRUMENTED)

/* By default, buffer mutexes and rwlocks will be excluded from
instrumentation due to their large number of instances. */
# define PFS_SKIP_BUFFER_MUTEX_RWLOCK

#endif /* UNIV_PFS_MUTEX || UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
/* Key defines to register InnoDB mutexes with performance schema */
extern mysql_pfs_key_t	autoinc_mutex_key;
extern mysql_pfs_key_t	btr_search_enabled_mutex_key;
extern mysql_pfs_key_t	buffer_block_mutex_key;
extern mysql_pfs_key_t	buf_pool_mutex_key;
extern mysql_pfs_key_t	buf_pool_zip_mutex_key;
extern mysql_pfs_key_t	cache_last_read_mutex_key;
extern mysql_pfs_key_t	dict_foreign_err_mutex_key;
extern mysql_pfs_key_t	dict_sys_mutex_key;
extern mysql_pfs_key_t	file_format_max_mutex_key;
extern mysql_pfs_key_t	fil_system_mutex_key;
extern mysql_pfs_key_t	flush_list_mutex_key;
extern mysql_pfs_key_t	hash_table_mutex_key;
extern mysql_pfs_key_t	ibuf_bitmap_mutex_key;
extern mysql_pfs_key_t	ibuf_mutex_key;
extern mysql_pfs_key_t	ibuf_pessimistic_insert_mutex_key;
extern mysql_pfs_key_t	log_sys_mutex_key;
extern mysql_pfs_key_t	log_flush_order_mutex_key;
extern mysql_pfs_key_t	kernel_mutex_key;
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
extern mysql_pfs_key_t	srv_monitor_file_mutex_key;
extern mysql_pfs_key_t	syn_arr_mutex_key;
# ifdef UNIV_SYNC_DEBUG
extern mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
extern mysql_pfs_key_t	trx_doublewrite_mutex_key;
extern mysql_pfs_key_t	trx_undo_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/******************************************************************//**
Initializes the synchronization data structures. */
UNIV_INTERN
void
sync_init(void);
/*===========*/
/******************************************************************//**
Frees the resources in synchronization data structures. */
UNIV_INTERN
void
sync_close(void);
/*===========*/

#undef mutex_free			/* Fix for MacOS X */

#ifdef UNIV_PFS_MUTEX
/**********************************************************************
Following mutex APIs would be performance schema instrumented
if "UNIV_PFS_MUTEX" is defined:

mutex_create
mutex_enter
mutex_exit
mutex_enter_nowait
mutex_free

These mutex APIs will point to corresponding wrapper functions that contain
the performance schema instrumentation if "UNIV_PFS_MUTEX" is defined.
The instrumented wrapper functions have the prefix of "innodb_".

NOTE! The following macro should be used in mutex operation, not the
corresponding function. */

/******************************************************************//**
Creates, or rather, initializes a mutex object to a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */
# ifdef UNIV_DEBUG
#  ifdef UNIV_SYNC_DEBUG
#   define mutex_create(K, M, level)				\
	pfs_mutex_create_func((K), (M), #M, (level), __FILE__, __LINE__)
#  else
#   define mutex_create(K, M, level)				\
	pfs_mutex_create_func((K), (M), #M, __FILE__, __LINE__)
#  endif/* UNIV_SYNC_DEBUG */
# else
#  define mutex_create(K, M, level)				\
	pfs_mutex_create_func((K), (M), __FILE__, __LINE__)
# endif	/* UNIV_DEBUG */

# define mutex_enter(M)						\
	pfs_mutex_enter_func((M), __FILE__, __LINE__)

# define mutex_enter_nowait(M)					\
	pfs_mutex_enter_nowait_func((M), __FILE__, __LINE__)

# define mutex_exit(M)	pfs_mutex_exit_func(M)

# define mutex_free(M)	pfs_mutex_free_func(M)

#else	/* UNIV_PFS_MUTEX */

/* If "UNIV_PFS_MUTEX" is not defined, the mutex APIs point to
original non-instrumented functions */
# ifdef UNIV_DEBUG
#  ifdef UNIV_SYNC_DEBUG
#   define mutex_create(K, M, level)			\
	mutex_create_func((M), #M, (level), __FILE__, __LINE__)
#  else /* UNIV_SYNC_DEBUG */
#   define mutex_create(K, M, level)				\
	mutex_create_func((M), #M, __FILE__, __LINE__)
#  endif /* UNIV_SYNC_DEBUG */
# else /* UNIV_DEBUG */
#  define mutex_create(K, M, level)				\
	mutex_create_func((M), __FILE__, __LINE__)
# endif	/* UNIV_DEBUG */

# define mutex_enter(M)	mutex_enter_func((M), __FILE__, __LINE__)

# define mutex_enter_nowait(M)	\
	mutex_enter_nowait_func((M), __FILE__, __LINE__)

# define mutex_exit(M)	mutex_exit_func(M)

# define mutex_free(M)	mutex_free_func(M)

#endif	/* UNIV_PFS_MUTEX */

/******************************************************************//**
Creates, or rather, initializes a mutex object in a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */
UNIV_INTERN
void
mutex_create_func(
/*==============*/
	mutex_t*	mutex,		/*!< in: pointer to memory */
#ifdef UNIV_DEBUG
	const char*	cmutex_name,	/*!< in: mutex name */
# ifdef UNIV_SYNC_DEBUG
	ulint		level,		/*!< in: level */
# endif /* UNIV_SYNC_DEBUG */
#endif /* UNIV_DEBUG */
	const char*	cfile_name,	/*!< in: file name where created */
	ulint		cline);		/*!< in: file line where created */

/******************************************************************//**
NOTE! Use the corresponding macro mutex_free(), not directly this function!
Calling this function is obligatory only if the memory buffer containing
the mutex is freed. Removes a mutex object from the mutex list. The mutex
is checked to be in the reset state. */
UNIV_INTERN
void
mutex_free_func(
/*============*/
	mutex_t*	mutex);	/*!< in: mutex */
/**************************************************************//**
NOTE! The following macro should be used in mutex locking, not the
corresponding function. */

/* NOTE! currently same as mutex_enter! */

#define mutex_enter_fast(M)	mutex_enter_func((M), __FILE__, __LINE__)
/******************************************************************//**
NOTE! Use the corresponding macro in the header file, not this function
directly. Locks a mutex for the current thread. If the mutex is reserved
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS) waiting
for the mutex before suspending the thread. */
UNIV_INLINE
void
mutex_enter_func(
/*=============*/
	mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where locked */
	ulint		line);		/*!< in: line where locked */
/********************************************************************//**
NOTE! Use the corresponding macro in the header file, not this function
directly. Tries to lock the mutex for the current thread. If the lock is not
acquired immediately, returns with return value 1.
@return	0 if succeed, 1 if not */
UNIV_INTERN
ulint
mutex_enter_nowait_func(
/*====================*/
	mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where mutex
					requested */
	ulint		line);		/*!< in: line where requested */
/******************************************************************//**
NOTE! Use the corresponding macro mutex_exit(), not directly this function!
Unlocks a mutex owned by the current thread. */
UNIV_INLINE
void
mutex_exit_func(
/*============*/
	mutex_t*	mutex);	/*!< in: pointer to mutex */


#ifdef UNIV_PFS_MUTEX
/******************************************************************//**
NOTE! Please use the corresponding macro mutex_create(), not directly
this function!
A wrapper function for mutex_create_func(), registers the mutex
with peformance schema if "UNIV_PFS_MUTEX" is defined when
creating the mutex */
UNIV_INLINE
void
pfs_mutex_create_func(
/*==================*/
	PSI_mutex_key	key,		/*!< in: Performance Schema key */
	mutex_t*	mutex,		/*!< in: pointer to memory */
# ifdef UNIV_DEBUG
	const char*	cmutex_name,	/*!< in: mutex name */
#  ifdef UNIV_SYNC_DEBUG
	ulint		level,		/*!< in: level */
#  endif /* UNIV_SYNC_DEBUG */
# endif /* UNIV_DEBUG */
	const char*	cfile_name,	/*!< in: file name where created */
	ulint		cline);		/*!< in: file line where created */
/******************************************************************//**
NOTE! Please use the corresponding macro mutex_enter(), not directly
this function!
This is a performance schema instrumented wrapper function for
mutex_enter_func(). */
UNIV_INLINE
void
pfs_mutex_enter_func(
/*=================*/
	mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where locked */
	ulint		line);		/*!< in: line where locked */
/********************************************************************//**
NOTE! Please use the corresponding macro mutex_enter_nowait(), not directly
this function!
This is a performance schema instrumented wrapper function for
mutex_enter_nowait_func.
@return	0 if succeed, 1 if not */
UNIV_INLINE
ulint
pfs_mutex_enter_nowait_func(
/*========================*/
	mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where mutex
					requested */
	ulint		line);		/*!< in: line where requested */
/******************************************************************//**
NOTE! Please use the corresponding macro mutex_exit(), not directly
this function!
A wrap function of mutex_exit_func() with peformance schema instrumentation.
Unlocks a mutex owned by the current thread. */
UNIV_INLINE
void
pfs_mutex_exit_func(
/*================*/
	mutex_t*	mutex);	/*!< in: pointer to mutex */

/******************************************************************//**
NOTE! Please use the corresponding macro mutex_free(), not directly
this function!
Wrapper function for mutex_free_func(). Also destroys the performance
schema probes when freeing the mutex */
UNIV_INLINE
void
pfs_mutex_free_func(
/*================*/
	mutex_t*	mutex);	/*!< in: mutex */

#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Returns TRUE if no mutex or rw-lock is currently locked.
Works only in the debug version.
@return	TRUE if no mutexes and rw-locks reserved */
UNIV_INTERN
ibool
sync_all_freed(void);
/*================*/
#endif /* UNIV_SYNC_DEBUG */
/*#####################################################################
FUNCTION PROTOTYPES FOR DEBUGGING */
/*******************************************************************//**
Prints wait info of the sync system. */
UNIV_INTERN
void
sync_print_wait_info(
/*=================*/
	FILE*	file);		/*!< in: file where to print */
/*******************************************************************//**
Prints info of the sync system. */
UNIV_INTERN
void
sync_print(
/*=======*/
	FILE*	file);		/*!< in: file where to print */
#ifdef UNIV_DEBUG
/******************************************************************//**
Checks that the mutex has been initialized.
@return	TRUE */
UNIV_INTERN
ibool
mutex_validate(
/*===========*/
	const mutex_t*	mutex);	/*!< in: mutex */
/******************************************************************//**
Checks that the current thread owns the mutex. Works only
in the debug version.
@return	TRUE if owns */
UNIV_INTERN
ibool
mutex_own(
/*======*/
	const mutex_t*	mutex)	/*!< in: mutex */
	__attribute__((warn_unused_result));
#endif /* UNIV_DEBUG */
#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread. */
UNIV_INTERN
void
sync_thread_add_level(
/*==================*/
	void*	latch,	/*!< in: pointer to a mutex or an rw-lock */
	ulint	level,	/*!< in: level in the latching order; if
			SYNC_LEVEL_VARYING, nothing is done */
	ibool	relock)	/*!< in: TRUE if re-entering an x-lock */
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
	void*	latch);	/*!< in: pointer to a mutex or an rw-lock */
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
	ibool	has_search_latch)
				/*!< in: TRUE if and only if the thread
				is supposed to hold btr_search_latch */
	__attribute__((warn_unused_result));

/******************************************************************//**
Gets the debug information for a reserved mutex. */
UNIV_INTERN
void
mutex_get_debug_info(
/*=================*/
	mutex_t*	mutex,		/*!< in: mutex */
	const char**	file_name,	/*!< out: file where requested */
	ulint*		line,		/*!< out: line where requested */
	os_thread_id_t* thread_id);	/*!< out: id of the thread which owns
					the mutex */
/******************************************************************//**
Counts currently reserved mutexes. Works only in the debug version.
@return	number of reserved mutexes */
UNIV_INTERN
ulint
mutex_n_reserved(void);
/*==================*/
#endif /* UNIV_SYNC_DEBUG */
/******************************************************************//**
NOT to be used outside this module except in debugging! Gets the value
of the lock word. */
UNIV_INLINE
lock_word_t
mutex_get_lock_word(
/*================*/
	const mutex_t*	mutex);	/*!< in: mutex */
#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
NOT to be used outside this module except in debugging! Gets the waiters
field in a mutex.
@return	value to set */
UNIV_INLINE
ulint
mutex_get_waiters(
/*==============*/
	const mutex_t*	mutex);	/*!< in: mutex */
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
Kernel mutex				If a kernel operation needs a file
|					page allocation, it must reserve the
|					fsp x-latch before acquiring the kernel
|					mutex.
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

/* Latching order levels */

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
#define	SYNC_DICT_OPERATION	1001	/* table create, drop, etc. reserve
					this in X-mode; implicit or backround
					operations purge, rollback, foreign
					key checks reserve this in S-mode */
#define SYNC_DICT		1000
#define SYNC_DICT_AUTOINC_MUTEX	999
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
/*------------------------------------- Insert buffer headers */
#define SYNC_IBUF_MUTEX		370	/* ibuf_mutex */
/*------------------------------------- Insert buffer tree */
#define SYNC_IBUF_INDEX_TREE	360
#define SYNC_IBUF_TREE_NODE_NEW	359
#define SYNC_IBUF_TREE_NODE	358
#define	SYNC_IBUF_BITMAP_MUTEX	351
#define	SYNC_IBUF_BITMAP	350
/*------------------------------------- MySQL query cache mutex */
/*------------------------------------- MySQL binlog mutex */
/*-------------------------------*/
#define	SYNC_KERNEL		300
#define SYNC_REC_LOCK		299
#define	SYNC_TRX_LOCK_HEAP	298
#define SYNC_TRX_SYS_HEADER	290
#define	SYNC_PURGE_QUEUE	200
#define SYNC_LOG		170
#define SYNC_LOG_FLUSH_ORDER	147
#define SYNC_RECV		168
#define	SYNC_WORK_QUEUE		162
#define	SYNC_SEARCH_SYS		160	/* NOTE that if we have a memory
					heap that can be extended to the
					buffer pool, its logical level is
					SYNC_SEARCH_SYS, as memory allocation
					can call routines there! Otherwise
					the level is SYNC_MEM_HASH. */
#define	SYNC_BUF_POOL		150	/* Buffer pool mutex */
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

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! The structure used in the spin lock
implementation of a mutual exclusion semaphore. */

/** InnoDB mutex */
struct mutex_struct {
	os_event_t	event;	/*!< Used by sync0arr.c for the wait queue */
	volatile lock_word_t	lock_word;	/*!< lock_word is the target
				of the atomic test-and-set instruction when
				atomic operations are enabled. */

#if !defined(HAVE_ATOMIC_BUILTINS)
	os_fast_mutex_t
		os_fast_mutex;	/*!< We use this OS mutex in place of lock_word
				when atomic operations are not enabled */
#endif
	ulint	waiters;	/*!< This ulint is set to 1 if there are (or
				may be) threads waiting in the global wait
				array for this mutex to be released.
				Otherwise, this is 0. */
	UT_LIST_NODE_T(mutex_t)	list; /*!< All allocated mutexes are put into
				a list.	Pointers to the next and prev. */
#ifdef UNIV_SYNC_DEBUG
	const char*	file_name;	/*!< File where the mutex was locked */
	ulint	line;		/*!< Line where the mutex was locked */
	ulint	level;		/*!< Level in the global latching order */
#endif /* UNIV_SYNC_DEBUG */
	const char*	cfile_name;/*!< File name where mutex created */
	ulint		cline;	/*!< Line where created */
#ifdef UNIV_DEBUG
	os_thread_id_t thread_id; /*!< The thread id of the thread
				which locked the mutex. */
	ulint		magic_n;	/*!< MUTEX_MAGIC_N */
/** Value of mutex_struct::magic_n */
# define MUTEX_MAGIC_N	(ulint)979585
#endif /* UNIV_DEBUG */
	ulong		count_os_wait;	/*!< count of os_wait */
#ifdef UNIV_DEBUG
	ulong		count_using;	/*!< count of times mutex used */
	ulong		count_spin_loop; /*!< count of spin loops */
	ulong		count_spin_rounds;/*!< count of spin rounds */
	ulong		count_os_yield;	/*!< count of os_wait */
	ulonglong	lspent_time;	/*!< mutex os_wait timer msec */
	ulonglong	lmax_spent_time;/*!< mutex os_wait timer msec */
	const char*	cmutex_name;	/*!< mutex name */
	ulint		mutex_type;	/*!< 0=usual mutex, 1=rw_lock mutex */
#endif /* UNIV_DEBUG */
#ifdef UNIV_PFS_MUTEX
	struct PSI_mutex* pfs_psi;	/*!< The performance schema
					instrumentation hook */
#endif
};

/** The global array of wait cells for implementation of the databases own
mutexes and read-write locks. */
extern sync_array_t*	sync_primary_wait_array;/* Appears here for
						debugging purposes only! */

/** Constant determining how long spin wait is continued before suspending
the thread. A value 600 rounds on a 1995 100 MHz Pentium seems to correspond
to 20 microseconds. */

#define	SYNC_SPIN_ROUNDS	srv_n_spin_wait_rounds

/** The number of mutex_exit calls. Intended for performance monitoring. */
extern	ib_int64_t	mutex_exit_count;

#ifdef UNIV_SYNC_DEBUG
/** Latching order checks start when this is set TRUE */
extern ibool	sync_order_checks_on;
#endif /* UNIV_SYNC_DEBUG */

/** This variable is set to TRUE when sync_init is called */
extern ibool	sync_initialized;

/** Global list of database mutexes (not OS mutexes) created. */
typedef UT_LIST_BASE_NODE_T(mutex_t)  ut_list_base_node_t;
/** Global list of database mutexes (not OS mutexes) created. */
extern ut_list_base_node_t  mutex_list;

/** Mutex protecting the mutex_list variable */
extern mutex_t mutex_list_mutex;


#ifndef UNIV_NONINL
#include "sync0sync.ic"
#endif

#endif
