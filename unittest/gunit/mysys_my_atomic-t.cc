/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <my_atomic.h>
#include <my_sys.h>
#include <atomic>

#include "my_compiler.h"
#include "my_config.h"
#include "my_inttypes.h"


namespace mysys_my_atomic_unittest {

#include "thr_template.cc"

volatile int32 b32;
volatile int32  c32;

// SUPPRESS_UBSAN: integer overflow when generating random data.
extern "C" void *test_atomic_add(void *arg) SUPPRESS_UBSAN;

/* add and sub a random number in a loop. Must get 0 at the end */
extern "C" void *test_atomic_add(void *arg)
{
  int m= (*(int *)arg)/2;
  int32 x;
  for (x= ((int)(intptr)(&m)); m ; m--)
  {
    x= (x*m+0x87654321) & INT_MAX32;
    my_atomic_add32(&bad, x);

    my_atomic_add32(&bad, -x);
  }
  mysql_mutex_lock(&mutex);
  if (!--running_threads) mysql_cond_signal(&cond);
  mysql_mutex_unlock(&mutex);
  return 0;
}

volatile int64 a64;
// SUPPRESS_UBSAN: integer overflow when generating random data.
extern "C" void *test_atomic_add64(void *arg) SUPPRESS_UBSAN;

/* add and sub a random number in a loop. Must get 0 at the end */
extern "C" void *test_atomic_add64(void *arg)
{
  int m= (*(int *)arg)/2;
  int64 x;
  for (x= ((int64)(intptr)(&m)); m ; m--)
  {
    x= (x*m+0xfdecba987654321LL) & INT_MAX64;
    my_atomic_add64(&a64, x);

    my_atomic_add64(&a64, -x);
  }
  mysql_mutex_lock(&mutex);
  if (!--running_threads)
  {
    bad= (a64 != 0);
    mysql_cond_signal(&cond);
  }
  mysql_mutex_unlock(&mutex);
  return 0;
}


/*
  1. generate thread number 0..N-1 from b32
  2. add it to bad
  3. swap thread numbers in c32
  4. (optionally) one more swap to avoid 0 as a result
  5. subtract result from bad
  must get 0 in bad at the end
*/
extern "C" void *test_atomic_fas(void *arg)
{
  int    m= *(int *)arg;
  int32  x;

  x= my_atomic_add32(&b32, 1);

  my_atomic_add32(&bad, x);

  for (; m ; m--)
  {
    x= my_atomic_fas32(&c32, x);
  }

  if (!x)
  {
    x= my_atomic_fas32(&c32, x);
  }

  my_atomic_add32(&bad, -x);

  mysql_mutex_lock(&mutex);
  if (!--running_threads) mysql_cond_signal(&cond);
  mysql_mutex_unlock(&mutex);
  return 0;
}

// SUPPRESS_UBSAN: integer overflow when generating random data.
extern "C" void *test_atomic_cas(void *arg) SUPPRESS_UBSAN;

/*
  same as test_atomic_add, but my_atomic_add32 is emulated with
  my_atomic_cas32 - notice that the slowdown is proportional to the
  number of CPUs
*/
extern "C" void *test_atomic_cas(void *arg)
{
  int m= (*(int *)arg)/2, ok= 0;
  int32 x, y;
  for (x= ((int)(intptr)(&m)); m ; m--)
  {
    y= my_atomic_load32(&bad);
    x= (x*m+0x87654321) & INT_MAX32;
    do {
      ok= my_atomic_cas32(&bad, &y, (uint32)y+x);
    } while (!ok) ;
    do {
      ok= my_atomic_cas32(&bad, &y, y-x);
    } while (!ok) ;
  }
  mysql_mutex_lock(&mutex);
  if (!--running_threads) mysql_cond_signal(&cond);
  mysql_mutex_unlock(&mutex);
  return 0;
}


void do_tests()
{
  b32= c32= 0;
  test_concurrently("my_atomic_add32", test_atomic_add, THREADS, CYCLES);
  b32= c32= 0;
  test_concurrently("my_atomic_fas32", test_atomic_fas, THREADS, CYCLES);
  b32= c32= 0;
  test_concurrently("my_atomic_cas32", test_atomic_cas, THREADS, CYCLES);

  {
    /*
      If b is not volatile, the wrong assembly code is generated on OSX Lion
      as the variable is optimized away as a constant.
      See Bug#62533 / Bug#13030056.
      Another workaround is to specify architecture explicitly using e.g.
      CFLAGS/CXXFLAGS= "-m64".
    */
    volatile int64 b=0x1000200030004000LL;
    a64=0;
    my_atomic_add64(&a64, b);
    EXPECT_EQ(a64, b) << "add64";
  }
  a64=0;
  test_concurrently("my_atomic_add64", test_atomic_add64, THREADS, CYCLES);
}


TEST(Mysys, Atomic)
{
  mysql_mutex_init(0, &mutex, 0);
  mysql_cond_init(0, &cond);
  my_thread_attr_init(&thr_attr);
  my_thread_attr_setdetachstate(&thr_attr, MY_THREAD_CREATE_DETACHED);

  do_tests();

  mysql_mutex_destroy(&mutex);
  mysql_cond_destroy(&cond);
  my_thread_attr_destroy(&thr_attr);
}


// A very simple perf test of load/store.

#ifndef DBUG_OFF
static const int num_iterations= 1; // No point in debug mode
#else
static const int num_iterations= 10000; // Should be increased for testing.
#endif

TEST(Mysys, AtomicPerfMy)
{
  volatile int64 a= 0;
  for (int64 i = 0; i < num_iterations; i++)
  {
    int64 v= my_atomic_load64(&a);
    EXPECT_EQ(v, i);
    my_atomic_add64(&a, 1);
  }
}


TEST(Mysys, AtomicPerfStd)
{
  std::atomic<int64> a { 0 };
  for (int64 i = 0; i < num_iterations; i++)
  {
    int64 v= a;
    EXPECT_EQ(v, i);
    a++;
  }
}

}
