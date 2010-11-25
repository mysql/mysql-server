/* Copyright (C) 2010 Monty Program Ab
   All Rights reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the following disclaimer
      in the documentation and/or other materials provided with the
      distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
  <COPYRIGHT HOLDER> BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.
*/

/*
  This code originates from the Unireg project.

  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.

  The queue can optionally store the position in queue in the element
  that is in the queue. This allows one to remove any element from the queue
  in O(1) time.

  Optimisation of _downheap() and queue_fix() is inspired by code done
  by Mikael Ronström, based on an optimisation of _downheap from
  Exercise 7.51 in "Data Structures & Algorithms in C++" by Mark Allen
  Weiss, Second Edition.
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
    offset_to_queue_pos If <> 0, then offset+1 in element to store position
                        in queue (for fast delete of element in queue)
    auto_extent         When the queue is full and there is insert operation
                        extend the queue.

  NOTES
    Will allocate max_element pointers for queue array

  RETURN
    0	ok
    1	Could not allocate memory
*/

int init_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
	       pbool max_at_top, int (*compare) (void *, uchar *, uchar *),
	       void *first_cmp_arg, uint offset_to_queue_pos,
               uint auto_extent)
               
{
  DBUG_ENTER("init_queue");
  if ((queue->root= (uchar **) my_malloc((max_elements + 1) * sizeof(void*),
					 MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  queue->elements=      	0;
  queue->compare=       	compare;
  queue->first_cmp_arg= 	first_cmp_arg;
  queue->max_elements=  	max_elements;
  queue->offset_to_key= 	offset_to_key;
  queue->offset_to_queue_pos=   offset_to_queue_pos;
  queue->auto_extent=   	auto_extent;
  queue_set_max_at_top(queue, max_at_top);
  DBUG_RETURN(0);
}


/*
  Reinitialize queue for other usage

  SYNOPSIS
    reinit_queue()
    queue		Queue to initialise
    For rest of arguments, see init_queue() above

  NOTES
    This will delete all elements from the queue.  If you don't want this,
    use resize_queue() instead.

  RETURN
    0			ok
    1			Wrong max_elements; Queue has old size
*/

int reinit_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
		 pbool max_at_top, int (*compare) (void *, uchar *, uchar *),
		 void *first_cmp_arg, uint offset_to_queue_pos,
                 uint auto_extent)
{
  DBUG_ENTER("reinit_queue");
  queue->elements=		0;
  queue->compare=		compare;
  queue->first_cmp_arg=		first_cmp_arg;
  queue->offset_to_key=		offset_to_key;
  queue->offset_to_queue_pos=   offset_to_queue_pos;
  queue->auto_extent= 		auto_extent;
  queue_set_max_at_top(queue, max_at_top);
  DBUG_RETURN(resize_queue(queue, max_elements));
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
  uchar **new_root;
  DBUG_ENTER("resize_queue");
  if (queue->max_elements == max_elements)
    DBUG_RETURN(0);
  if ((new_root= (uchar **) my_realloc((void *)queue->root,
                                       (max_elements + 1)* sizeof(void*),
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
  queue->root=0;                              /* Allow multiple calls */
  DBUG_VOID_RETURN;
}


/*
  Insert element in queue

  SYNOPSIS
    queue_insert()
    queue		Queue to use
    element		Element to insert
*/

void queue_insert(register QUEUE *queue, uchar *element)
{
  reg2 uint idx, next;
  uint offset_to_queue_pos= queue->offset_to_queue_pos;
  DBUG_ASSERT(queue->elements < queue->max_elements);

  idx= ++queue->elements;
  /* max_at_top swaps the comparison if we want to order by desc */
  while (idx > 1 &&
         (queue->compare(queue->first_cmp_arg,
                         element + queue->offset_to_key,
                         queue->root[(next= idx >> 1)] +
                         queue->offset_to_key) * queue->max_at_top) < 0)
  {
    queue->root[idx]= queue->root[next];
    if (offset_to_queue_pos)
      (*(uint*) (queue->root[idx] + offset_to_queue_pos-1))= idx;
    idx= next;
  }
  queue->root[idx]= element;
  if (offset_to_queue_pos)
    (*(uint*) (element+ offset_to_queue_pos-1))= idx;
}


/*
  Like queue_insert, but resize queue if queue is full

  SYNOPSIS
    queue_insert_safe()
    queue		Queue to use
    element		Element to insert

  RETURN
    0	OK
    1	Cannot allocate more memory
    2   auto_extend is 0; No insertion done
*/

int queue_insert_safe(register QUEUE *queue, uchar *element)
{

  if (queue->elements == queue->max_elements)
  {
    if (!queue->auto_extent)
      return 2;
    if (resize_queue(queue, queue->max_elements + queue->auto_extent))
      return 1;
  }
  
  queue_insert(queue, element);
  return 0;
}


/*
  Remove item from queue

  SYNOPSIS
    queue_remove()
    queue		Queue to use
    element		Index of element to remove.
			First element in queue is 'queue_first_element(queue)'

  RETURN
   pointer to removed element
*/

uchar *queue_remove(register QUEUE *queue, uint idx)
{
  uchar *element;
  DBUG_ASSERT(idx >= 1 && idx <= queue->elements);
  element= queue->root[idx];
  _downheap(queue, idx, queue->root[queue->elements--]);
  return element;
}


/*
  Add element to fixed position and update heap

  SYNOPSIS
    _downheap()
    queue	Queue to use
    idx         Index of element to change
    element     Element to store at 'idx'

  NOTE
    This only works if element is >= all elements <= start_idx
*/

void _downheap(register QUEUE *queue, uint start_idx, uchar *element)
{
  uint elements,half_queue,offset_to_key, next_index, offset_to_queue_pos;
  register uint idx= start_idx;
  my_bool first= TRUE;

  offset_to_key=queue->offset_to_key;
  offset_to_queue_pos= queue->offset_to_queue_pos;
  half_queue= (elements= queue->elements) >> 1;

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
      if (offset_to_queue_pos)
        (*(uint*) (element + offset_to_queue_pos-1))= idx;
      return;
    }
    first= FALSE;
    queue->root[idx]= queue->root[next_index];
    if (offset_to_queue_pos)
      (*(uint*) (queue->root[idx] + offset_to_queue_pos-1))= idx;
    idx=next_index;
  }

  /*
    Insert the element into the right position. This is the same code
    as we have in queue_insert()
  */
  while ((next_index= (idx >> 1)) > start_idx &&
         queue->compare(queue->first_cmp_arg,
                        element+offset_to_key,
                        queue->root[next_index]+offset_to_key)*
         queue->max_at_top < 0)
  {
    queue->root[idx]= queue->root[next_index];
    if (offset_to_queue_pos)
      (*(uint*) (queue->root[idx] + offset_to_queue_pos-1))= idx;
    idx= next_index;
  }
  queue->root[idx]= element;
  if (offset_to_queue_pos)
    (*(uint*) (element + offset_to_queue_pos-1))= idx;
}


/*
  Fix heap when every element was changed.

  SYNOPSIS
    queue_fix()
    queue	Queue to use
*/

void queue_fix(QUEUE *queue)
{
  uint i;
  for (i= queue->elements >> 1; i > 0; i--)
    _downheap(queue, i, queue_element(queue, i));
}


/*
  Change element at fixed position

  SYNOPSIS
    queue_replace()
    queue	Queue to use
    idx         Index of element to change
    element     Element to store at 'idx'
*/

void queue_replace(QUEUE *queue, uint idx)
{
  uchar *element= queue->root[idx];
  DBUG_ASSERT(idx >= 1 && idx <= queue->elements);
  queue_remove(queue, idx);
  queue_insert(queue, element);
}
