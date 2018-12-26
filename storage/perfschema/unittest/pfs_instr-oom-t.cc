/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <my_thread.h>
#include <pfs_instr.h>
#include <pfs_stat.h>
#include <pfs_global.h>
#include <pfs_user.h>
#include <pfs_host.h>
#include <pfs_account.h>
#include <pfs_instr_class.h>
#include <pfs_buffer_container.h>
#include <tap.h>

#include "stub_pfs_global.h"
#include "stub_global_status_var.h"

#include <string.h> /* memset */

extern struct PSI_bootstrap PFS_bootstrap;

PSI_thread_key thread_key_1;
PSI_thread_info all_thread[]=
{
  {&thread_key_1, "T-1", 0}
};

/** Simulate initialize_performance_schema(). */

PSI * initialize_performance_schema_helper(PFS_global_param *param)
{
  PSI *psi;

  stub_alloc_always_fails= false;
  stub_alloc_fails_after_count= 1000;

  param->m_enabled= true;
  param->m_thread_class_sizing= 10;
  param->m_thread_sizing= 1000;

  pre_initialize_performance_schema();

  init_event_name_sizing(param);
  init_sync_class(param->m_mutex_class_sizing, param->m_rwlock_class_sizing, param->m_cond_class_sizing);
  init_thread_class(param->m_thread_class_sizing);
  init_table_share(param->m_table_share_sizing);
  init_table_share_lock_stat(param->m_table_lock_stat_sizing);
  init_table_share_index_stat(param->m_index_stat_sizing);
  init_file_class(param->m_file_class_sizing);
  init_stage_class(param->m_stage_class_sizing);
  init_statement_class(param->m_statement_class_sizing);
  init_socket_class(param->m_socket_class_sizing);
  init_memory_class(param->m_memory_class_sizing);
  init_instruments(param);
  init_events_waits_history_long(param->m_events_waits_history_long_sizing);
  init_events_stages_history_long(param->m_events_stages_history_long_sizing);
  init_events_statements_history_long(param->m_events_statements_history_long_sizing);
  init_events_transactions_history_long(param->m_events_transactions_history_long_sizing);
  init_file_hash(param);
  init_table_share_hash(param);
  init_setup_actor(param);
  init_setup_actor_hash(param);
  init_setup_object(param);
  init_setup_object_hash(param);
  init_host(param);
  init_host_hash(param);
  init_user(param);
  init_user_hash(param);
  init_account(param);
  init_account_hash(param);
  init_digest(param);
  init_digest_hash(param);
  init_program(param);
  init_program_hash(param);
  init_prepared_stmt(param);
  pfs_initialized= true;

  PSI_bootstrap *boot= &PFS_bootstrap;
  psi= (PSI *)boot->get_interface(PSI_VERSION_1);
  psi->register_thread("test", all_thread, 1);
  return (psi);
}

void test_oom()
{
  int rc;
  PSI *psi;
  PFS_global_param param;

  stub_alloc_always_fails= false;
  stub_alloc_fails_after_count= 1000;

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

  memset(& param, 0xFF, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 1;
  param.m_rwlock_class_sizing= 1;
  param.m_cond_class_sizing= 1;
  param.m_thread_class_sizing= 1;
  param.m_table_share_sizing= 1;
  param.m_file_class_sizing= 1;
  param.m_socket_class_sizing= 1;
  param.m_mutex_sizing= 1;
  param.m_rwlock_sizing= 1;
  param.m_cond_sizing= 1;
  param.m_thread_sizing= 1;
  param.m_table_sizing= 1;
  param.m_file_sizing= 1;
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
  param.m_events_transactions_history_sizing= 0;
  param.m_events_transactions_history_long_sizing= 0;
  param.m_digest_sizing= 0;
  param.m_session_connect_attrs_sizing= 0;
  param.m_program_sizing= 0;
  param.m_prepared_stmt_sizing= 0;
  param.m_statement_stack_sizing= 0;
  param.m_memory_class_sizing= 1;
  param.m_metadata_lock_sizing= 0;
  param.m_max_digest_length= 0;
  param.m_max_sql_text_length= 0;

  init_event_name_sizing(&param);
  rc= init_instruments(&param);
  ok(rc == 0, "instances init");

  dummy_mutex_class.m_event_name_index= 0;
  dummy_mutex_class.m_flags= 0;
  dummy_mutex_class.m_enabled= true;
  dummy_mutex_class.m_volatility= PSI_VOLATILITY_UNKNOWN;
  dummy_rwlock_class.m_event_name_index= 1;
  dummy_rwlock_class.m_flags= 0;
  dummy_rwlock_class.m_enabled= true;
  dummy_rwlock_class.m_volatility= PSI_VOLATILITY_UNKNOWN;
  dummy_cond_class.m_event_name_index= 2;
  dummy_cond_class.m_flags= 0;
  dummy_cond_class.m_enabled= true;
  dummy_cond_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_file_class.m_event_name_index= 3;
  dummy_file_class.m_flags= 0;
  dummy_file_class.m_enabled= true;
  dummy_file_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_socket_class.m_event_name_index= 4;
  dummy_socket_class.m_flags= 0;
  dummy_socket_class.m_enabled= true;
  dummy_socket_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_table_share.m_enabled= true;
  dummy_table_share.m_timed= true;

  /* Create mutex. */
  stub_alloc_always_fails= false;
  mutex_1= create_mutex(&dummy_mutex_class, NULL);
  ok(mutex_1 != NULL, "create mutex");
  destroy_mutex(mutex_1);
  cleanup_instruments();

  stub_alloc_always_fails= true;
  mutex_2= create_mutex(&dummy_mutex_class, NULL);
  ok(mutex_2 == NULL, "oom (create mutex)");

  /* Create rwlock. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  rwlock_1= create_rwlock(&dummy_rwlock_class, NULL);
  ok(rwlock_1 != NULL, "create rwlock");
  destroy_rwlock(rwlock_1);
  cleanup_instruments();

  stub_alloc_always_fails= true;
  rwlock_2= create_rwlock(&dummy_rwlock_class, NULL);
  ok(rwlock_2 == NULL, "oom (create rwlock)");

  /* Create cond. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  cond_1= create_cond(&dummy_cond_class, NULL);
  ok(cond_1 != NULL, "create cond");
  destroy_cond(cond_1);
  cleanup_instruments();

  stub_alloc_always_fails= true;
  cond_2= create_cond(&dummy_cond_class, NULL);
  ok(cond_2 == NULL, "oom (create cond)");

  /* Create file. */
  PFS_thread fake_thread;
  rc = init_instruments(&param);
  fake_thread.m_filename_hash_pins= NULL;
  init_file_hash(&param);

  stub_alloc_always_fails = true;
  file_2 = find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file_2 == NULL, "oom (create file)");

  stub_alloc_always_fails= false;
  file_1= find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file_1 != NULL, "create file");
  release_file(file_1);
  cleanup_instruments();

  /* Create socket. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  socket_1= create_socket(&dummy_socket_class, NULL, NULL, 0);
  ok(socket_1 != NULL, "create socket");
  destroy_socket(socket_1);
  cleanup_instruments();

  stub_alloc_always_fails= true;
  socket_2= create_socket(&dummy_socket_class, NULL, NULL, 0);
  ok(socket_2 == NULL, "oom (create socket)");

  /* Create table. */
  stub_alloc_always_fails= false;
  rc = init_instruments(&param);
  table_1= create_table(&dummy_table_share, &fake_thread, NULL);
  ok(table_1 != NULL, "create table");
  destroy_table(table_1);
  cleanup_instruments();

  stub_alloc_always_fails= true;
  table_2= create_table(&dummy_table_share, &fake_thread, NULL);
  ok(table_2 == NULL, "oom (create table)");

  /* Create thread. */
  stub_alloc_always_fails= false;
  rc = init_instruments(&param);
  thread_1= create_thread(&dummy_thread_class, NULL, 0);
  ok(thread_1 != NULL, "create thread");
  destroy_thread(thread_1);
  cleanup_instruments();

  stub_alloc_always_fails= true;
  thread_2= create_thread(&dummy_thread_class, NULL, 0);
  ok(thread_2 == NULL, "oom (create thread)");

  PSI_thread *thread;

  /* Per thread wait. */
  memset(&param, 0, sizeof(param));
  param.m_mutex_class_sizing= 50;
  param.m_rwlock_class_sizing= 50;
  param.m_cond_class_sizing= 50;
  param.m_file_class_sizing= 50;
  param.m_socket_class_sizing= 0;
  psi= initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 2;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (per thread wait)");

  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_file_class();
  cleanup_instruments();

  /* Thread waits history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_enabled= true;
  param.m_events_waits_history_sizing= 10;
  psi= initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (thread waits history sizing)");

  cleanup_thread_class();
  cleanup_instruments();

  /* Per thread stages. */
  memset(&param, 0, sizeof(param));
  param.m_stage_class_sizing= 50;
  psi= initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (per thread stages)");

  cleanup_stage_class();
  cleanup_thread_class();
  cleanup_instruments();
  cleanup_stage_class();

  /* Thread stages history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_events_stages_history_sizing= 10;
  psi= initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (thread stages history sizing)");
  
  cleanup_instruments();
  cleanup_thread_class();

  /* Per thread statements. */
  memset(&param, 0, sizeof(param));
  param.m_stage_class_sizing= 50;
  psi= initialize_performance_schema_helper(&param);
  init_statement_class(param.m_statement_class_sizing);
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (per thread statements)");

  cleanup_stage_class();
  cleanup_statement_class();
  cleanup_thread_class();
  cleanup_instruments();

  /* Thread statements history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_events_statements_history_sizing= 10;
  psi= initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (thread statements history sizing)");
  
  cleanup_thread_class();
  cleanup_instruments();

  /* Per thread transactions. */
  memset(&param, 0, sizeof(param));
  psi= initialize_performance_schema_helper(&param);
  transaction_class_max= 1; // set by register_global_classes();
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (per thread transactions)");
  transaction_class_max= 0;

  cleanup_thread_class();
  cleanup_instruments();

  /* Thread transactions history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_events_transactions_history_sizing= 10;
  psi= initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  thread= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread == NULL, "oom (thread transactions history sizing)");

  cleanup_thread_class();
  cleanup_instruments();

  /* Global stages. */
  memset(&param, 0, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 10;
  param.m_stage_class_sizing= 20;

  stub_alloc_fails_after_count= 2;
  init_event_name_sizing(&param);
  rc= init_stage_class(param.m_stage_class_sizing);
  ok(rc == 0, "init stage class");
  rc= init_instruments(& param);
  ok(rc == 1, "oom (global stages)");

  cleanup_stage_class();
  cleanup_instruments();

  /* Global statements. */
  memset(&param, 0, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 10;
  param.m_statement_class_sizing= 20;

  stub_alloc_fails_after_count= 2;
  init_event_name_sizing(&param);
  rc= init_statement_class(param.m_statement_class_sizing);
  ok(rc == 0, "init statement class");
  rc= init_instruments(&param);
  ok(rc == 1, "oom (global statements)");

  cleanup_statement_class();
  cleanup_instruments();

  /* Global memory. */
  memset(&param, 0, sizeof(param));
  param.m_enabled= true;
  param.m_mutex_class_sizing= 10;
  param.m_memory_class_sizing= 20;

  stub_alloc_fails_after_count= 2;
  init_event_name_sizing(&param);
  rc= init_memory_class(param.m_memory_class_sizing);
  ok(rc == 0, "init memory class");
  rc= init_instruments(& param);
  ok(rc == 1, "oom (global memory)");

  cleanup_memory_class();
  cleanup_instruments();
}

void do_all_tests()
{
  test_oom();
}

int main(int, char **)
{
  plan(32);
  MY_INIT("pfs_instr-oom-t");
  do_all_tests();
  return (exit_status());
}

