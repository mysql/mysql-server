/* ==== machdep.h ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 * $Id$
 *
 * m68k work by Andy Finnell <andyf@vei.net> based off work by
 *  David Leonard and Chris Provenzano.
 *
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>

/*
 * The first machine dependent functions are the SEMAPHORES
 * needing the test and set instruction.
 */
#define SEMAPHORE_CLEAR 0
#define SEMAPHORE_SET   0x80;

#define SEMAPHORE_TEST_AND_SET(lock)            \
({                                              \
        volatile long temp = SEMAPHORE_CLEAR;   \
        __asm__ volatile(                       \
          "tas %2; bpl 0f; movl #1,%0; 0:"      \
          :"=r" (temp)                          \
          :"0" (temp),"m" (*lock));             \
        temp;                                   \
})

#define SEMAPHORE_RESET(lock)           *lock = SEMAPHORE_CLEAR

/*
 * New types
 */
typedef char    semaphore;

/*
 * sigset_t macros
 */
#define	SIG_ANY(sig)		(sig)
#define SIGMAX				31

/*
 * New Strutures
 */
struct machdep_pthread {
    void        		*(*start_routine)(void *);
    void        		*start_argument;
    void        		*machdep_stack;
	struct itimerval	machdep_timer;
    jmp_buf     		machdep_state;
    char			machdep_fstate[92];
};

/*
 * Min pthread stacksize
 */
#define PTHREAD_STACK_MIN	1024

/*
 * Some fd flag defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK 		O_NONBLOCK

/*
 * Static machdep_pthread initialization values.
 * For initial thread only.
 */
#define MACHDEP_PTHREAD_INIT    \
{ NULL, NULL, NULL, { { 0, 0 }, { 0, 100000 } }, 0 }

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)


#ifndef __machdep_stack_get
#define __machdep_stack_get(x)      (x)->machdep_stack
#endif
#ifndef __machdep_stack_set
#define __machdep_stack_set(x, y)   (x)->machdep_stack = y
#endif
#ifndef __machdep_stack_repl
#define __machdep_stack_repl(x, y)                          \
{                                                           \
    if (stack = __machdep_stack_get(x)) {                   \
        __machdep_stack_free(stack);                        \
    }                                                       \
    __machdep_stack_set(x, y);                              \
}
#endif

void *  __machdep_stack_alloc       __P_((size_t));
void    __machdep_stack_free        __P_((void *));
    
int machdep_save_state      __P_((void));

#endif

__END_DECLS
