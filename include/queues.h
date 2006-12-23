/* Copyright (C) 2000 MySQL AB

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

/*
  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.
  Copyright Monty Program KB.
  By monty.
*/

#ifndef _queues_h
#define _queues_h
#ifdef	__cplusplus
extern "C" {
#endif

typedef struct st_queue {
  byte **root;
  void *first_cmp_arg;
  uint elements;
  uint max_elements;
  uint offset_to_key;			/* compare is done on element+offset */
  int max_at_top;			/* Set if queue_top gives max */
  int  (*compare)(void *, byte *,byte *);
} QUEUE;

#define queue_top(queue) ((queue)->root[1])
#define queue_element(queue,index) ((queue)->root[index+1])
#define queue_end(queue) ((queue)->root[(queue)->elements])
#define queue_replaced(queue) _downheap(queue,1)
typedef int (*queue_compare)(void *,byte *, byte *);

int init_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
	       pbool max_at_top, queue_compare compare,
	       void *first_cmp_arg);
int reinit_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
                 pbool max_at_top, queue_compare compare,
                 void *first_cmp_arg);
int resize_queue(QUEUE *queue, uint max_elements);
void delete_queue(QUEUE *queue);
void queue_insert(QUEUE *queue,byte *element);
byte *queue_remove(QUEUE *queue,uint idx);
#define queue_remove_all(queue) { (queue)->elements= 0; }
void _downheap(QUEUE *queue,uint idx);
void queue_fix(QUEUE *queue);
#define is_queue_inited(queue) ((queue)->root != 0)

#ifdef	__cplusplus
}
#endif
#endif
