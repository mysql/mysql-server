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

#include <my_config.h>

#include <stdlib.h>
#ifdef __GNUC__
/* Required for useconds_t and C99 */
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "workqueue.h"
#include <ndbmemcache_config.h>

#ifdef HAVE_SRANDOMDEV
#define seed_randomizer(X) srandomdev()
#else 
#define seed_randomizer(X) srandom(X)
#endif

#define TEST_ITERATIONS 1000000
#define DO_SLEEP 1


void * producer_thread(void *arg);
void * consumer_thread(void *arg);

struct threadinfo { 
  struct workqueue *q;
  int queue_size;
  int nconsumers;
  int producer_batch_size;
  int producer_median_sleep;
  int consumer_median_sleep;
  int iterations;
  int report_interval;
};  

struct threadreturn {
  unsigned int nrecv;
};

int run_test(struct threadinfo *);
void express_nanosec(Uint64 nsec);

int sleep_microseconds(int);

int main() {
  const char * status;
  struct workqueue *queue=
      (struct workqueue *) calloc(1, sizeof(struct workqueue));

  struct threadinfo test0 = { queue, 32768, 1, 0, 0, 0, 50000, 1000000 };
  struct threadinfo test1 = { queue, 32768, 2, 0, 0, 0, 50000, 1000000 };
  struct threadinfo test2 = { queue, 8192, 2, 10, 400, 100, 10000, 25000 };
  struct threadinfo test3 = { queue, 8192, 2, 1, 425, 25, 15000, 25000 };
  struct threadinfo test4 = { queue, 8192, 2, 20, 25, 250, 10000, 25000 };
  struct threadinfo test5 = { queue, 8192, 2, 1, 50, 0, 10000, 50000 };
  struct threadinfo test6 = { queue, 16384, 8, 1, 20, 160, 200000, 50000 };
  
  seed_randomizer(1);

  /* Note! Note! For TAP, tests are numbered 1 to 7 */
  printf("1..7\n");
  fflush(stdout);

  /* Test 0: no-sleep with 1 consumer */
  status = run_test(& test0) ? "ok" : "not ok";
  printf("%s 1 No-sleep test with 1 consumer\n", status);
  fflush(stdout);

  /* Test 1: the no-sleep test */
  status = run_test(& test1) ? "ok" : "not ok";
  printf("%s 2 No-sleep wham!bam! test with %d iterations\n", status,
         test1.iterations);
  fflush(stdout);
  
  /* Test 2: fast producer, slow consumer */
  status = run_test(& test2) ? "ok" : "not ok";
  printf("%s 3 Fast producer / slow consumer test\n", status);
  fflush(stdout);

  /* Test 3: slow producer, fast consumer */
  status = run_test(& test3) ? "ok" : "not ok";
  printf("%s 4 Slow producer / fast consumer test\n", status);
  fflush(stdout);

  /* Test 4: very slow consumer */  
  status = run_test(& test4) ? "ok" : "not ok";
  printf("%s 5 very slow consumer test\n", status);
  fflush(stdout);

  /* Test 5: whambam! consumer */  
  status = run_test(& test5) ? "ok" : "not ok";
  printf("%s 6 Sluggish producer, whambam! consumer test\n", status);
  fflush(stdout);

  /* Test 6: simulation */
  status = run_test(& test6) ? "ok" : "not ok";
  printf("%s 7 Memcached simluation test\n", status);
  fflush(stdout);
}

int run_test(struct threadinfo *params) {
  pthread_t producer_thd_id;
  pthread_t *consumer_thd_ids;
  int i;
  int total_consumed = 0;
  
  consumer_thd_ids = calloc(sizeof(pthread_t), params->nconsumers);

  if(workqueue_init(params->q, params->queue_size, params->nconsumers) ) {
    printf("Bail out!  Workqueue init failed.\n");
    exit(1);
  }
  
  pthread_create(&producer_thd_id, NULL, producer_thread, (void *) params);  
  for(i = 0; i < params->nconsumers; i++) 
    pthread_create(& consumer_thd_ids[i], NULL, consumer_thread, (void *) params);
  
  pthread_join(producer_thd_id, NULL);
  for(i = 0; i < params->nconsumers; i++) {
    void *ret;
    pthread_join(consumer_thd_ids[i], &ret);
    total_consumed += ((struct threadreturn *) ret)->nrecv;
  }

  workqueue_destroy(params->q);  
  free(consumer_thd_ids);  
  return (total_consumed == params->iterations);
}


void * producer_thread(void *arg) {
  long long total_sleep = 0;
  int slp = 0;
  size_t i = 1;
  int n_ints;
  int sample_interval = 1000;
  int nsamples = 0, total_depth = 0;
  int do_sample = random() % sample_interval;

  struct threadinfo *testinfo = (struct threadinfo *) arg;  
  struct workqueue *queue = testinfo->q;
  int batchsize = testinfo->producer_batch_size;
  int sleeptime = testinfo->producer_median_sleep;
  unsigned int iterations = testinfo->iterations + 1;
  
  /* Generate consecutive integers, in random batches.  And sleep for random
     amounts of time between them.  Put them on the queue. 
  */
  while(i < iterations) {  /* we count from e.g. "1" to "100000" */
    /* sleep time: non-uniform ("two dice") distribution */
    if(sleeptime)
      slp =  ( (random() % sleeptime) + (random() % sleeptime) );   

    /* how many numbers to generate: */
    if(batchsize)
      n_ints = (random() % batchsize) + 1;
    else 
      n_ints = 1;
  
    while(n_ints-- && i < iterations) {
      workqueue_add(queue, (void *) i);
      i++;
    }
    if(sleeptime) total_sleep += sleep_microseconds(slp);

    if(do_sample-- == 0) {
      nsamples++;
      total_depth += queue->depth;
      do_sample = random() % sample_interval;
    }
  }

  printf(" .. Producer thread sent %d. Slept for %f sec.  Average depth: %d\n",
         (int) i-1, (double) total_sleep / 1000000, total_depth / nsamples);

  workqueue_abort(queue);
  return 0;
}


void * consumer_thread(void *arg) {
  int slp = 0;
  long long total_sleep = 0;
  size_t i;
  size_t last_i = 0;
  
  struct threadinfo *testinfo = (struct threadinfo *) arg;  
  struct workqueue *queue = testinfo->q;
  int sleeptime = testinfo->consumer_median_sleep;
  struct threadreturn *ret = malloc(sizeof(struct threadreturn));
  ret->nrecv = 0;
  
  /* fetch items from the queue, 1 at a time, and sleep for some time to 
     simulate processing them */
  
  while(1) {
    /* sleep time: non-uniform ("two dice") distribution */
    if(sleeptime) 
      slp =  ( (random() % sleeptime) + (random() % sleeptime) );

    i = (size_t) workqueue_consumer_wait(queue);
    if(i) {
      ret->nrecv++;
      if(i == 10) printf(" .. read 10.\n");
      if(i % testinfo->report_interval == 0) printf(" .. read %d.\n", (int) i);
      assert(i > last_i);
      last_i = i;
      if(sleeptime) total_sleep += sleep_microseconds(slp);
    }
    else {    
      printf("  .. Consumer thread read %d; slept for %f sec. \n", ret->nrecv,
             (double) total_sleep / 1000000);
      return (void *) ret;
    }
  }
}


/* sleep for some number of microseconds, less than a full second.
   returns number of microseconds slept.
*/
int sleep_microseconds(int usec) {
  struct timespec time_to_sleep;
  struct timespec time_slept;
  
  time_to_sleep.tv_nsec = (usec * 1000);
  time_to_sleep.tv_sec = 0;
  if(nanosleep(&time_to_sleep, &time_slept))  /* interrupted */
    return (( time_to_sleep.tv_nsec - time_slept.tv_nsec) / 1000);
  else return usec;
}


void express_nanosec(Uint64 ns) {
  const char *units[4] = { "ns", "us", "ms", "s" };
  int p;
  for(p = 0; ns > 1000 && p < 4; ns /= 1000, p++);
  
  printf("%llu %s\n", (unsigned long long) ns, units[p]);  
}
