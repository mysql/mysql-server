/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/myisam/queues.cc
  Code for handling of priority Queues.
  Implementation of queues from "Algoritms in C" by Robert Sedgewick.
  An optimisation of _downheap suggested in Exercise 7.51 in "Data
  Structures & Algorithms in C++" by Mark Allen Weiss, Second Edition
  was implemented by Mikael Ronstrom 2005. Also the O(N) algorithm
  of queue_fix was implemented.
*/

#include "storage/myisam/queues.h"

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_macros.h"
#include "my_sys.h"
#include "my_systime.h"
#include "mysql/service_mysql_alloc.h"
#include "storage/myisam/myisamdef.h"

static int resize_queue(QUEUE *queue, PSI_memory_key key, uint max_elements);

/*
  Init queue

  SYNOPSIS
    init_queue()
    queue		Queue to initialise
    max_elements	Max elements that will be put in queue
    offset_to_key	Offset to key in element stored in queue
			Used when sending pointers to compare function
    max_at_top		Set to 1 if you want biggest element on top.
    compare		Compare function for elements, takes 3 arguments.
    first_cmp_arg	First argument to compare function

  NOTES
    Will allocate max_element pointers for queue array

  RETURN
    0	ok
    1	Could not allocate memory
*/

extern "C" int init_queue(QUEUE *queue, PSI_memory_key psi_key,
                          uint max_elements, uint offset_to_key,
                          bool max_at_top,
                          int (*compare) (void *, uchar *, uchar *),
                          void *first_cmp_arg)
{
  DBUG_ENTER("init_queue");
  if ((queue->root= (uchar **) my_malloc(psi_key,
                                         (max_elements+1)*sizeof(void*),
					 MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->max_elements=max_elements;
  queue->offset_to_key=offset_to_key;
  queue_set_max_at_top(queue, max_at_top);
  DBUG_RETURN(0);
}



/*
  Init queue, uses init_queue internally for init work but also accepts
  auto_extent as parameter

  SYNOPSIS
    init_queue_ex()
    queue		Queue to initialise
    max_elements	Max elements that will be put in queue
    offset_to_key	Offset to key in element stored in queue
			Used when sending pointers to compare function
    max_at_top		Set to 1 if you want biggest element on top.
    compare		Compare function for elements, takes 3 arguments.
    first_cmp_arg	First argument to compare function
    auto_extent         When the queue is full and there is insert operation
                        extend the queue.

  NOTES
    Will allocate max_element pointers for queue array

  RETURN
    0	ok
    1	Could not allocate memory
*/

extern "C" int init_queue_ex(QUEUE *queue, PSI_memory_key psi_key, uint max_elements,
                             uint offset_to_key, bool max_at_top,
                             int (*compare) (void *, uchar *, uchar *),
                             void *first_cmp_arg, uint auto_extent)
{
  int ret;
  DBUG_ENTER("init_queue_ex");

  if ((ret= init_queue(queue, psi_key, max_elements, offset_to_key, max_at_top, compare,
                       first_cmp_arg)))
    DBUG_RETURN(ret);
  
  queue->auto_extent= auto_extent;
  DBUG_RETURN(0);
}

/*
  Reinitialize queue for other usage

  SYNOPSIS
    reinit_queue()
    queue		Queue to initialise
    max_elements	Max elements that will be put in queue
    offset_to_key	Offset to key in element stored in queue
			Used when sending pointers to compare function
    max_at_top		Set to 1 if you want biggest element on top.
    compare		Compare function for elements, takes 3 arguments.
    first_cmp_arg	First argument to compare function

  NOTES
    This will delete all elements from the queue.  If you don't want this,
    use resize_queue() instead.

  RETURN
    0			ok
    EE_OUTOFMEMORY	Wrong max_elements
*/

extern "C" int reinit_queue(QUEUE *queue, PSI_memory_key psi_key, uint max_elements, uint offset_to_key,
                            bool max_at_top,
                            int (*compare) (void *, uchar *, uchar *),
                            void *first_cmp_arg)
{
  DBUG_ENTER("reinit_queue");
  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->offset_to_key=offset_to_key;
  queue_set_max_at_top(queue, max_at_top);
  resize_queue(queue, psi_key, max_elements);
  DBUG_RETURN(0);
}


/*
  Resize queue

  SYNOPSIS
    resize_queue()
    queue			Queue
    max_elements		New max size for queue

  NOTES
    If you resize queue to be less than the elements you have in it,
    the extra elements will be deleted

  RETURN
    0	ok
    1	Error.  In this case the queue is unchanged
*/

static int resize_queue(QUEUE *queue, PSI_memory_key psi_key, uint max_elements)
{
  uchar **new_root;
  DBUG_ENTER("resize_queue");
  if (queue->max_elements == max_elements)
    DBUG_RETURN(0);
  if ((new_root= (uchar **) my_realloc(psi_key,
                                       (void *)queue->root,
				      (max_elements+1)*sizeof(void*),
				      MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  set_if_smaller(queue->elements, max_elements);
  queue->max_elements= max_elements;
  queue->root= new_root;
  DBUG_RETURN(0);
}


/*
  Delete queue

  SYNOPSIS
   delete_queue()
   queue		Queue to delete

  IMPLEMENTATION
    Just free allocated memory.

  NOTES
    Can be called safely multiple times
*/

void delete_queue(QUEUE *queue)
{
  DBUG_ENTER("delete_queue");
  my_free(queue->root);
  queue->root= NULL;
  DBUG_VOID_RETURN;
}


	/* Code for insert, search and delete of elements */

void queue_insert(QUEUE *queue, uchar *element)
{
  uint idx, next;
  DBUG_ASSERT(queue->elements < queue->max_elements);
  queue->root[0]= element;
  idx= ++queue->elements;
  /* max_at_top swaps the comparison if we want to order by desc */
  while ((queue->compare(queue->first_cmp_arg,
                         element + queue->offset_to_key,
                         queue->root[(next= idx >> 1)] +
                         queue->offset_to_key) * queue->max_at_top) < 0)
  {
    queue->root[idx]= queue->root[next];
    idx= next;
  }
  queue->root[idx]= element;
}

	/* Remove item from queue */
	/* Returns pointer to removed element */

uchar *queue_remove(QUEUE *queue, uint idx)
{
  uchar *element;
  DBUG_ASSERT(idx < queue->max_elements);
  element= queue->root[++idx];  /* Intern index starts from 1 */
  queue->root[idx]= queue->root[queue->elements--];
  _downheap(queue, idx);
  return element;
}


void _downheap(QUEUE *queue, uint idx)
{
  uchar *element;
  uint elements,half_queue,offset_to_key, next_index;
  bool first= TRUE;
  uint start_idx= idx;

  offset_to_key=queue->offset_to_key;
  element=queue->root[idx];
  half_queue=(elements=queue->elements) >> 1;

  while (idx <= half_queue)
  {
    next_index=idx+idx;
    if (next_index < elements &&
	(queue->compare(queue->first_cmp_arg,
			queue->root[next_index]+offset_to_key,
			queue->root[next_index+1]+offset_to_key) *
	 queue->max_at_top) > 0)
      next_index++;
    if (first && 
        (((queue->compare(queue->first_cmp_arg,
                          queue->root[next_index]+offset_to_key,
                          element+offset_to_key) * queue->max_at_top) >= 0)))
    {
      queue->root[idx]= element;
      return;
    }
    queue->root[idx]=queue->root[next_index];
    idx=next_index;
    first= FALSE;
  }

  next_index= idx >> 1;
  while (next_index > start_idx)
  {
    if ((queue->compare(queue->first_cmp_arg,
                       queue->root[next_index]+offset_to_key,
                       element+offset_to_key) *
         queue->max_at_top) < 0)
      break;
    queue->root[idx]=queue->root[next_index];
    idx=next_index;
    next_index= idx >> 1;
  }
  queue->root[idx]=element;
}

/*
  Fix heap when every element was changed.
*/

void queue_fix(QUEUE *queue)
{
  uint i;
  for (i= queue->elements >> 1; i > 0; i--)
    _downheap(queue, i);
}
