/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
*/

#include "mysys_priv.h"
#include "mysys_err.h"
#include <queues.h>


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

int init_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
	       pbool max_at_top, int (*compare) (void *, byte *, byte *),
	       void *first_cmp_arg)
{
  DBUG_ENTER("init_queue");
  if ((queue->root= (byte **) my_malloc((max_elements+1)*sizeof(void*),
					 MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->max_elements=max_elements;
  queue->offset_to_key=offset_to_key;
  queue->max_at_top= max_at_top ? (-1 ^ 1) : 0;
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

int reinit_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
		 pbool max_at_top, int (*compare) (void *, byte *, byte *),
		 void *first_cmp_arg)
{
  DBUG_ENTER("reinit_queue");
  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->offset_to_key=offset_to_key;
  queue->max_at_top= max_at_top ? (-1 ^ 1) : 0;
  resize_queue(queue, max_elements);
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

int resize_queue(QUEUE *queue, uint max_elements)
{
  byte **new_root;
  DBUG_ENTER("resize_queue");
  if (queue->max_elements == max_elements)
    DBUG_RETURN(0);
  if ((new_root= (byte **) my_realloc((void *)queue->root,
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
  if (queue->root)
  {
    my_free((gptr) queue->root,MYF(0));
    queue->root=0;
  }
  DBUG_VOID_RETURN;
}


	/* Code for insert, search and delete of elements */

void queue_insert(register QUEUE *queue, byte *element)
{
  reg2 uint idx, next;
  int cmp;
  DBUG_ASSERT(queue->elements < queue->max_elements);
  queue->root[0]= element;
  idx= ++queue->elements;
  /* max_at_top swaps the comparison if we want to order by desc */
  while ((cmp= queue->compare(queue->first_cmp_arg,
                              element + queue->offset_to_key,
                              queue->root[(next= idx >> 1)] +
                              queue->offset_to_key)) &&
         (cmp ^ queue->max_at_top) < 0)
  {
    queue->root[idx]= queue->root[next];
    idx= next;
  }
  queue->root[idx]= element;
}

	/* Remove item from queue */
	/* Returns pointer to removed element */

byte *queue_remove(register QUEUE *queue, uint idx)
{
  byte *element;
  DBUG_ASSERT(idx < queue->max_elements);
  element= queue->root[++idx];  /* Intern index starts from 1 */
  queue->root[idx]= queue->root[queue->elements--];
  _downheap(queue, idx);
  return element;
}

	/* Fix when element on top has been replaced */

#ifndef queue_replaced
void queue_replaced(QUEUE *queue)
{
  _downheap(queue,1);
}
#endif

	/* Fix heap when index have changed */

void _downheap(register QUEUE *queue, uint idx)
{
  byte *element;
  uint elements,half_queue,next_index,offset_to_key;
  int cmp;

  offset_to_key=queue->offset_to_key;
  element=queue->root[idx];
  half_queue=(elements=queue->elements) >> 1;

  while (idx <= half_queue)
  {
    next_index=idx+idx;
    if (next_index < elements &&
	(queue->compare(queue->first_cmp_arg,
			queue->root[next_index]+offset_to_key,
			queue->root[next_index+1]+offset_to_key) ^
	 queue->max_at_top) > 0)
      next_index++;
    if ((cmp=queue->compare(queue->first_cmp_arg,
			    queue->root[next_index]+offset_to_key,
			    element+offset_to_key)) == 0 ||
	(cmp ^ queue->max_at_top) > 0)
      break;
    queue->root[idx]=queue->root[next_index];
    idx=next_index;
  }
  queue->root[idx]=element;
}


static int queue_fix_cmp(QUEUE *queue, void **a, void **b)
{
  return queue->compare(queue->first_cmp_arg,
			(byte*) (*a)+queue->offset_to_key,
			(byte*) (*b)+queue->offset_to_key);
}

/*
  Fix heap when every element was changed,
  actually, it can be done better, in linear time, not in n*log(n)
*/

void queue_fix(QUEUE *queue)
{
  qsort2(queue->root+1,queue->elements, sizeof(void *),
	 (qsort2_cmp)queue_fix_cmp, queue);
}
