/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.
*/

#include "mysys_priv.h"
#include "mysys_err.h"
#include <queues.h>


/* Init queue */

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
  Reinitialize queue for new usage;  Note that you can't currently resize
  the number of elements!  If you need this, fix it :)
*/


int reinit_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
               pbool max_at_top, int (*compare) (void *, byte *, byte *),
               void *first_cmp_arg)
{
  DBUG_ENTER("reinit_queue");
  if (queue->max_elements < max_elements)
    /* It's real easy to do realloc here, just don't want to bother */
    DBUG_RETURN(my_errno=EE_OUTOFMEMORY);

  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->offset_to_key=offset_to_key;
  queue->max_at_top= max_at_top ? (-1 ^ 1) : 0;
  DBUG_RETURN(0);
}

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
  reg2 uint idx,next;
  int cmp;

#ifndef DBUG_OFF
  if (queue->elements < queue->max_elements)
#endif
  {
    queue->root[0]=element;
    idx= ++queue->elements;

    /* max_at_top swaps the comparison if we want to order by desc */
    while ((cmp=queue->compare(queue->first_cmp_arg,
			       element+queue->offset_to_key,
			       queue->root[(next=idx >> 1)] +
			       queue->offset_to_key)) &&
	   (cmp ^ queue->max_at_top) < 0)
    {
      queue->root[idx]=queue->root[next];
      idx=next;
    }
    queue->root[idx]=element;
  }
}

	/* Remove item from queue */
	/* Returns pointer to removed element */

byte *queue_remove(register QUEUE *queue, uint idx)
{
#ifndef DBUG_OFF
  if (idx >= queue->max_elements)
    return 0;
#endif
  {
    byte *element=queue->root[++idx];		/* Intern index starts from 1 */
    queue->root[idx]=queue->root[queue->elements--];
    _downheap(queue,idx);
    return element;
  }
}


	/* Fix when element on top has been replaced */

#ifndef queue_replaced
void queue_replaced(queue)
QUEUE *queue;
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
