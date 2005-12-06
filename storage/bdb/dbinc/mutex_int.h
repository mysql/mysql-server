/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mutex_int.h,v 12.17 2005/11/08 22:26:49 mjc Exp $
 */

#ifndef _DB_MUTEX_INT_H_
#define	_DB_MUTEX_INT_H_

/*********************************************************************
 * POSIX.1 pthreads interface.
 *********************************************************************/
#ifdef HAVE_MUTEX_PTHREADS
#include <pthread.h>

#define	MUTEX_FIELDS							\
	pthread_mutex_t mutex;		/* Mutex. */			\
	pthread_cond_t  cond;		/* Condition variable. */
#endif

#ifdef HAVE_MUTEX_UI_THREADS
#include <thread.h>
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

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	0
#define	MUTEX_SET(x)	(!_check_lock(x, 0, 1))
#define	MUTEX_UNSET(x)	_clear_lock(x, 0)
#endif
#endif

/*********************************************************************
 * Apple/Darwin library functions.
 *********************************************************************/
#ifdef HAVE_MUTEX_DARWIN_SPIN_LOCK_TRY
typedef u_int32_t tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
extern int _spin_lock_try(tsl_t *);
extern void _spin_unlock(tsl_t *);
#define	MUTEX_SET(tsl)          _spin_lock_try(tsl)
#define	MUTEX_UNSET(tsl)        _spin_unlock(tsl)
#define	MUTEX_INIT(tsl)         (MUTEX_UNSET(tsl), 0)
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
#define	MUTEX_ALIGN	16
#endif

#if defined(HAVE_MUTEX_MSEM_INIT) || defined(HAVE_MUTEX_HPPA_MSEM_INIT)
#include <sys/mman.h>
typedef msemaphore tsl_t;

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
typedef volatile unsigned char tsl_t;

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
#include "taskLib.h"
typedef SEM_ID tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_SET(tsl)		(semTake((*tsl), WAIT_FOREVER) == OK)
#define	MUTEX_UNSET(tsl)	(semGive((*tsl)))
#define	MUTEX_INIT(tsl)							\
	((*(tsl) = semBCreate(SEM_Q_FIFO, SEM_FULL)) == NULL)
#define	MUTEX_DESTROY(tsl)	semDelete(*tsl)
#endif
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

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)		0
#define	MUTEX_SET(tsl)		(*(tsl) = 1)
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#endif
#endif

/*********************************************************************
 * Win32
 *********************************************************************/
#if defined(HAVE_MUTEX_WIN32) || defined(HAVE_MUTEX_WIN32_GCC)
#define	MUTEX_FIELDS							\
	LONG volatile tas;						\
	LONG nwaiters;							\
	u_int32_t id;	/* ID used for creating events */		\

#if defined(LOAD_ACTUAL_MUTEX_CODE)
#define	MUTEX_SET(tsl)		(!InterlockedExchange((PLONG)tsl, 1))
#define	MUTEX_UNSET(tsl)	InterlockedExchange((PLONG)tsl, 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)

/*
 * From Intel's performance tuning documentation (and see SR #6975):
 * ftp://download.intel.com/design/perftool/cbts/appnotes/sse2/w_spinlock.pdf
 *
 * "For this reason, it is highly recommended that you insert the PAUSE
 * instruction into all spin-wait code immediately. Using the PAUSE
 * instruction does not affect the correctness of programs on existing
 * platforms, and it improves performance on Pentium 4 processor platforms."
 */
#ifdef HAVE_MUTEX_WIN32
#ifndef _WIN64
#define	MUTEX_PAUSE		{__asm{_emit 0xf3}; __asm{_emit 0x90}}
#endif
#endif
#ifdef HAVE_MUTEX_WIN32_GCC
#define	MUTEX_PAUSE		asm volatile ("rep; nop" : : );
#endif
#endif
#endif

/*********************************************************************
 * 68K/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_68K_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/* gcc/68K: 0 is clear, 1 is set. */
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

#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * Tru64/cc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_TRU64_CC_ASSEMBLY
typedef volatile u_int32_t tsl_t;

#define	MUTEX_ALIGN	4

#ifdef LOAD_ACTUAL_MUTEX_CODE
#include <alpha/builtins.h>
#define	MUTEX_SET(tsl)		(__LOCK_LONG_RETRY((tsl), 1) != 0)
#define	MUTEX_UNSET(tsl)	(__UNLOCK_LONG(tsl))

#define	MUTEX_INIT(tsl)		(MUTEX_UNSET(tsl), 0)
#endif
#endif

/*********************************************************************
 * ARM/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_ARM_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/* gcc/arm: 0 is clear, 1 is set. */
#define	MUTEX_SET(tsl) ({						\
	int __r;							\
	asm volatile(							\
		"swpb	%0, %1, [%2]\n\t"				\
		"eor	%0, %0, #1\n\t"					\
	    : "=&r" (__r)						\
	    : "r" (1), "r" (tsl)					\
	    );								\
	__r & 1;							\
})

#define	MUTEX_UNSET(tsl)	(*(volatile tsl_t *)(tsl) = 0)
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

#define	MUTEX_UNSET(tsl)        (*(volatile tsl_t *)(tsl) = -1)
#define	MUTEX_INIT(tsl)		(MUTEX_UNSET(tsl), 0)
#endif
#endif

/*********************************************************************
 * IA64/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_IA64_GCC_ASSEMBLY
typedef volatile unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/* gcc/ia64: 0 is clear, 1 is set. */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	long __r;							\
	asm volatile("xchg1 %0=%1,%2" :					\
		     "=r"(__r), "+m"(*__l) : "r"(1));			\
	__r ^ 1;							\
})

/*
 * Store through a "volatile" pointer so we get a store with "release"
 * semantics.
 */
#define	MUTEX_UNSET(tsl)	(*(tsl_t *)(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * PowerPC/gcc assembly.
 *********************************************************************/
#if defined(HAVE_MUTEX_PPC_GCC_ASSEMBLY)
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
 * gcc/ppc: 0 is clear, 1 is set.
 */
static inline int
MUTEX_SET(int *tsl)  {
        int __r;
        asm volatile (
"0:                             \n\t"
"       lwarx   %0,0,%1         \n\t"
"       cmpwi   %0,0            \n\t"
"       bne-    1f              \n\t"
"       stwcx.  %1,0,%1         \n\t"
"       isync                   \n\t"
"       beq+    2f              \n\t"
"       b       0b              \n\t"
"1:                             \n\t"
"       li      %1,0            \n\t"
"2:                             \n\t"
         : "=&r" (__r), "+r" (tsl)
         :
         : "cr0", "memory");
         return (int)tsl;
}

static inline int
MUTEX_UNSET(tsl_t *tsl) {
         asm volatile("sync" : : : "memory");
         return *tsl = 0;
}
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * OS/390 C
 *********************************************************************/
#ifdef HAVE_MUTEX_S390_CC_ASSEMBLY
typedef int tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * cs() is declared in <stdlib.h> but is built in to the compiler.
 * Must use LANGLVL(EXTENDED) to get its declaration.
 */
#define	MUTEX_SET(tsl)		(!cs(&zero, (tsl), 1))
#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * S/390 32-bit assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_S390_GCC_ASSEMBLY
typedef int tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/* gcc/S390: 0 is clear, 1 is set. */
static inline int
MUTEX_SET(tsl_t *tsl) {							\
	register tsl_t *__l = (tsl);					\
	int __r;							\
  asm volatile(								\
       "    la    1,%1\n"						\
       "    lhi   0,1\n"						\
       "    l     %0,%1\n"						\
       "0:  cs    %0,0,0(1)\n"						\
       "    jl    0b"							\
       : "=&d" (__r), "+m" (*__l)					\
       : : "0", "1", "cc");						\
	return !__r;							\
}

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
 * cc/x86: 0 is clear, 1 is set.
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
 * so is functional there as well.  For v7, stbar may generate an illegal
 * instruction and we have no way to tell what we're running on.  Some
 * operating systems notice and skip this instruction in the fault handler.
 *
 * gcc/sparc: 0 is clear, 1 is set.
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

#ifdef LOAD_ACTUAL_MUTEX_CODE
#define	MUTEX_INIT(x)	0
#define	MUTEX_SET(x)	(!uts_lock(x, 1))
#define	MUTEX_UNSET(x)	(*(x) = 0)
#endif
#endif

/*********************************************************************
 * MIPS/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_MIPS_GCC_ASSEMBLY
typedef u_int32_t tsl_t;

#define MUTEX_ALIGN	4

#ifdef LOAD_ACTUAL_MUTEX_CODE
/*
 * For gcc/MIPS.  Should return 0 if could not acquire the lock, 1 if
 * lock was acquired properly.
 */
static inline int
MUTEX_SET(tsl_t *tsl) {
       register tsl_t *__l = tsl;
       register tsl_t __r;
       __asm__ __volatile__(
               "       .set push           \n"
               "       .set mips2          \n"
               "       .set noreorder      \n"
               "       .set nomacro        \n"
               "1:     ll      %0,%1       \n"
               "       bne     %0,$0,1f    \n"
               "       xori    %0,%0,1     \n"
               "       sc      %0,%1       \n"
               "       beql    %0,$0,1b    \n"
               "       xori    %0,1        \n"
               "1:     .set pop              "
               : "=&r" (__r), "+R" (*__l));
       return __r;
}

#define	MUTEX_UNSET(tsl)        (*(volatile tsl_t *)(tsl) = 0)
#define	MUTEX_INIT(tsl)         MUTEX_UNSET(tsl)
#endif
#endif

/*********************************************************************
 * x86/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_X86_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/* gcc/x86: 0 is clear, 1 is set. */
#define	MUTEX_SET(tsl) ({						\
	register tsl_t *__l = (tsl);					\
	int __r;							\
	asm volatile("movl $1,%%eax\n"					\
		     "lock\n"						\
		     "xchgb %1,%%al\n"					\
		     "xorl $1,%%eax"					\
	    : "=&a" (__r), "=m" (*__l)					\
	    : "m1" (*__l)						\
	    );								\
	__r & 1;							\
})

#define	MUTEX_UNSET(tsl)        (*(volatile tsl_t *)(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)

/*
 * From Intel's performance tuning documentation (and see SR #6975):
 * ftp://download.intel.com/design/perftool/cbts/appnotes/sse2/w_spinlock.pdf
 *
 * "For this reason, it is highly recommended that you insert the PAUSE
 * instruction into all spin-wait code immediately. Using the PAUSE
 * instruction does not affect the correctness of programs on existing
 * platforms, and it improves performance on Pentium 4 processor platforms."
 */
#define	MUTEX_PAUSE		asm volatile ("rep; nop" : : );
#endif
#endif

/*********************************************************************
 * x86_64/gcc assembly.
 *********************************************************************/
#ifdef HAVE_MUTEX_X86_64_GCC_ASSEMBLY
typedef unsigned char tsl_t;

#ifdef LOAD_ACTUAL_MUTEX_CODE
/* gcc/x86_64: 0 is clear, 1 is set. */
#define  MUTEX_SET(tsl) ({           					\
	register tsl_t *__l = (tsl);          				\
	int __r;              						\
	asm volatile("mov $1,%%rax\n"					\
		     "lock\n"						\
		     "xchgb %1,%%al\n"					\
		     "xor $1,%%rax"					\
		: "=&a" (__r), "=m" (*__l)				\
		: "1m" (*__l)						\
		);							\
	__r & 1;							\
})

#define	MUTEX_UNSET(tsl)	(*(tsl) = 0)
#define	MUTEX_INIT(tsl)		MUTEX_UNSET(tsl)
#endif
#endif

/*
 * Mutex alignment defaults to sizeof(unsigned int).
 *
 * !!!
 * Various systems require different alignments for mutexes (the worst we've
 * seen so far is 16-bytes on some HP architectures).  Malloc(3) is assumed
 * to return reasonable alignment, all other mutex users must ensure proper
 * alignment locally.
 */
#ifndef	MUTEX_ALIGN
#define	MUTEX_ALIGN	sizeof(unsigned int)
#endif

/*
 * Mutex destruction defaults to a no-op.
 */
#ifndef	MUTEX_DESTROY
#define	MUTEX_DESTROY(x)
#endif

/*
 * DB_MUTEXMGR --
 *	The mutex manager encapsulates the mutex system.
 */
typedef struct __db_mutexmgr {
	/* These fields are never updated after creation, so not protected. */
	DB_ENV	*dbenv;			/* Environment */
	REGINFO	 reginfo;		/* Region information */

	void	*mutex_array;		/* Base of the mutex array */
} DB_MUTEXMGR;

/* Macros to lock/unlock the mutex region as a whole. */
#define	MUTEX_SYSTEM_LOCK(dbenv)					\
	MUTEX_LOCK(dbenv, ((DB_MUTEXREGION *)((DB_MUTEXMGR *)		\
	    (dbenv)->mutex_handle)->reginfo.primary)->mtx_region)
#define	MUTEX_SYSTEM_UNLOCK(dbenv)					\
	MUTEX_UNLOCK(dbenv, ((DB_MUTEXREGION *)((DB_MUTEXMGR *)		\
	    (dbenv)->mutex_handle)->reginfo.primary)->mtx_region)

/*
 * DB_MUTEXREGION --
 *	The primary mutex data structure in the shared memory region.
 */
typedef struct __db_mutexregion {
	/* These fields are initialized at create time and never modified. */
	roff_t		mutex_offset;	/* Offset of mutex array */
	size_t		mutex_size;	/* Size of the aligned mutex */
	roff_t		thread_off;	/* Offset of the thread area. */

	db_mutex_t	mtx_region;	/* Region mutex. */

	/* Protected using the region mutex. */
	u_int32_t	mutex_next;	/* Next free mutex */

	DB_MUTEX_STAT	stat;		/* Mutex statistics */
} DB_MUTEXREGION;

typedef struct __mutex_t {		/* Mutex. */
#ifdef MUTEX_FIELDS
	MUTEX_FIELDS
#endif
#if !defined(MUTEX_FIELDS) && !defined(HAVE_MUTEX_FCNTL)
	tsl_t		tas;		/* Test and set. */
#endif
	pid_t 		pid;		/* Process owning mutex */
	db_threadid_t 	tid;		/* Thread owning mutex */

	u_int32_t mutex_next_link;	/* Linked list of free mutexes. */

#ifdef HAVE_STATISTICS
	int	  alloc_id;		/* Allocation ID. */

	u_int32_t mutex_set_wait;	/* Granted after wait. */
	u_int32_t mutex_set_nowait;	/* Granted without waiting. */
#endif

	/*
	 * A subset of the flag arguments for __mutex_alloc().
	 *
	 * Flags should be an unsigned integer even if it's not required by
	 * the possible flags values, getting a single byte on some machines
	 * is expensive, and the mutex structure is a MP hot spot.
	 */
	u_int32_t flags;		/* MUTEX_XXX */
} DB_MUTEX;

/* Macro to get a reference to a specific mutex. */
#define	MUTEXP_SET(indx)						\
	(DB_MUTEX *)							\
	    ((u_int8_t *)mtxmgr->mutex_array + (indx) * mtxregion->mutex_size);

#endif /* !_DB_MUTEX_INT_H_ */
