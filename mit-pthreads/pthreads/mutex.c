/* ==== mutex.c ==============================================================
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
 * Description : Mutex functions.
 *
 *  1.00 93/07/19 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

/* ==========================================================================
 * pthread_mutex_is_debug()
 *
 * Check that mutex is a debug mutex and if so returns entry number into
 * array of debug mutexes.
 */
static int pthread_mutex_debug_count = 0;
static pthread_mutex_t ** pthread_mutex_debug_ptrs = NULL;

static inline int pthread_mutex_is_debug(pthread_mutex_t * mutex) 
{
	int i;

	for (i = 0; i < pthread_mutex_debug_count; i++) {
		if (pthread_mutex_debug_ptrs[i] == mutex) {
			return(i);
		}
	}
	return(NOTOK);
}

/* ==========================================================================
 * pthread_mutex_init()
 *
 * In this implementation I don't need to allocate memory.
 * ENOMEM, EAGAIN should never be returned. Arch that have
 * weird constraints may need special coding.
 */
int pthread_mutex_init(pthread_mutex_t *mutex, 
  const pthread_mutexattr_t *mutex_attr)
{
	enum pthread_mutextype type;

	/* Only check if attr specifies some mutex type other than fast */
	if ((mutex_attr) && (mutex_attr->m_type != MUTEX_TYPE_FAST)) {
		if (mutex_attr->m_type >= MUTEX_TYPE_MAX) {
			return(EINVAL);
		}
		type = mutex_attr->m_type;
	} else {
		type = MUTEX_TYPE_FAST;
	}
	mutex->m_flags = 0;

	pthread_sched_prevent();

	switch(type) {
    case MUTEX_TYPE_FAST:
        break;
    case MUTEX_TYPE_STATIC_FAST:
		pthread_sched_resume();
        return(EINVAL);
        break;
	case MUTEX_TYPE_COUNTING_FAST:
		mutex->m_data.m_count = 0;
        break;
    case MUTEX_TYPE_DEBUG:
		if (pthread_mutex_is_debug(mutex) == NOTOK) {
			pthread_mutex_t ** new;

			if ((new = (pthread_mutex_t **)realloc(pthread_mutex_debug_ptrs,
			  (pthread_mutex_debug_count + 1) * (sizeof(void *)))) == NULL) {
				pthread_sched_resume();
				return(ENOMEM);
			}
			pthread_mutex_debug_ptrs = new;
			pthread_mutex_debug_ptrs[pthread_mutex_debug_count++] = mutex;
		} else {
			pthread_sched_resume();
            return(EBUSY);
        }
        break;
    default:
		pthread_sched_resume();
        return(EINVAL);
		break;
	}
	/* Set all other paramaters */
	pthread_queue_init(&mutex->m_queue);
	mutex->m_flags 	|= MUTEX_FLAGS_INITED;
	mutex->m_owner	= NULL;
	mutex->m_type	= type;

	pthread_sched_resume();
	return(OK);
}

/* ==========================================================================
 * pthread_mutex_destroy()
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	int i;

	pthread_sched_prevent();

	/* Only check if mutex is of type other than fast */
	switch(mutex->m_type) {
	case MUTEX_TYPE_FAST:
		break;
	case MUTEX_TYPE_STATIC_FAST:
		pthread_sched_resume();
		return(EINVAL);
		break;
	case MUTEX_TYPE_COUNTING_FAST:
		mutex->m_data.m_count = 0;
		break;
	case MUTEX_TYPE_DEBUG:
		if ((i = pthread_mutex_is_debug(mutex)) == NOTOK) {
			pthread_sched_resume();
            return(EINVAL);
        }
        if (mutex->m_owner) {
			pthread_sched_resume();
            return(EBUSY);
        }

		/* Remove the mutex from the list of debug mutexes */
		pthread_mutex_debug_ptrs[i] = 
		  pthread_mutex_debug_ptrs[--pthread_mutex_debug_count];
		pthread_mutex_debug_ptrs[pthread_mutex_debug_count] = NULL;
        break;
	default:
		pthread_sched_resume();
		return(EINVAL);
		break;
	}

	/* Cleanup mutex, others might want to use it. */
	pthread_queue_init(&mutex->m_queue);
	mutex->m_owner	= NULL;
	mutex->m_flags	= 0;

	pthread_sched_resume();
	return(OK);
}

/* ==========================================================================
 * pthread_mutex_trylock()
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int rval;

	pthread_sched_prevent();
	switch (mutex->m_type) {
	/*
	 * Fast mutexes do not check for any error conditions.
     */
	case MUTEX_TYPE_FAST: 
	case MUTEX_TYPE_STATIC_FAST:
		if (!mutex->m_owner) {
			mutex->m_owner = pthread_run;
			rval = OK;
		} else {
			rval = EBUSY;
		}
		break;
	case MUTEX_TYPE_COUNTING_FAST:
		if (mutex->m_owner) {
			if (mutex->m_owner == pthread_run) {
				mutex->m_data.m_count++;
				rval = OK;
			} else {
				rval = EBUSY;
			}
		} else {
			mutex->m_owner = pthread_run;
			rval = OK;
		}
		break;
	case MUTEX_TYPE_DEBUG:
        if (pthread_mutex_is_debug(mutex) != NOTOK) {
            if (!mutex->m_owner) {
                mutex->m_owner = pthread_run;
                rval = OK;
            } else {
                rval = EBUSY;
            }
        } else {
            rval = EINVAL;
        }
        break;
	default:
		rval = EINVAL;
		break;
	}

	pthread_sched_resume();
	return(rval);
}

/* ==========================================================================
 * pthread_mutex_lock()
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int rval;

	pthread_sched_prevent();
	switch (mutex->m_type) {
	/*
	 * Fast mutexes do not check for any error conditions.
     */
	case MUTEX_TYPE_FAST: 
	case MUTEX_TYPE_STATIC_FAST:
		if (mutex->m_owner) {
			pthread_queue_enq(&mutex->m_queue, pthread_run);

			/* Reschedule will unlock scheduler */
			pthread_resched_resume(PS_MUTEX_WAIT);
			return(OK);
		}
		mutex->m_owner = pthread_run;
		rval = OK;
		break;
	case MUTEX_TYPE_COUNTING_FAST:
		if (mutex->m_owner) {
			if (mutex->m_owner != pthread_run) {
				pthread_queue_enq(&mutex->m_queue, pthread_run);

				/* Reschedule will unlock scheduler */
				pthread_resched_resume(PS_MUTEX_WAIT);
				return(OK);
			} else {
				mutex->m_data.m_count++;
			}	
		} else {
			mutex->m_owner = pthread_run;
		}
		rval = OK;
		break;
    case MUTEX_TYPE_DEBUG:
        if (pthread_mutex_is_debug(mutex) != NOTOK) {
            if (mutex->m_owner) {
                if (mutex->m_owner != pthread_run) {
                    pthread_queue_enq(&mutex->m_queue, pthread_run);

                    /* Reschedule will unlock pthread_run */
					pthread_resched_resume(PS_MUTEX_WAIT);

                    if (mutex->m_owner != pthread_run) {
                        PANIC();
                    }
                    return(OK);
                }
                rval = EDEADLK;
                break;
            }
            mutex->m_owner = pthread_run;
            rval = OK;
            break;
        }
        rval = EINVAL;
        break;
	default:
		rval = EINVAL;
		break;
	}

	pthread_sched_resume();
	return(rval);
}

/* ==========================================================================
 * pthread_mutex_unlock()
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	struct pthread *pthread;
	int rval;

	pthread_sched_prevent();
	
	switch (mutex->m_type) {
    /*
     * Fast mutexes do not check for any error conditions.
     */
    case MUTEX_TYPE_FAST:
    case MUTEX_TYPE_STATIC_FAST:
		if (mutex->m_owner = pthread_queue_deq(&mutex->m_queue)) {

			/* Reschedule will unlock scheduler */
			pthread_sched_other_resume(mutex->m_owner);
			return(OK);
		}
		rval = OK;
		break;
	case MUTEX_TYPE_COUNTING_FAST:
		if (mutex->m_data.m_count) {
			mutex->m_data.m_count--;
			rval = OK;
			break;
		}
		if (mutex->m_owner = pthread_queue_deq(&mutex->m_queue)) {

			/* Reschedule will unlock scheduler */
			pthread_sched_other_resume(mutex->m_owner);
			return(OK);
		}
		rval = OK;
		break;
	 case MUTEX_TYPE_DEBUG:
		if (pthread_mutex_is_debug(mutex) != NOTOK) {
        	if (mutex->m_owner == pthread_run) {
            	if (mutex->m_owner = pthread_queue_deq(&mutex->m_queue)) {

					/* Reschedule will unlock scheduler */
					pthread_sched_other_resume(mutex->m_owner);
					return(OK);
            	}
            	rval = OK;
        	} else {
            	rval = EPERM;
        	}
		} else {
			rval = EINVAL;
		}
        break;
	default:
		rval = EINVAL;
		break;
	}
	pthread_sched_resume();
	return(rval);
}
