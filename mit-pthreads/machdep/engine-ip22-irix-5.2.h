/* ==== machdep.h ============================================================
 * Copyright (c) 1994 Chris Provenzano, proven@athena.mit.edu
 *
 * $Id$
 *
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/cdefs.h>

/*
 * The first machine dependent functions are the SEMAPHORES
 * needing the test and set instruction.
 */
#define SEMAPHORE_CLEAR 0
#define SEMAPHORE_SET   0xff

/*
 * More machine dependent macros
 */
#ifdef PTHREAD_KERNEL

#define machdep_save_float_state(x)		
#define machdep_restore_float_state()

#endif

/*
 * New types
 */
typedef char    semaphore;

/*
 * sigset_t macros
 */
#define SIGMAX				32
#define SIG_ANY(sig)                        \
({                                          \
    sigset_t *sig_addr = (sigset_t *)&sig;  \
    int ret = 0;                            \
    int i;                                  \
                                            \
    for (i = 1; i <= SIGMAX; i++) {         \
        if (sigismember(sig_addr, i)) {     \
            ret = 1;                        \
            break;                          \
        }                                   \
    }                                       \
    ret;                                    \
})

/*
 * New Strutures
 */
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
 * Minimum stack size
 */
#define PTHREAD_STACK_MIN	1024

/*
 * Some fd flag defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK 		O_NONBLOCK

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)

#define __machdep_stack_get(x)      (x)->machdep_stack
#define __machdep_stack_set(x, y)   (x)->machdep_stack = y
#define __machdep_stack_repl(x, y)                          \
{                                                           \
    if (stack = __machdep_stack_get(x)) {                   \
        __machdep_stack_free(stack);                        \
    }                                                       \
    __machdep_stack_set(x, y);                              \
}

void *  __machdep_stack_alloc       __P_((size_t));
void    __machdep_stack_free        __P_((void *));

int machdep_save_state      __P_((void));

#endif

__END_DECLS
