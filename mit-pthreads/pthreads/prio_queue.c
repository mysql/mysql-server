/* ==== prio_queue.c ==========================================================
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
 * Description : Priority Queue functions.
 *
 *  1.00 94/09/19 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>

/* A thread when it becomes eligeble to run is placed on the run queue.
  This requires locking the kernel lock
*/

/* ==========================================================================
 * pthread_prio_queue_init()
 */
void pthread_prio_queue_init(struct pthread_prio_queue * queue)
{
	int i;

	for (i = 0; i <= PTHREAD_MAX_PRIORITY; i++) {
		queue->level[i].first = NULL;
		queue->level[i].last = NULL;
	}
	queue->next = NULL;
	queue->data = NULL;
}

/* ==========================================================================
 * pthread_priority_enq()
 */
void pthread_prio_queue_enq(struct pthread_prio_queue * queue,
  struct pthread * pthread)
{
	int priority = pthread->pthread_priority;

	if (queue->next) {
		if (queue->level[priority].first) {
			pthread->next = (queue->level[priority].last)->next;
			(queue->level[priority].last)->next = pthread;
			queue->level[priority].last = pthread;
			return;
		} 
		if (priority != PTHREAD_MAX_PRIORITY) {
			int prev_priority;
			/* Find first higher priority thread queued on queue */
			for (prev_priority = priority + 1; prev_priority <=
			  PTHREAD_MAX_PRIORITY; prev_priority++) {
				if (queue->level[prev_priority].first) {
					pthread->next = (queue->level[prev_priority].last)->next;
					(queue->level[prev_priority].last)->next = pthread;
					queue->level[priority].first = pthread;
					queue->level[priority].last = pthread;
					return;
				}
			}
		}
	}
	queue->level[priority].first = pthread;
	queue->level[priority].last = pthread;
	pthread->next = queue->next;
	queue->next = pthread;
}

/* ==========================================================================
 * pthread_prio_queue_deq()
 */
struct pthread * pthread_prio_queue_deq(struct pthread_prio_queue * queue)
{
	struct pthread * pthread;
	int priority;

	if (pthread = queue->next) {
		priority = queue->next->pthread_priority;
		if (queue->level[priority].first == queue->level[priority].last) {
			queue->level[priority].first = NULL;
			queue->level[priority].last = NULL;
		} else {
			queue->level[priority].first = pthread->next;
		}
		queue->next = pthread->next;	
		pthread->next = NULL;
	}
	return(pthread);
}

/* ==========================================================================
 * pthread_prio_queue_remove()
 */
int pthread_prio_queue_remove(struct pthread_prio_queue *queue, 
			      struct pthread *thread)
{
  /* XXX This is slow, should start with thread priority */
  int priority = thread->pthread_priority;
  struct pthread **current = &(queue->level[priority].first);
  struct pthread *prev = NULL;

  if (thread==*current) {
    int current_priority=priority+1;
		
    if (*current == queue->next){
      pthread_prio_queue_deq(queue);
      thread->next = NULL;
      return(OK);
    }
    for (current_priority; current_priority <= PTHREAD_MAX_PRIORITY;
	 current_priority++) {
      if (queue->level[current_priority].last) {
	queue->level[current_priority].last->next = (*current)->next;
	if ((*current)->next &&
	    (*current)->next->pthread_priority == priority) 
	  queue->level[priority].first = (*current)->next;
	else {
	  queue->level[priority].first = NULL;
	  queue->level[priority].last  = NULL;
	}
	thread->next = NULL;
	return(OK);
      }
    }
  }

  if (*current == NULL)			/* Mati Sauks */
  {
    return (NOTOK);
  }
  for (prev=*current,current=&((*current)->next); 
       *current && ((*current)->pthread_priority == priority);
       prev=*current,current=&((*current)->next)) {
    if (*current == thread) {
      if (*current == queue->level[priority].last) {
	queue->level[priority].last = prev;
      }
			
      *current = (*current)->next;
      thread->next=NULL;
      return(OK);
    }
  }
  return(NOTOK);
}

