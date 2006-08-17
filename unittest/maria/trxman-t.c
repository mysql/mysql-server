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
#include "../../storage/maria/trxman.h"

pthread_attr_t rt_attr;
pthread_mutex_t rt_mutex;
pthread_cond_t rt_cond;
int rt_num_threads;

int litmus;

/* template for a test: the goal is to have litmus==0 if the test passed

#define ITER nnn
pthread_handler_t test_XXXXXXXX(void *arg)
{
  int    m=(*(int *)arg)/ITER, x;

  for (x=((int)(intptr)(&m)); m ; m--)
  {
    // do something with litmus
  }
  // do something more with litmus

  pthread_mutex_lock(&rt_mutex);
  rt_num_threads--;
  if (!rt_num_threads)
  {
    diag("whatever diagnostics we want", blabla, foobar);
    pthread_cond_signal(&rt_cond);
  }
  pthread_mutex_unlock(&rt_mutex);
  return 0;
}
#undef ITER

*/

/*
  create and end (commit or rollback) transactions randomly
*/
#define MAX_ITER 100
pthread_handler_t test_trxman(void *arg)
{
  int    m=(*(int *)arg);
  uint   x, y, i, j, n;
  TRX    *trx[MAX_ITER];

  for (x=((int)(intptr)(&m)); m > 0; )
  {
    y= x= (x*3628273133 + 1500450271) % 9576890767; /* three prime numbers */
    m-= n= x % MAX_ITER;
    for (i=0; i < n; i++)
      trx[i]=trxman_new_trx();
    for (i=0; i < n; i++)
    {
      y=(y*19 + 7) % 31;
      trxman_end_trx(trx[i], y & 1);
    }
  }

  pthread_mutex_lock(&rt_mutex);
  rt_num_threads--;
  if (!rt_num_threads)
    pthread_cond_signal(&rt_cond);
  pthread_mutex_unlock(&rt_mutex);
  return 0;
}
#undef MAX_ITER

void run_test(const char *test, pthread_handler handler, int n, int m)
{
  pthread_t t;
  ulonglong now=my_getsystime();

  litmus= 0;

  diag("Testing %s with %d threads, %d iterations... ", test, n, m);
  for (rt_num_threads=n ; n ; n--)
    pthread_create(&t, &rt_attr, handler, &m);
  pthread_mutex_lock(&rt_mutex);
  while (rt_num_threads)
    pthread_cond_wait(&rt_cond, &rt_mutex);
  pthread_mutex_unlock(&rt_mutex);
  now=my_getsystime()-now;
  ok(litmus == 0, "tested %s in %g secs (%d)", test, ((double)now)/1e7, litmus);
}

int global_malloc=0;
int main()
{
  plan(1);

  if (my_atomic_initialize())
    return exit_status();

  my_init();

  pthread_attr_init(&rt_attr);
  pthread_attr_setdetachstate(&rt_attr,PTHREAD_CREATE_DETACHED);
  pthread_mutex_init(&rt_mutex, 0);
  pthread_cond_init(&rt_cond, 0);

#define CYCLES 10000
#define THREADS 10

  trxman_init();
  run_test("trxman", test_trxman, THREADS,CYCLES);
  trxman_destroy();
  diag("mallocs: %d\n", global_malloc);

  pthread_mutex_destroy(&rt_mutex);
  pthread_cond_destroy(&rt_cond);
  pthread_attr_destroy(&rt_attr);
  my_end(0);
  return exit_status();
}

