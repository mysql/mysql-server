/* ==== machdep.h ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 * $Id$
 *
 *  Description : Machine dependent header for decstation with r2000/r3000
 *				  running Ultrix-4.2
 *
 *  1.00 93/07/21 proven
 *      -Started coding this file.
 */

#include <setjmp.h>
#include <sys/time.h>
#include <sys/cdefs.h>

/*
 * The first machine dependent functions are the SEMAPHORES
 * needing the test and set instruction.
 */
#define SEMAPHORE_CLEAR 0
#define SEMAPHORE_SET   1

#define SEMAPHORE_TEST_AND_SET(lock)	semaphore_test_and_set(lock)
#define SEMAPHORE_RESET(lock)			*lock = SEMAPHORE_CLEAR

/*
 * New types
 */
typedef long	semaphore;

#if !defined(_POSIX_SOURCE)

/* typedef int		ssize_t; */

#if !defined(__GNUC__)

/*
 * sigset_t macros
 */
typedef int		sigset_t; 
#define sigaddset(set, num)		((*set) |= (1 << (num - 1)))
#define sigemptyset(set)		(*set = 0) 

#endif
#endif

#define	SIG_ANY(sig)			(sig)
#define SIGMAX 					31

/*
 * New Structures
 */
struct machdep_pthread {
	void						*(*start_routine)(void *);
	void						*start_argument;
	void						*machdep_stack;
	struct itimerval			machdep_timer;
	jmp_buf						machdep_state;
};

/*
 * Static machdep_pthread initialization values.
 * For initial thread only.
 */
#define MACHDEP_PTHREAD_INIT	\
{ NULL, NULL, NULL, { { 0, 0 }, { 0, 100000 } }, 0 }


/*
 * Min stacksize, arch dependent
 */
#define PTHREAD_STACK_MIN	1024

/*
 * Some fd flag defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK 		(O_NONBLOCK | O_NDELAY)

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)

#define __machdep_stack_get(x)		(x)->machdep_stack
#define __machdep_stack_set(x, y)	(x)->machdep_stack = y
#define __machdep_stack_repl(x, y)							\
{															\
	if (stack = __machdep_stack_get(x)) {					\
		__machdep_stack_free(stack);						\
	}														\
	__machdep_stack_set(x, y);								\
}

void *	__machdep_stack_alloc		__P_((size_t));
void	__machdep_stack_free		__P_((void *));

int	semaphore_test_and_set			__P_((semaphore *));
int machdep_save_state				__P_((void));

#endif

__END_DECLS
