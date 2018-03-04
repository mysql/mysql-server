/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


/* Lock-free SPSC queue.  Safe for one consumer thread and one producer.
 *
 */

#include <stdlib.h>

#include "ndbmemcache_global.h"
#include "atomics.h"

#define VPSZ sizeof(void *)
#define CACHE_PADDING (CACHE_LINE_SIZE - VPSZ)


template<typename T> class Queue {
private:
  struct Node {
    T * value;
    Node * volatile next;
  };

  Node *head;
  char padding1[CACHE_PADDING];

  Node *sep;
  char padding2[CACHE_PADDING];
  
  Node *tail;
  Node *nodelist;
  char *nodepool;
  atomic_int32_t is_active;

public:   /* public instance variable */

public:   /* public interface methods */
  Queue(int maxnodes) {
    /* Initialize the node allocation pool */
    maxnodes += 1;  // add one for a dummy node at the head.
    assert(sizeof(Node) < CACHE_LINE_SIZE);
    nodepool = (char *) calloc(maxnodes, CACHE_LINE_SIZE);
    nodelist = 0;
    for(int i = 0; i < maxnodes; i++) { 
      Node *n = (Node *) (nodepool + (i * CACHE_LINE_SIZE));
      n->next = nodelist;
      nodelist = n;
    }
    
    /* Initialize the queue */
    head = node_alloc(0);   // a dummy node
    sep = head;
    tail = sep;          // an empty queue
    
    is_active = 1;
  };
  
  ~Queue() {
    free(nodepool);
  };
  
  T * consume() {
    T * val = 0;
    Node *nxt = sep->next;
    if(nxt) {             /* queue is non-empty */
      val = nxt->value;   /* the value to be returned. */
      nxt->value = 0;     /* This node becomes the new (empty) dummy node... */
      sep = nxt;          /* at the head of the queue. */
    }
    return val;
  };
  
  void produce(T *t) {
    /* First do deferred cleanup of the old nodes between head and sep */
    while(head != sep) {
      Node *tmp = head;
      assert(tmp->value == 0); 
      head = tmp->next;
      node_free(tmp);
    }
    /* Now add a node at the tail of the queue */
    Node * n = node_alloc(t);
    atomic_set_ptr((void * volatile *)& tail->next, n);
    atomic_barrier();
    tail = n;
  };
  
  void abort() {
    atomic_cmp_swap_int(& is_active, 1, 0);
  }
  
  bool is_aborted() {
    return (is_active == 0);
  }
  
private:  /* internal allocator */
  Node *node_alloc(T *val) {
    assert(nodelist);
    Node *n = nodelist;  
    nodelist = n->next;
    n->next = 0;
    n->value = val;
    return n;
  };
  
  void node_free(Node *n) {
    n->next = nodelist;
    nodelist = n;
  };
};

