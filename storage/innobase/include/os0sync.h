/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.
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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file include/os0sync.h
The interface to the operating system
synchronization primitives.

Created 9/6/1995 Heikki Tuuri
*******************************************************/

#ifndef os0sync_h
#define os0sync_h

#include "univ.i"
#include "ut0lst.h"

#if defined __i386__ || defined __x86_64__ || defined _M_IX86 \
    || defined _M_X64 || defined __WIN__

#define IB_STRONG_MEMORY_MODEL

#endif /* __i386__ || __x86_64__ || _M_IX86 || M_X64 || __WIN__ */

#ifdef HAVE_WINDOWS_ATOMICS
typedef LONG lock_word_t;	/*!< On Windows, InterlockedExchange operates
				on LONG variable */
#else
typedef byte lock_word_t;
#endif

#ifdef __WIN__
/** Native event (slow)*/
typedef HANDLE			os_native_event_t;
/** Native mutex */
typedef CRITICAL_SECTION	os_fast_mutex_t;
/** Native condition variable. */
typedef CONDITION_VARIABLE	os_cond_t;
#else
/** Native mutex */
typedef pthread_mutex_t		os_fast_mutex_t;
/** Native condition variable */
typedef pthread_cond_t		os_cond_t;
#endif

/** Operating system event */
typedef struct os_event_struct	os_event_struct_t;
/** Operating system event handle */
typedef os_event_struct_t*	os_event_t;

/** An asynchronous signal sent between threads */
struct os_event_struct {
#ifdef __WIN__
	HANDLE		handle;		/*!< kernel event object, slow,
					used on older Windows */
#endif
	os_fast_mutex_t	os_mutex;	/*!< this mutex protects the next
					fields */
	ibool		is_set;		/*!< this is TRUE when the event is
					in the signaled state, i.e., a thread
					does not stop if it tries to wait for
					this event */
	ib_int64_t	signal_count;	/*!< this is incremented each time
					the event becomes signaled */
	os_cond_t	cond_var;	/*!< condition variable is used in
					waiting for the event */
	UT_LIST_NODE_T(os_event_struct_t) os_event_list;
					/*!< list of all created events */
};

/** Denotes an infinite delay for os_event_wait_time() */
#define OS_SYNC_INFINITE_TIME   ULINT_UNDEFINED

/** Return value of os_event_wait_time() when the time is exceeded */
#define OS_SYNC_TIME_EXCEEDED   1

/** Operating system mutex */
typedef struct os_mutex_struct	os_mutex_str_t;
/** Operating system mutex handle */
typedef os_mutex_str_t*		os_mutex_t;

/** Mutex protecting counts and the event and OS 'slow' mutex lists */
extern os_mutex_t	os_sync_mutex;

/** This is incremented by 1 in os_thread_create and decremented by 1 in
os_thread_exit */
extern ulint		os_thread_count;

extern ulint		os_event_count;
extern ulint		os_mutex_count;
extern ulint		os_fast_mutex_count;

/*********************************************************//**
Initializes global event and OS 'slow' mutex lists. */
UNIV_INTERN
void
os_sync_init(void);
/*==============*/
/*********************************************************//**
Frees created events and OS 'slow' mutexes. */
UNIV_INTERN
void
os_sync_free(void);
/*==============*/
/*********************************************************//**
Creates an event semaphore, i.e., a semaphore which may just have two states:
signaled and nonsignaled. The created event is manual reset: it must be reset
explicitly by calling sync_os_reset_event.
@return	the event handle */
UNIV_INTERN
os_event_t
os_event_create(
/*============*/
	const char*	name);	/*!< in: the name of the event, if NULL
				the event is created without a name */
/**********************************************************//**
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */
UNIV_INTERN
void
os_event_set(
/*=========*/
	os_event_t	event);	/*!< in: event to set */
/**********************************************************//**
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event.
The return value should be passed to os_even_wait_low() if it is desired
that this thread should not wait in case of an intervening call to
os_event_set() between this os_event_reset() and the
os_event_wait_low() call. See comments for os_event_wait_low(). */
UNIV_INTERN
ib_int64_t
os_event_reset(
/*===========*/
	os_event_t	event);	/*!< in: event to reset */
/**********************************************************//**
Frees an event object. */
UNIV_INTERN
void
os_event_free(
/*==========*/
	os_event_t	event);	/*!< in: event to free */

/**********************************************************//**
Waits for an event object until it is in the signaled state.

Typically, if the event has been signalled after the os_event_reset()
we'll return immediately because event->is_set == TRUE.
There are, however, situations (e.g.: sync_array code) where we may
lose this information. For example:

thread A calls os_event_reset()
thread B calls os_event_set()   [event->is_set == TRUE]
thread C calls os_event_reset() [event->is_set == FALSE]
thread A calls os_event_wait()  [infinite wait!]
thread C calls os_event_wait()  [infinite wait!]

Where such a scenario is possible, to avoid infinite wait, the
value returned by os_event_reset() should be passed in as
reset_sig_count. */
UNIV_INTERN
void
os_event_wait_low(
/*==============*/
	os_event_t	event,		/*!< in: event to wait */
	ib_int64_t	reset_sig_count);/*!< in: zero or the value
					returned by previous call of
					os_event_reset(). */

#define os_event_wait(event) os_event_wait_low(event, 0)
#define os_event_wait_time(e, t) os_event_wait_time_low(event, t, 0)

/**********************************************************//**
Waits for an event object until it is in the signaled state or
a timeout is exceeded. In Unix the timeout is always infinite.
@return	0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
UNIV_INTERN
ulint
os_event_wait_time_low(
/*===================*/
	os_event_t	event,			/*!< in: event to wait */
	ulint		time_in_usec,		/*!< in: timeout in
						microseconds, or
						OS_SYNC_INFINITE_TIME */
	ib_int64_t	reset_sig_count);	/*!< in: zero or the value
						returned by previous call of
						os_event_reset(). */
/*********************************************************//**
Creates an operating system mutex semaphore. Because these are slow, the
mutex semaphore of InnoDB itself (mutex_t) should be used where possible.
@return	the mutex handle */
UNIV_INTERN
os_mutex_t
os_mutex_create(void);
/*=================*/
/**********************************************************//**
Acquires ownership of a mutex semaphore. */
UNIV_INTERN
void
os_mutex_enter(
/*===========*/
	os_mutex_t	mutex);	/*!< in: mutex to acquire */
/**********************************************************//**
Releases ownership of a mutex. */
UNIV_INTERN
void
os_mutex_exit(
/*==========*/
	os_mutex_t	mutex);	/*!< in: mutex to release */
/**********************************************************//**
Frees an mutex object. */
UNIV_INTERN
void
os_mutex_free(
/*==========*/
	os_mutex_t	mutex);	/*!< in: mutex to free */
/**********************************************************//**
Acquires ownership of a fast mutex. Currently in Windows this is the same
as os_fast_mutex_lock!
@return	0 if success, != 0 if was reserved by another thread */
UNIV_INLINE
ulint
os_fast_mutex_trylock(
/*==================*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: mutex to acquire */
/**********************************************************//**
Acquires ownership of a fast mutex. Implies a full memory barrier even on
platforms such as PowerPC where this is not normally required.
@return	0 if success, != 0 if was reserved by another thread */
UNIV_INLINE
ulint
os_fast_mutex_trylock_full_barrier(
/*==================*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: mutex to acquire */
/**********************************************************//**
Releases ownership of a fast mutex. */
UNIV_INTERN
void
os_fast_mutex_unlock(
/*=================*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: mutex to release */
/**********************************************************//**
Releases ownership of a fast mutex. Implies a full memory barrier even on
platforms such as PowerPC where this is not normally required. */
UNIV_INTERN
void
os_fast_mutex_unlock_full_barrier(
/*=================*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: mutex to release */
/*********************************************************//**
Initializes an operating system fast mutex semaphore. */
UNIV_INTERN
void
os_fast_mutex_init(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: fast mutex */
/**********************************************************//**
Acquires ownership of a fast mutex. */
UNIV_INTERN
void
os_fast_mutex_lock(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: mutex to acquire */
/**********************************************************//**
Frees an mutex object. */
UNIV_INTERN
void
os_fast_mutex_free(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/*!< in: mutex to free */

/**********************************************************//**
Atomic compare-and-swap and increment for InnoDB. */

#if defined(HAVE_IB_GCC_ATOMIC_BUILTINS)

#define HAVE_ATOMIC_BUILTINS

/**********************************************************//**
Returns true if swapped, ptr is pointer to target, old_val is value to
compare to, new_val is the value to swap in. */

# define os_compare_and_swap(ptr, old_val, new_val) \
	__sync_bool_compare_and_swap(ptr, old_val, new_val)

# define os_compare_and_swap_ulint(ptr, old_val, new_val) \
	os_compare_and_swap(ptr, old_val, new_val)

# define os_compare_and_swap_lint(ptr, old_val, new_val) \
	os_compare_and_swap(ptr, old_val, new_val)

# ifdef HAVE_IB_ATOMIC_PTHREAD_T_GCC
#  define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	os_compare_and_swap(ptr, old_val, new_val)
#  define INNODB_RW_LOCKS_USE_ATOMICS
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use GCC atomic builtins"
# else /* HAVE_IB_ATOMIC_PTHREAD_T_GCC */
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes use GCC atomic builtins, rw_locks do not"
# endif /* HAVE_IB_ATOMIC_PTHREAD_T_GCC */

/**********************************************************//**
Returns the resulting value, ptr is pointer to target, amount is the
amount of increment. */

# define os_atomic_increment(ptr, amount) \
	__sync_add_and_fetch(ptr, amount)

# define os_atomic_increment_lint(ptr, amount) \
	os_atomic_increment(ptr, amount)

# define os_atomic_increment_ulint(ptr, amount) \
	os_atomic_increment(ptr, amount)

# if defined(IB_STRONG_MEMORY_MODEL)

/** Do an atomic test and set.
@param[in,out]	ptr		Memory location to set to non-zero
@return the previous value */
static inline
lock_word_t
os_atomic_test_and_set(volatile lock_word_t* ptr)
{
	return(__sync_lock_test_and_set(ptr, 1));
}

/** Do an atomic release.
@param[in,out]	ptr		Memory location to write to
@return the previous value */
static inline
void
os_atomic_clear(volatile lock_word_t* ptr)
{
	__sync_lock_release(ptr);
}

# elif defined(HAVE_IB_GCC_ATOMIC_TEST_AND_SET)

/** Do an atomic test-and-set.
@param[in,out]	ptr		Memory location to set to non-zero
@return the previous value */
static inline
lock_word_t
os_atomic_test_and_set(volatile lock_word_t* ptr)
{
       return(__atomic_test_and_set(ptr, __ATOMIC_ACQUIRE));
}

/** Do an atomic clear.
@param[in,out]	ptr		Memory location to set to zero */
static inline
void
os_atomic_clear(volatile lock_word_t* ptr)
{
	__atomic_clear(ptr, __ATOMIC_RELEASE);
}

# else

#  error "Unsupported platform"

# endif /* HAVE_IB_GCC_ATOMIC_TEST_AND_SET */

#elif defined(HAVE_IB_SOLARIS_ATOMICS)

#define HAVE_ATOMIC_BUILTINS

/* If not compiling with GCC or GCC doesn't support the atomic
intrinsics and running on Solaris >= 10 use Solaris atomics */

#include <atomic.h>

/**********************************************************//**
Returns true if swapped, ptr is pointer to target, old_val is value to
compare to, new_val is the value to swap in. */

# define os_compare_and_swap_ulint(ptr, old_val, new_val) \
	(atomic_cas_ulong(ptr, old_val, new_val) == old_val)

# define os_compare_and_swap_lint(ptr, old_val, new_val) \
	((lint)atomic_cas_ulong((ulong_t*) ptr, old_val, new_val) == old_val)

# ifdef HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS
#  if SIZEOF_PTHREAD_T == 4
#   define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	((pthread_t)atomic_cas_32(ptr, old_val, new_val) == old_val)
#  elif SIZEOF_PTHREAD_T == 8
#   define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	((pthread_t)atomic_cas_64(ptr, old_val, new_val) == old_val)
#  else
#   error "SIZEOF_PTHREAD_T != 4 or 8"
#  endif /* SIZEOF_PTHREAD_T CHECK */
#  define INNODB_RW_LOCKS_USE_ATOMICS
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use Solaris atomic functions"
# else /* HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS */
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes use Solaris atomic functions, rw_locks do not"
# endif /* HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS */

/**********************************************************//**
Returns the resulting value, ptr is pointer to target, amount is the
amount of increment. */

# define os_atomic_increment_lint(ptr, amount) \
	atomic_add_long_nv((ulong_t*) ptr, amount)

# define os_atomic_increment_ulint(ptr, amount) \
	atomic_add_long_nv(ptr, amount)

/** Do an atomic xchg and set to non-zero.
@param[in,out]	ptr		Memory location to set to non-zero
@return the previous value */
static inline
lock_word_t
os_atomic_test_and_set(volatile lock_word_t* ptr)
{
	return(atomic_swap_uchar(ptr, 1));
}

/** Do an atomic xchg and set to zero.
@param[in,out]	ptr		Memory location to set to zero
@return the previous value */
static inline
lock_word_t
os_atomic_clear(volatile lock_word_t* ptr)
{
	return(atomic_swap_uchar(ptr, 0));
}

#elif defined(HAVE_WINDOWS_ATOMICS)

#define HAVE_ATOMIC_BUILTINS

/* On Windows, use Windows atomics / interlocked */
# ifdef _WIN64
#  define win_cmp_and_xchg InterlockedCompareExchange64
#  define win_xchg_and_add InterlockedExchangeAdd64
# else /* _WIN64 */
#  define win_cmp_and_xchg InterlockedCompareExchange
#  define win_xchg_and_add InterlockedExchangeAdd
# endif

/**********************************************************//**
Returns true if swapped, ptr is pointer to target, old_val is value to
compare to, new_val is the value to swap in. */

# define os_compare_and_swap_ulint(ptr, old_val, new_val) \
	(win_cmp_and_xchg(ptr, new_val, old_val) == old_val)

# define os_compare_and_swap_lint(ptr, old_val, new_val) \
	(win_cmp_and_xchg(ptr, new_val, old_val) == old_val)

/* windows thread objects can always be passed to windows atomic functions */
# define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	(InterlockedCompareExchange(ptr, new_val, old_val) == old_val)
# define INNODB_RW_LOCKS_USE_ATOMICS
# define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use Windows interlocked functions"

/**********************************************************//**
Returns the resulting value, ptr is pointer to target, amount is the
amount of increment. */

# define os_atomic_increment_lint(ptr, amount) \
	(win_xchg_and_add(ptr, amount) + amount)

# define os_atomic_increment_ulint(ptr, amount) \
	((ulint) (win_xchg_and_add(ptr, amount) + amount))

/** Do an atomic test and set.
InterlockedExchange() operates on LONG, and the LONG will be clobbered
@param[in,out]	ptr		Memory location to set to non-zero
@return the previous value */
static inline
lock_word_t
os_atomic_test_and_set(volatile lock_word_t* ptr)
{
	return(InterlockedExchange(ptr, 1));
}

/** Do an atomic release.
InterlockedExchange() operates on LONG, and the LONG will be clobbered
@param[in,out]	ptr		Memory location to set to zero
@return the previous value */
static inline
lock_word_t
os_atomic_clear(volatile lock_word_t* ptr)
{
	return(InterlockedExchange(ptr, 0));
}

#else
# define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use InnoDB's own implementation"
#endif

/** barrier definitions for memory ordering */
#ifdef HAVE_IB_GCC_ATOMIC_THREAD_FENCE
# define HAVE_MEMORY_BARRIER
# define os_rmb	__atomic_thread_fence(__ATOMIC_ACQUIRE)
# define os_wmb	__atomic_thread_fence(__ATOMIC_RELEASE)
# define os_mb __atomic_thread_fence(__ATOMIC_SEQ_CST)

# define IB_MEMORY_BARRIER_STARTUP_MSG \
	"GCC builtin __atomic_thread_fence() is used for memory barrier"

#elif defined(HAVE_IB_GCC_SYNC_SYNCHRONISE)
# define HAVE_MEMORY_BARRIER
# define os_rmb	__sync_synchronize()
# define os_wmb	__sync_synchronize()
# define os_mb	__sync_synchronize()
# define IB_MEMORY_BARRIER_STARTUP_MSG \
	"GCC builtin __sync_synchronize() is used for memory barrier"

#elif defined(HAVE_IB_MACHINE_BARRIER_SOLARIS)
# define HAVE_MEMORY_BARRIER
# include <mbarrier.h>
# define os_rmb	__machine_r_barrier()
# define os_wmb	__machine_w_barrier()
# define os_mb __machine_rw_barrier()
# define IB_MEMORY_BARRIER_STARTUP_MSG \
	"Soralis memory ordering functions are used for memory barrier"

#elif defined(HAVE_WINDOWS_MM_FENCE)
# define HAVE_MEMORY_BARRIER
# include <intrin.h>
# define os_rmb	_mm_lfence()
# define os_wmb	_mm_sfence()
# define os_mb	_mm_mfence()
# define IB_MEMORY_BARRIER_STARTUP_MSG \
	"_mm_lfence() and _mm_sfence() are used for memory barrier"

#else
# define os_rmb do { } while(0)
# define os_wmb do { } while(0)
# define os_mb do { } while(0)
# define IB_MEMORY_BARRIER_STARTUP_MSG \
	"Memory barrier is not used"
#endif

#ifndef UNIV_NONINL
#include "os0sync.ic"
#endif

#endif
