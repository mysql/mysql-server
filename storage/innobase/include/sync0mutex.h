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
@file include/sync0mutex.h
InnoDB mutex implementation header file

Created 2012-08-15 Sunny Bains.
***********************************************************************/

#include "univ.i"
#include "ut0lst.h"
#include "os0thread.h"
#include "os0sync.h"
#include "sync0sync.h"

#ifndef sync0mutex_h

#define sync0mutex_h

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
#   define mutex_create(K, M, level)					\
	pfs_mutex_create_func((K), (M), #M, (level), __FILE__, __LINE__)
#  else
#   define mutex_create(K, M, level)					\
	pfs_mutex_create_func((K), (M), #M, __FILE__, __LINE__)
#  endif/* UNIV_SYNC_DEBUG */
# else
#  define mutex_create(K, M, level)					\
	pfs_mutex_create_func((K), (M), __FILE__, __LINE__)
# endif	/* UNIV_DEBUG */

# define mutex_enter(M)							\
	pfs_mutex_enter_func((M), __FILE__, __LINE__, false)

# define mutex_enter_spinonly(M)					\
	pfs_mutex_enter_func((M), __FILE__, __LINE__, true)

# define mutex_enter_nowait(M)						\
	pfs_mutex_enter_nowait_func((M), __FILE__, __LINE__)

# define mutex_exit(M)	pfs_mutex_exit_func(M)

# define mutex_free(M)	pfs_mutex_free_func(M)

#else	/* UNIV_PFS_MUTEX */

/* If "UNIV_PFS_MUTEX" is not defined, the mutex APIs point to
original non-instrumented functions */
# ifdef UNIV_DEBUG
#  ifdef UNIV_SYNC_DEBUG
#   define mutex_create(K, M, level)					\
	mutex_create_func((M), #M, (level), __FILE__, __LINE__)
#  else /* UNIV_SYNC_DEBUG */
#   define mutex_create(K, M, level)					\
	mutex_create_func((M), #M, __FILE__, __LINE__)
#  endif /* UNIV_SYNC_DEBUG */
# else /* UNIV_DEBUG */
#  define mutex_create(K, M, level)					\
	mutex_create_func((M), __FILE__, __LINE__)
# endif	/* UNIV_DEBUG */

# define mutex_enter(M)	mutex_enter_func((M), __FILE__, __LINE__)

# define mutex_enter_spinonly(M)					\
	mutex_enter_func((M), __FILE__, __LINE__, true)

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
	ib_mutex_t*	mutex,		/*!< in: pointer to memory */
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
	ib_mutex_t*	mutex);		/*!< in/own: mutex */

/******************************************************************//**
NOTE! Use the corresponding macro in the header file, not this function
directly. Locks a mutex for the current thread. If the mutex is reserved
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS) waiting
for the mutex before suspending the thread. */
UNIV_INLINE
void
mutex_enter_func(
/*=============*/
	ib_mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where locked */
	ulint		line,		/*!< in: line where locked */
	bool		spin_only = false);
					/*!< in: Don't use the sync array to
					wait if set to true */

/********************************************************************//**
NOTE! Use the corresponding macro in the header file, not this function
directly. Tries to lock the mutex for the current thread. If the lock is not
acquired immediately, returns with return value 1.
@return	0 if succeed, 1 if not */
UNIV_INTERN
ulint
mutex_enter_nowait_func(
/*====================*/
	ib_mutex_t*	mutex,		/*!< in: pointer to mutex */
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
	ib_mutex_t*	mutex);	/*!< in: pointer to mutex */


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
	ib_mutex_t*	mutex,		/*!< in: pointer to memory */
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
	ib_mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where locked */
	ulint		line,		/*!< in: line where locked */
	bool		spin_only = false);
					/*!< in: Don't use the sync array to
					wait if set to true */

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
	ib_mutex_t*	mutex,		/*!< in: pointer to mutex */
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
	ib_mutex_t*	mutex);		/*!< in: pointer to mutex */

/******************************************************************//**
NOTE! Please use the corresponding macro mutex_free(), not directly
this function!
Wrapper function for mutex_free_func(). Also destroys the performance
schema probes when freeing the mutex */
UNIV_INLINE
void
pfs_mutex_free_func(
/*================*/
	ib_mutex_t*	mutex);		/*!< in: mutex */

#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_DEBUG
/******************************************************************//**
Checks that the mutex has been initialized.
@return	TRUE */
UNIV_INTERN
ibool
mutex_validate(
/*===========*/
	const ib_mutex_t*	mutex);	/*!< in: mutex */

/******************************************************************//**
Checks that the current thread owns the mutex. Works only
in the debug version.
@return	TRUE if owns */
UNIV_INTERN
ibool
mutex_own(
/*======*/
	const ib_mutex_t*	mutex)	/*!< in: mutex */
	__attribute__((warn_unused_result));

#endif /* UNIV_DEBUG */

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Gets the debug information for a reserved mutex. */
UNIV_INTERN
void
mutex_get_debug_info(
/*=================*/
	ib_mutex_t*	mutex,		/*!< in: mutex */
	const char**	file_name,	/*!< out: file where requested */
	ulint*		line,		/*!< out: line where requested */
	os_thread_id_t* thread_id);	/*!< out: id of the thread which owns
					the mutex */

/******************************************************************//**
Counts currently reserved mutexes. Works only in the debug version.
@return	number of reserved mutexes */
UNIV_INTERN
ulint
mutex_n_reserved();
/*===============*/

/******************************************************************//**
NOT to be used outside this module except in debugging! Gets the value
of the lock word. */
UNIV_INLINE
lock_word_t
mutex_get_lock_word(
/*================*/
	const ib_mutex_t*	mutex);	/*!< in: mutex */

/******************************************************************//**
NOT to be used outside this module except in debugging! Gets the waiters
field in a mutex.
@return	value to set */
UNIV_INLINE
ulint
mutex_get_waiters(
/*==============*/
	const ib_mutex_t*	mutex);	/*!< in: mutex */

/******************************************************************//**
Prints debug info of currently reserved mutexes. */
UNIV_INTERN
void
mutex_list_print_info(
/*==================*/
	FILE*	file);			/*!< in: file where to print */

/******************************************************************//**
@return total number of spin rounds since startup. */
UNIV_INTERN
ib_uint64_t
mutex_spin_round_count_get();
/*=========================*/

/******************************************************************//**
@return total number of spin wait calls since startup. */
UNIV_INTERN
ib_uint64_t
mutex_spin_wait_count_get();
/*=========================*/

/******************************************************************//**
@return total number of OS waits since startup. */
UNIV_INTERN
ib_uint64_t
mutex_os_wait_count_get();
/*======================*/
#endif /* UNIV_SYNC_DEBUG */

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! The structure used in the spin lock
implementation of a mutual exclusion semaphore. */

struct mutex_base_t {
#ifdef UNIV_SYNC_DEBUG
	const char*	file_name;	 /*!< File where the mutex was locked */

	ulint		line;		/*!< Line where the mutex was locked */

	ulint		level;		/*!< Level in the global latching
					order */
#endif /* UNIV_SYNC_DEBUG */

	const char*	cfile_name;	/*!< File name where mutex created */

	ulint		cline;		/*!< Line where created */

	ulong		count_os_wait;	/*!< count of os_wait */

#ifdef UNIV_DEBUG

/** Value of ib_mutex_t::magic_n */
# define MUTEX_MAGIC_N	979585UL

	os_thread_id_t thread_id; 	/*!< The thread id of the thread
					which locked the mutex. */
	ulint		magic_n;	/*!< MUTEX_MAGIC_N */
	const char*	cmutex_name;	/*!< mutex name */
	ulint		ib_mutex_type;	/*!< 0=usual mutex, 1=rw_lock mutex */
#endif /* UNIV_DEBUG */

#ifdef UNIV_PFS_MUTEX
	struct PSI_mutex* pfs_psi;	/*!< The performance schema
					instrumentation hook */
#endif /* UNIV_PFS_MUTEX */
	UT_LIST_NODE_T(ib_mutex_t)
			list;		/*!< All allocated mutexes are put into
					a list.	Pointers to the next and
					prev. */
};

/** InnoDB mutex */
struct spin_mutex_t : public mutex_base_t {
	volatile lock_word_t
			lock_word;	/*!< lock_word is the target
					of the atomic test-and-set instruction
					when atomic operations are enabled. */

	ulint		waiters;	/*!< This ulint is set to 1 if there
					are (or may be) threads waiting in
					the global wait array for this mutex
					to be released.  Otherwise, this
					is 0. */
};

/** InnoDB mutex */
struct ib_mutex_t : public spin_mutex_t {
#ifndef HAVE_ATOMIC_BUILTINS
	os_fast_mutex_t	
			os_fast_mutex;
					/*!< We use this OS mutex in place of
					lock_word when atomic operations are
					not enabled */
#endif /* !HAVE_ATOMIC_BUILTINS */
	os_event_t	event;		/*!< Used by sync0arr.cc for the
					wait queue */
};

#ifndef UNIV_NONINL
#include "sync0mutex.ic"
#endif /* !UNIV_NOINL */

#endif /* !sync0mutex_h */
