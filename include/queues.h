/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.
  Copyright Monty Program KB.
  By monty.
*/

#ifndef _queues_h
#define _queues_h

#include "my_global.h"                          /* uchar */

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct st_queue {
  uchar **root;
  void *first_cmp_arg;
  uint elements;
  uint max_elements;
  uint offset_to_key;	/* compare is done on element+offset */
  int max_at_top;	/* Normally 1, set to -1 if queue_top gives max */
  int  (*compare)(void *, uchar *,uchar *);
  uint auto_extent;
} QUEUE;

#define queue_top(queue) ((queue)->root[1])
#define queue_element(queue,index) ((queue)->root[index+1])
#define queue_end(queue) ((queue)->root[(queue)->elements])
#define queue_replaced(queue) _downheap(queue,1)
#define queue_set_compare(queue, cmp) (queue)->compare= cmp
#define queue_set_cmp_arg(queue, set_arg) (queue)->first_cmp_arg= set_arg
#define queue_set_max_at_top(queue, set_arg) \
  (queue)->max_at_top= set_arg ? -1 : 1
typedef int (*queue_compare)(void *,uchar *, uchar *);

int init_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
	       pbool max_at_top, queue_compare compare,
	       void *first_cmp_arg);
int init_queue_ex(QUEUE *queue,uint max_elements,uint offset_to_key,
	       pbool max_at_top, queue_compare compare,
	       void *first_cmp_arg, uint auto_extent);
int reinit_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
                 pbool max_at_top, queue_compare compare,
                 void *first_cmp_arg);
int resize_queue(QUEUE *queue, uint max_elements);
void delete_queue(QUEUE *queue);
void queue_insert(QUEUE *queue,uchar *element);
int queue_insert_safe(QUEUE *queue, uchar *element);
uchar *queue_remove(QUEUE *queue,uint idx);
#define queue_remove_all(queue) { (queue)->elements= 0; }
#define queue_is_full(queue) (queue->elements == queue->max_elements)
void _downheap(QUEUE *queue,uint idx);
void queue_fix(QUEUE *queue);
#define is_queue_inited(queue) ((queue)->root != 0)

#ifdef	__cplusplus
}
#endif
#endif
