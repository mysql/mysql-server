/*
   Copyright (c) 2007, 2008, Sun Microsystems, Inc,
   Copyright (c) 2011, 2012, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef WQUEUE_INCLUDED
#define WQUEUE_INCLUDED

#include <my_global.h>
#include <my_pthread.h>

/* info about requests in a waiting queue */
typedef struct st_pagecache_wqueue
{
  struct st_my_thread_var *last_thread;         /* circular list of waiting
                                                   threads */
} WQUEUE;

void wqueue_link_into_queue(WQUEUE *wqueue, struct st_my_thread_var *thread);
void wqueue_unlink_from_queue(WQUEUE *wqueue, struct st_my_thread_var *thread);
void wqueue_add_to_queue(WQUEUE *wqueue, struct st_my_thread_var *thread);
void wqueue_add_and_wait(WQUEUE *wqueue,
                         struct st_my_thread_var *thread,
                         mysql_mutex_t *lock);
void wqueue_release_queue(WQUEUE *wqueue);
void wqueue_release_one_locktype_from_queue(WQUEUE *wqueue);

#endif
