/* ==== machdep.h ============================================================
 *  Copyright (c) 1993 John F. Carr, jfc@athena.mit.edu
 *
 *  Description : Machine dependent header for IBM/RT
 *
 *  1.00 93/09/xx jfc
 *      -Coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <setjmp.h>
#include <sys/time.h>
#include <sys/types.h>

/*
 * Stuff for compiling
 */
#if defined(__GNUC__)
#if defined(__cplusplus)
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS     };
#else
#define __BEGIN_DECLS
#define __END_DECLS
#if !defined(__STDC__)
#define const           __const
#define inline          __inline
#define signed          __signed
#define volatile        __volatile
#endif
#endif
#else /* !__GNUC__ */
#define __BEGIN_DECLS
#define __END_DECLS
#define const
#define inline
#define signed
#define volatile
#endif

#define SEMAPHORE_CLEAR 0x0000
#define SEMAPHORE_SET   0xff00
#define SEMAPHORE_TEST_AND_SET(lock) _tsh(lock)
#define SEMAPHORE_RESET(lock) *(lock) = SEMAPHORE_CLEAR
extern unsigned short _tsh(volatile unsigned short *);

typedef unsigned short semaphore;

/*
 * sigset_t macros
 */
#define	SIG_ANY(sig)		(sig)
#define SIGMAX 				31


struct machdep_pthread {
    void        		*(*start_routine)(void *);
    void        		*start_argument;
    void        		*machdep_stack;
    struct itimerval	machdep_timer;
    jmp_buf     		machdep_state;
};

/*
 * Static machdep_pthread initialization values.
 * For initial thread only.
 */
#define MACHDEP_PTHREAD_INIT    \
{ NULL, NULL, NULL, { { 0, 0 }, { 0, 100000 } }, 0 }

/*
 * Min pthread stacksize
 */
#define PTHREAD_STACK_MIN	1024

/*
 * Some fd defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK 		O_NONBLOCK 

#if defined(PTHREAD_KERNEL)

int machdep_save_state      __P_((void));

/* save(jmp_buf, stack pointer, restart proc) */
extern int _pthread_save(jmp_buf, void *, void (*)());
extern void _pthread_restore(jmp_buf);

typedef int ssize_t;
typedef unsigned int sigset_t;
#define sigemptyset(sp) *(sp) = 0
#define sigprocmask(op, nssp, ossp) if (ossp) *(int *)ossp = sigsetmask(*nssp); else sigsetmask(*nssp)
#define sigdelset(sp, i) *(sp) &= ~(1 << (i))
#define sigaddset(sp, i) *(sp) |= (1 << (i))
#define sigismember(sp, i) (*(sp) & (1 << (i)))
#endif
