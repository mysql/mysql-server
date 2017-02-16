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

/*
  lockman for row and table locks
*/

/* #define EXTRA_VERBOSE */

#include <tap.h>

#include <my_global.h>
#include <my_sys.h>
#include <my_atomic.h>
#include <lf.h>
#include "../lockman.h"

#define Nlos 100
LOCK_OWNER loarray[Nlos];
pthread_mutex_t mutexes[Nlos];
pthread_cond_t conds[Nlos];
LOCKMAN lockman;

#ifndef EXTRA_VERBOSE
#define print_lockhash(X)       /* no-op */
#define DIAG(X)                 /* no-op */
#else
#define DIAG(X) diag X
#endif

LOCK_OWNER *loid2lo(uint16 loid)
{
  return loarray+loid-1;
}

#define unlock_all(O) diag("lo" #O "> release all locks");              \
  lockman_release_locks(&lockman, loid2lo(O));print_lockhash(&lockman)
#define test_lock(O, R, L, S, RES)                                      \
  ok(lockman_getlock(&lockman, loid2lo(O), R, L) == RES,                \
     "lo" #O "> " S "lock resource " #R " with " #L "-lock");           \
  print_lockhash(&lockman)
#define lock_ok_a(O, R, L)                                              \
  test_lock(O, R, L, "", GOT_THE_LOCK)
#define lock_ok_i(O, R, L)                                              \
  test_lock(O, R, L, "", GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE)
#define lock_ok_l(O, R, L)                                              \
  test_lock(O, R, L, "", GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE)
#define lock_conflict(O, R, L)                                          \
  test_lock(O, R, L, "cannot ", DIDNT_GET_THE_LOCK);

void test_lockman_simple()
{
  /* simple */
  lock_ok_a(1, 1, S);
  lock_ok_i(2, 2, IS);
  lock_ok_i(1, 2, IX);
  /* lock escalation */
  lock_ok_a(1, 1, X);
  lock_ok_i(2, 2, IX);
  /* failures */
  lock_conflict(2, 1, X);
  unlock_all(2);
  lock_ok_a(1, 2, S);
  lock_ok_a(1, 2, IS);
  lock_ok_a(1, 2, LS);
  lock_ok_i(1, 3, IX);
  lock_ok_a(2, 3, LS);
  lock_ok_i(1, 3, IX);
  lock_ok_l(2, 3, IS);
  unlock_all(1);
  unlock_all(2);

  lock_ok_i(1, 1, IX);
  lock_conflict(2, 1, S);
  lock_ok_a(1, 1, LS);
  unlock_all(1);
  unlock_all(2);

  lock_ok_i(1, 1, IX);
  lock_ok_a(2, 1, LS);
  lock_ok_a(1, 1, LS);
  lock_ok_i(1, 1, IX);
  lock_ok_i(3, 1, IS);
  unlock_all(1);
  unlock_all(2);
  unlock_all(3);

  lock_ok_i(1, 4, IS);
  lock_ok_i(2, 4, IS);
  lock_ok_i(3, 4, IS);
  lock_ok_a(3, 4, LS);
  lock_ok_i(4, 4, IS);
  lock_conflict(4, 4, IX);
  lock_conflict(2, 4, IX);
  lock_ok_a(1, 4, LS);
  unlock_all(1);
  unlock_all(2);
  unlock_all(3);
  unlock_all(4);

  lock_ok_i(1, 1, IX);
  lock_ok_i(2, 1, IX);
  lock_conflict(1, 1, S);
  lock_conflict(2, 1, X);
  unlock_all(1);
  unlock_all(2);
}

int rt_num_threads;
int litmus;
int thread_number= 0, timeouts= 0;
void run_test(const char *test, pthread_handler handler, int n, int m)
{
  pthread_t *threads;
  ulonglong now= my_getsystime();
  int i;

  thread_number= timeouts= 0;
  litmus= 0;

  threads= (pthread_t *)my_malloc(sizeof(void *)*n, MYF(0));
  if (!threads)
  {
    diag("Out of memory");
    abort();
  }

  diag("Running %s with %d threads, %d iterations... ", test, n, m);
  rt_num_threads= n;
  for (i= 0; i < n ; i++)
    if (pthread_create(threads+i, 0, handler, &m))
    {
      diag("Could not create thread");
      abort();
    }
  for (i= 0 ; i < n ; i++)
    pthread_join(threads[i], 0);
  now= my_getsystime()-now;
  ok(litmus == 0, "Finished %s in %g secs (%d)", test, ((double)now)/1e7, litmus);
  my_free((void*)threads, MYF(0));
}

pthread_mutex_t rt_mutex;
int Nrows= 100;
int Ntables= 10;
int table_lock_ratio= 10;
enum lockman_lock_type lock_array[6]= {S, X, LS, LX, IS, IX};
char *lock2str[6]= {"S", "X", "LS", "LX", "IS", "IX"};
char *res2str[4]= {
  "DIDN'T GET THE LOCK",
  "GOT THE LOCK",
  "GOT THE LOCK NEED TO LOCK A SUBRESOURCE",
  "GOT THE LOCK NEED TO INSTANT LOCK A SUBRESOURCE"};
pthread_handler_t test_lockman(void *arg)
{
  int    m= (*(int *)arg);
  uint   x, loid, row, table, res, locklevel, timeout= 0;
  LOCK_OWNER *lo;

  pthread_mutex_lock(&rt_mutex);
  loid= ++thread_number;
  pthread_mutex_unlock(&rt_mutex);
  lo= loid2lo(loid);

  for (x= ((int)(intptr)(&m)); m > 0; m--)
  {
    x= (x*3628273133 + 1500450271) % 9576890767; /* three prime numbers */
    row=  x % Nrows + Ntables;
    table= row % Ntables;
    locklevel= (x/Nrows) & 3;
    if (table_lock_ratio && (x/Nrows/4) % table_lock_ratio == 0)
    { /* table lock */
      res= lockman_getlock(&lockman, lo, table, lock_array[locklevel]);
      DIAG(("loid %2d, table %d, lock %s, res %s", loid, table,
            lock2str[locklevel], res2str[res]));
      if (res == DIDNT_GET_THE_LOCK)
      {
        lockman_release_locks(&lockman, lo);
        DIAG(("loid %2d, release all locks", loid));
        timeout++;
        continue;
      }
      DBUG_ASSERT(res == GOT_THE_LOCK);
    }
    else
    { /* row lock */
      locklevel&= 1;
      res= lockman_getlock(&lockman, lo, table, lock_array[locklevel + 4]);
      DIAG(("loid %2d, row %d, lock %s, res %s", loid, row,
            lock2str[locklevel+4], res2str[res]));
      switch (res)
      {
      case DIDNT_GET_THE_LOCK:
        lockman_release_locks(&lockman, lo);
        DIAG(("loid %2d, release all locks", loid));
        timeout++;
        continue;
      case GOT_THE_LOCK:
        continue;
      case GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE:
        /* not implemented, so take a regular lock */
      case GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE:
        res= lockman_getlock(&lockman, lo, row, lock_array[locklevel]);
        DIAG(("loid %2d, ROW %d, lock %s, res %s", loid, row,
              lock2str[locklevel], res2str[res]));
        if (res == DIDNT_GET_THE_LOCK)
        {
          lockman_release_locks(&lockman, lo);
          DIAG(("loid %2d, release all locks", loid));
          timeout++;
          continue;
        }
        DBUG_ASSERT(res == GOT_THE_LOCK);
        continue;
      default:
        DBUG_ASSERT(0);
      }
    }
  }

  lockman_release_locks(&lockman, lo);

  pthread_mutex_lock(&rt_mutex);
  rt_num_threads--;
  timeouts+= timeout;
  if (!rt_num_threads)
    diag("number of timeouts: %d", timeouts);
  pthread_mutex_unlock(&rt_mutex);

  return 0;
}

int main()
{
  int i;

  my_init();
  pthread_mutex_init(&rt_mutex, 0);

  plan(35);

  if (my_atomic_initialize())
    return exit_status();


  lockman_init(&lockman, &loid2lo, 50);

  for (i= 0; i < Nlos; i++)
  {
    loarray[i].pins= lf_alloc_get_pins(&lockman.alloc);
    loarray[i].all_locks= 0;
    loarray[i].waiting_for= 0;
    pthread_mutex_init(&mutexes[i], MY_MUTEX_INIT_FAST);
    pthread_cond_init (&conds[i], 0);
    loarray[i].mutex= &mutexes[i];
    loarray[i].cond= &conds[i];
    loarray[i].loid= i+1;
  }

  test_lockman_simple();

#define CYCLES 10000
#define THREADS Nlos /* don't change this line */

  /* mixed load, stress-test with random locks */
  Nrows= 100;
  Ntables= 10;
  table_lock_ratio= 10;
  run_test("\"random lock\" stress test", test_lockman, THREADS, CYCLES);

  /* "real-life" simulation - many rows, no table locks */
  Nrows= 1000000;
  Ntables= 10;
  table_lock_ratio= 0;
  run_test("\"real-life\" simulation test", test_lockman, THREADS, CYCLES*10);

  for (i= 0; i < Nlos; i++)
  {
    lockman_release_locks(&lockman, &loarray[i]);
    pthread_mutex_destroy(loarray[i].mutex);
    pthread_cond_destroy(loarray[i].cond);
    lf_pinbox_put_pins(loarray[i].pins);
  }

  {
    ulonglong now= my_getsystime();
    lockman_destroy(&lockman);
    now= my_getsystime()-now;
    diag("lockman_destroy: %g secs", ((double)now)/1e7);
  }

  pthread_mutex_destroy(&rt_mutex);
  my_end(0);
  return exit_status();
}

