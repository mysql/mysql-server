/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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
#include "my_config.h"
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "Queue.h"

#include "all_tests.h"

/* Test an instantiation of the Queue template class */

typedef struct q_test_obj {
  void *vp;
  int n;
} q_test_obj;

extern "C" {
  void * producer_thread(void *arg);
}

q_test_obj * get(Queue<q_test_obj> &q, int v);
const int n_loop_items = 50000;


int run_queue_test(QueryPlan *, Ndb *, int v) {
  Queue<q_test_obj> q(n_loop_items);
  
  /* nothing there */
  require(q.consume() == 0);

  /* First test: */
  q_test_obj * obj = new q_test_obj;  
  q.produce(obj);  
  sleep(1);  
  require(q.consume() == obj);
  q.produce(obj);
  sleep(1);
  require(q.consume() == obj);

  /* Second Test */
  q_test_obj *o1 = new q_test_obj;
  q_test_obj *o2 = new q_test_obj;
  q.produce(o1);
  q.produce(o2);
  require(q.consume() == o1);  // FIFO
  require(q.consume() == o2);
  require(q.consume() == 0);   // empty


  /* Third test */
  pthread_t producer_thd_id;
  pthread_create(&producer_thd_id, NULL, producer_thread, (void *) & q);

  for(int n = 1; n < n_loop_items ; n++) {
    q_test_obj *s = get(q, v);
    require(s);
    require(s->n == n);
  };
  
  /* end of queue */
  require(q.consume() == 0);
  
  pthread_join(producer_thd_id, NULL);
  
  
  pass;
}


q_test_obj * get(Queue<q_test_obj> &q, int v) {
  int l = 0;
  q_test_obj *obj = 0;
 
  do {                   /* Tight loop around consume */
    obj = q.consume();
    l++;
  } while(obj == 0);

  if(v && l > 1) printf("Looped %d times then got # %d\n", l, obj->n);
  return obj;
}


void * producer_thread(void *arg) {
  Queue<q_test_obj> *q = (Queue<q_test_obj> *) arg;

  for(int n = 1; n < n_loop_items ; n++) {
    q_test_obj *i = new q_test_obj;
    i->n = n;
    q->produce(i);
  }

  return 0;
}
