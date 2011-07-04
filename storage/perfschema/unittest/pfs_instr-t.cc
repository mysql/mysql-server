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
#include <pfs_instr.h>
#include <pfs_stat.h>
#include <pfs_global.h>
#include <tap.h>

#include <memory.h>

#include "stub_server_misc.h"

void test_no_instruments()
{
  int rc;
  PFS_global_param param;

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 0, "zero init");

  cleanup_instruments();
}

void test_no_instances()
{
  int rc;
  PFS_mutex_class dummy_mutex_class;
  PFS_rwlock_class dummy_rwlock_class;
  PFS_cond_class dummy_cond_class;
  PFS_thread_class dummy_thread_class;
  PFS_file_class dummy_file_class;
  PFS_table_share dummy_table_share;
  PFS_mutex *mutex;
  PFS_rwlock *rwlock;
  PFS_cond *cond;
  PFS_thread *thread;
  PFS_file *file;
  PFS_table *table;
  PFS_global_param param;

  param.m_enabled= true;
  param.m_mutex_class_sizing= 1;
  param.m_rwlock_class_sizing= 1;
  param.m_cond_class_sizing= 1;
  param.m_thread_class_sizing= 1;
  param.m_table_share_sizing= 1;
  param.m_file_class_sizing= 1;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 0, "no instances init");

  mutex= create_mutex(& dummy_mutex_class, NULL);
  ok(mutex == NULL, "no mutex");
  ok(mutex_lost == 1, "lost 1");
  mutex= create_mutex(& dummy_mutex_class, NULL);
  ok(mutex == NULL, "no mutex");
  ok(mutex_lost == 2, "lost 2");

  rwlock= create_rwlock(& dummy_rwlock_class, NULL);
  ok(rwlock == NULL, "no rwlock");
  ok(rwlock_lost == 1, "lost 1");
  rwlock= create_rwlock(& dummy_rwlock_class, NULL);
  ok(rwlock == NULL, "no rwlock");
  ok(rwlock_lost == 2, "lost 2");

  cond= create_cond(& dummy_cond_class, NULL);
  ok(cond == NULL, "no cond");
  ok(cond_lost == 1, "lost 1");
  cond= create_cond(& dummy_cond_class, NULL);
  ok(cond == NULL, "no cond");
  ok(cond_lost == 2, "lost 2");

  thread= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread == NULL, "no thread");
  ok(thread_lost == 1, "lost 1");
  thread= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread == NULL, "no thread");
  ok(thread_lost == 2, "lost 2");

  PFS_thread fake_thread;
  fake_thread.m_filename_hash_pins= NULL;

  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5);
  ok(file == NULL, "no file");
  ok(file_lost == 1, "lost 1");
  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5);
  ok(file == NULL, "no file");
  ok(file_lost == 2, "lost 2");

  init_file_hash();

  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5);
  ok(file == NULL, "no file");
  ok(file_lost == 3, "lost 3");
  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5);
  ok(file == NULL, "no file");
  ok(file_lost == 4, "lost 4");

  char long_file_name[10000];
  int size= sizeof(long_file_name);
  memset(long_file_name, 'X', size);

  file= find_or_create_file(& fake_thread, & dummy_file_class, long_file_name, size);
  ok(file == NULL, "no file");
  ok(file_lost == 5, "lost 5");

  table= create_table(& dummy_table_share, NULL);
  ok(table == NULL, "no table");
  ok(table_lost == 1, "lost 1");
  table= create_table(& dummy_table_share, NULL);
  ok(table == NULL, "no table");
  ok(table_lost == 2, "lost 2");

  /* No result to test, just make sure it does not crash */
  reset_events_waits_by_instance();
  reset_per_thread_wait_stat();

  cleanup_file_hash();
  cleanup_instruments();
}

void test_with_instances()
{
  int rc;
  PFS_mutex_class dummy_mutex_class;
  PFS_rwlock_class dummy_rwlock_class;
  PFS_cond_class dummy_cond_class;
  PFS_thread_class dummy_thread_class;
  PFS_file_class dummy_file_class;
  PFS_table_share dummy_table_share;
  PFS_mutex *mutex_1;
  PFS_mutex *mutex_2;
  PFS_rwlock *rwlock_1;
  PFS_rwlock *rwlock_2;
  PFS_cond *cond_1;
  PFS_cond *cond_2;
  PFS_thread *thread_1;
  PFS_thread *thread_2;
  PFS_file *file_1;
  PFS_file *file_2;
  PFS_table *table_1;
  PFS_table *table_2;
  PFS_global_param param;

  param.m_enabled= true;
  param.m_mutex_class_sizing= 1;
  param.m_rwlock_class_sizing= 1;
  param.m_cond_class_sizing= 1;
  param.m_thread_class_sizing= 1;
  param.m_table_share_sizing= 1;
  param.m_file_class_sizing= 1;
  param.m_mutex_sizing= 2;
  param.m_rwlock_sizing= 2;
  param.m_cond_sizing= 2;
  param.m_thread_sizing= 2;
  param.m_table_sizing= 2;
  param.m_file_sizing= 2;
  param.m_file_handle_sizing= 100;
  param.m_events_waits_history_sizing= 10;
  param.m_events_waits_history_long_sizing= 10000;

  rc= init_instruments(& param);
  ok(rc == 0, "instances init");

  mutex_1= create_mutex(& dummy_mutex_class, NULL);
  ok(mutex_1 != NULL, "mutex");
  ok(mutex_lost == 0, "not lost");
  mutex_2= create_mutex(& dummy_mutex_class, NULL);
  ok(mutex_2 != NULL, "mutex");
  ok(mutex_lost == 0, "not lost");
  mutex_2= create_mutex(& dummy_mutex_class, NULL);
  ok(mutex_2 == NULL, "no mutex");
  ok(mutex_lost == 1, "lost 1");
  destroy_mutex(mutex_1);
  mutex_2= create_mutex(& dummy_mutex_class, NULL);
  ok(mutex_2 != NULL, "mutex");
  ok(mutex_lost == 1, "no new loss");

  rwlock_1= create_rwlock(& dummy_rwlock_class, NULL);
  ok(rwlock_1 != NULL, "rwlock");
  ok(rwlock_lost == 0, "not lost");
  rwlock_2= create_rwlock(& dummy_rwlock_class, NULL);
  ok(rwlock_2 != NULL, "rwlock");
  ok(rwlock_lost == 0, "not lost");
  rwlock_2= create_rwlock(& dummy_rwlock_class, NULL);
  ok(rwlock_2 == NULL, "no rwlock");
  ok(rwlock_lost == 1, "lost 1");
  destroy_rwlock(rwlock_1);
  rwlock_2= create_rwlock(& dummy_rwlock_class, NULL);
  ok(rwlock_2 != NULL, "rwlock");
  ok(rwlock_lost == 1, "no new loss");

  cond_1= create_cond(& dummy_cond_class, NULL);
  ok(cond_1 != NULL, "cond");
  ok(cond_lost == 0, "not lost");
  cond_2= create_cond(& dummy_cond_class, NULL);
  ok(cond_2 != NULL, "cond");
  ok(cond_lost == 0, "not lost");
  cond_2= create_cond(& dummy_cond_class, NULL);
  ok(cond_2 == NULL, "no cond");
  ok(cond_lost == 1, "lost 1");
  destroy_cond(cond_1);
  cond_2= create_cond(& dummy_cond_class, NULL);
  ok(cond_2 != NULL, "cond");
  ok(cond_lost == 1, "no new loss");

  thread_1= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread_1 != NULL, "thread");
  ok(thread_lost == 0, "not lost");
  thread_2= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread_2 != NULL, "thread");
  ok(thread_lost == 0, "not lost");
  thread_2= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread_2 == NULL, "no thread");
  ok(thread_lost == 1, "lost 1");
  destroy_thread(thread_1);
  thread_2= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread_2 != NULL, "thread");
  ok(thread_lost == 1, "no new loss");

  PFS_thread fake_thread;
  fake_thread.m_filename_hash_pins= NULL;

  file_1= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5);
  ok(file_1 == NULL, "no file");
  ok(file_lost == 1, "lost 1");
  file_1= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5);
  ok(file_1 == NULL, "no file");
  ok(file_lost == 2, "lost 2");

  init_file_hash();
  file_lost= 0;

  file_1= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_A", 7);
  ok(file_1 != NULL, "file");
  ok(file_1->m_file_stat.m_open_count == 1, "open count 1");
  ok(file_lost == 0, "not lost");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_A", 7);
  ok(file_1 == file_2, "same file");
  ok(file_1->m_file_stat.m_open_count == 2, "open count 2");
  ok(file_lost == 0, "not lost");
  release_file(file_2);
  ok(file_1->m_file_stat.m_open_count == 1, "open count 1");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_B", 7);
  ok(file_2 != NULL, "file");
  ok(file_lost == 0, "not lost");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_C", 7);
  ok(file_2 == NULL, "no file");
  ok(file_lost == 1, "lost");
  release_file(file_1);
  /* the file still exists, not destroyed */
  ok(file_1->m_file_stat.m_open_count == 0, "open count 0");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_D", 7);
  ok(file_2 == NULL, "no file");
  ok(file_lost == 2, "lost");

  table_1= create_table(& dummy_table_share, NULL);
  ok(table_1 != NULL, "table");
  ok(table_lost == 0, "not lost");
  table_2= create_table(& dummy_table_share, NULL);
  ok(table_2 != NULL, "table");
  ok(table_lost == 0, "not lost");
  table_2= create_table(& dummy_table_share, NULL);
  ok(table_2 == NULL, "no table");
  ok(table_lost == 1, "lost 1");
  destroy_table(table_1);
  table_2= create_table(& dummy_table_share, NULL);
  ok(table_2 != NULL, "table");
  ok(table_lost == 1, "no new loss");

  //TODO: test that cleanup works
  reset_events_waits_by_instance();
  reset_per_thread_wait_stat();

  cleanup_file_hash();
  cleanup_instruments();
}

void test_per_thread_wait()
{
  int rc;
  PFS_mutex_class dummy_mutex_class;
  PFS_rwlock_class dummy_rwlock_class;
  PFS_cond_class dummy_cond_class;
  PFS_thread_class dummy_thread_class;
  PFS_file_class dummy_file_class;
  PFS_thread *thread;
  PFS_single_stat_chain *base;
  PFS_single_stat_chain *stat;
  PFS_global_param param;


  /* Per mutex info waits should be at [0..9] */
  mutex_class_max= 10;
  /* Per rwlock info waits should be at [10..29] */
  rwlock_class_max= 20;
  /* Per cond info waits should be at [30..69] */
  cond_class_max= 40;
  /* Per file info waits should be at [70..149] */
  file_class_max= 80;
  /* Per table info waits should be at [150..309] */
  table_share_max= 160;

  param.m_enabled= true;
  param.m_mutex_class_sizing= mutex_class_max;
  param.m_rwlock_class_sizing= rwlock_class_max;
  param.m_cond_class_sizing= cond_class_max;
  param.m_thread_class_sizing= 2;
  param.m_table_share_sizing= table_share_max;
  param.m_file_class_sizing= file_class_max;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 2;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 10;
  param.m_events_waits_history_long_sizing= 10000;

  rc= init_instruments(& param);
  ok(rc == 0, "instances init");

  thread= create_thread(& dummy_thread_class, NULL, 0);
  ok(thread != NULL, "thread");
  ok(thread_lost == 0, "not lost");

  base= & thread->m_instr_class_wait_stats[0];

  dummy_mutex_class.m_index= 0;
  stat= find_per_thread_mutex_class_wait_stat(thread, & dummy_mutex_class);
  ok(base + 0 == stat, "fist mutex info slot at 0");
  dummy_mutex_class.m_index= mutex_class_max - 1;
  stat= find_per_thread_mutex_class_wait_stat(thread, & dummy_mutex_class);
  ok(base + 9 == stat, "last mutex info slot at 9");

  dummy_rwlock_class.m_index= 0;
  stat= find_per_thread_rwlock_class_wait_stat(thread, & dummy_rwlock_class);
  ok(base + 10 == stat, "fist rwlock info slot at 10");
  dummy_rwlock_class.m_index= rwlock_class_max - 1;
  stat= find_per_thread_rwlock_class_wait_stat(thread, & dummy_rwlock_class);
  ok(base + 29 == stat, "last rwlock info slot at 29");

  dummy_cond_class.m_index= 0;
  stat= find_per_thread_cond_class_wait_stat(thread, & dummy_cond_class);
  ok(base + 30 == stat, "fist cond info slot at 30");
  dummy_cond_class.m_index= cond_class_max - 1;
  stat= find_per_thread_cond_class_wait_stat(thread, & dummy_cond_class);
  ok(base + 69 == stat, "last cond info slot at 69");

  dummy_file_class.m_index= 0;
  stat= find_per_thread_file_class_wait_stat(thread, & dummy_file_class);
  ok(base + 70 == stat, "fist file info slot at 70");
  dummy_file_class.m_index= file_class_max - 1;
  stat= find_per_thread_file_class_wait_stat(thread, & dummy_file_class);
  ok(base + 149 == stat, "last file info slot at 149");

  cleanup_instruments();
}

void do_all_tests()
{
  PFS_atomic::init();

  test_no_instruments();
  test_no_instances();
  test_with_instances();
  test_per_thread_wait();

  PFS_atomic::cleanup();
}

int main(int, char **)
{
  plan(102);
  MY_INIT("pfs_instr-t");
  do_all_tests();
  return 0;
}

