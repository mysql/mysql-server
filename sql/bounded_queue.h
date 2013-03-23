/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef BOUNDED_QUEUE_INCLUDED
#define BOUNDED_QUEUE_INCLUDED

#include <string.h>
#include "my_global.h"
#include "my_base.h"
#include "my_sys.h"
#include "queues.h"

class Sort_param;

/**
  A priority queue with a fixed, limited size.

  This is a wrapper on top of QUEUE and the queue_xxx() functions.
  It keeps the top-N elements which are inserted.

  Elements of type Element_type are pushed into the queue.
  For each element, we call a user-supplied keymaker_function,
  to generate a key of type Key_type for the element.
  Instances of Key_type are compared with the user-supplied compare_function.

  The underlying QUEUE implementation needs one extra element for replacing
  the lowest/highest element when pushing into a full queue.
 */
template<typename Element_type, typename Key_type>
class Bounded_queue
{
public:
  Bounded_queue()
  {
    memset(&m_queue, 0, sizeof(m_queue));
  }

  ~Bounded_queue()
  {
    delete_queue(&m_queue);
  }

  /**
     Function for making sort-key from input data.
     @param param Sort parameters.
     @param to    Where to put the key.
     @param from  The input data.
  */
  typedef void (*keymaker_function)(Sort_param *param,
                                    Key_type *to,
                                    Element_type *from);

  /**
     Function for comparing two keys.
     @param  n Pointer to number of bytes to compare.
     @param  a First key.
     @param  b Second key.
     @retval -1, 0, or 1 depending on whether the left argument is 
             less than, equal to, or greater than the right argument.
   */
  typedef int (*compare_function)(size_t *n, Key_type **a, Key_type **b);

  /**
    Initialize the queue.

    @param max_elements   The size of the queue.
    @param max_at_top     Set to true if you want biggest element on top.
           false: We keep the n largest elements.
                  pop() will return the smallest key in the result set.
           true:  We keep the n smallest elements.
                  pop() will return the largest key in the result set.
    @param compare        Compare function for elements, takes 3 arguments.
                          If NULL, we use get_ptr_compare(compare_length).
    @param compare_length Length of the data (i.e. the keys) used for sorting.
    @param keymaker       Function which generates keys for elements.
    @param sort_param     Sort parameters.
    @param sort_keys      Array of pointers to keys to sort.

    @retval 0 OK, 1 Could not allocate memory.

    We do *not* take ownership of any of the input pointer arguments.
   */
  int init(ha_rows max_elements, bool max_at_top,
           compare_function compare, size_t compare_length,
           keymaker_function keymaker, Sort_param *sort_param,
           Key_type **sort_keys);

  /**
    Pushes an element on the queue.
    If the queue is already full, we discard one element.
    Calls keymaker_function to generate a key for the element.

    @param element        The element to be pushed.
   */
  void push(Element_type *element);

  /**
    Removes the top element from the queue.

    @retval Pointer to the (key of the) removed element.

    @note This function is for unit testing, where we push elements into to the
          queue, and test that the appropriate keys are retained.
          Interleaving of push() and pop() operations has not been tested.
   */
  Key_type **pop()
  {
    // Don't return the extra element to the client code.
    if (queue_is_full((&m_queue)))
      queue_remove(&m_queue, 0);
    DBUG_ASSERT(m_queue.elements > 0);
    if (m_queue.elements == 0)
      return NULL;
    return reinterpret_cast<Key_type**>(queue_remove(&m_queue, 0));
  }

  /**
    The number of elements in the queue.
   */
  uint num_elements() const { return m_queue.elements; }

  /**
    Is the queue initialized?
   */
  bool is_initialized() const { return m_queue.max_elements > 0; }

private:
  Key_type         **m_sort_keys;
  size_t             m_compare_length;
  keymaker_function  m_keymaker;
  Sort_param        *m_sort_param;
  st_queue           m_queue;
};


template<typename Element_type, typename Key_type>
int Bounded_queue<Element_type, Key_type>::init(ha_rows max_elements,
                                                bool max_at_top,
                                                compare_function compare,
                                                size_t compare_length,
                                                keymaker_function keymaker,
                                                Sort_param *sort_param,
                                                Key_type **sort_keys)
{
  DBUG_ASSERT(sort_keys != NULL);

  m_sort_keys=      sort_keys;
  m_compare_length= compare_length;
  m_keymaker=       keymaker;
  m_sort_param=     sort_param;
  // init_queue() takes an uint, and also does (max_elements + 1)
  if (max_elements >= (UINT_MAX - 1))
    return 1;
  if (compare == NULL)
    compare=
      reinterpret_cast<compare_function>(get_ptr_compare(compare_length));

  DBUG_EXECUTE_IF("bounded_queue_init_fail",
                  DBUG_SET("+d,simulate_out_of_memory"););

  // We allocate space for one extra element, for replace when queue is full.
  return init_queue(&m_queue, (uint) max_elements + 1,
                    0, max_at_top,
                    reinterpret_cast<queue_compare>(compare),
                    &m_compare_length);
}


template<typename Element_type, typename Key_type>
void Bounded_queue<Element_type, Key_type>::push(Element_type *element)
{
  DBUG_ASSERT(is_initialized());
  if (queue_is_full((&m_queue)))
  {
    // Replace top element with new key, and re-order the queue.
    Key_type **pq_top= reinterpret_cast<Key_type **>(queue_top(&m_queue));
    (*m_keymaker)(m_sort_param, *pq_top, element);
    queue_replaced(&m_queue);
  } else {
    // Insert new key into the queue.
    (*m_keymaker)(m_sort_param, m_sort_keys[m_queue.elements], element);
    queue_insert(&m_queue,
                 reinterpret_cast<uchar*>(&m_sort_keys[m_queue.elements]));
  }
}

#endif  // BOUNDED_QUEUE_INCLUDED
