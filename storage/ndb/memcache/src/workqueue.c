/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

/* Implementation of a multi-thread work queue */

/* There are several locks here.
   The signal_lock mutex protects signaling of queue full/empty conditions.
   The freelist is written only by the producer thread, and updated atomically.
   The worklist is written only by consumers, and updated atomically.
   In each of these cases the atomic update protects an item that may be 
   *read* by the other side. 
   Finally the consumer spinlock ensures that only one consumer at a time 
   has access to the worklist. 
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "workqueue.h"
#include "timing.h"

/* Initialize a queue that has been allocated */ 
int workqueue_init(struct workqueue *q, int size, int nconsumers) {
  
  q->size = size;
  q->p_mask = q->c_mask = size - 1;  
  assert(size > 0);  /* these two asserts prove that size is a power of 2 */
  assert((size & q->p_mask) == 0);
  q->freelist = 0;            /* updated only by producers */
  q->worklist = 0;            /* updated only by consumers */
  q->depth = 0;                
  q->minfree = size / 16;     
  assert(nconsumers > 0);
  q->threads = nconsumers - 1;  /* boolean false if there is just one consumer */
    
  pthread_cond_init(& q->not_empty, NULL);  
  pthread_cond_init(& q->not_full, NULL);  
  pthread_mutex_init(& q->signal_lock, NULL);
  
  q->p_items = calloc(q->size, sizeof(void *));
  q->c_items = q->p_items;     /* consumer's copy */
  q->is_active = 1;
  return 0;
}


/* Free the resources used internally by a queue */
void workqueue_destroy(struct workqueue *q) {
  pthread_cond_destroy(& q->not_empty);
  pthread_cond_destroy(& q->not_full);
  pthread_mutex_destroy(& q->signal_lock);
  free(q->p_items);
}


/* Add an item to a workqueue.  */
int workqueue_add (struct workqueue *q, void *item) {
  int did_cas;
  if(! q->is_active) return 0;
  /* You cannot put a null pointer on the queue, because the consumer function
     returns null to signal an exception. */
  if(item == 0) return 0;    

  while(((q->freelist + 1) & q->p_mask) == q->worklist) {  /* the queue is full */
    pthread_mutex_lock(& q->signal_lock);
    pthread_cond_signal(& q->not_empty);                 /* ping a consumer */
    pthread_cond_wait(& q->not_full, & q->signal_lock);  /* sleep */
    pthread_mutex_unlock(& q->signal_lock);
  }
    
  do {
    /* Place the item on the queue */
    int item_idx = q->freelist;
    q->p_items[item_idx] = item;
    
    /* Now commit, by updating the free item pointer */
    did_cas = atomic_cmp_swap_int(& q->freelist, item_idx, ((item_idx + 1) & q->p_mask));
  } while (did_cas == 0);  

  q->depth++;
    
  /* Signal a consumer thread that there is something to do */
  pthread_cond_signal(& q->not_empty);
  
  return 1;
}


/* Sleep until an item is available on the queue, then return it.
   Return null if the queue has been aborted and is empty. 
   
   Implementation notes:
   Test for q->threads before trying to get the spinlock, to avoid issuing 
   a memory barrier when there is just one consumer.
*/
void * workqueue_consumer_wait(struct workqueue *q) {  
  int did_cas;
  register int item_idx;
  void *work_item = NULL;
 
  wait_for_work: 
  while(q->is_active && q->worklist == q->freelist) {  /* i.e. nothing to do */
    pthread_mutex_lock(& q->signal_lock);
    pthread_cond_signal(& q->not_full);                 /* ping the producer */
    pthread_cond_wait(& q->not_empty, & q->signal_lock);            /* sleep */
    pthread_mutex_unlock(& q->signal_lock);
  }

  /* Even if the queue has been shut down, acquire the spinlock. */
  if(q->threads) while(! atomic_cmp_swap_int(& q->consumer_spinlock, 0, 1));
   
  if(q->worklist == q->freelist) {  /* Nothing to do! */
    /* Release the spinlock: */
    if(q->threads) while(! atomic_cmp_swap_int(& q->consumer_spinlock, 1, 0));
  
    if(q->is_active) 
      goto wait_for_work; 
    /* else fall through and return null. */
  }
  else {  
    /* Get the item from the queue */
    do {
      item_idx = q->worklist;
      work_item = q->c_items[item_idx];
      
      /* To commit: update the worklist item pointer */
      did_cas = atomic_cmp_swap_int(& q->worklist, item_idx, 
                                    (item_idx + 1) & q->c_mask);
    } while (did_cas == 0); 

    q->depth--;
    
    /* Release the spinlock: */
    if(q->threads) while(! atomic_cmp_swap_int(& q->consumer_spinlock, 1, 0));
            
    /* Sometimes we signal the producer that there is space on the queue.  
      But not always.  When the queue is full, this allows for some congestion 
      relief before the producer is allowed to send again. 
    */
    if(q->worklist % q->minfree == 0 && q->is_active) {
      pthread_cond_signal(& q->not_full);
    }
  }
  
  return work_item;
}


void workqueue_abort(struct workqueue *q) {
  atomic_cmp_swap_int(& q->is_active, 1, 0);
  pthread_cond_broadcast(& q->not_full);
  pthread_cond_broadcast(& q->not_empty);
}


int workqueue_is_aborted(struct workqueue *q) {
  return q->is_active ? 0 : 1;
}

