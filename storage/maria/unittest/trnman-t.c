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
#include "../trnman.h"

pthread_attr_t rt_attr;
pthread_mutex_t rt_mutex;
pthread_cond_t rt_cond;
int rt_num_threads;

int litmus;

/*
  create and end (commit or rollback) transactions randomly
*/
#define MAX_ITER 100
pthread_handler_t test_trnman(void *arg)
{
  int    m= (*(int *)arg);
  uint   x, y, i, j, n;
  TRN    *trn[MAX_ITER];
  pthread_mutex_t mutexes[MAX_ITER];
  pthread_cond_t conds[MAX_ITER];

  for (i=0; i < MAX_ITER; i++)
  {
    pthread_mutex_init(&mutexes[i], MY_MUTEX_INIT_FAST);
    pthread_cond_init(&conds[i], 0);
  }

  for (x= ((int)(intptr)(&m)); m > 0; )
  {
    y= x= (x*3628273133 + 1500450271) % 9576890767; /* three prime numbers */
    m-= n= x % MAX_ITER;
    for (i= 0; i < n; i++)
      trn[i]= trnman_new_trn(&mutexes[i], &conds[i]);
    for (i= 0; i < n; i++)
    {
      y= (y*19 + 7) % 31;
      trnman_end_trn(trn[i], y & 1);
    }
  }

  for (i=0; i < MAX_ITER; i++)
  {
    pthread_mutex_destroy(&mutexes[i]);
    pthread_cond_destroy(&conds[i]);
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
  ulonglong now= my_getsystime();

  litmus= 0;

  diag("Testing %s with %d threads, %d iterations... ", test, n, m);
  for (rt_num_threads= n ; n ; n--)
    pthread_create(&t, &rt_attr, handler, &m);
  pthread_mutex_lock(&rt_mutex);
  while (rt_num_threads)
    pthread_cond_wait(&rt_cond, &rt_mutex);
  pthread_mutex_unlock(&rt_mutex);
  now= my_getsystime()-now;
  ok(litmus == 0, "tested %s in %g secs (%d)", test, ((double)now)/1e7, litmus);
}

#define ok_read_from(T1, T2, RES)                       \
  i=trnman_can_read_from(trn[T1], trid[T2]);            \
  ok(i == RES, "trn" #T1 " %s read from trn" #T2, i ? "can" : "cannot")
#define start_transaction(T)                            \
  trn[T]= trnman_new_trn(&mutexes[T], &conds[T]);       \
  trid[T]= trn[T]->trid
#define commit(T)               trnman_commit_trn(trn[T])
#define abort(T)                trnman_abort_trn(trn[T])

#define Ntrns 4
void test_trnman_read_from()
{
  TRN *trn[Ntrns];
  TrID trid[Ntrns];
  pthread_mutex_t mutexes[Ntrns];
  pthread_cond_t conds[Ntrns];
  int i;

  for (i=0; i < Ntrns; i++)
  {
    pthread_mutex_init(&mutexes[i], MY_MUTEX_INIT_FAST);
    pthread_cond_init(&conds[i], 0);
  }

  start_transaction(0);                    /* start trn1 */
  start_transaction(1);                    /* start trn2 */
  ok_read_from(1,0,0);
  commit(0);                               /* commit trn1 */
  start_transaction(2);                    /* start trn4 */
  abort(2);                                /* abort trn4 */
  start_transaction(3);                    /* start trn5 */
  ok_read_from(3,0,1);
  ok_read_from(3,1,0);
  ok_read_from(3,2,0);
  commit(1);                               /* commit trn2 */
  ok_read_from(3,1,0);
  commit(3);                               /* commit trn5 */

  for (i=0; i < Ntrns; i++)
  {
    pthread_mutex_destroy(&mutexes[i]);
    pthread_cond_destroy(&conds[i]);
  }
}

int main()
{
  my_init();

  plan(6);

  if (my_atomic_initialize())
    return exit_status();

  pthread_attr_init(&rt_attr);
  pthread_attr_setdetachstate(&rt_attr,PTHREAD_CREATE_DETACHED);
  pthread_mutex_init(&rt_mutex, 0);
  pthread_cond_init(&rt_cond, 0);

#define CYCLES 10000
#define THREADS 10

  trnman_init();

  test_trnman_read_from();
  run_test("trnman", test_trnman, THREADS,CYCLES);

  diag("mallocs: %d", trnman_allocated_transactions);
  {
    ulonglong now= my_getsystime();
    trnman_destroy();
    now= my_getsystime()-now;
    diag("trnman_destroy: %g", ((double)now)/1e7);
  }

  pthread_mutex_destroy(&rt_mutex);
  pthread_cond_destroy(&rt_cond);
  pthread_attr_destroy(&rt_attr);
  my_end(0);
  return exit_status();
}

