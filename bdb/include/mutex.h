/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mutex.h,v 11.41 2000/12/22 19:28:15 bostic Exp $
 */

/*
 * Some of the Berkeley DB ports require single-threading at various
 * places in the code.  In those cases, these #defines will be set.
 */
#define	DB_BEGIN_SINGLE_THREAD
#define	DB_END_SINGLE_THREAD

/*
 * When the underlying system mutexes require system resources, we have
 * to clean up after application failure.  This violates the rule that
 * we never look at a shared region after a failure, but there's no other
 * choice.  In those cases, this #define is set.
 */
#ifdef HAVE_QNX
#define	MUTEX_SYSTEM_RESOURCES
#endif

/*********************************************************************
 * POSIX.1 pthreads interface.
 *********************************************************************/
#ifdef HAVE_MUTEX_PTHREADS
#include <pthread.h>

#define	MUTEX_FIELDS							\
	pthread_mutex_t mutex;		/* Mutex. */			\
	pthread_cond_t  cond;		/* Condition variable. */
#endif

/*********************************************************************
 * Solaris lwp threads interface.
 *
 * !!!
 * We use LWP mutexes on Solaris instead of UI or POSIX mutexes (both of
 * which are available), for two reasons.  First, the Solaris C library
 * includes versions of the both UI and POSIX thread mutex interfaces, but
 * they are broken in that they don't support inter-process locking, and
 * there's no way to detect it, e.g., calls to configure the mutexes for
 * inter-process locking succeed without error.  So, we use LWP mutexes so
 * that we don't fail in fairly undetectable ways because the application
 * wasn't linked with the appropriate threads library.  Second, there were
 * bugs in SunOS 5.7 (Solaris 7) where if an application loaded the C library
 * before loading the libthread/libpthread threads libraries (e.g., by using
 * dlopen to load the DB library), the pwrite64 interface would be translated
 * into a call to pwrite and DB would drop core.
 *********************************************************************/
#ifdef HAVE_MUTEX_SOLARIS_LWP
/*
 * XXX
 * Don't change <synch.h> to <sys/lwp.h> -- although lwp.h is listed in the
 * Solaris manual page as the correct include to use, it causes the Solaris
 * compiler on SunOS 2.6 to fail.
 */
#include <synch.h>

#define	MUTEX_FIELDS							\
	lwp_mutex_t mutex;		/* Mutex. */			\
	lwp_cond_t cond;		/* Condition variable. */
#endif

/*********************************************************************
 * Solaris/Unixware threads interface.
 *********************************************************************/
#ifdef HAVE_MUTEX_UI_THREADS
#include <thread.h>
#include <synch.h>

#define	MUTEX_FIELDS							\
	mutex_t mutex;			/* Mutex. */			\
	cond_t  cond;			/* Condition variable. */
#endif

/*********************************************************************
 * AIX C library functions.
 *********************************************************************/
#ifdef HAVE_MUTEX_AIX_CHECK_LOCK
#include <sys/atomic_op.h>
typedef int tsl_t;
#define	MUTEX_ALIGN	sizeof(int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	0
#define	MUTEX_SET(x)	(!_check_lock(x, 0, 1))
#define	MUTEX_UNSET(x)	_clear_lock(x, 0)
#endif
#endif

/*********************************************************************
 * General C library functions (msemaphore).
 *
 * !!!
 * Check for HPPA as a special case, because it requires unusual alignment,
 * and doesn't support semaphores in malloc(3) or shmget(2) memory.
 *
 * !!!
 * Do not remove the MSEM_IF_NOWAIT flag.  The problem is that if a single
 * process makes two msem_lock() calls in a row, the second one returns an
 * error.  We depend on the fact that we can lock against ourselves in the
 * locking subsystem, where we set up a mutex so that we can block ourselves.
 * Tested on OSF1 v4.0.
 *********************************************************************/
#ifdef HAVE_MUTEX_HPPA_MSEM_INIT
#define	MUTEX_NO_MALLOC_LOCKS
#define	MUTEX_NO_SHMGET_LOCKS

#define	MUTEX_ALIGN	16
#endif

#if defined(HAVE_MUTEX_MSEM_INIT) || defined(HAVE_MUTEX_HPPA_MSEM_INIT)
#include <sys/mman.h>
typedef msemaphore tsl_t;

#ifndef MUTEX_ALIGN
#define	MUTEX_ALIGN	sizeof(int)
#endif

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	(msem_init(x, MSEM_UNLOCKED) <= (msemaphore *)0)
#define	MUTEX_SET(x)	(!msem_lock(x, MSEM_IF_NOWAIT))
#define	MUTEX_UNSET(x)	msem_unlock(x, 0)
#endif
#endif

/*********************************************************************
 * Plan 9 library functions.
 *********************************************************************/
#ifdef HAVE_MUTEX_PLAN9
typedef Lock tsl_t;

#define	MUTEX_ALIGN	sizeof(int)

#define	MUTEX_INIT(x)	(memset(x, 0, sizeof(Lock)), 0)
#define	MUTEX_SET(x)	canlock(x)
#define	MUTEX_UNSET(x)	unlock(x)
#endif

/*********************************************************************
 * Reliant UNIX C library functions.
 *********************************************************************/
#ifdef HAVE_MUTEX_RELIANTUNIX_INITSPIN
#include <ulocks.h>
typedef spinlock_t tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	(initspin(x, 1), 0)
#define	MUTEX_SET(x)	(cspinlock(x) == 0)
#define	MUTEX_UNSET(x)	spinunlock(x)
#endif
#endif

/*********************************************************************
 * General C library functions (POSIX 1003.1 sema_XXX).
 *
 * !!!
 * Never selected by autoconfig in this release (semaphore calls are known
 * to not work in Solaris 5.5).
 *********************************************************************/
#ifdef HAVE_MUTEX_SEMA_INIT
#include <synch.h>
typedef sema_t tsl_t;
#define	MUTEX_ALIGN	 sizeof(int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_DESTROY(x) sema_destroy(x)
#define	MUTEX_INIT(x)	 (sema_init(x, 1, USYNC_PROCESS, NULL) != 0)
#define	MUTEX_SET(x)	 (sema_wait(x) == 0)
#define	MUTEX_UNSET(x)	 sema_post(x)
#endif
#endif

/*********************************************************************
 * SGI C library functions.
 *********************************************************************/
#ifdef HAVE_MUTEX_SGI_INIT_LOCK
#include <abi_mutex.h>
typedef abilock_t tsl_t;
#define	MUTEX_ALIGN	sizeof(int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	(init_lock(x) != 0)
#define	MUTEX_SET(x)	(!acquire_lock(x))
#define	MUTEX_UNSET(x)	release_lock(x)
#endif
#endif

/*********************************************************************
 * Solaris C library functions.
 *
 * !!!
 * These are undocumented functions, but they're the only ones that work
 * correctly as far as we know.
 *********************************************************************/
#ifdef HAVE_MUTEX_SOLARIS_LOCK_TRY
#include <sys/machlock.h>
typedef lock_t tsl_t;
#define	MUTEX_ALIGN	sizeof(int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	0
#define	MUTEX_SET(x)	_lock_try(x)
#define	MUTEX_UNSET(x)	_lock_clear(x)
#endif
#endif

/*********************************************************************
 * VMS.
 *********************************************************************/
#ifdef HAVE_MUTEX_VMS
#include <sys/mman.h>;
#include <builtins.h>
typedef unsigned char tsl_t;
#define	MUTEX_ALIGN		sizeof(unsigned int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#ifdef __ALPHA
#define	MUTEX_SET(tsl)		(!__TESTBITSSI(tsl, 0))
#else /* __VAX */
#define	MUTEX_SET(tsl)		(!(int)_BBSSI(0, tsl))
#endif
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * VxWorks
 * Use basic binary semaphores in VxWorks, as we currently do not need
 * any special features.  We do need the ability to single-thread the
 * entire system, however, because VxWorks doesn't support the open(2)
 * flag O_EXCL, the mechanism we normally use to single thread access
 * when we're first looking for a DB environment.
 *********************************************************************/
#ifdef HAVE_MUTEX_VXWORKS
#define	MUTEX_SYSTEM_RESOURCES

#include "semLib.h"
typedef SEM_ID tsl_t;
#define	MUTEX_ALIGN		sizeof(unsigned int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_SET(tsl)		(semTake((*tsl), WAIT_FOREVER) == OK)
#define	MUTEX_UNSET(tsl)	(semGive((*tsl)) == OK)
#define	MUTEX_INIT(tsl)							\
	((*(tsl) = semBCreate(SEM_Q_FIFO, SEM_FULL)) == NULL)
#define	MUTEX_DESTROY(tsl)	semDelete(*tsl)
#endif

/*
 * Use the taskLock() mutex to eliminate a race where two tasks are
 * trying to initialize the global lock at the same time.
 */
#undef	DB_BEGIN_SINGLE_THREAD
#define	DB_BEGIN_SINGLE_THREAD						\
do {									\
	if (DB_GLOBAL(db_global_init))					\
		(void)semTake(DB_GLOBAL(db_global_lock), WAIT_FOREVER);	\
	else {								\
		taskLock();						\
		if (DB_GLOBAL(db_global_init)) {			\
			taskUnlock();					\
			(void)semTake(DB_GLOBAL(db_global_lock),	\
			    WAIT_FOREVER);				\
			continue;					\
		}							\
		DB_GLOBAL(db_global_lock) =				\
		    semBCreate(SEM_Q_FIFO, SEM_EMPTY);			\
		if (DB_GLOBAL(db_global_lock) != NULL)			\
			DB_GLOBAL(db_global_init) = 1;			\
		taskUnlock();						\
	}								\
} while (DB_GLOBAL(db_global_init) == 0)
#undef	DB_END_SINGLE_THREAD
#define	DB_END_SINGLE_THREAD	(void)semGive(DB_GLOBAL(db_global_lock))
#endif

/*********************************************************************
 * Win16
 *
 * Win16 spinlocks are simple because we cannot possibly be preempted.
 *
 * !!!
 * We should simplify this by always returning a no-need-to-lock lock
 * when we initialize the mutex.
 *********************************************************************/
#ifdef HAVE_MUTEX_WIN16
typedef unsigned int tsl_t;
#define	MUTEX_ALIGN		sizeof(unsigned int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)		0
#define	MUTEX_SET(tsl)		(*(tsl) = 1)
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#endif
#endif

/*********************************************************************
 * Win32
 *********************************************************************/
#ifdef HAVE_MUTEX_WIN32
typedef unsigned int tsl_t;
#define	MUTEX_ALIGN		sizeof(unsigned int)

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)		0
#define	MUTEX_SET(tsl)		(!InterlockedExchange((PLONG)tsl, 1))
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#endif
#endif

/*********************************************************************
 * 68K/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_68K_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * For gcc/68K, 0 is clear, 1 is set.
 */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	int __r;							\
	    asm volatile("tas  %1; \n					\
			  seq  %0"					\
		: "=dm" (__r), "=m" (*__l)				\
		: "1" (*__l)						\
		);							\
	__r & 1;							\
})

#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * ALPHA/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_ALPHA_GCC_ASSEMBLY
typedef u_int32_t tsl_t;
#define	MUTEX_ALIGN	4

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * For gcc/alpha.  Should return 0 if could not acquire the lock, 1 if
 * lock was acquired properly.
 */
#ifdef __GNUC__
static inline int
MUTEX_SET(tsl_t *tsl) {
	register tsl_t *__l = tsl;
	register tsl_t __r;
	asm volatile(
		"1:	ldl_l	%0,%2\n"
		"	blbs	%0,2f\n"
		"	or	$31,1,%0\n"
		"	stl_c	%0,%1\n"
		"	beq	%0,3f\n"
		"	mb\n"
		"	br	3f\n"
		"2:	xor	%0,%0\n"
		"3:"
		: "=&r"(__r), "=m"(*__l) : "1"(*__l) : "memory");
	return __r;
}

/*
 * Unset mutex. Judging by Alpha Architecture Handbook, the mb instruction
 * might be necessary before unlocking
 */
static inline int
MUTEX_UNSET(tsl_t *tsl) {
	asm volatile("	mb\n");
	return *tsl = 0;
}
#endif

#ifdef __DECC
#include <alpha/builtins.h>
#define	MUTEX_SET(tsl)		(__LOCK_LONG_RETRY((tsl), 1) != 0)
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#endif

#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * HPPA/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_HPPA_GCC_ASSEMBLY
typedef u_int32_t tsl_t;
#define	MUTEX_ALIGN	16

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * The PA-RISC has a "load and clear" instead of a "test and set" instruction.
 * The 32-bit word used by that instruction must be 16-byte aligned.  We could
 * use the "aligned" attribute in GCC but that doesn't work for stack variables.
 */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	int __r;							\
	asm volatile("ldcws 0(%1),%0" : "=r" (__r) : "r" (__l));	\
	__r & 1;							\
})

#define	MUTEX_UNSET(tsl)	(*(tsl) = -1)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * IA64/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_IA64_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * For gcc/ia64, 0 is clear, 1 is set.
 */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	long __r;							\
	asm volatile("xchg1 %0=%1,%3" : "=r"(__r), "=m"(*__l) : "1"(*__l), "r"(1));\
	__r ^ 1;							\
})

/*
 * Store through a "volatile" pointer so we get a store with "release"
 * semantics.
 */
#define	MUTEX_UNSET(tsl)	(*(volatile unsigned char *)(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * PowerPC/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_PPC_GCC_ASSEMBLY
typedef u_int32_t tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * The PowerPC does a sort of pseudo-atomic locking.  You set up a
 * 'reservation' on a chunk of memory containing a mutex by loading the
 * mutex value with LWARX.  If the mutex has an 'unlocked' (arbitrary)
 * value, you then try storing into it with STWCX.  If no other process or
 * thread broke your 'reservation' by modifying the memory containing the
 * mutex, then the STCWX succeeds; otherwise it fails and you try to get
 * a reservation again.
 *
 * While mutexes are explicitly 4 bytes, a 'reservation' applies to an
 * entire cache line, normally 32 bytes, aligned naturally.  If the mutex
 * lives near data that gets changed a lot, there's a chance that you'll
 * see more broken reservations than you might otherwise.  The only
 * situation in which this might be a problem is if one processor is
 * beating on a variable in the same cache block as the mutex while another
 * processor tries to acquire the mutex.  That's bad news regardless
 * because of the way it bashes caches, but if you can't guarantee that a
 * mutex will reside in a relatively quiescent cache line, you might
 * consider padding the mutex to force it to live in a cache line by
 * itself.  No, you aren't guaranteed that cache lines are 32 bytes.  Some
 * embedded processors use 16-byte cache lines, while some 64-bit
 * processors use 128-bit cache lines.  But assuming a 32-byte cache line
 * won't get you into trouble for now.
 *
 * If mutex locking is a bottleneck, then you can speed it up by adding a
 * regular LWZ load before the LWARX load, so that you can test for the
 * common case of a locked mutex without wasting cycles making a reservation.
 *
 * 'set' mutexes have the value 1, like on Intel; the returned value from
 * MUTEX_SET() is 1 if the mutex previously had its low bit set, 0 otherwise.
 */
#define	MUTEX_SET(tsl)	({		\
	int __one = 1;			\
	int __r;			\
	tsl_t *__l = (tsl);		\
	asm volatile ("			\
0:					\
	lwarx %0,0,%1;			\
	cmpwi %0,0;			\
	bne 1f;				\
	stwcx. %2,0,%1;			\
	bne- 0b;			\
1:"					\
	: "=&r" (__r)			\
	: "r" (__l), "r" (__one));	\
	__r & 1;			\
})

#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * SCO/cc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_SCO_X86_CC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * UnixWare has threads in libthread, but OpenServer doesn't (yet).
 *
 * For cc/x86, 0 is clear, 1 is set.
 */

#if defined(__USLC__)
asm int
_tsl_set(void *tsl)
{
%mem tsl
	movl	tsl, %ecx
	movl	$1, %eax
	lock
	xchgb	(%ecx),%al
	xorl	$1,%eax
}
#endif

#define	MUTEX_SET(tsl)		_tsl_set(tsl)
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * Sparc/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_SPARC_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 *
 * The ldstub instruction takes the location specified by its first argument
 * (a register containing a memory address) and loads its contents into its
 * second argument (a register) and atomically sets the contents the location
 * specified by its first argument to a byte of 1s.  (The value in the second
 * argument is never read, but only overwritten.)
 *
 * The stbar is needed for v8, and is implemented as membar #sync on v9,
 + so is functional there as well.  For v7, stbar may generate an illegal
 + instruction and we have no way to tell what we're running on.  Some
 + operating systems notice and skip this instruction in the fault handler.
 *
 * For gcc/sparc, 0 is clear, 1 is set.
 */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	register tsl_t __r;						\
	__asm__ volatile						\
	    ("ldstub [%1],%0; stbar"					\
	    : "=r"( __r) : "r" (__l));					\
	!__r;								\
})

#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * UTS/cc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_UTS_CC_ASSEMBLY
typedef int tsl_t;

#define	MUTEX_ALIGN	sizeof(int)
#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	0
#define	MUTEX_SET(x)	(!uts_lock(x, 1))
#define	MUTEX_UNSET(x)	(*(x) = 0)
#endif
#endif

/*********************************************************************
 * x86/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_X86_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * For gcc/x86, 0 is clear, 1 is set.
 */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	int __r;							\
	asm volatile("movl $1,%%eax; lock; xchgb %1,%%al; xorl $1,%%eax"\
	    : "=&a" (__r), "=m" (*__l)					\
	    : "1" (*__l)						\
	    );								\
	__r & 1;							\
})

#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*
 * Mutex alignment defaults to one byte.
 *
 * !!!
 * Various systems require different alignments for mutexes (the worst we've
 * seen so far is 16-bytes on some HP architectures).  Malloc(3) is assumed
 * to return reasonable alignment, all other mutex users must ensure proper
 * alignment locally.
 */
#ifndef	MUTEX_ALIGN
#define	MUTEX_ALIGN	1
#endif

/*
 * Mutex destruction defaults to a no-op.
 */
#ifdef LOAD_ACTUAL_MUTEX_CODE
#ifndef	MUTEX_DESTROY
#define	MUTEX_DESTROY(x)
#endif
#endif

#define	MUTEX_IGNORE		0x001	/* Ignore, no lock required. */
#define	MUTEX_INITED		0x002	/* Mutex is successfully initialized */
#define	MUTEX_SELF_BLOCK	0x004	/* Must block self. */
#define	MUTEX_THREAD		0x008	/* Thread-only mutex. */

/* Mutex. */
struct __mutex_t {
#ifdef	HAVE_MUTEX_THREADS
#ifdef	MUTEX_FIELDS
	MUTEX_FIELDS
#else
	tsl_t	tas;			/* Test and set. */
#endif
	u_int32_t spins;		/* Spins before block. */
	u_int32_t locked;		/* !0 if locked. */
#else
	u_int32_t off;			/* Byte offset to lock. */
	u_int32_t pid;			/* Lock holder: 0 or process pid. */
#endif
	u_int32_t mutex_set_wait;	/* Granted after wait. */
	u_int32_t mutex_set_nowait;	/* Granted without waiting. */
#ifdef MUTEX_SYSTEM_RESOURCES
	roff_t	  reg_off;		/* Shared lock info offset. */
#endif

	u_int8_t  flags;		/* MUTEX_XXX */
};

/* Redirect calls to the correct functions. */
#ifdef HAVE_MUTEX_THREADS
#if defined(HAVE_MUTEX_PTHREADS) || defined(HAVE_MUTEX_SOLARIS_LWP) || defined(HAVE_MUTEX_UI_THREADS)
#define	__db_mutex_init(a, b, c, d)	__db_pthread_mutex_init(a, b, d)
#define	__db_mutex_lock(a, b, c)	__db_pthread_mutex_lock(a, b)
#define	__db_mutex_unlock(a, b)		__db_pthread_mutex_unlock(a, b)
#define	__db_mutex_destroy(a)		__db_pthread_mutex_destroy(a)
#else
#define	__db_mutex_init(a, b, c, d)	__db_tas_mutex_init(a, b, d)
#define	__db_mutex_lock(a, b, c)	__db_tas_mutex_lock(a, b)
#define	__db_mutex_unlock(a, b)		__db_tas_mutex_unlock(a, b)
#define	__db_mutex_destroy(a)		__db_tas_mutex_destroy(a)
#endif
#else
#define	__db_mutex_init(a, b, c, d)	__db_fcntl_mutex_init(a, b, c)
#define	__db_mutex_lock(a, b, c)	__db_fcntl_mutex_lock(a, b, c)
#define	__db_mutex_unlock(a, b)		__db_fcntl_mutex_unlock(a, b)
#define	__db_mutex_destroy(a)		__db_fcntl_mutex_destroy(a)
#endif

/* Redirect system resource calls to correct functions */
#ifdef MUTEX_SYSTEM_RESOURCES
#define	__db_maintinit(a, b, c)		__db_shreg_maintinit(a, b, c)
#define	__db_shlocks_clear(a, b, c)	__db_shreg_locks_clear(a, b, c)
#define	__db_shlocks_destroy(a, b)	__db_shreg_locks_destroy(a, b)
#define	__db_shmutex_init(a, b, c, d, e, f)	\
    __db_shreg_mutex_init(a, b, c, d, e, f)
#else
#define	__db_maintinit(a, b, c)
#define	__db_shlocks_clear(a, b, c)
#define	__db_shlocks_destroy(a, b)
#define	__db_shmutex_init(a, b, c, d, e, f)	__db_mutex_init(a, b, c, d)
#endif

/*
 * Lock/unlock a mutex.  If the mutex was marked as uninteresting, the thread
 * of control can proceed without it.
 *
 * If the lock is for threads-only, then it was optionally not allocated and
 * file handles aren't necessary, as threaded applications aren't supported by
 * fcntl(2) locking.
 */
#ifdef DIAGNOSTIC
	/*
	 * XXX
	 * We want to switch threads as often as possible.  Yield every time
	 * we get a mutex to ensure contention.
	 */
#define	MUTEX_LOCK(dbenv, mp, fh)					\
	if (!F_ISSET((MUTEX *)(mp), MUTEX_IGNORE))			\
		(void)__db_mutex_lock(dbenv, mp, fh);			\
	if (DB_GLOBAL(db_pageyield))					\
		__os_yield(NULL, 1);
#else
#define	MUTEX_LOCK(dbenv, mp, fh)					\
	if (!F_ISSET((MUTEX *)(mp), MUTEX_IGNORE))			\
		(void)__db_mutex_lock(dbenv, mp, fh);
#endif
#define	MUTEX_UNLOCK(dbenv, mp)						\
	if (!F_ISSET((MUTEX *)(mp), MUTEX_IGNORE))			\
		(void)__db_mutex_unlock(dbenv, mp);
#define	MUTEX_THREAD_LOCK(dbenv, mp)					\
	if (mp != NULL)							\
		MUTEX_LOCK(dbenv, mp, NULL)
#define	MUTEX_THREAD_UNLOCK(dbenv, mp)					\
	if (mp != NULL)							\
		MUTEX_UNLOCK(dbenv, mp)

/*
 * We use a single file descriptor for fcntl(2) locking, and (generally) the
 * object's offset in a shared region as the byte that we're locking.  So,
 * there's a (remote) possibility that two objects might have the same offsets
 * such that the locks could conflict, resulting in deadlock.  To avoid this
 * possibility, we offset the region offset by a small integer value, using a
 * different offset for each subsystem's locks.  Since all region objects are
 * suitably aligned, the offset guarantees that we don't collide with another
 * region's objects.
 */
#define	DB_FCNTL_OFF_GEN	0		/* Everything else. */
#define	DB_FCNTL_OFF_LOCK	1		/* Lock subsystem offset. */
#define	DB_FCNTL_OFF_MPOOL	2		/* Mpool subsystem offset. */
