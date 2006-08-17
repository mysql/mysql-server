/* Copyright (C) 2006 MySQL AB

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

#include <tap.h>

#include <my_global.h>
#include <my_sys.h>
#include <my_atomic.h>
#include <lf.h>

volatile uint32 a32,b32,c32;
my_atomic_rwlock_t rwl;
LF_ALLOCATOR lf_allocator;
LF_HASH lf_hash;

pthread_attr_t thr_attr;
pthread_mutex_t mutex;
pthread_cond_t cond;
int N;

/* add and sub a random number in a loop. Must get 0 at the end */
pthread_handler_t test_atomic_add_handler(void *arg)
{
  int    m=(*(int *)arg)/2;
  int32 x;
  for (x=((int)(intptr)(&m)); m ; m--)
  {
    x=(x*m+0x87654321) & INT_MAX32;
    my_atomic_rwlock_wrlock(&rwl);
    my_atomic_add32(&a32, x);
    my_atomic_rwlock_wrunlock(&rwl);

    my_atomic_rwlock_wrlock(&rwl);
    my_atomic_add32(&a32, -x);
    my_atomic_rwlock_wrunlock(&rwl);
  }
  pthread_mutex_lock(&mutex);
  N--;
  if (!N) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  return 0;
}

/*
  1. generate thread number 0..N-1 from b32
  2. add it to a32
  3. swap thread numbers in c32
  4. (optionally) one more swap to avoid 0 as a result
  5. subtract result from a32
  must get 0 in a32 at the end
*/
pthread_handler_t test_atomic_swap_handler(void *arg)
{
  int    m=*(int *)arg;
  uint32  x=my_atomic_add32(&b32, 1);

  my_atomic_add32(&a32, x);

  for (; m ; m--)
  {
    my_atomic_rwlock_wrlock(&rwl);
    x=my_atomic_swap32(&c32, x);
    my_atomic_rwlock_wrunlock(&rwl);
  }

  if (!x)
  {
    my_atomic_rwlock_wrlock(&rwl);
    x=my_atomic_swap32(&c32, x);
    my_atomic_rwlock_wrunlock(&rwl);
  }

  my_atomic_rwlock_wrlock(&rwl);
  my_atomic_add32(&a32, -x);
  my_atomic_rwlock_wrunlock(&rwl);

  pthread_mutex_lock(&mutex);
  N--;
  if (!N) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  return 0;
}

/*
  same as test_atomic_add_handler, but my_atomic_add32 is emulated with
  my_atomic_cas32 - notice that the slowdown is proportional to the
  number of CPUs
*/
pthread_handler_t test_atomic_cas_handler(void *arg)
{
  int    m=(*(int *)arg)/2, ok=0;
  int32 x, y;
  for (x=((int)(intptr)(&m)); m ; m--)
  {
    my_atomic_rwlock_wrlock(&rwl);
    y=my_atomic_load32(&a32);
    my_atomic_rwlock_wrunlock(&rwl);
    x=(x*m+0x87654321) & INT_MAX32;
    do {
      my_atomic_rwlock_wrlock(&rwl);
      ok=my_atomic_cas32(&a32, &y, y+x);
      my_atomic_rwlock_wrunlock(&rwl);
    } while (!ok) ;
    do {
      my_atomic_rwlock_wrlock(&rwl);
      ok=my_atomic_cas32(&a32, &y, y-x);
      my_atomic_rwlock_wrunlock(&rwl);
    } while (!ok) ;
  }
  pthread_mutex_lock(&mutex);
  N--;
  if (!N) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  return 0;
}

/*
  pin allocator - alloc and release an element in a loop
*/
pthread_handler_t test_lf_pinbox(void *arg)
{
  int    m=*(int *)arg;
  int32 x=0;
  LF_PINS *pins;

  pins=lf_pinbox_get_pins(&lf_allocator.pinbox);

  for (x=((int)(intptr)(&m)); m ; m--)
  {
    lf_pinbox_put_pins(pins);
    pins=lf_pinbox_get_pins(&lf_allocator.pinbox);
  }
  lf_pinbox_put_pins(pins);
  pthread_mutex_lock(&mutex);
  N--;
  if (!N)
    pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  return 0;
}

typedef union {
  int32 data;
  void *not_used; /* to guarantee sizeof(TLA) >= sizeof(void *) */
} TLA;

pthread_handler_t test_lf_alloc(void *arg)
{
  int    m=(*(int *)arg)/2;
  int32 x,y=0;
  LF_PINS *pins;

  pins=lf_alloc_get_pins(&lf_allocator);

  for (x=((int)(intptr)(&m)); m ; m--)
  {
    TLA *node1, *node2;
    x=(x*m+0x87654321) & INT_MAX32;
    node1=(TLA *)lf_alloc_new(pins);
    node1->data=x;
    y+=node1->data;
    node1->data=0;
    node2=(TLA *)lf_alloc_new(pins);
    node2->data=x;
    y-=node2->data;
    node2->data=0;
    lf_alloc_free(pins, node1);
    lf_alloc_free(pins, node2);
  }
  lf_alloc_put_pins(pins);
  my_atomic_rwlock_wrlock(&rwl);
  my_atomic_add32(&a32, y);
  my_atomic_rwlock_wrunlock(&rwl);
  pthread_mutex_lock(&mutex);
  N--;
  if (!N)
  {
    diag("%d mallocs, %d pins in stack",
         lf_allocator.mallocs, lf_allocator.pinbox.pins_in_stack);
#ifdef MY_LF_EXTRA_DEBUG
    a32|=lf_allocator.mallocs - lf_alloc_in_pool(&lf_allocator);
#endif
    pthread_cond_signal(&cond);
  }
  pthread_mutex_unlock(&mutex);
  return 0;
}

#define N_TLH 1000
pthread_handler_t test_lf_hash(void *arg)
{
  int    m=(*(int *)arg)/(2*N_TLH);
  int32 x,y,z,sum=0, ins=0;
  LF_PINS *pins;

  pins=lf_hash_get_pins(&lf_hash);

  for (x=((int)(intptr)(&m)); m ; m--)
  {
    int i;
    y=x;
    for (i=0; i < N_TLH; i++)
    {
      x=(x*(m+i)+0x87654321) & INT_MAX32;
      z=(x<0) ? -x : x;
      if (lf_hash_insert(&lf_hash, pins, &z))
      {
        sum+=z;
        ins++;
      }
    }
    for (i=0; i < N_TLH; i++)
    {
      y=(y*(m+i)+0x87654321) & INT_MAX32;
      z=(y<0) ? -y : y;
      if (lf_hash_delete(&lf_hash, pins, (uchar *)&z, sizeof(z)))
        sum-=z;
    }
  }
  lf_hash_put_pins(pins);
  my_atomic_rwlock_wrlock(&rwl);
  my_atomic_add32(&a32, sum);
  my_atomic_add32(&b32, ins);
  my_atomic_rwlock_wrunlock(&rwl);
  pthread_mutex_lock(&mutex);
  N--;
  if (!N)
  {
    diag("%d mallocs, %d pins in stack, %d hash size, %d inserts",
         lf_hash.alloc.mallocs, lf_hash.alloc.pinbox.pins_in_stack,
         lf_hash.size, b32);
    a32|=lf_hash.count;
    pthread_cond_signal(&cond);
  }
  pthread_mutex_unlock(&mutex);
  return 0;
}

void test_atomic(const char *test, pthread_handler handler, int n, int m)
{
  pthread_t t;
  ulonglong now=my_getsystime();

  a32= 0;
  b32= 0;
  c32= 0;

  diag("Testing %s with %d threads, %d iterations... ", test, n, m);
  for (N=n ; n ; n--)
    pthread_create(&t, &thr_attr, handler, &m);
  pthread_mutex_lock(&mutex);
  while (N)
    pthread_cond_wait(&cond, &mutex);
  pthread_mutex_unlock(&mutex);
  now=my_getsystime()-now;
  ok(a32 == 0, "tested %s in %g secs (%d)", test, ((double)now)/1e7, a32);
}

int main()
{
  int err;

  my_init();

  diag("N CPUs: %d, atomic ops: %s", my_getncpus(), MY_ATOMIC_MODE);
  err= my_atomic_initialize();

  plan(7);
  ok(err == 0, "my_atomic_initialize() returned %d", err);

  pthread_attr_init(&thr_attr);
  pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  pthread_mutex_init(&mutex, 0);
  pthread_cond_init(&cond, 0);
  my_atomic_rwlock_init(&rwl);
  lf_alloc_init(&lf_allocator, sizeof(TLA));
  lf_hash_init(&lf_hash, sizeof(int), LF_HASH_UNIQUE, 0, sizeof(int), 0,
               &my_charset_bin);

#ifdef MY_ATOMIC_MODE_RWLOCKS
#define CYCLES 10000
#else
#define CYCLES 1000000
#endif
#define THREADS 100

  test_atomic("my_atomic_add32",  test_atomic_add_handler,    THREADS,CYCLES);
  test_atomic("my_atomic_swap32", test_atomic_swap_handler,   THREADS,CYCLES);
  test_atomic("my_atomic_cas32",  test_atomic_cas_handler,    THREADS,CYCLES);
  test_atomic("lf_pinbox",        test_lf_pinbox,             THREADS,CYCLES);
  test_atomic("lf_alloc",         test_lf_alloc,              THREADS,CYCLES);
  test_atomic("lf_hash",          test_lf_hash,               THREADS,CYCLES);

  lf_hash_destroy(&lf_hash);
  lf_alloc_destroy(&lf_allocator);
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
  pthread_attr_destroy(&thr_attr);
  my_atomic_rwlock_destroy(&rwl);
  my_end(0);
  return exit_status();
}

