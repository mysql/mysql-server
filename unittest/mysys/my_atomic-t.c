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

#include <my_global.h>
#include <tap.h>
#include <my_sys.h>
#include <my_atomic.h>

int32 a32,b32,c32;
my_atomic_rwlock_t rwl;

pthread_attr_t thr_attr;
pthread_mutex_t mutex;
pthread_cond_t cond;
int N;

/* add and sub a random number in a loop. Must get 0 at the end */
pthread_handler_t test_atomic_add_handler(void *arg)
{
  int    m=*(int *)arg;
  int32 x;
  for (x=((int)(&m)); m ; m--)
  {
    x=x*m+0x87654321;
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
  int32 x;

  my_atomic_rwlock_wrlock(&rwl);
  x=my_atomic_add32(&b32, 1);
  my_atomic_rwlock_wrunlock(&rwl);

  my_atomic_rwlock_wrlock(&rwl);
  my_atomic_add32(&a32, x);
  my_atomic_rwlock_wrunlock(&rwl);

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
  (slower) my_atomic_cas32
*/
pthread_handler_t test_atomic_cas_handler(void *arg)
{
  int    m=*(int *)arg, ok;
  int32 x,y;
  for (x=((int)(&m)); m ; m--)
  {
    my_atomic_rwlock_wrlock(&rwl);
    y=my_atomic_load32(&a32);
    my_atomic_rwlock_wrunlock(&rwl);

    x=x*m+0x87654321;
    do {
      my_atomic_rwlock_wrlock(&rwl);
      ok=my_atomic_cas32(&a32, &y, y+x);
      my_atomic_rwlock_wrunlock(&rwl);
    } while (!ok);
    do {
      my_atomic_rwlock_wrlock(&rwl);
      ok=my_atomic_cas32(&a32, &y, y-x);
      my_atomic_rwlock_wrunlock(&rwl);
    } while (!ok);
  }
  pthread_mutex_lock(&mutex);
  N--;
  if (!N) pthread_cond_signal(&cond);
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
  ok(a32 == 0, "tested %s in %g secs", test, ((double)now)/1e7);
}

int main()
{
  int err;

  diag("N CPUs: %d", my_getncpus());
  err= my_atomic_initialize();

  plan(4);
  ok(err == 0, "my_atomic_initialize() returned %d", err);

  pthread_attr_init(&thr_attr);
  pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  pthread_mutex_init(&mutex, 0);
  pthread_cond_init(&cond, 0);
  my_atomic_rwlock_init(&rwl);

  test_atomic("my_atomic_add32", test_atomic_add_handler, 100,10000);
  test_atomic("my_atomic_swap32", test_atomic_swap_handler, 100,10000);
  test_atomic("my_atomic_cas32", test_atomic_cas_handler, 100,10000);

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
  pthread_attr_destroy(&thr_attr);
  my_atomic_rwlock_destroy(&rwl);
  return exit_status();
}

