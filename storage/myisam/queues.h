/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef QUEUES_INCLUDED
#define QUEUES_INCLUDED

/**
  @file storage/myisam/queues.h
  Code for handling of priority Queues.
  Implementation of queues from "Algoritms in C" by Robert Sedgewick.
  By monty.
*/

#include <stddef.h>
#include <sys/types.h>

#include "my_inttypes.h"
#include "mysql/psi/psi_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

struct QUEUE {
  uchar **root;
  void *first_cmp_arg;
  uint elements;
  uint max_elements;
  uint offset_to_key; /* compare is done on element+offset */
  int max_at_top;     /* Normally 1, set to -1 if queue_top gives max */
  int (*compare)(void *, uchar *, uchar *);
  uint auto_extent;
};

void _downheap(QUEUE *queue, uint idx);
void queue_fix(QUEUE *queue);

#define queue_top(queue) ((queue)->root[1])
#define queue_element(queue, index) ((queue)->root[index + 1])
#define queue_end(queue) ((queue)->root[(queue)->elements])

static inline void queue_replaced(QUEUE *queue) { _downheap(queue, 1); }

static inline void queue_set_max_at_top(QUEUE *queue, int set_arg) {
  queue->max_at_top = set_arg ? -1 : 1;
}

typedef int (*queue_compare)(void *, uchar *, uchar *);

int init_queue(QUEUE *queue, PSI_memory_key psi_key, uint max_elements,
               uint offset_to_key, bool max_at_top, queue_compare compare,
               void *first_cmp_arg);
int reinit_queue(QUEUE *queue, PSI_memory_key psi_key, uint max_elements,
                 uint offset_to_key, bool max_at_top, queue_compare compare,
                 void *first_cmp_arg);
void delete_queue(QUEUE *queue);
void queue_insert(QUEUE *queue, uchar *element);
uchar *queue_remove(QUEUE *queue, uint idx);

static inline void queue_remove_all(QUEUE *queue) { queue->elements = 0; }

static inline bool queue_is_full(QUEUE *queue) {
  return queue->elements == queue->max_elements;
}

static inline bool is_queue_inited(QUEUE *queue) { return queue->root != NULL; }

#ifdef __cplusplus
}
#endif

#endif  // QUEUES_INCLUDED
