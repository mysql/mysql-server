/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "storage/perfschema/unittest/pfs_unit_test_conf.h"

#include <string.h> /* memset */

#include "my_thread.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_global.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_server_logs.h"
#include "storage/perfschema/unittest/stub_server_telemetry.h"
#include "storage/perfschema/unittest/stub_telemetry_metrics.h"
#include "unittest/mytap/tap.h"

PSI_thread_key thread_key_1;
PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", "T-1", 0, 0, ""}};

/** Simulate initialize_performance_schema(). */

static PSI_thread_service_t *initialize_performance_schema_helper(
    PFS_global_param *param) {
  PSI_thread_service_t *thread_service;
  stub_alloc_always_fails = false;
  stub_alloc_fails_after_count = 1000;

  param->m_enabled = true;
  param->m_thread_class_sizing = 10;
  param->m_thread_sizing = 1000;

  pre_initialize_performance_schema();

  init_event_name_sizing(param);
  init_sync_class(param->m_mutex_class_sizing, param->m_rwlock_class_sizing,
                  param->m_cond_class_sizing);
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
  init_events_statements_history_long(
      param->m_events_statements_history_long_sizing);
  init_events_transactions_history_long(
      param->m_events_transactions_history_long_sizing);
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
  pfs_initialized = true;

  thread_service = (PSI_thread_service_t *)pfs_thread_bootstrap.get_interface(
      PSI_CURRENT_THREAD_VERSION);
  thread_service->register_thread("test", all_thread, 1);
  return (thread_service);
}

static void test_oom() {
  int rc;
  PSI_thread_service_t *thread_service;
  PFS_global_param param;

  stub_alloc_always_fails = false;
  stub_alloc_fails_after_count = 1000;

  PFS_mutex_class dummy_mutex_class;
  PFS_rwlock_class dummy_rwlock_class;
  PFS_cond_class dummy_cond_class;
  PFS_thread_class dummy_thread_class;
  PFS_file_class dummy_file_class;
  PFS_socket_class dummy_socket_class;
  PFS_table_share dummy_table_share;
  PFS_mutex *mutex_1;
  const PFS_mutex *mutex_2;
  PFS_rwlock *rwlock_1;
  const PFS_rwlock *rwlock_2;
  PFS_cond *cond_1;
  const PFS_cond *cond_2;
  PFS_thread *thread_1;
  const PFS_thread *thread_2;
  PFS_file *file_1;
  const PFS_file *file_2;
  PFS_socket *socket_1;
  const PFS_socket *socket_2;
  PFS_table *table_1;
  const PFS_table *table_2;

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 1;
  param.m_rwlock_class_sizing = 1;
  param.m_cond_class_sizing = 1;
  param.m_thread_class_sizing = 1;
  param.m_table_share_sizing = 1;
  param.m_file_class_sizing = 1;
  param.m_socket_class_sizing = 1;
  param.m_mutex_sizing = 1;
  param.m_rwlock_sizing = 1;
  param.m_cond_sizing = 1;
  param.m_thread_sizing = 1;
  param.m_table_sizing = 1;
  param.m_file_sizing = 1;
  param.m_file_handle_sizing = 100;
  param.m_socket_sizing = 2;
  param.m_events_waits_history_sizing = 10;
  param.m_events_waits_history_long_sizing = 10000;
  param.m_setup_actor_sizing = 0;
  param.m_setup_object_sizing = 0;
  param.m_host_sizing = 0;
  param.m_user_sizing = 0;
  param.m_account_sizing = 0;
  param.m_stage_class_sizing = 0;
  param.m_events_stages_history_sizing = 0;
  param.m_events_stages_history_long_sizing = 0;
  param.m_statement_class_sizing = 0;
  param.m_events_statements_history_sizing = 0;
  param.m_events_statements_history_long_sizing = 0;
  param.m_events_transactions_history_sizing = 0;
  param.m_events_transactions_history_long_sizing = 0;
  param.m_digest_sizing = 0;
  param.m_session_connect_attrs_sizing = 0;
  param.m_program_sizing = 0;
  param.m_prepared_stmt_sizing = 0;
  param.m_statement_stack_sizing = 0;
  param.m_memory_class_sizing = 1;
  param.m_metadata_lock_sizing = 0;
  param.m_max_digest_length = 0;
  param.m_max_sql_text_length = 0;

  init_event_name_sizing(&param);
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");

  dummy_mutex_class.m_event_name_index = 0;
  dummy_mutex_class.m_flags = 0;
  dummy_mutex_class.m_enabled = true;
  dummy_mutex_class.m_timed = true;
  dummy_mutex_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  dummy_rwlock_class.m_event_name_index = 1;
  dummy_rwlock_class.m_flags = 0;
  dummy_rwlock_class.m_enabled = true;
  dummy_rwlock_class.m_timed = true;
  dummy_rwlock_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  dummy_thread_class.m_enabled = false;
  dummy_thread_class.m_flags = 0;
  dummy_thread_class.m_singleton = nullptr;
  dummy_thread_class.m_history = false;
  snprintf(dummy_thread_class.m_os_name, PFS_MAX_OS_NAME_LENGTH, "OS_NAME");

  dummy_cond_class.m_event_name_index = 2;
  dummy_cond_class.m_flags = 0;
  dummy_cond_class.m_enabled = true;
  dummy_cond_class.m_timed = true;
  dummy_cond_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  dummy_file_class.m_event_name_index = 3;
  dummy_file_class.m_flags = 0;
  dummy_file_class.m_enabled = true;
  dummy_file_class.m_timed = true;
  dummy_file_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  dummy_socket_class.m_event_name_index = 4;
  dummy_socket_class.m_flags = 0;
  dummy_socket_class.m_enabled = true;
  dummy_socket_class.m_timed = true;
  dummy_socket_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  dummy_table_share.m_enabled = true;
  dummy_table_share.m_timed = true;

  /* Create mutex. */
  stub_alloc_always_fails = false;
  mutex_1 = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex_1 != nullptr, "create mutex");
  destroy_mutex(mutex_1);
  cleanup_instruments();

  stub_alloc_always_fails = true;
  mutex_2 = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex_2 == nullptr, "oom (create mutex)");

  /* Create rwlock. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  rwlock_1 = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock_1 != nullptr, "create rwlock");
  destroy_rwlock(rwlock_1);
  cleanup_instruments();

  stub_alloc_always_fails = true;
  rwlock_2 = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock_2 == nullptr, "oom (create rwlock)");

  /* Create cond. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  cond_1 = create_cond(&dummy_cond_class, nullptr);
  ok(cond_1 != nullptr, "create cond");
  destroy_cond(cond_1);
  cleanup_instruments();

  stub_alloc_always_fails = true;
  cond_2 = create_cond(&dummy_cond_class, nullptr);
  ok(cond_2 == nullptr, "oom (create cond)");

  /* Create file. */
  stub_alloc_always_fails = false;
  PFS_thread fake_thread;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  fake_thread.m_filename_hash_pins = nullptr;
  init_file_hash(&param);
  file_1 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy1", 6, true);
  ok(file_1 != nullptr, "create file");
  release_file(file_1);

  stub_alloc_always_fails = true;
  file_2 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy2", 6, true);
  ok(file_2 == nullptr, "oom (create file)");
  cleanup_instruments();

  /* Create socket. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  ok(rc == 0, "instances init");
  socket_1 = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket_1 != nullptr, "create socket");
  destroy_socket(socket_1);
  cleanup_instruments();

  stub_alloc_always_fails = true;
  socket_2 = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket_2 == nullptr, "oom (create socket)");

  /* Create table. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  table_1 = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table_1 != nullptr, "create table");
  destroy_table(table_1);
  cleanup_instruments();

  stub_alloc_always_fails = true;
  table_2 = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table_2 == nullptr, "oom (create table)");

  /* Create thread. */
  stub_alloc_always_fails = false;
  rc = init_instruments(&param);
  thread_1 = create_thread(&dummy_thread_class, 12, nullptr, 0);
  ok(thread_1 != nullptr, "create thread");
  destroy_thread(thread_1);
  cleanup_instruments();

  stub_alloc_always_fails = true;
  thread_2 = create_thread(&dummy_thread_class, 12, nullptr, 0);
  ok(thread_2 == nullptr, "oom (create thread)");

  const PSI_thread *thread;

  /* Per thread wait. */
  memset(&param, 0, sizeof(param));
  param.m_mutex_class_sizing = 50;
  param.m_rwlock_class_sizing = 50;
  param.m_cond_class_sizing = 50;
  param.m_file_class_sizing = 50;
  thread_service = initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 2;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (per thread wait)");

  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_file_class();
  cleanup_instruments();

  /* Thread waits history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_enabled = true;
  param.m_events_waits_history_sizing = 10;
  thread_service = initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (thread waits history sizing)");

  cleanup_thread_class();
  cleanup_instruments();

  /* Per thread stages. */
  memset(&param, 0, sizeof(param));
  param.m_stage_class_sizing = 50;
  thread_service = initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (per thread stages)");

  cleanup_stage_class();
  cleanup_thread_class();
  cleanup_instruments();

  /* Thread stages history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_events_stages_history_sizing = 10;
  thread_service = initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (thread stages history sizing)");

  cleanup_instruments();
  cleanup_thread_class();

  /* Per thread statements. */
  memset(&param, 0, sizeof(param));
  param.m_stage_class_sizing = 50;
  thread_service = initialize_performance_schema_helper(&param);
  init_statement_class(param.m_statement_class_sizing);
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (per thread statements)");

  cleanup_stage_class();
  cleanup_statement_class();
  cleanup_thread_class();
  cleanup_instruments();

  /* Thread statements history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_events_statements_history_sizing = 10;
  thread_service = initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (thread statements history sizing)");

  cleanup_thread_class();
  cleanup_instruments();

  /* Per thread transactions. */
  memset(&param, 0, sizeof(param));
  thread_service = initialize_performance_schema_helper(&param);
  transaction_class_max = 1;  // set by register_global_classes();
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (per thread transactions)");
  transaction_class_max = 0;

  cleanup_thread_class();
  cleanup_instruments();

  /* Thread transactions history sizing. */
  memset(&param, 0, sizeof(param));
  param.m_events_transactions_history_sizing = 10;
  thread_service = initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  thread = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread == nullptr, "oom (thread transactions history sizing)");

  cleanup_thread_class();
  cleanup_instruments();

  /* Global stages. */
  memset(&param, 0, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 10;
  param.m_stage_class_sizing = 20;

  stub_alloc_fails_after_count = 2;
  init_event_name_sizing(&param);
  rc = init_stage_class(param.m_stage_class_sizing);
  ok(rc == 0, "init stage class");
  rc = init_instruments(&param);
  ok(rc == 1, "oom (global stages)");

  cleanup_stage_class();
  cleanup_instruments();

  /* Global statements. */
  memset(&param, 0, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 10;
  param.m_statement_class_sizing = 20;

  stub_alloc_fails_after_count = 2;
  init_event_name_sizing(&param);
  rc = init_statement_class(param.m_statement_class_sizing);
  ok(rc == 0, "init statement class");
  rc = init_instruments(&param);
  ok(rc == 1, "oom (global statements)");

  cleanup_statement_class();
  cleanup_instruments();

  /* Global memory. */
  memset(&param, 0, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 10;
  param.m_memory_class_sizing = 20;

  stub_alloc_fails_after_count = 2;
  init_event_name_sizing(&param);
  rc = init_memory_class(param.m_memory_class_sizing);
  ok(rc == 0, "init memory class");
  rc = init_instruments(&param);
  ok(rc == 1, "oom (global memory)");

  cleanup_memory_class();
  cleanup_instruments();
}

static void do_all_tests() { test_oom(); }

int main(int, char **) {
  plan(33);
  MY_INIT("pfs_instr-oom-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
