/* ==== pthread.h ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * $Id$
 *
 * Description : Basic pthread header.
 *
 *  1.00 93/07/20 proven
 *      -Started coding this file.
 *
 *  93/9/28 streepy - Added support for pthread cancel
 *	
 */
 
#ifndef _PTHREAD_H_
#define	_PTHREAD_H_

#include <pthread/types.h>

#include <pthread/version.h>
#include <pthread/machdep.h>
#include <pthread/cleanup.h>
#include <pthread/kernel.h>
#include <pthread/prio_queue.h>
#include <pthread/queue.h>
#include <pthread/sleep.h>
#include <pthread/mutex.h>
#include <pthread/cond.h>
#include <pthread/fd.h>
#include <pthread/debug_out.h>

/* Requires mutex.h */
#include <pthread/specific.h>

#include <pthread/util.h>

/* More includes */
#include <pthread/pthread_once.h>

/* More includes, that need size_t */
#include <pthread/pthread_attr.h>

#include <signal.h> /* for sigset_t */	/* Moved by monty */

/* Constants for use with pthread_setcancelstate and pthread_setcanceltype */
#define PTHREAD_CANCEL_DISABLE		0
#define PTHREAD_CANCEL_ENABLE		1
#define PTHREAD_CANCEL_DEFERRED		0
#define PTHREAD_CANCEL_ASYNCHRONOUS	1

#define PTHREAD_CANCELLED (void *)1		/* Exit status of a cancelled thread */


#ifdef PTHREAD_KERNEL

enum pthread_state {
#define __pthread_defstate(S,NAME) S,
#include "pthread/state.def"
#undef __pthread_defstate

  /* enum lists aren't supposed to end with a comma, sigh */
  PS_STATE_MAX
};

/* Put PANIC inside an expression that evaluates to non-void type, to
   make it easier to combine it in expressions.  */
#define DO_PANIC()	(PANIC (), 0)
#define PANICIF(x)	((x) ? DO_PANIC () : 0)

/* In the thread flag field, we use a series of bit flags. Flags can
 * organized into "groups" of mutually exclusive flags.  Other flags
 * are unrelated and can be set and cleared with a single bit operation.
 */

#define PF_WAIT_EVENT		0x01
#define PF_DONE_EVENT		0x02
#define PF_EVENT_GROUP		0x03		/* All event bits */

#define PF_CANCEL_STATE			0x04	/* cancellability state */
#define PF_CANCEL_TYPE			0x08	/* cancellability type */
#define PF_THREAD_CANCELLED		0x10	/* thread has been cancelled */
#define PF_RUNNING_TO_CANCEL	0x20	/* Thread is running so it can cancel*/
#define PF_AT_CANCEL_POINT		0x40	/* Thread is at a cancel point */

/* Flag operations */

#define SET_PF_FLAG(x,f)				( (x)->flags |= (f) )
#define TEST_PF_FLAG(x,f)				( (x)->flags & (f) )
#define CLEAR_PF_FLAG(x,f)				( (x)->flags &= ~(f) )
#define CLEAR_PF_GROUP(x,g)				( (x)->flags &= ~(g) )
#define SET_PF_FLAG_IN_GROUP(x,g,f)		( CLEAR_PF_GROUP(x,g),SET_PF_FLAG(x,f))
#define TEST_PF_GROUP(x,g)				( (x)->flags & (g) )

#define SET_PF_DONE_EVENT(x)			\
( !TEST_PF_FLAG(x,PF_DONE_EVENT)		\
  ? ( TEST_PF_FLAG(x,PF_WAIT_EVENT)		\
	  ? (SET_PF_FLAG_IN_GROUP(x,PF_EVENT_GROUP,PF_DONE_EVENT), OK)	\
	  : DO_PANIC ())					\
  : NOTOK )

#define SET_PF_WAIT_EVENT(x)			\
( PANICIF (TEST_PF_GROUP(x,PF_EVENT_GROUP) ),	\
  SET_PF_FLAG_IN_GROUP(x,PF_EVENT_GROUP,PF_WAIT_EVENT), 0)

#define CLEAR_PF_DONE_EVENT(x)			\
( PANICIF (!TEST_PF_FLAG(x,PF_DONE_EVENT)),	\
  CLEAR_PF_GROUP(x,PF_EVENT_GROUP) )

#define SET_PF_CANCELLED(x)			( SET_PF_FLAG(x,PF_THREAD_CANCELLED) )
#define TEST_PF_CANCELLED(x)		( TEST_PF_FLAG(x,PF_THREAD_CANCELLED) )

#define SET_PF_RUNNING_TO_CANCEL(x)	( SET_PF_FLAG(x,PF_RUNNING_TO_CANCEL) )
#define CLEAR_PF_RUNNING_TO_CANCEL(x)( CLEAR_PF_FLAG(x,PF_RUNNING_TO_CANCEL) )
#define TEST_PF_RUNNING_TO_CANCEL(x)( TEST_PF_FLAG(x,PF_RUNNING_TO_CANCEL) )

#define SET_PF_AT_CANCEL_POINT(x)	( SET_PF_FLAG(x,PF_AT_CANCEL_POINT) )
#define CLEAR_PF_AT_CANCEL_POINT(x)	( CLEAR_PF_FLAG(x,PF_AT_CANCEL_POINT) )
#define TEST_PF_AT_CANCEL_POINT(x)	( TEST_PF_FLAG(x,PF_AT_CANCEL_POINT) )

#define SET_PF_CANCEL_STATE(x,f) \
	( (f) ? SET_PF_FLAG(x,PF_CANCEL_STATE) : CLEAR_PF_FLAG(x,PF_CANCEL_STATE) )
#define TEST_PF_CANCEL_STATE(x) \
	( (TEST_PF_FLAG(x,PF_CANCEL_STATE)) ? PTHREAD_CANCEL_ENABLE \
										: PTHREAD_CANCEL_DISABLE )

#define SET_PF_CANCEL_TYPE(x,f) \
	( (f) ? SET_PF_FLAG(x,PF_CANCEL_TYPE) : CLEAR_PF_FLAG(x,PF_CANCEL_TYPE) )
#define TEST_PF_CANCEL_TYPE(x) \
	( (TEST_PF_FLAG(x,PF_CANCEL_TYPE)) ? PTHREAD_CANCEL_ASYNCHRONOUS \
									   : PTHREAD_CANCEL_DEFERRED )

/* See if a thread is in a state that it can be cancelled */
#define TEST_PTHREAD_IS_CANCELLABLE(x) \
( (TEST_PF_CANCEL_STATE(x) == PTHREAD_CANCEL_ENABLE && TEST_PF_CANCELLED(x)) \
    ? ((TEST_PF_CANCEL_TYPE(x) == PTHREAD_CANCEL_ASYNCHRONOUS) \
        ? 1 \
        : TEST_PF_AT_CANCEL_POINT(x)) \
	: 0 )


struct pthread_select_data {
	int		nfds;
	fd_set	readfds;
	fd_set	writefds;
	fd_set	exceptfds;
};

union pthread_wait_data {
	pthread_mutex_t   * mutex;
	pthread_cond_t 	  * cond;
	const sigset_t	  * sigwait;		/* Waiting on a signal in sigwait */
	struct {
		short	fd;						/* Used when thread waiting on fd */
		short	branch;					/* line number, for debugging */
	} fd;
	struct pthread_select_data * select_data;
};

#define PTT_USER_THREAD		0x0001

struct pthread {
	int			thread_type;
	struct machdep_pthread	machdep_data;
	pthread_attr_t		attr;

	/* Signal interface */
	sigset_t		sigmask;
	sigset_t		sigpending;
	int			sigcount; /* Number of signals pending */
	int			sighandled; /* Set when signal has been handled */
	/* Timeout time */
	struct timespec		wakeup_time;

	/* Join queue for waiting threads */
	struct pthread_queue	join_queue;

	/*
	 * Thread implementations are just multiple queue type implemenations,
	 * Below are the various link lists currently necessary
	 * It is possible for a thread to be on multiple, or even all the
	 * queues at once, much care must be taken during queue manipulation.
	 *
	 * The pthread structure must be locked before you can even look at
	 * the link lists.
	 */ 

	/*
	 * ALL threads, in any state. 
	 * Must lock kernel lock before manipulating.
	 */
	struct pthread		* pll;		

	/*
	 * Standard link list for running threads, mutexes, etc ...
	 * It can't be on both a running link list and a wait queue.
	 * Must lock kernel lock before manipulating.
	 */
	struct pthread		* next;	
	union pthread_wait_data data;

	/*
	 * Actual queue state and priority of thread.
	 * (Note: "priority" is a reserved word in Concurrent C, please
	 * don't use it.  --KR)
	 */
	struct pthread_queue	* queue;
	enum pthread_state		  state;
	enum pthread_state		  old_state; /* Used when cancelled */
	char					  flags;
	char 					  pthread_priority;

	/*
	 * Sleep queue, this is different from the standard link list
	 * because it is possible to be on both (pthread_cond_timedwait();
	 * Must lock sleep mutex before manipulating
	 */
	struct pthread		*sll;	/* For sleeping threads */

	/*
	 * Data that doesn't need to be locked
	 * Mostly because only the thread owning the data can manipulate it
	 */
	void 			* ret;
	int				  error;
	int 			* error_p;
	const void		** specific_data;
	int			specific_data_count;

	/* Cleanup handlers Link List */
	struct pthread_cleanup *cleanup;
};

#else /* not PTHREAD_KERNEL */

struct pthread;

#endif

typedef struct pthread *pthread_t;

/*
 * Globals
 */
#ifdef PTHREAD_KERNEL

extern	struct pthread 		* pthread_run;
extern	struct pthread 		* pthread_initial;
extern	struct pthread 		* pthread_link_list;
extern	struct pthread_queue	pthread_dead_queue;
extern	struct pthread_queue	pthread_alloc_queue;

extern	pthread_attr_t		pthread_attr_default;
extern	volatile int		fork_lock;
extern	pthread_size_t		pthread_pagesize;

extern	sigset_t		* uthread_sigmask;

/* Kernel global functions */
extern void pthread_sched_prevent(void);
extern void pthread_sched_resume(void);
extern int __pthread_is_valid( pthread_t );
extern void pthread_cancel_internal( int freelocks );

#endif

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(DCE_COMPAT)

typedef	void * (*pthread_startroutine_t)(void *);
typedef void * pthread_addr_t;

int	pthread_create __P_((pthread_t *, pthread_attr_t,
			    pthread_startroutine_t,
			    pthread_addr_t));
void	pthread_exit __P_((pthread_addr_t));
int	pthread_join __P_((pthread_t, pthread_addr_t *));

#else

void	pthread_init __P_((void));
int	pthread_create __P_((pthread_t *,
			    const pthread_attr_t *,
			    void * (*start_routine)(void *),
			    void *));
void	pthread_exit __P_((void *));
pthread_t pthread_self __P_((void));
int	pthread_equal __P_((pthread_t, pthread_t));
int	pthread_join __P_((pthread_t, void **));
int	pthread_detach __P_((pthread_t));
void	pthread_yield __P_((void));
int	pthread_setschedparam __P_((pthread_t pthread, int policy,
				   struct sched_param * param));
int	pthread_getschedparam __P_((pthread_t pthread, int * policy,
				   struct sched_param * param));
int	pthread_kill __P_((struct pthread *, int));
void	(*pthread_signal __P_((int, void (*)(int))))();
int	pthread_cancel __P_(( pthread_t pthread ));
int	pthread_setcancelstate __P_(( int state, int *oldstate ));
int	pthread_setcanceltype __P_(( int type, int *oldtype ));
void	pthread_testcancel __P_(( void ));

int 	pthread_sigmask __P_((int how, const sigset_t *set,
			     sigset_t * oset)); /* added by Monty */
int	sigwait __P_((const sigset_t * set, int * sig));
int	sigsetwait __P_((const sigset_t * set, int * sig));
#endif

#if defined(PTHREAD_KERNEL)

/* Not valid, but I can't spell so this will be caught at compile time */
#define		pthread_yeild(notvalid)

#endif

__END_DECLS

/*
 * Static constructors
 */
#ifdef __cplusplus

extern	struct pthread 		  * pthread_initial;

class __pthread_init_t {
/* struct __pthread_init_t { */
	public:
	__pthread_init_t() {
		if (pthread_initial == NULL) {
			pthread_init();
		}
	}
};

static __pthread_init_t __pthread_init_this_file;

#endif /* __cplusplus */

#endif
