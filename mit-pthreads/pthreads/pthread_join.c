/* ==== pthread_join.c =======================================================
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

#include <pthread.h>
#include <errno.h>

static int testDeadlock( struct pthread_queue *queue, pthread_t target );

/* ==========================================================================
 * pthread_join()
 */
int pthread_join(pthread_t pthread, void **thread_return)
{
	int ret;

	pthread_sched_prevent();

	/* Ensure they gave us a legal pthread pointer */
	if( ! __pthread_is_valid( pthread ) ) {
		pthread_sched_resume();
		return(EINVAL);
	} 

	/* Check that thread isn't detached already */
	if (pthread->attr.flags & PTHREAD_DETACHED) {
		pthread_sched_resume();
		return(ESRCH);
	} 

	/*
	 * Now check if other thread has exited
	 * Note: This must happen after checking detached state.
	 */
	if (pthread_queue_remove(&pthread_dead_queue, pthread) != OK) {

		/* Before we pend on the join, ensure there is no dead lock */

		if( testDeadlock( &pthread_run->join_queue, pthread ) == NOTOK ) {
			ret = EDEADLK;
		} else {
			pthread_queue_enq(&(pthread->join_queue), pthread_run);
			SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
			pthread_resched_resume(PS_JOIN);
			CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */
			pthread_sched_prevent();

			if (pthread_queue_remove(&pthread_dead_queue, pthread) == OK) {
				pthread_queue_enq(&pthread_alloc_queue, pthread);
				pthread->attr.flags |= PTHREAD_DETACHED;
				pthread->state = PS_UNALLOCED;
				if (thread_return) {
					*thread_return = pthread->ret;
				}
				ret = OK;
			} else {
				ret = ESRCH;
			}
		}
    } else {
		/* Just get the return value and detach the thread */
		pthread_queue_enq(&pthread_alloc_queue, pthread);
		pthread->attr.flags |= PTHREAD_DETACHED;
		pthread->state = PS_UNALLOCED;
		if (thread_return) {
            *thread_return = pthread->ret;
        }
		ret = OK;
	}
	pthread_sched_resume();
	return(ret);
}

/*----------------------------------------------------------------------
 * Function:	testDeadlock
 * Purpose:		recursive queue walk to check for deadlocks
 * Args:
 *		queue	= the queue to walk
 *		pthread	= target to scan for
 * Returns:
 *		OK = no deadlock, NOTOK = deadlock
 * Notes:
 *----------------------------------------------------------------------*/
static int
testDeadlock( struct pthread_queue *queue, pthread_t target )
{
	pthread_t t;

	if( queue == NULL )
		return OK;						/* Empty queue, obviously ok */

	for( t = queue->q_next; t; t = t->next ) {
		if( t == target )
			return NOTOK;				/* bang, your dead */

		if( testDeadlock( &t->join_queue, target ) == NOTOK ) {
			return NOTOK;
		}
	}

	return OK;							/* No deadlock */
}
