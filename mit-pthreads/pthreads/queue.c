/* ==== queue.c ============================================================
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
 * Description : Queue functions.
 *
 *  1.00 93/07/15 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>

/*
 * All routines in this file assume that the queue has been appropriatly
 * locked.
 */

/* ==========================================================================
 * pthread_queue_init()
 */
void pthread_queue_init(struct pthread_queue *queue)
{
	queue->q_next = NULL;
	queue->q_last = NULL;
	queue->q_data = NULL;
}

/* ==========================================================================
 * pthread_queue_enq()
 */
void pthread_queue_enq(struct pthread_queue *queue, struct pthread *thread)
{
	if (queue->q_last) {
		queue->q_last->next = thread;
	} else {
		queue->q_next = thread;
	}
	queue->q_last = thread;
	thread->queue = queue;
	thread->next = NULL;
	
}

/* ==========================================================================
 * pthread_queue_get()
 */
struct pthread *pthread_queue_get(struct pthread_queue *queue)
{
	return(queue->q_next);
}

/* ==========================================================================
 * pthread_queue_deq()
 */
struct pthread *pthread_queue_deq(struct pthread_queue *queue)
{
	struct pthread *thread = NULL;

	if (queue->q_next) {
		thread = queue->q_next;
		if (!(queue->q_next = queue->q_next->next)) {
			queue->q_last = NULL;
		}
		thread->queue = NULL;
		thread->next = NULL;
	}
	return(thread);
}

/* ==========================================================================
 * pthread_queue_remove()
 */
int pthread_queue_remove(struct pthread_queue *queue, struct pthread *thread)
{
	struct pthread **current = &(queue->q_next);
	struct pthread *prev = NULL;
	int ret = NOTOK;

	while (*current) {
		if (*current == thread) {
			if ((*current)->next) {
				*current = (*current)->next;
			} else {
				queue->q_last = prev;
				*current = NULL;
			}
			thread->queue = NULL;
			thread->next = NULL;
			ret = OK;
			break;
		}
		prev = *current;
		current = &((*current)->next);
	}
	return(ret);
}

/* ==========================================================================
 * pthread_llist_remove()
 */
int pthread_llist_remove(struct pthread **llist, struct pthread *thread)
{
	while (*llist) {
		if (*llist == thread) {
			*llist = thread->next;
			return(OK);
		}
		llist = &(*llist)->next;
	}
	return(NOTOK);
}

