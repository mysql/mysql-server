/* Copyright (C) 2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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

#include "thr_template.c"
#include <waiting_threads.h>
#include <m_string.h>

struct test_wt_thd {
  WT_THD thd;
  pthread_mutex_t lock;
} thds[THREADS];

uint i, cnt;
pthread_mutex_t lock;
pthread_cond_t thread_sync;

ulong wt_timeout_short=100, wt_deadlock_search_depth_short=4;
ulong wt_timeout_long=10000, wt_deadlock_search_depth_long=15;

#define reset(ARRAY) bzero(ARRAY, sizeof(ARRAY))

/* see explanation of the kill strategies in waiting_threads.h */
enum { LATEST, RANDOM, YOUNGEST, LOCKS } kill_strategy;

WT_RESOURCE_TYPE restype={ wt_resource_id_memcmp, 0};

#define rnd() ((uint)(my_rnd(&rand) * INT_MAX32))

/*
  stress test: wait on a random number of random threads.
  it always succeeds (unless crashes or hangs).
*/
pthread_handler_t test_wt(void *arg)
{
  int    m, n, i, id, res;
  struct my_rnd_struct rand;

  my_thread_init();

  pthread_mutex_lock(&mutex);
  id= cnt++;
  wt_thd_lazy_init(& thds[id].thd,
                   & wt_deadlock_search_depth_short, & wt_timeout_short,
                   & wt_deadlock_search_depth_long, & wt_timeout_long);

  /* now, wait for everybody to be ready to run */
  if (cnt >= THREADS)
    pthread_cond_broadcast(&thread_sync);
  else
    while (cnt < THREADS)
      pthread_cond_wait(&thread_sync, &mutex);
  pthread_mutex_unlock(&mutex);

  my_rnd_init(&rand, (ulong)(intptr)&m, id);
  if (kill_strategy == YOUNGEST)
    thds[id].thd.weight= (ulong)~my_getsystime();
  if (kill_strategy == LOCKS)
    thds[id].thd.weight= 0;

  for (m= *(int *)arg; m ; m--)
  {
    WT_RESOURCE_ID resid;
    int blockers[THREADS/10], j, k;

    resid.value= id;
    resid.type= &restype;

    res= 0;

    /* prepare for waiting for a random number of random threads */
    for (j= n= (rnd() % THREADS)/10; !res && j >= 0; j--)
    {
retry:
      i= rnd() % (THREADS-1); /* pick a random thread */
      if (i >= id) i++;   /* with a number from 0 to THREADS-1 excluding ours */

      for (k=n; k >=j; k--) /* the one we didn't pick before */
        if (blockers[k] == i)
          goto retry;
      blockers[j]= i;

      if (kill_strategy == RANDOM)
        thds[id].thd.weight= rnd();

      pthread_mutex_lock(& thds[i].lock);
      res= wt_thd_will_wait_for(& thds[id].thd, & thds[i].thd, &resid);
      pthread_mutex_unlock(& thds[i].lock);
    }

    if (!res)
    {
      pthread_mutex_lock(&lock);
      res= wt_thd_cond_timedwait(& thds[id].thd, &lock);
      pthread_mutex_unlock(&lock);
    }

    if (res)
    {
      pthread_mutex_lock(& thds[id].lock);
      pthread_mutex_lock(&lock);
      wt_thd_release_all(& thds[id].thd);
      pthread_mutex_unlock(&lock);
      pthread_mutex_unlock(& thds[id].lock);
      if (kill_strategy == LOCKS)
        thds[id].thd.weight= 0;
      if (kill_strategy == YOUNGEST)
        thds[id].thd.weight= (ulong)~my_getsystime();
    }
    else if (kill_strategy == LOCKS)
      thds[id].thd.weight++;
  }

  pthread_mutex_lock(&mutex);
  /* wait for everybody to finish */
  if (!--cnt)
    pthread_cond_broadcast(&thread_sync);
  else
    while (cnt)
      pthread_cond_wait(&thread_sync, &mutex);

  pthread_mutex_lock(& thds[id].lock);
  pthread_mutex_lock(&lock);
  wt_thd_release_all(& thds[id].thd);
  pthread_mutex_unlock(&lock);
  pthread_mutex_unlock(& thds[id].lock);
  wt_thd_destroy(& thds[id].thd);

  if (!--running_threads) /* now, signal when everybody is done with deinit */
    pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  DBUG_PRINT("wt", ("exiting"));
  my_thread_end();
  return 0;
}

void do_one_test()
{
  double sum, sum0;
  DBUG_ENTER("do_one_test");

  reset(wt_cycle_stats);
  reset(wt_wait_stats);
  wt_success_stats=0;
  cnt=0;
  test_concurrently("waiting_threads", test_wt, THREADS, CYCLES);

  sum=sum0=0;
  for (cnt=0; cnt < WT_CYCLE_STATS; cnt++)
    sum+= wt_cycle_stats[0][cnt] + wt_cycle_stats[1][cnt];
  for (cnt=0; cnt < WT_CYCLE_STATS; cnt++)
    if (wt_cycle_stats[0][cnt] + wt_cycle_stats[1][cnt] > 0)
    {
      sum0+=wt_cycle_stats[0][cnt] + wt_cycle_stats[1][cnt];
      diag("deadlock cycles of length %2u: %4u %4u %8.2f %%", cnt,
           wt_cycle_stats[0][cnt], wt_cycle_stats[1][cnt], 1e2*sum0/sum);
    }
  diag("depth exceeded: %u %u",
       wt_cycle_stats[0][cnt], wt_cycle_stats[1][cnt]);
  for (cnt=0; cnt < WT_WAIT_STATS; cnt++)
    if (wt_wait_stats[cnt]>0)
      diag("deadlock waits up to %7llu us: %5u",
           wt_wait_table[cnt], wt_wait_stats[cnt]);
  diag("timed out: %u", wt_wait_stats[cnt]);
  diag("successes: %u", wt_success_stats);

  DBUG_VOID_RETURN;
}

void do_tests()
{
  DBUG_ENTER("do_tests");
  plan(14);
  compile_time_assert(THREADS >= 4);

  DBUG_PRINT("wt", ("================= initialization ==================="));

  bad= my_atomic_initialize();
  ok(!bad, "my_atomic_initialize() returned %d", bad);

  pthread_cond_init(&thread_sync, 0);
  pthread_mutex_init(&lock, 0);
  wt_init();
  for (cnt=0; cnt < THREADS; cnt++)
    pthread_mutex_init(& thds[cnt].lock, 0);
  {
    WT_RESOURCE_ID resid[4];
    for (i=0; i < array_elements(resid); i++)
    {
      wt_thd_lazy_init(& thds[i].thd,
                       & wt_deadlock_search_depth_short, & wt_timeout_short,
                       & wt_deadlock_search_depth_long, & wt_timeout_long);
      resid[i].value= i+1;
      resid[i].type= &restype;
    }

    DBUG_PRINT("wt", ("================= manual test ==================="));

#define ok_wait(X,Y, R) \
    ok(wt_thd_will_wait_for(& thds[X].thd, & thds[Y].thd, &resid[R]) == 0, \
      "thd[" #X "] will wait for thd[" #Y "]")
#define ok_deadlock(X,Y,R) \
    ok(wt_thd_will_wait_for(& thds[X].thd, & thds[Y].thd, &resid[R]) == WT_DEADLOCK, \
      "thd[" #X "] will wait for thd[" #Y "] - deadlock")

    ok_wait(0,1,0);
    ok_wait(0,2,0);
    ok_wait(0,3,0);

    pthread_mutex_lock(&lock);
    bad= wt_thd_cond_timedwait(& thds[0].thd, &lock);
    pthread_mutex_unlock(&lock);
    ok(bad == WT_TIMEOUT, "timeout test returned %d", bad);

    ok_wait(0,1,0);
    ok_wait(1,2,1);
    ok_deadlock(2,0,2);

    pthread_mutex_lock(&lock);
    ok(wt_thd_cond_timedwait(& thds[0].thd, &lock) == WT_TIMEOUT, "as always");
    ok(wt_thd_cond_timedwait(& thds[1].thd, &lock) == WT_TIMEOUT, "as always");
    wt_thd_release_all(& thds[0].thd);
    wt_thd_release_all(& thds[1].thd);
    wt_thd_release_all(& thds[2].thd);
    wt_thd_release_all(& thds[3].thd);

    for (i=0; i < array_elements(resid); i++)
    {
      wt_thd_release_all(& thds[i].thd);
      wt_thd_destroy(& thds[i].thd);
    }
    pthread_mutex_unlock(&lock);
  }

  wt_deadlock_search_depth_short=6;
  wt_timeout_short=1000;
  wt_timeout_long= 100;
  wt_deadlock_search_depth_long=16;
  DBUG_PRINT("wt", ("================= stress test ==================="));

  diag("timeout_short=%lu us, deadlock_search_depth_short=%lu",
       wt_timeout_short, wt_deadlock_search_depth_short);
  diag("timeout_long=%lu us, deadlock_search_depth_long=%lu",
       wt_timeout_long, wt_deadlock_search_depth_long);

#ifndef _WIN32
#define test_kill_strategy(X)                   \
  diag("kill strategy: " #X);                   \
  DBUG_EXECUTE("reset_file",                    \
               { rewind(DBUG_FILE); ftruncate(fileno(DBUG_FILE), 0); }); \
  DBUG_PRINT("info", ("kill strategy: " #X));   \
  kill_strategy=X;                              \
  do_one_test();
#else
  #define test_kill_strategy(X)                 \
  diag("kill strategy: " #X);                   \
  DBUG_PRINT("info", ("kill strategy: " #X));   \
  kill_strategy=X;                              \
  do_one_test();
#endif

  test_kill_strategy(LATEST);
  test_kill_strategy(RANDOM);
  test_kill_strategy(YOUNGEST);
  test_kill_strategy(LOCKS);

  DBUG_PRINT("wt", ("================= cleanup ==================="));
  for (cnt=0; cnt < THREADS; cnt++)
    pthread_mutex_destroy(& thds[cnt].lock);
  wt_end();
  pthread_mutex_destroy(&lock);
  pthread_cond_destroy(&thread_sync);
  DBUG_VOID_RETURN;
}

