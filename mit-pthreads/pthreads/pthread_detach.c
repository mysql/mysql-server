/* ==== pthread_detach.c =======================================================
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
 * Description : pthread_join function.
 *
 *  1.00 94/01/15 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <errno.h>
#include <pthread.h>

/* ==========================================================================
 * pthread_detach()
 */
int pthread_detach(pthread_t pthread)
{
	struct pthread * next_thread, * high_thread, * low_thread;
	int ret;

	pthread_sched_prevent();

	/* Check that thread isn't detached already */
	if (!(pthread->attr.flags & PTHREAD_DETACHED)) {

		pthread->attr.flags |= PTHREAD_DETACHED;

		/* Wakeup all threads waiting on a join */
		if (next_thread = pthread_queue_deq(&(pthread->join_queue))) {
			high_thread = next_thread;
			
			while (next_thread = pthread_queue_deq(&(pthread->join_queue))) {
				if (high_thread->pthread_priority < next_thread->pthread_priority) {
					low_thread = high_thread;
					high_thread = next_thread;
				} else {
					low_thread = next_thread;
				}
				pthread_prio_queue_enq(pthread_current_prio_queue, low_thread);
				low_thread->state = PS_RUNNING;
			}
			/* If the thread is dead then move it to the alloc queue */
			if (pthread_queue_remove(&pthread_dead_queue, pthread) == OK) {
				pthread_queue_enq(&pthread_alloc_queue, pthread);
			}
			pthread_sched_other_resume(high_thread);
			return(OK);
		}
		/* If the thread is dead then move it to the alloc queue */
		if (pthread_queue_remove(&pthread_dead_queue, pthread) == OK) {
			pthread_queue_enq(&pthread_alloc_queue, pthread);
			pthread->state = PS_UNALLOCED;
		}
		ret = OK;
	} else {
		ret = ESRCH;
	}
	pthread_sched_resume();
	return(ret);
}
