/* ==== pthread.c ============================================================
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
 * Description : Pthread functions.
 *
 *  1.00 93/07/26 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sched.h>

/* ==========================================================================
 * sched_yield()
 */
int sched_yield()
{
	sig_handler_fake(SIGVTALRM);
	return(OK);
}

/* ==========================================================================
 * pthread_yield()
 */
void pthread_yield()
{
	sig_handler_fake(SIGVTALRM);
}

/* ==========================================================================
 * pthread_self()
 */
pthread_t pthread_self()
{
	return(pthread_run);
}

/* ==========================================================================
 * pthread_equal()
 */
int pthread_equal(pthread_t t1, pthread_t t2)
{
	return(t1 == t2);
}

/* ==========================================================================
 * pthread_exit()
 */
extern void pthread_cleanupspecific(void);

void pthread_exit(void *status)
{
	pthread_t pthread;

	/* Save return value */
	pthread_run->ret = status;

	/* First execute all cleanup handlers */
	while (pthread_run->cleanup) {
		pthread_cleanup_pop(1);
	}

	/* Don't forget the cleanup attr */
	if (pthread_run->attr.cleanup_attr) {
		pthread_run->attr.cleanup_attr(pthread_run->attr.arg_attr);
	}

	/* Next run thread-specific data desctructors */
	if (pthread_run->specific_data) {
		pthread_cleanupspecific();
	}

	pthread_sched_prevent();

	if (!(pthread_run->attr.flags & PTHREAD_DETACHED)) {
		/*
		 * Are there any threads joined to this one,
		 * if so wake them and let them detach this thread.
		 */
		while (pthread = pthread_queue_deq(&(pthread_run->join_queue))) {
			pthread_prio_queue_enq(pthread_current_prio_queue, pthread);
			pthread->state = PS_RUNNING;
		}
		pthread_queue_enq(&pthread_dead_queue, pthread_run);
		pthread_resched_resume(PS_DEAD);
	} else {
		pthread_queue_enq(&pthread_alloc_queue, pthread_run);
		pthread_resched_resume(PS_UNALLOCED);
	}

	/* This thread will never run again */
	PANIC();

}

/*----------------------------------------------------------------------
 * Function: __pthread_is_valid
 * Purpose:	 Scan the list of threads to see if a specified thread exists
 * Args:
 *     pthread = The thread to scan for
 * Returns:
 *     int     = 1 if found, 0 if not
 * Notes:
 *     The kernel is assumed to be locked
 *----------------------------------------------------------------------*/
int
__pthread_is_valid( pthread_t pthread )
{
	int rtn = 0;						/* Assume not found */
	pthread_t t;

	for( t = pthread_link_list; t; t = t->pll ) {
		if( t == pthread ) {
			rtn = 1;					/* Found it */
			break;
		}
	}

	return rtn;
}

/* ==========================================================================
 * __pthread_free()
 */
static inline void __pthread_free(pthread_t new_thread)
{
	pthread_sched_prevent();
	new_thread->state = PS_UNALLOCED;
	new_thread->attr.stacksize_attr = 0; 
	new_thread->attr.stackaddr_attr = NULL; 
	pthread_queue_enq(&pthread_alloc_queue, new_thread);
	pthread_sched_resume();
}
/* ==========================================================================
 * __pthread_alloc()
 */
/* static inline pthread_t __pthread_alloc(const pthread_attr_t *attr) */
static pthread_t __pthread_alloc(const pthread_attr_t *attr)
{
	pthread_t thread;
	void * stack;
	void * old;

	pthread_sched_prevent();
	thread = pthread_queue_deq(&pthread_alloc_queue);
	pthread_sched_resume();

	if (thread) {
		if (stack = attr->stackaddr_attr) {
			__machdep_stack_repl(&(thread->machdep_data), stack);
		} else {
			if ((__machdep_stack_get(&(thread->machdep_data)) == NULL)
			  || (attr->stacksize_attr > thread->attr.stacksize_attr)) {
				if (stack = __machdep_stack_alloc(attr->stacksize_attr)) {
					__machdep_stack_repl(&(thread->machdep_data), stack);
				} else {
					__pthread_free(thread);
					thread = NULL;
				}
			}
		}
	} else {
		/* We should probable allocate several for efficiency */
		if (thread = (pthread_t)malloc(sizeof(struct pthread))) {
			/* Link new thread into list of all threads */

			pthread_sched_prevent();
			thread->state = PS_UNALLOCED;
			thread->pll = pthread_link_list;
			pthread_link_list = thread;
			pthread_sched_resume();

			if ((stack = attr->stackaddr_attr) ||
			  (stack = __machdep_stack_alloc(attr->stacksize_attr))) {
				__machdep_stack_set(&(thread->machdep_data), stack);
			} else {
				__machdep_stack_set(&(thread->machdep_data), NULL);
				__pthread_free(thread);
				thread = NULL;
			}
		}
	}
	return(thread);
}

/* ==========================================================================
 * pthread_create()
 *
 * After the new thread structure is allocated and set up, it is added to
 * pthread_run_next_queue, which requires a sig_prevent(),
 * sig_check_and_resume()
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
  void * (*start_routine)(void *), void *arg)
{
	pthread_t new_thread;
	int nsec = 100000000;
	int retval = OK;

	if (! attr) 
		attr = &pthread_attr_default; 

	if (new_thread = __pthread_alloc(attr)) {

		__machdep_pthread_create(&(new_thread->machdep_data),
		  start_routine, arg, attr->stacksize_attr, nsec, 0);

		memcpy(&new_thread->attr, attr, sizeof(pthread_attr_t));
		if (new_thread->attr.flags & PTHREAD_INHERIT_SCHED) {
			new_thread->pthread_priority = pthread_run->pthread_priority; 
			new_thread->attr.sched_priority = pthread_run->pthread_priority;
			new_thread->attr.schedparam_policy = 
			  pthread_run->attr.schedparam_policy;
		}  else {
			new_thread->pthread_priority = new_thread->attr.sched_priority;
		}

		if (!(new_thread->attr.flags & PTHREAD_NOFLOAT)) {
			machdep_save_float_state(new_thread);
		}

		/* Initialize signalmask */
		new_thread->sigmask = pthread_run->sigmask;
		sigemptyset(&(new_thread->sigpending));
		new_thread->sigcount = 0;

		pthread_queue_init(&(new_thread->join_queue));
		new_thread->specific_data = NULL;
		new_thread->specific_data_count = 0;
		new_thread->cleanup = NULL;
		new_thread->queue = NULL;
		new_thread->next = NULL;
		new_thread->flags = 0;

		/* PTHREADS spec says we start with cancellability on and deferred */
		SET_PF_CANCEL_STATE(new_thread, PTHREAD_CANCEL_ENABLE);
		SET_PF_CANCEL_TYPE(new_thread, PTHREAD_CANCEL_DEFERRED);

		new_thread->error_p = NULL;
		new_thread->sll = NULL;

		pthread_sched_prevent();


		pthread_sched_other_resume(new_thread);
		/*
         * Assignment must be outside of the locked pthread kernel incase
         * thread is a bogus address resulting in a seg-fault. We want the
         * original thread to be capable of handling the resulting signal.
         * --proven
         */
		(*thread) = new_thread;
	} else {
		retval = EAGAIN;
	}
	return(retval);
}
