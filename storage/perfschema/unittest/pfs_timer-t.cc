/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <my_global.h>
#include <my_pthread.h>
#include <pfs_timer.h>
#include "my_sys.h"
#include <tap.h>

void test_timers()
{
  ulonglong t1_a;
  ulonglong t2_a;
  ulonglong t3_a;
  ulonglong t4_a;
  ulonglong t5_a;
  ulonglong t1_b;
  ulonglong t2_b;
  ulonglong t3_b;
  ulonglong t4_b;
  ulonglong t5_b;

  init_timers();

  t1_a= get_timer_pico_value(TIMER_NAME_CYCLE);
  /* Wait 5 seconds */
  my_sleep(5000000);
  t1_b= get_timer_pico_value(TIMER_NAME_CYCLE);

  t2_a= get_timer_pico_value(TIMER_NAME_NANOSEC);
  my_sleep(5000000);
  t2_b= get_timer_pico_value(TIMER_NAME_NANOSEC);

  t3_a= get_timer_pico_value(TIMER_NAME_MICROSEC);
  my_sleep(5000000);
  t3_b= get_timer_pico_value(TIMER_NAME_MICROSEC);

  t4_a= get_timer_pico_value(TIMER_NAME_MILLISEC);
  my_sleep(5000000);
  t4_b= get_timer_pico_value(TIMER_NAME_MILLISEC);

  t5_a= get_timer_pico_value(TIMER_NAME_TICK);
  my_sleep(5000000);
  t5_b= get_timer_pico_value(TIMER_NAME_TICK);

  /*
    Print the timer values, for manual inspection by a human.
    Tests involving low level timers can not be automated.
  */
  diag("cycle a: %13llu", t1_a);
  diag("nano a: %13llu", t2_a);
  diag("micro a: %13llu", t3_a);
  diag("milli a: %13llu", t4_a);
  diag("tick a: %13llu", t5_a);

  diag("cycle b: %13llu", t1_b);
  diag("nano b: %13llu", t2_b);
  diag("micro b: %13llu", t3_b);
  diag("milli b: %13llu", t4_b);
  diag("tick b: %13llu", t5_b);

  diag("cycle b-a: %13llu", t1_b-t1_a);
  diag("nano b-a: %13llu", t2_b-t2_a);
  diag("micro b-a: %13llu", t3_b-t3_a);
  diag("milli b-a: %13llu", t4_b-t4_a);
  diag("tick b-a: %13llu", t5_b-t5_a);

  if ((t1_a == 0) && (t1_b == 0))
    skip(1, "cycle timer not implemented");
  else
    ok(t1_b > t1_a, "cycle timer ascending");

  if ((t2_a == 0) && (t2_b == 0))
    skip(1, "nano timer not implemented");
  else
    ok(t2_b > t2_a, "nano timer ascending");

  if ((t3_a == 0) && (t3_b == 0))
    skip(1, "micro timer not implemented");
  else
    ok(t3_b > t3_a, "micro timer ascending");

  if ((t4_a == 0) && (t4_b == 0))
    skip(1, "milli timer not implemented");
  else
    ok(t4_b > t4_a, "milli timer ascending");

  if ((t5_a == 0) && (t5_b == 0))
    skip(1, "tick timer not implemented");
  else
    ok(t5_b > t5_a, "tick timer ascending");
}

void do_all_tests()
{
  PFS_atomic::init();

  test_timers();

  PFS_atomic::cleanup();
}

int main(int, char **)
{
  plan(5);
  MY_INIT("pfs_timer-t");
  do_all_tests();
  return 0;
}

