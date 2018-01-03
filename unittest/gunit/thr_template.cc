/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_systime.h"

volatile int32 bad;
my_thread_attr_t thr_attr;
mysql_mutex_t mutex;
mysql_cond_t cond;
uint running_threads;
const int THREADS= 30;
const int CYCLES= 3000;

void test_concurrently(const char *test, my_start_routine handler, int n, int m)
{
  my_thread_handle t;
  ulonglong now= my_getsystime();

  my_thread_attr_init(&thr_attr);
  bad= 0;

  for (running_threads= n ; n ; n--)
  {
    if (my_thread_create(&t, &thr_attr, handler, &m) != 0)
    {
      ADD_FAILURE() << "Could not create thread";
      abort();
    }
  }
  mysql_mutex_lock(&mutex);
  while (running_threads)
    mysql_cond_wait(&cond, &mutex);
  mysql_mutex_unlock(&mutex);

  now= my_getsystime()-now;
  EXPECT_FALSE(bad)
    << "tested " << test
    << " in " << ((double)now)/1e7 << " secs "
    << "(" << bad << ")";
}
