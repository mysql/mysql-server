
#ifndef _wqueue_h
#define _wqueue_h

#include <my_global.h>
#include <my_pthread.h>

/* info about requests in a waiting queue */
typedef struct st_pagecache_wqueue
{
  struct st_my_thread_var *last_thread;         /* circular list of waiting
                                                   threads */
} WQUEUE;

#ifdef THREAD
void wqueue_link_into_queue(WQUEUE *wqueue, struct st_my_thread_var *thread);
void wqueue_unlink_from_queue(WQUEUE *wqueue, struct st_my_thread_var *thread);
void wqueue_add_to_queue(WQUEUE *wqueue, struct st_my_thread_var *thread);
void wqueue_add_and_wait(WQUEUE *wqueue,
                         struct st_my_thread_var *thread,
                         pthread_mutex_t *lock);
void wqueue_release_queue(WQUEUE *wqueue);
void wqueue_release_one_locktype_from_queue(WQUEUE *wqueue);

#endif

#endif
