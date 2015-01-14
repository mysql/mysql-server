/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

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
