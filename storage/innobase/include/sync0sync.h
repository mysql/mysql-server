/******************************************************
Mutex, the basic synchronization primitive

(c) 1995 Innobase Oy

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

#ifndef UNIV_HOTBACKUP
extern my_bool  timed_mutexes;
#endif /* UNIV_HOTBACKUP */

/**********************************************************************
Initializes the synchronization data structures. */

void
sync_init(void);
/*===========*/
/**********************************************************************
Frees the resources in synchronization data structures. */

void
sync_close(void);
/*===========*/
/**********************************************************************
Creates, or rather, initializes a mutex object to a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */

#define mutex_create(M)	mutex_create_func((M), __FILE__, __LINE__, #M)
/*===================*/
/**********************************************************************
Creates, or rather, initializes a mutex object in a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */

void
mutex_create_func(
/*==============*/
	mutex_t*	mutex,		/* in: pointer to memory */
	const char*	cfile_name,	/* in: file name where created */
  ulint cline,  /* in: file line where created */
  const char* cmutex_name); /* in: mutex name */
/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the mutex is freed. Removes a mutex object from the mutex list. The mutex
is checked to be in the reset state. */

#undef mutex_free			/* Fix for MacOS X */
void
mutex_free(
/*=======*/
	mutex_t*	mutex);	/* in: mutex */
/******************************************************************
NOTE! The following macro should be used in mutex locking, not the
corresponding function. */

#define mutex_enter(M)    mutex_enter_func((M), __FILE__, __LINE__)
/**********************************************************************
A noninlined function that reserves a mutex. In ha_innodb.cc we have disabled
inlining of InnoDB functions, and no inlined functions should be called from
there. That is why we need to duplicate the inlined function here. */

void
mutex_enter_noninline(
/*==================*/
	mutex_t*	mutex);	/* in: mutex */
/******************************************************************
NOTE! The following macro should be used in mutex locking, not the
corresponding function. */

/* NOTE! currently same as mutex_enter! */

#define mutex_enter_fast(M)    	mutex_enter_func((M), __FILE__, __LINE__)
#define mutex_enter_fast_func  	mutex_enter_func;
/**********************************************************************
NOTE! Use the corresponding macro in the header file, not this function
directly. Locks a mutex for the current thread. If the mutex is reserved
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS) waiting
for the mutex before suspending the thread. */
UNIV_INLINE
void
mutex_enter_func(
/*=============*/
	mutex_t*	mutex,		/* in: pointer to mutex */
	const char*	file_name, 	/* in: file name where locked */
	ulint		line);		/* in: line where locked */
/************************************************************************
Tries to lock the mutex for the current thread. If the lock is not acquired
immediately, returns with return value 1. */

ulint
mutex_enter_nowait(
/*===============*/
					/* out: 0 if succeed, 1 if not */
	mutex_t*	mutex,		/* in: pointer to mutex */
	const char*	file_name,	/* in: file name where mutex
					requested */
	ulint		line);		/* in: line where requested */
/**********************************************************************
Unlocks a mutex owned by the current thread. */
UNIV_INLINE
void
mutex_exit(
/*=======*/
	mutex_t*	mutex);	/* in: pointer to mutex */
/**********************************************************************
Releases a mutex. */

void
mutex_exit_noninline(
/*=================*/
	mutex_t*	mutex);	/* in: mutex */
/**********************************************************************
Returns TRUE if no mutex or rw-lock is currently locked.
Works only in the debug version. */

ibool
sync_all_freed(void);
/*================*/
/*#####################################################################
FUNCTION PROTOTYPES FOR DEBUGGING */
/***********************************************************************
Prints wait info of the sync system. */

void
sync_print_wait_info(
/*=================*/
	FILE*	file);		/* in: file where to print */
/***********************************************************************
Prints info of the sync system. */

void
sync_print(
/*=======*/
	FILE*	file);		/* in: file where to print */
/**********************************************************************
Checks that the mutex has been initialized. */

ibool
mutex_validate(
/*===========*/
	mutex_t*	mutex);
/**********************************************************************
Sets the mutex latching level field. */

void
mutex_set_level(
/*============*/
	mutex_t*	mutex,	/* in: mutex */
	ulint		level);	/* in: level */
/**********************************************************************
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread. */

void
sync_thread_add_level(
/*==================*/
	void*	latch,	/* in: pointer to a mutex or an rw-lock */
	ulint	level);	/* in: level in the latching order; if SYNC_LEVEL_NONE,
			nothing is done */			
/**********************************************************************
Removes a latch from the thread level array if it is found there. */

ibool
sync_thread_reset_level(
/*====================*/
			/* out: TRUE if found from the array; it is no error
			if the latch is not found, as we presently are not
			able to determine the level for every latch
			reservation the program does */
	void*	latch);	/* in: pointer to a mutex or an rw-lock */
/**********************************************************************
Checks that the level array for the current thread is empty. */

ibool
sync_thread_levels_empty(void);
/*==========================*/
			/* out: TRUE if empty */
/**********************************************************************
Checks that the level array for the current thread is empty. */

ibool
sync_thread_levels_empty_gen(
/*=========================*/
					/* out: TRUE if empty except the
					exceptions specified below */
	ibool	dict_mutex_allowed);	/* in: TRUE if dictionary mutex is
					allowed to be owned by the thread,
					also purge_is_running mutex is
					allowed */
#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
Checks that the current thread owns the mutex. Works only
in the debug version. */

ibool
mutex_own(
/*======*/
				/* out: TRUE if owns */
	mutex_t*	mutex);	/* in: mutex */
/**********************************************************************
Gets the debug information for a reserved mutex. */

void
mutex_get_debug_info(
/*=================*/
	mutex_t*	mutex,		/* in: mutex */
	const char**	file_name,	/* out: file where requested */
	ulint*		line,		/* out: line where requested */
	os_thread_id_t* thread_id);	/* out: id of the thread which owns
					the mutex */
/**********************************************************************
Counts currently reserved mutexes. Works only in the debug version. */

ulint
mutex_n_reserved(void);
/*==================*/
/**********************************************************************
Prints debug info of currently reserved mutexes. */

void
mutex_list_print_info(void);
/*========================*/
#endif /* UNIV_SYNC_DEBUG */
/**********************************************************************
NOT to be used outside this module except in debugging! Gets the value
of the lock word. */
UNIV_INLINE
ulint
mutex_get_lock_word(
/*================*/
	mutex_t*	mutex);	/* in: mutex */
#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
NOT to be used outside this module except in debugging! Gets the waiters
field in a mutex. */
UNIV_INLINE
ulint
mutex_get_waiters(
/*==============*/
				/* out: value to set */		
	mutex_t*	mutex);	/* in: mutex */
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
#define	SYNC_LEVEL_NONE		2000	/* default: level not defined */
#define	SYNC_DICT_OPERATION	1001	/* table create, drop, etc. reserve
					this in X-mode, implicit or backround
					operations purge, rollback, foreign
					key checks reserve this in S-mode */
#define SYNC_DICT		1000
#define SYNC_DICT_AUTOINC_MUTEX	999
#define SYNC_DICT_HEADER	995
#define SYNC_IBUF_HEADER	914
#define SYNC_IBUF_PESS_INSERT_MUTEX 912
#define SYNC_IBUF_MUTEX		910	/* ibuf mutex is really below
					SYNC_FSP_PAGE: we assign a value this
					high only to make the program to pass
					the debug checks */
/*-------------------------------*/
#define	SYNC_INDEX_TREE		900
#define SYNC_TREE_NODE_NEW	892
#define SYNC_TREE_NODE_FROM_HASH 891
#define SYNC_TREE_NODE		890
#define	SYNC_PURGE_SYS		810
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
/*------------------------------------- ibuf_mutex */
/*------------------------------------- Insert buffer tree */
#define	SYNC_IBUF_BITMAP_MUTEX	351
#define	SYNC_IBUF_BITMAP	350
/*------------------------------------- MySQL query cache mutex */
/*------------------------------------- MySQL binlog mutex */
/*-------------------------------*/
#define	SYNC_KERNEL		300
#define SYNC_REC_LOCK		299
#define	SYNC_TRX_LOCK_HEAP	298
#define SYNC_TRX_SYS_HEADER	290
#define SYNC_LOG		170
#define SYNC_RECV		168
#define	SYNC_SEARCH_SYS		160	/* NOTE that if we have a memory
					heap that can be extended to the
					buffer pool, its logical level is
					SYNC_SEARCH_SYS, as memory allocation
					can call routines there! Otherwise
					the level is SYNC_MEM_HASH. */
#define	SYNC_BUF_POOL		150
#define	SYNC_BUF_BLOCK		149
#define SYNC_DOUBLEWRITE	140
#define	SYNC_ANY_LATCH		135
#define SYNC_THR_LOCAL		133
#define	SYNC_MEM_HASH		131
#define	SYNC_MEM_POOL		130

/* Codes used to designate lock operations */
#define RW_LOCK_NOT_LOCKED 	350
#define RW_LOCK_EX		351
#define RW_LOCK_EXCLUSIVE	351
#define RW_LOCK_SHARED		352
#define RW_LOCK_WAIT_EX		353
#define SYNC_MUTEX		354

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! The structure used in the spin lock
implementation of a mutual exclusion semaphore. */

struct mutex_struct {
	ulint	lock_word;	/* This ulint is the target of the atomic
				test-and-set instruction in Win32 */
#if !defined(_WIN32) || !defined(UNIV_CAN_USE_X86_ASSEMBLER)
	os_fast_mutex_t
		os_fast_mutex;	/* In other systems we use this OS mutex
				in place of lock_word */
#endif
	ulint	waiters;	/* This ulint is set to 1 if there are (or
				may be) threads waiting in the global wait
				array for this mutex to be released.
				Otherwise, this is 0. */
	UT_LIST_NODE_T(mutex_t)	list; /* All allocated mutexes are put into
				a list.	Pointers to the next and prev. */
#ifdef UNIV_SYNC_DEBUG
	const char*	file_name;	/* File where the mutex was locked */
	ulint	line;		/* Line where the mutex was locked */
	os_thread_id_t thread_id; /* Debug version: The thread id of the
				thread which locked the mutex. */
#endif /* UNIV_SYNC_DEBUG */
	ulint	level;		/* Level in the global latching
				order; default SYNC_LEVEL_NONE */
	const char*	cfile_name;/* File name where mutex created */
	ulint	cline;		/* Line where created */
	ulint	magic_n;
#ifndef UNIV_HOTBACKUP
  ulong count_using; /* count of times mutex used */
  ulong count_spin_loop; /* count of spin loops */
  ulong count_spin_rounds; /* count of spin rounds */
  ulong count_os_wait; /* count of os_wait */
  ulong count_os_yield; /* count of os_wait */
  ulonglong lspent_time; /* mutex os_wait timer msec */
  ulonglong lmax_spent_time; /* mutex os_wait timer msec */
  const char* cmutex_name;/* mutex name  */
  ulint mutex_type;/* 0 - usual mutex 1 - rw_lock mutex  */
#endif /* !UNIV_HOTBACKUP */
};

#define MUTEX_MAGIC_N	(ulint)979585

/* The global array of wait cells for implementation of the databases own
mutexes and read-write locks. Appears here for debugging purposes only! */

extern sync_array_t*	sync_primary_wait_array;

/* Constant determining how long spin wait is continued before suspending
the thread. A value 600 rounds on a 1995 100 MHz Pentium seems to correspond
to 20 microseconds. */

#define	SYNC_SPIN_ROUNDS	srv_n_spin_wait_rounds

#define SYNC_INFINITE_TIME	((ulint)(-1))

/* Means that a timeout elapsed when waiting */

#define SYNC_TIME_EXCEEDED	(ulint)1

/* The number of system calls made in this module. Intended for performance
monitoring. */

extern 	ulint	mutex_system_call_count;
extern	ulint	mutex_exit_count;

/* Latching order checks start when this is set TRUE */
extern ibool	sync_order_checks_on;

/* This variable is set to TRUE when sync_init is called */
extern ibool	sync_initialized;

/* Global list of database mutexes (not OS mutexes) created. */
typedef UT_LIST_BASE_NODE_T(mutex_t)  ut_list_base_node_t;
extern ut_list_base_node_t  mutex_list;

/* Mutex protecting the mutex_list variable */
extern mutex_t mutex_list_mutex;


#ifndef UNIV_NONINL
#include "sync0sync.ic"
#endif

#endif
