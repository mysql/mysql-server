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

#include "thr_template.c"
#include <waiting_threads.h>
#include <m_string.h>
#include <locale.h>

struct test_wt_thd {
  WT_THD thd;
  pthread_mutex_t lock;
} thds[THREADS];

uint i, cnt;
pthread_mutex_t lock;

ulong wt_timeout_short=100, wt_deadlock_search_depth_short=4;
ulong wt_timeout_long=10000, wt_deadlock_search_depth_long=15;

#define reset(ARRAY) bzero(ARRAY, sizeof(ARRAY))

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

  pthread_mutex_lock(&lock);
  id= cnt++;
  pthread_mutex_unlock(&lock);

  my_rnd_init(&rand, (ulong)(intptr)&m, id);
  if (kill_strategy == YOUNGEST)
    thds[id].thd.weight= ~my_getsystime();
  if (kill_strategy == LOCKS)
    thds[id].thd.weight= 0;

  for (m= *(int *)arg; m ; m--)
  {
    WT_RESOURCE_ID resid;
    int blockers[THREADS/10], j, k;
    bzero(&resid, sizeof(resid));

    resid.value.num= id; //rnd() % THREADS;
    resid.type= &restype;

    res= 0;

    for (j= n= (rnd() % THREADS)/10; !res && j >= 0; j--)
    {
retry:
      i= rnd() % (THREADS-1);
      if (i >= id) i++;

      for (k=n; k >=j; k--)
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
        thds[id].thd.weight= ~my_getsystime();
    }
    else if (kill_strategy == LOCKS)
      thds[id].thd.weight++;
  }

  pthread_mutex_lock(& thds[id].lock);
  pthread_mutex_lock(&lock);
  wt_thd_release_all(& thds[id].thd);
  pthread_mutex_unlock(&lock);
  pthread_mutex_unlock(& thds[id].lock);

#ifndef DBUG_OFF
  {
#define DEL "(deleted)"
    char *x=malloc(strlen(thds[id].thd.name)+sizeof(DEL)+1);
    strxmov(x, thds[id].thd.name, DEL, 0);
    thds[id].thd.name=x; /* it's a memory leak, go on, shoot me */
  }
#endif

  pthread_mutex_lock(&mutex);
  if (!--running_threads) pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  DBUG_PRINT("wt", ("exiting"));
  my_thread_end();
  return 0;
}

void do_one_test()
{
  double sum, sum0;

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
}

void do_tests()
{
  plan(12);
  compile_time_assert(THREADS >= 3);

  DBUG_PRINT("wt", ("================= initialization ==================="));

  bad= my_atomic_initialize();
  ok(!bad, "my_atomic_initialize() returned %d", bad);

  pthread_mutex_init(&lock, 0);
  wt_init();
  for (cnt=0; cnt < THREADS; cnt++)
  {
    wt_thd_lazy_init(& thds[cnt].thd,
                     & wt_deadlock_search_depth_short, & wt_timeout_short,
                     & wt_deadlock_search_depth_long, & wt_timeout_long);
    pthread_mutex_init(& thds[cnt].lock, 0);
  }
  {
    WT_RESOURCE_ID resid[3];
    for (i=0; i < 3; i++)
    {
      bzero(&resid[i], sizeof(resid[i]));
      resid[i].value.num= i+1;
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
    ok(bad == ETIMEDOUT, "timeout test returned %d", bad);

    ok_wait(0,1,0);
    ok_wait(1,2,1);
    ok_deadlock(2,0,2);

    // FIXME remove wt_thd_dontwait calls below
    wt_thd_dontwait(& thds[0].thd);
    wt_thd_dontwait(& thds[1].thd);
    wt_thd_dontwait(& thds[2].thd);
    wt_thd_dontwait(& thds[3].thd);
    pthread_mutex_lock(&lock);
    wt_thd_release_all(& thds[0].thd);
    wt_thd_release_all(& thds[1].thd);
    wt_thd_release_all(& thds[2].thd);
    wt_thd_release_all(& thds[3].thd);
    pthread_mutex_unlock(&lock);

    for (cnt=0; cnt < 3; cnt++)
    {
      wt_thd_destroy(& thds[cnt].thd);
      wt_thd_lazy_init(& thds[cnt].thd,
                       & wt_deadlock_search_depth_short, & wt_timeout_short,
                       & wt_deadlock_search_depth_long, & wt_timeout_long);
    }
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

#define test_kill_strategy(X)                   \
  diag("kill strategy: " #X);                   \
  kill_strategy=X;                              \
  do_one_test();

  test_kill_strategy(LATEST);
  test_kill_strategy(RANDOM);
  test_kill_strategy(YOUNGEST);
  test_kill_strategy(LOCKS);

  DBUG_PRINT("wt", ("================= cleanup ==================="));
  pthread_mutex_lock(&lock);
  for (cnt=0; cnt < THREADS; cnt++)
  {
    wt_thd_release_all(& thds[cnt].thd);
    wt_thd_destroy(& thds[cnt].thd);
    pthread_mutex_destroy(& thds[cnt].lock);
  }
  pthread_mutex_unlock(&lock);
  wt_end();
  pthread_mutex_destroy(&lock);
}

