/* ==== machdep.h ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>

/*
 * Stuff for compiling
 */
#if defined(__GNUC__)
#if defined(__cplusplus)
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS             };
#else
#define __BEGIN_DECLS
#define __END_DECLS
#if !defined(__STDC__)
#define const           __const
#define inline          __inline
#define signed          __signed
#define volatile                __volatile
#endif
#endif
#else /* !__GNUC__ */
#define __BEGIN_DECLS
#define __END_DECLS
#if !defined(__STDC__) 
#define const
#endif
#define inline
#define signed
#define volatile
#endif

/*
 * The first machine dependent functions are the SEMAPHORES
 * needing the test and set instruction.
 *
 * Note: The set and clear defines are backwards.
 */
#define SEMAPHORE_CLEAR { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 		\
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 	\
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
#define SEMAPHORE_SET   0

#define SEMAPHORE_TEST_AND_SET(lock)    			\
({													\
long real_addr;										\
long temp;							    			\
													\
real_addr = ((long)((*lock) + 15) & ~15);			\
													\
__asm__ volatile("ldcwx %%r0(%2),%0"				\
        :"=r" (temp)                    			\
        :"0" (temp),"r" (real_addr));   			\
temp ? 0 : 1;                              			\
})

#define SEMAPHORE_RESET(lock)           			\
({													\
char *real_addr;									\
													\
real_addr = (char*)((long)((*lock) + 15) & ~15);	\
*real_addr = 0xff;									\
})

/*
 * New types
 * The semaphore is really 16 bytes but must be aligened on a 16 byte
 * boundary. By specifing 31 bytes the macros can frob it correctly.
 */
typedef char semaphore[31];

/*
 * Macros for sigset_t
 */
#define SIGMAX	30
/* see hpux-9.03/__signal.h for SIG_ANY */

/*
 * New Strutures
 */
struct machdep_pthread {
    void        		*(*start_routine)(void *);
    void        		*start_argument;
    void        		*machdep_stack;
    struct itimerval		machdep_timer;
    jmp_buf			machdep_state;
 /*   long	     		machdep_state[_JBLEN]; */
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
#define PTHREAD_STACK_MIN	4096

/*
 * Some fd flag defines that are necessary to distinguish between posix
 * behavior and bsd4.3 behavior.
 */
#define __FD_NONBLOCK 		O_NONBLOCK

/*
 * page size
 */
#define getpagesize()		4096

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
