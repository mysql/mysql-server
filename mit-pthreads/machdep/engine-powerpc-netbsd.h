/* ==== machdep.h ============================================================
 * Copyright (c) 1994 Chris Provenzano (proven@athena.mit.edu) and
 * Ken Raeburn (raeburn@mit.edu).
 *
 * engine-alpha-osf1.h,v 1.4.4.1 1995/12/13 05:41:42 proven Exp
 *
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/cdefs.h>
#include <sys/signal.h>  /* for _NSIG */

/*
 * The first machine dependent functions are the SEMAPHORES
 * needing the test and set instruction.
 */
#define SEMAPHORE_CLEAR 0
#define SEMAPHORE_SET   0xffff

#define SEMAPHORE_TEST_AND_SET(lock)            \
({                                              \
        volatile long t1, temp = SEMAPHORE_SET; \
        __asm__ volatile(                       \
	  "1: lwarx  %0,0,%1;			\
	      cmpwi  %0, 0;                     \
	      bne    2f;                        \
	      stwcx. %2,0,%1;                   \
	      bne-   1b;                        \
	   2: "                                 \
          :"=r" (t1)                            \
          :"m" (lock), "r" (temp));             \
        t1;                                     \
})

#define SEMAPHORE_RESET(lock)           *lock = SEMAPHORE_CLEAR

/*
 * New types
 */
typedef int semaphore;

/*
 * sigset_t macros
 */
#define        SIG_ANY(sig)            (sig)
#define        SIGMAX                  (_NSIG-1)

/*
 * New Strutures
 */
struct machdep_pthread {
    void                       *(*start_routine)(void *);
    void                       *start_argument;
    void                       *machdep_stack;
    struct itimerval           machdep_timer;
    jmp_buf	               machdep_istate;
    unsigned long              machdep_fstate[66];
				/* 64-bit fp regs 0-31 + fpscr */
				/* We pretend the fpscr is 64 bits */
};

/*
 * Static machdep_pthread initialization values.
 * For initial thread only.
 */
#define MACHDEP_PTHREAD_INIT    \
       { NULL, NULL, NULL, { { 0, 0 }, { 0, 100000 } }, { 0 }, { 0 } }

/*
 * Minimum stack size
 */
#define PTHREAD_STACK_MIN      2048

/*
 * Some fd flag defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK          O_NONBLOCK

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)

#define __machdep_stack_get(x)      (x)->machdep_stack
#define __machdep_stack_set(x, y)   (x)->machdep_stack = y
#define __machdep_stack_repl(x, y)                          \
{                                                           \
    if ((stack = __machdep_stack_get(x))) {                 \
        __machdep_stack_free(stack);                        \
    }                                                       \
    __machdep_stack_set(x, y);                              \
}

int machdep_save_state(void);

void __machdep_save_fp_state(unsigned long *);
void __machdep_restore_fp_state(unsigned long *);
void *__machdep_stack_alloc(size_t);
void __machdep_stack_free(void *);

#endif

__END_DECLS
