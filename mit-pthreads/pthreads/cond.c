/* ==== cond.c ============================================================
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
 * Description : Condition variable functions.
 *
 *  1.00 93/10/28 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <timers.h>
#include <errno.h>

#ifndef ETIME
#define ETIME ETIMEDOUT  
#endif

/* ==========================================================================
 * pthread_cond_is_debug()
 *
 * Check that cond is a debug cond and if so returns entry number into
 * array of debug condes.
 */
static int pthread_cond_debug_count = 0;
static pthread_cond_t ** pthread_cond_debug_ptrs = NULL;
static pthread_mutex_t pthread_cond_debug_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline int pthread_cond_is_debug(pthread_cond_t * cond) 
{
	int i;

	for (i = 0; i < pthread_cond_debug_count; i++) {
		if (pthread_cond_debug_ptrs[i] == cond) {
			return(i);
		}
	}
	return(NOTOK);
}
/* ==========================================================================
 * pthread_cond_init()
 *
 * In this implementation I don't need to allocate memory.
 * ENOMEM, EAGAIN should never be returned. Arch that have
 * weird constraints may need special coding.
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	enum pthread_condtype type;

	/* Only check if attr specifies some mutex type other than fast */
	if ((cond_attr) && (cond_attr->c_type != COND_TYPE_FAST)) {
		if (cond_attr->c_type >= COND_TYPE_MAX) {
			return(EINVAL);
		}
		type = cond_attr->c_type;
	} else {
		type = COND_TYPE_FAST;
	}

	switch (type) {
	case COND_TYPE_FAST:
	case COND_TYPE_COUNTING_FAST: 
		break;
	case COND_TYPE_DEBUG:
		pthread_mutex_lock(&pthread_cond_debug_mutex);
		if (pthread_cond_is_debug(cond) == NOTOK) {
			pthread_cond_t ** new;

			if ((new = (pthread_cond_t **)realloc(pthread_cond_debug_ptrs,
			  (pthread_cond_debug_count + 1) * (sizeof(void *)))) == NULL) {
				pthread_mutex_unlock(&pthread_cond_debug_mutex);
				return(ENOMEM);
			}
			pthread_cond_debug_ptrs = new;
			pthread_cond_debug_ptrs[pthread_cond_debug_count++] = cond;
		} else {
			pthread_mutex_unlock(&pthread_cond_debug_mutex);
			return(EBUSY);
		}
		pthread_mutex_unlock(&pthread_cond_debug_mutex);
        break;
    case COND_TYPE_STATIC_FAST:
	defualt:
        return(EINVAL);
        break;
	}

	/* Set all other paramaters */
	pthread_queue_init(&cond->c_queue);
	cond->c_flags 	|= COND_FLAGS_INITED;
	cond->c_type 	= type;
	return(OK);
}

/* ==========================================================================
 * pthread_cond_destroy()
 */
int pthread_cond_destroy(pthread_cond_t *cond)
{
	int i;

	/* Only check if cond is of type other than fast */
	switch(cond->c_type) {
	case COND_TYPE_FAST:
	case COND_TYPE_COUNTING_FAST: 
		break;
    case COND_TYPE_DEBUG:
        if (pthread_queue_get(&(cond->c_queue))) {
            return(EBUSY);
        }
		pthread_mutex_lock(&pthread_cond_debug_mutex);
		if ((i = pthread_cond_is_debug(cond)) == NOTOK) {
			pthread_mutex_unlock(&pthread_cond_debug_mutex);
            return(EINVAL);
        }

		/* Remove the cond from the list of debug condition variables */
		pthread_cond_debug_ptrs[i] = 
		  pthread_cond_debug_ptrs[--pthread_cond_debug_count];
		pthread_cond_debug_ptrs[pthread_cond_debug_count] = NULL;
		pthread_mutex_unlock(&pthread_cond_debug_mutex);
        break;
	case COND_TYPE_STATIC_FAST:
	default:
		return(EINVAL);
		break;
	}

	/* Cleanup cond, others might want to use it. */
	pthread_queue_init(&cond->c_queue);
	cond->c_flags	= 0;
	return(OK);
}

/* ==========================================================================
 * pthread_cond_wait()
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int rval;

	pthread_sched_prevent();
	switch (cond->c_type) {
	case COND_TYPE_DEBUG: 
		pthread_mutex_lock(&pthread_cond_debug_mutex);
		if (pthread_cond_is_debug(cond) == NOTOK) {
			pthread_mutex_unlock(&pthread_cond_debug_mutex);
			pthread_sched_resume();
			return(EINVAL);
		}
		pthread_mutex_unlock(&pthread_cond_debug_mutex);

	/*
	 * Fast condition variables do not check for any error conditions.
     */
	case COND_TYPE_FAST: 
	case COND_TYPE_STATIC_FAST:
		pthread_queue_enq(&cond->c_queue, pthread_run);
		pthread_mutex_unlock(mutex);

		pthread_run->data.mutex = mutex;

		SET_PF_WAIT_EVENT(pthread_run);
		SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
		/* Reschedule will unlock pthread_run */
		pthread_resched_resume(PS_COND_WAIT);
		CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */
		CLEAR_PF_DONE_EVENT(pthread_run);

		pthread_run->data.mutex = NULL;

		rval = pthread_mutex_lock(mutex);
		return(rval);
		break;
	case COND_TYPE_COUNTING_FAST: 
		{
		int count = mutex->m_data.m_count;

		pthread_queue_enq(&cond->c_queue, pthread_run);
		pthread_mutex_unlock(mutex);
		mutex->m_data.m_count = 1;

		pthread_run->data.mutex = mutex;

		SET_PF_WAIT_EVENT(pthread_run);
		SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
		/* Reschedule will unlock pthread_run */
		pthread_resched_resume(PS_COND_WAIT);
		CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */
		CLEAR_PF_DONE_EVENT(pthread_run);

		pthread_run->data.mutex = NULL;

		rval = pthread_mutex_lock(mutex);
		mutex->m_data.m_count = count;
		return(rval);
		break;
		}
	default:
		rval = EINVAL;
		break;
	}
	pthread_sched_resume();
	return(rval);
}

/* ==========================================================================
 * pthread_cond_timedwait()
 */
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
  const struct timespec * abstime)
{
	struct timespec current_time, new_time;
	int rval = OK;
	
	pthread_sched_prevent();
	machdep_gettimeofday(& current_time);

	switch (cond->c_type) {
	case COND_TYPE_DEBUG: 
		pthread_mutex_lock(&pthread_cond_debug_mutex);
		if (pthread_cond_is_debug(cond) == NOTOK) {
			pthread_mutex_unlock(&pthread_cond_debug_mutex);
			pthread_sched_resume();
			return(EINVAL);
		}
		pthread_mutex_unlock(&pthread_cond_debug_mutex);

	/*
	 * Fast condition variables do not check for any error conditions.
     */
	case COND_TYPE_FAST: 
	case COND_TYPE_STATIC_FAST:

		/* Set pthread wakeup time*/
		pthread_run->wakeup_time = *abstime;

		/* Install us on the sleep queue */
		sleep_schedule (&current_time, &(pthread_run->wakeup_time));

		pthread_queue_enq(&cond->c_queue, pthread_run);
		SET_PF_WAIT_EVENT(pthread_run);
		pthread_mutex_unlock(mutex);

		pthread_run->data.mutex = mutex;

		SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
		/* Reschedule will unlock pthread_run */
		pthread_resched_resume(PS_COND_WAIT);
		CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */

		pthread_run->data.mutex = NULL;

		/* Remove ourselves from sleep queue. If we fail then we timedout */
        if (sleep_cancel(pthread_run) == NOTOK) {
			SET_ERRNO(ETIME);
			rval = ETIME;
		}

		CLEAR_PF_DONE_EVENT(pthread_run);
		pthread_mutex_lock(mutex);
		return(rval);
		break;
	case COND_TYPE_COUNTING_FAST: 
		{
		int count = mutex->m_data.m_count;

		/* Set pthread wakeup time*/
		pthread_run->wakeup_time = *abstime;

		/* Install us on the sleep queue */
		sleep_schedule (&current_time, &(pthread_run->wakeup_time));

		pthread_queue_enq(&cond->c_queue, pthread_run);
		SET_PF_WAIT_EVENT(pthread_run);
		pthread_mutex_unlock(mutex);

		pthread_run->data.mutex = mutex;

		SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
		/* Reschedule will unlock pthread_run */
		pthread_resched_resume(PS_COND_WAIT);
		CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */

		pthread_run->data.mutex = NULL;

		/* Remove ourselves from sleep queue. If we fail then we timedout */
        if (sleep_cancel(pthread_run) == NOTOK) {
			SET_ERRNO(ETIME);
			rval = ETIME;
		}

		CLEAR_PF_DONE_EVENT(pthread_run);
		pthread_mutex_lock(mutex);
		mutex->m_data.m_count = count;
		return(rval);
		break;
		}
	default:
		rval = EINVAL;
		break;
	}
	pthread_sched_resume();
	return(rval);
}

/* ==========================================================================
 * pthread_cond_signal()
 */
int pthread_cond_signal(pthread_cond_t *cond)
{
	struct pthread *pthread;
	int rval;

	pthread_sched_prevent();
	switch (cond->c_type) {
	case COND_TYPE_DEBUG: 
		pthread_mutex_lock(&pthread_cond_debug_mutex);
		if (pthread_cond_is_debug(cond) == NOTOK) {
			pthread_mutex_unlock(&pthread_cond_debug_mutex);
			pthread_sched_resume();
			return(EINVAL);
		}
		pthread_mutex_unlock(&pthread_cond_debug_mutex);

	case COND_TYPE_FAST: 
	case COND_TYPE_STATIC_FAST:
		if (pthread = pthread_queue_deq(&cond->c_queue)) {
 			if ((SET_PF_DONE_EVENT(pthread)) == OK) {
				pthread_sched_other_resume(pthread);
			} else {
				pthread_sched_resume();
			}
			return(OK);
		}
		rval = OK;
		break;
	default:
		rval = EINVAL;
		break;
	}
	pthread_sched_resume();
	return(rval);
}

/* ==========================================================================
 * pthread_cond_broadcast() 
 *
 * Not much different then the above routine.
 */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
	struct pthread * pthread, * high_pthread, * low_pthread;
	int rval;

	pthread_sched_prevent();
	switch (cond->c_type) {
	case COND_TYPE_DEBUG: 
		pthread_mutex_lock(&pthread_cond_debug_mutex);
		if (pthread_cond_is_debug(cond) == NOTOK) {
			pthread_mutex_unlock(&pthread_cond_debug_mutex);
			pthread_sched_resume();
			return(EINVAL);
		}
		pthread_mutex_unlock(&pthread_cond_debug_mutex);

	case COND_TYPE_FAST: 
	case COND_TYPE_STATIC_FAST:
		if (pthread = pthread_queue_deq(&cond->c_queue)) {
			pthread->state = PS_RUNNING;
			high_pthread = pthread;

			while (pthread = pthread_queue_deq(&cond->c_queue)) {
				if (pthread->pthread_priority > 
				  high_pthread->pthread_priority) {
					low_pthread = high_pthread;
					high_pthread = pthread;
				} else {
					low_pthread	= pthread;
				}
 				if ((SET_PF_DONE_EVENT(low_pthread)) == OK) {
					pthread_prio_queue_enq(pthread_current_prio_queue, 
					  low_pthread);
					low_pthread->state = PS_RUNNING;
				}
			}
 			if ((SET_PF_DONE_EVENT(high_pthread)) == OK) {
				pthread_sched_other_resume(high_pthread);
			} else {
				pthread_sched_resume();
			}
			return(OK);
		}
		rval = OK;
		break;
	default:
		rval = EINVAL;
		break;
	}
	pthread_sched_resume();
	return(rval);
}

