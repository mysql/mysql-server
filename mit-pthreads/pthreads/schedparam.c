/* ==== schedparam.c =======================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 * Description : Pthread schedparam functions.
 *
 *  1.38 94/06/15 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <sched.h>
#include <errno.h>

/* ==========================================================================
 * sched_get_priority_max
 */
int	sched_get_priority_max(int policy)
{
	return PTHREAD_MAX_PRIORITY;
}

/* ==========================================================================
 * sched_get_priority_min
 */
int	sched_get_priority_min(int policy)
{
	return PTHREAD_MIN_PRIORITY;
}

/* Currently only policy is supported */
/* ==========================================================================
 * pthread_setschedparam()
 */
int pthread_setschedparam(pthread_t pthread, int policy,
  struct sched_param * param)
{
	enum schedparam_policy new_policy, old_policy;
	int ret = OK;
	int prio;

	new_policy = policy;
	pthread_sched_prevent();
	old_policy = pthread->attr.schedparam_policy;

	if (param) {
		if ((param->sched_priority < PTHREAD_MIN_PRIORITY) ||
		  (param->sched_priority > PTHREAD_MAX_PRIORITY)) {
			pthread_sched_resume();
			return(EINVAL);
		}
		prio = param->sched_priority;
	} else {
		prio = pthread->pthread_priority;
	}

	if (pthread == pthread_run) {
		switch(new_policy) {
		case SCHED_RR:
			pthread->attr.schedparam_policy = new_policy;
			switch (old_policy) { 
			case SCHED_FIFO:
				machdep_unset_thread_timer(NULL);
			default:
				pthread->pthread_priority = prio;
				break;
			}
			break;
		case SCHED_FIFO:
			pthread->attr.schedparam_policy = new_policy;
			switch (old_policy) { 
			case SCHED_IO:
			case SCHED_RR:
				if (pthread->pthread_priority < prio) {
					pthread->pthread_priority = prio;
					pthread_sched_resume();
					pthread_yield();
					return(OK);
				}
			default:
				pthread->pthread_priority = prio;
				break;
			}
			break;
		case SCHED_IO:
			pthread->attr.schedparam_policy = new_policy;
			switch (old_policy) { 
			case SCHED_FIFO:
				machdep_unset_thread_timer(NULL);
			default:
				pthread->pthread_priority = prio;
				break;
			}
			break;
		default:
			SET_ERRNO(EINVAL);
			ret = EINVAL;
			break;
		}
	} else {
		switch(new_policy) {
		case SCHED_FIFO:
		case SCHED_IO:
		case SCHED_RR:
			if(pthread_prio_queue_remove(pthread_current_prio_queue,pthread) == OK) {
				pthread->attr.schedparam_policy = new_policy;
				pthread->pthread_priority 		= prio;
				pthread_sched_other_resume(pthread);
			} else {
				pthread->attr.schedparam_policy = new_policy;
				pthread->pthread_priority 		= prio;
				pthread_sched_resume();
			}
			return(OK);
			break;
		default:
			SET_ERRNO(EINVAL);
			ret = EINVAL;
			break;
		}
	}
		
	pthread_sched_resume();
	return(ret);
}

/* ==========================================================================
 * pthread_getschedparam()
 */
int pthread_getschedparam(pthread_t pthread, int * policy,
  struct sched_param * param)
{
	*policy = pthread->attr.schedparam_policy;
	if (param) {
		param->sched_priority = pthread->pthread_priority;
	}
	return(OK);
}

