/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
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
#include <pthread.h>

#include "ndbmemcache_global.h"
#include "timing.h"
#include "atomics.h"

/* Bounded FIFO SPMC work queue:
     single producer, multiple consumers
*/          


#define Q_INT_SIZE sizeof(int)
#define Q_GROUP1_SIZE (4 + Q_INT_SIZE + Q_INT_SIZE + sizeof (void *))
#define Q_CONDVAR_SIZE sizeof(pthread_cond_t)
#define Q_MUTEX_SIZE sizeof(pthread_mutex_t)
#define Q_GROUP2_SIZE (4 + Q_CONDVAR_SIZE + Q_CONDVAR_SIZE + Q_MUTEX_SIZE)

struct workqueue { 
  /* this is the producer's cache line */
  atomic_int32_t freelist;             /*< producer's current free item */
  unsigned int size;                   /*< number of slots in the queue */
  unsigned int p_mask;                 /*< used for modulo division */
  void ** p_items;                     /*< the workqueue array */

  char padding1[CACHE_LINE_SIZE - Q_GROUP1_SIZE];

  /* these are used for empty/full signalling */
  atomic_int32_t   is_active;          /*< set to 0 when the queue is shut down */
  pthread_cond_t   not_empty;          /*< signal that there is data available */
  pthread_cond_t   not_full;           /*< signal that there is free space */
  pthread_mutex_t  signal_lock;        /*< mutex to protect empty/full signals */

  char padding2[CACHE_LINE_SIZE - (Q_GROUP2_SIZE % CACHE_LINE_SIZE)];
  
  volatile int depth;                  /*< heuristic indicator of queue depth */
  
  char padding3[CACHE_LINE_SIZE - Q_INT_SIZE];

  /* this is the consumer's cache line */
  int threads;                         /*< actually nconsumers - 1 */
  unsigned int c_mask;                 /*< consumer's copy of mask */
  unsigned int minfree;                /*< heuristic number of free slots desired */
  atomic_int32_t consumer_spinlock;    /*< for multiple consumer threads */
  atomic_int32_t worklist;             /*< consumer's current work item */
  void ** c_items;                     /*< consumer's copy of the array addr */
};


/* This API has C linkage */
DECLARE_FUNCTIONS_WITH_C_LINKAGE

/*< Initialize a queue that has been allocated 
    @param size *must be a power of 2*
    @param nconsumers the number of expected consumer threads
*/ 
int workqueue_init(struct workqueue *q, int size, int nconsumers);

/* Add an item to a workqueue.  This can be called by one producer thread. */
int workqueue_add (struct workqueue *q, void *item);

/* Sleep until an item is available on the queue, then fetch it. */
void * workqueue_consumer_wait(struct workqueue *q);

/* Returns true if at item is available on the queue */
int workqueue_consumer_poll(struct workqueue *q);

/* Free the internal resources used by a workqueue. 
   The caller is still responsible for freeing the workqueue itself. */
void workqueue_destroy(struct workqueue *q);

/* Abort an active workqueue */
void workqueue_abort(struct workqueue *q);

/* Find out if a workqueue has been aborted */
int workqueue_is_aborted(struct workqueue *q);

END_FUNCTIONS_WITH_C_LINKAGE

