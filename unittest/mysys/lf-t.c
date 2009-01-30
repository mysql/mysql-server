/* Copyright (C) 2008-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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

/**
  @file

  Unit tests for lock-free algorithms of mysys
*/

#include "thr_template.c"

#include <lf.h>

int32 inserts= 0, N;
LF_ALLOCATOR lf_allocator;
LF_HASH lf_hash;

/*
  pin allocator - alloc and release an element in a loop
*/
pthread_handler_t test_lf_pinbox(void *arg)
{
  int    m= *(int *)arg;
  int32 x= 0;
  LF_PINS *pins;

  my_thread_init();

  pins= lf_pinbox_get_pins(&lf_allocator.pinbox);

  for (x= ((int)(intptr)(&m)); m ; m--)
  {
    lf_pinbox_put_pins(pins);
    pins= lf_pinbox_get_pins(&lf_allocator.pinbox);
  }
  lf_pinbox_put_pins(pins);
  pthread_mutex_lock(&mutex);
  if (!--running_threads) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  my_thread_end();
  return 0;
}

/*
  thread local data area, allocated using lf_alloc.
  union is required to enforce the minimum required element size (sizeof(ptr))
*/
typedef union {
  int32 data;
  void *not_used;
} TLA;

pthread_handler_t test_lf_alloc(void *arg)
{
  int    m= (*(int *)arg)/2;
  int32 x,y= 0;
  LF_PINS *pins;

  my_thread_init();

  pins= lf_alloc_get_pins(&lf_allocator);

  for (x= ((int)(intptr)(&m)); m ; m--)
  {
    TLA *node1, *node2;
    x= (x*m+0x87654321) & INT_MAX32;
    node1= (TLA *)lf_alloc_new(pins);
    node1->data= x;
    y+= node1->data;
    node1->data= 0;
    node2= (TLA *)lf_alloc_new(pins);
    node2->data= x;
    y-= node2->data;
    node2->data= 0;
    lf_alloc_free(pins, node1);
    lf_alloc_free(pins, node2);
  }
  lf_alloc_put_pins(pins);
  pthread_mutex_lock(&mutex);
  bad+= y;

  if (--N == 0)
  {
    diag("%d mallocs, %d pins in stack",
         lf_allocator.mallocs, lf_allocator.pinbox.pins_in_array);
#ifdef MY_LF_EXTRA_DEBUG
    bad|= lf_allocator.mallocs - lf_alloc_pool_count(&lf_allocator);
#endif
  }
  if (!--running_threads) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  my_thread_end();
  return 0;
}

#define N_TLH 1000
pthread_handler_t test_lf_hash(void *arg)
{
  int    m= (*(int *)arg)/(2*N_TLH);
  int32 x,y,z,sum= 0, ins= 0;
  LF_PINS *pins;

  my_thread_init();

  pins= lf_hash_get_pins(&lf_hash);

  for (x= ((int)(intptr)(&m)); m ; m--)
  {
    int i;
    y= x;
    for (i= 0; i < N_TLH; i++)
    {
      x= (x*(m+i)+0x87654321) & INT_MAX32;
      z= (x<0) ? -x : x;
      if (lf_hash_insert(&lf_hash, pins, &z))
      {
        sum+= z;
        ins++;
      }
    }
    for (i= 0; i < N_TLH; i++)
    {
      y= (y*(m+i)+0x87654321) & INT_MAX32;
      z= (y<0) ? -y : y;
      if (lf_hash_delete(&lf_hash, pins, (uchar *)&z, sizeof(z)))
        sum-= z;
    }
  }
  lf_hash_put_pins(pins);
  pthread_mutex_lock(&mutex);
  bad+= sum;
  inserts+= ins;

  if (--N == 0)
  {
    diag("%d mallocs, %d pins in stack, %d hash size, %d inserts",
         lf_hash.alloc.mallocs, lf_hash.alloc.pinbox.pins_in_array,
         lf_hash.size, inserts);
    bad|= lf_hash.count;
  }
  if (!--running_threads) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  my_thread_end();
  return 0;
}


void do_tests()
{
  plan(4);

  lf_alloc_init(&lf_allocator, sizeof(TLA), offsetof(TLA, not_used));
  lf_hash_init(&lf_hash, sizeof(int), LF_HASH_UNIQUE, 0, sizeof(int), 0,
               &my_charset_bin);

  bad= my_atomic_initialize();
  ok(!bad, "my_atomic_initialize() returned %d", bad);

  test_concurrently("lf_pinbox", test_lf_pinbox, N= THREADS, CYCLES);
  test_concurrently("lf_alloc",  test_lf_alloc,  N= THREADS, CYCLES);
  test_concurrently("lf_hash",   test_lf_hash,   N= THREADS, CYCLES/10);

  lf_hash_destroy(&lf_hash);
  lf_alloc_destroy(&lf_allocator);
}

