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
#include <pfs_instr_class.h>
#include <tap.h>

#include <memory.h>

#include "stub_server_misc.h"

void test_no_instruments()
{
  int rc;
  PFS_global_param param;

  memset(& param, 0xFF, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_socket_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_socket_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;
  param.m_setup_actor_sizing= 0;
  param.m_setup_object_sizing= 0;
  param.m_host_sizing= 0;
  param.m_user_sizing= 0;
  param.m_account_sizing= 0;
  param.m_stage_class_sizing= 0;
  param.m_events_stages_history_sizing= 0;
  param.m_events_stages_history_long_sizing= 0;
  param.m_statement_class_sizing= 0;
  param.m_events_statements_history_sizing= 0;
  param.m_events_statements_history_long_sizing= 0;
  param.m_digest_sizing= 0;
  param.m_session_connect_attrs_sizing= 0;

  init_event_name_sizing(& param);
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
  PFS_socket_class dummy_socket_class;
  PFS_mutex *mutex;
  PFS_rwlock *rwlock;
  PFS_cond *cond;
  PFS_thread *thread;
  PFS_file *file;
  PFS_socket *socket;
  PFS_table *table;
  PFS_global_param param;

  memset(& param, 0xFF, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 1;
  param.m_rwlock_class_sizing= 1;
  param.m_cond_class_sizing= 1;
  param.m_thread_class_sizing= 1;
  param.m_table_share_sizing= 1;
  param.m_file_class_sizing= 1;
  param.m_socket_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_socket_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;
  param.m_setup_actor_sizing= 0;
  param.m_setup_object_sizing= 0;
  param.m_host_sizing= 0;
  param.m_user_sizing= 0;
  param.m_account_sizing= 0;
  param.m_stage_class_sizing= 0;
  param.m_events_stages_history_sizing= 0;
  param.m_events_stages_history_long_sizing= 0;
  param.m_statement_class_sizing= 0;
  param.m_events_statements_history_sizing= 0;
  param.m_events_statements_history_long_sizing= 0;
  param.m_digest_sizing= 0;
  param.m_session_connect_attrs_sizing= 0;

  init_event_name_sizing(& param);
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

  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5, true);
  ok(file == NULL, "no file");
  ok(file_lost == 1, "lost 1");
  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5, true);
  ok(file == NULL, "no file");
  ok(file_lost == 2, "lost 2");

  init_file_hash();

  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5, true);
  ok(file == NULL, "no file");
  ok(file_lost == 3, "lost 3");
  file= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5, true);
  ok(file == NULL, "no file");
  ok(file_lost == 4, "lost 4");

  char long_file_name[10000];
  int size= sizeof(long_file_name);
  memset(long_file_name, 'X', size);

  file= find_or_create_file(& fake_thread, & dummy_file_class, long_file_name, size, true);
  ok(file == NULL, "no file");
  ok(file_lost == 5, "lost 5");

  table= create_table(& dummy_table_share, & fake_thread, NULL);
  ok(table == NULL, "no table");
  ok(table_lost == 1, "lost 1");
  table= create_table(& dummy_table_share, & fake_thread, NULL);
  ok(table == NULL, "no table");
  ok(table_lost == 2, "lost 2");

  socket= create_socket(& dummy_socket_class, NULL, NULL, 0);
  ok(socket == NULL, "no socket");
  ok(socket_lost == 1, "lost 1");
  socket= create_socket(& dummy_socket_class, NULL, NULL, 0);
  ok(socket == NULL, "no socket");
  ok(socket_lost == 2, "lost 2");

  /* No result to test, just make sure it does not crash */
  reset_events_waits_by_instance();
  reset_events_waits_by_thread();

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
  PFS_socket_class dummy_socket_class;
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
  PFS_socket *socket_1;
  PFS_socket *socket_2;
  PFS_table *table_1;
  PFS_table *table_2;
  PFS_global_param param;

  memset(& param, 0xFF, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 1;
  param.m_rwlock_class_sizing= 1;
  param.m_cond_class_sizing= 1;
  param.m_thread_class_sizing= 1;
  param.m_table_share_sizing= 1;
  param.m_file_class_sizing= 1;
  param.m_socket_class_sizing= 1;
  param.m_mutex_sizing= 2;
  param.m_rwlock_sizing= 2;
  param.m_cond_sizing= 2;
  param.m_thread_sizing= 2;
  param.m_table_sizing= 2;
  param.m_file_sizing= 2;
  param.m_file_handle_sizing= 100;
  param.m_socket_sizing= 2;
  param.m_events_waits_history_sizing= 10;
  param.m_events_waits_history_long_sizing= 10000;
  param.m_setup_actor_sizing= 0;
  param.m_setup_object_sizing= 0;
  param.m_host_sizing= 0;
  param.m_user_sizing= 0;
  param.m_account_sizing= 0;
  param.m_stage_class_sizing= 0;
  param.m_events_stages_history_sizing= 0;
  param.m_events_stages_history_long_sizing= 0;
  param.m_statement_class_sizing= 0;
  param.m_events_statements_history_sizing= 0;
  param.m_events_statements_history_long_sizing= 0;
  param.m_digest_sizing= 0;
  param.m_session_connect_attrs_sizing= 0;

  init_event_name_sizing(& param);
  rc= init_instruments(& param);
  ok(rc == 0, "instances init");

  dummy_mutex_class.m_event_name_index= 0;
  dummy_rwlock_class.m_event_name_index= 1;
  dummy_cond_class.m_event_name_index= 2;
  dummy_file_class.m_event_name_index= 3;
  dummy_socket_class.m_event_name_index= 4;

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

  file_1= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5, true);
  ok(file_1 == NULL, "no file");
  ok(file_lost == 1, "lost 1");
  file_1= find_or_create_file(& fake_thread, & dummy_file_class, "dummy", 5, true);
  ok(file_1 == NULL, "no file");
  ok(file_lost == 2, "lost 2");

  init_file_hash();
  file_lost= 0;

  file_1= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_A", 7, true);
  ok(file_1 != NULL, "file");
  ok(file_1->m_file_stat.m_open_count == 1, "open count 1");
  ok(file_lost == 0, "not lost");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_A", 7, true);
  ok(file_1 == file_2, "same file");
  ok(file_1->m_file_stat.m_open_count == 2, "open count 2");
  ok(file_lost == 0, "not lost");
  release_file(file_2);
  ok(file_1->m_file_stat.m_open_count == 1, "open count 1");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_B", 7, true);
  ok(file_2 != NULL, "file");
  ok(file_lost == 0, "not lost");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_C", 7, true);
  ok(file_2 == NULL, "no file");
  ok(file_lost == 1, "lost");
  release_file(file_1);
  /* the file still exists, not destroyed */
  ok(file_1->m_file_stat.m_open_count == 0, "open count 0");
  file_2= find_or_create_file(& fake_thread, & dummy_file_class, "dummy_D", 7, true);
  ok(file_2 == NULL, "no file");
  ok(file_lost == 2, "lost");

  socket_1= create_socket(& dummy_socket_class, NULL, NULL, 0);
  ok(socket_1 != NULL, "socket");
  ok(socket_lost == 0, "not lost");
  socket_2= create_socket(& dummy_socket_class, NULL, NULL, 0);
  ok(socket_2 != NULL, "socket");
  ok(socket_lost == 0, "not lost");
  socket_2= create_socket(& dummy_socket_class, NULL, NULL, 0);
  ok(socket_2 == NULL, "no socket");
  ok(socket_lost == 1, "lost 1");
  destroy_socket(socket_1);
  socket_2= create_socket(& dummy_socket_class, NULL, NULL, 0);
  ok(socket_2 != NULL, "socket");
  ok(socket_lost == 1, "no new loss");

  table_1= create_table(& dummy_table_share, & fake_thread, NULL);
  ok(table_1 != NULL, "table");
  ok(table_lost == 0, "not lost");
  table_2= create_table(& dummy_table_share, & fake_thread, NULL);
  ok(table_2 != NULL, "table");
  ok(table_lost == 0, "not lost");
  table_2= create_table(& dummy_table_share, & fake_thread, NULL);
  ok(table_2 == NULL, "no table");
  ok(table_lost == 1, "lost 1");
  destroy_table(table_1);
  table_2= create_table(& dummy_table_share, & fake_thread, NULL);
  ok(table_2 != NULL, "table");
  ok(table_lost == 1, "no new loss");

  //TODO: test that cleanup works
  reset_events_waits_by_instance();
  reset_events_waits_by_thread();

  cleanup_file_hash();
  cleanup_instruments();
}

void do_all_tests()
{
  PFS_atomic::init();

  test_no_instruments();
  test_no_instances();
  test_with_instances();

  PFS_atomic::cleanup();
}

int main(int, char **)
{
  plan(103);
  MY_INIT("pfs_instr-t");
  do_all_tests();
  return 0;
}

