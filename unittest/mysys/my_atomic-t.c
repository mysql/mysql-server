/* Copyright (C) 2006 MySQL AB

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

#include <my_global.h>
#include <my_sys.h>
#include <my_atomic.h>
#include <tap.h>

/* at least gcc 3.4.5 and 3.4.6 (but not 3.2.3) on RHEL */
#if __GNUC__ == 3 && __GNUC_MINOR__ == 4
#define GCC_BUG_WORKAROUND volatile
#else
#define GCC_BUG_WORKAROUND
#endif

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
  GCC_BUG_WORKAROUND int32 x;
  for (x=((int)((long)(&m))); m ; m--)
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
  GCC_BUG_WORKAROUND int32 x,y;
  for (x=((int)((long)(&m))); m ; m--)
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
  {
    if (pthread_create(&t, &thr_attr, handler, &m) != 0)
    {
      diag("Could not create thread");
      a32= 1;
      goto err;
    }
  }

  pthread_mutex_lock(&mutex);
  while (N)
    pthread_cond_wait(&cond, &mutex);
  pthread_mutex_unlock(&mutex);
  now=my_getsystime()-now;
err:
  ok(a32 == 0, "tested %s in %g secs", test, ((double)now)/1e7);
}

int main()
{
  int err;
  MY_INIT("my_atomic-t.c");

  diag("N CPUs: %d", my_getncpus());
  err= my_atomic_initialize();

  plan(4);
  ok(err == 0, "my_atomic_initialize() returned %d", err);

  pthread_attr_init(&thr_attr);
  pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  pthread_mutex_init(&mutex, 0);
  pthread_cond_init(&cond, 0);
  my_atomic_rwlock_init(&rwl);

#ifdef HPUX11
#define CYCLES 1000
#else
#define CYCLES 10000
#endif
#define THREADS 100
  test_atomic("my_atomic_add32", test_atomic_add_handler, THREADS, CYCLES);
  test_atomic("my_atomic_swap32", test_atomic_swap_handler, THREADS, CYCLES);
  test_atomic("my_atomic_cas32", test_atomic_cas_handler, THREADS, CYCLES);
  /*
    workaround until we know why it crashes randomly on some machine
    (BUG#22320).
  */
  sleep(2);

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
  pthread_attr_destroy(&thr_attr);
  my_atomic_rwlock_destroy(&rwl);
  return exit_status();
}

