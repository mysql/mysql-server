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

#include <memory.h>

#include "my_thread.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_pfs_tls_channel.h"
#include "storage/perfschema/unittest/stub_server_telemetry.h"
#include "storage/perfschema/unittest/stub_telemetry_metrics.h"
#include "unittest/mytap/tap.h"

static void test_no_instruments() {
  int rc;
  PFS_global_param param;

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 0;
  param.m_rwlock_class_sizing = 0;
  param.m_cond_class_sizing = 0;
  param.m_thread_class_sizing = 0;
  param.m_table_share_sizing = 0;
  param.m_file_class_sizing = 0;
  param.m_socket_class_sizing = 0;
  param.m_mutex_sizing = 0;
  param.m_rwlock_sizing = 0;
  param.m_cond_sizing = 0;
  param.m_thread_sizing = 0;
  param.m_table_sizing = 0;
  param.m_file_sizing = 0;
  param.m_file_handle_sizing = 0;
  param.m_socket_sizing = 0;
  param.m_events_waits_history_sizing = 0;
  param.m_events_waits_history_long_sizing = 0;
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
  param.m_memory_class_sizing = 0;
  param.m_metadata_lock_sizing = 0;
  param.m_error_sizing = 0;

  init_event_name_sizing(&param);
  rc = init_instruments(&param);
  ok(rc == 0, "zero init");

  cleanup_instruments();
}

static void test_no_instances() {
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

  dummy_mutex_class.m_event_name_index = 0;
  dummy_mutex_class.m_flags = 0;
  dummy_mutex_class.m_enabled = true;
  dummy_mutex_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_rwlock_class.m_event_name_index = 1;
  dummy_rwlock_class.m_flags = 0;
  dummy_rwlock_class.m_enabled = true;
  dummy_rwlock_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_cond_class.m_event_name_index = 2;
  dummy_cond_class.m_flags = 0;
  dummy_cond_class.m_enabled = true;
  dummy_cond_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_file_class.m_event_name_index = 3;
  dummy_file_class.m_flags = 0;
  dummy_file_class.m_enabled = true;
  dummy_file_class.m_volatility = PSI_VOLATILITY_UNKNOWN;
  dummy_socket_class.m_event_name_index = 4;
  dummy_socket_class.m_flags = 0;
  dummy_socket_class.m_enabled = true;
  dummy_socket_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 1;
  param.m_rwlock_class_sizing = 1;
  param.m_cond_class_sizing = 1;
  param.m_thread_class_sizing = 1;
  param.m_table_share_sizing = 1;
  param.m_file_class_sizing = 1;
  param.m_socket_class_sizing = 0;
  param.m_mutex_sizing = 0;
  param.m_rwlock_sizing = 0;
  param.m_cond_sizing = 0;
  param.m_thread_sizing = 0;
  param.m_table_sizing = 0;
  param.m_file_sizing = 0;
  param.m_file_handle_sizing = 0;
  param.m_socket_sizing = 0;
  param.m_events_waits_history_sizing = 0;
  param.m_events_waits_history_long_sizing = 0;
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
  param.m_error_sizing = 0;

  init_event_name_sizing(&param);
  rc = init_instruments(&param);
  ok(rc == 0, "no instances init");

  mutex = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex == nullptr, "no mutex");
  ok(global_mutex_container.get_lost_counter() == 1, "lost 1");
  mutex = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex == nullptr, "no mutex");
  ok(global_mutex_container.get_lost_counter() == 2, "lost 2");

  rwlock = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock == nullptr, "no rwlock");
  ok(global_rwlock_container.m_lost == 1, "lost 1");
  rwlock = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock == nullptr, "no rwlock");
  ok(global_rwlock_container.m_lost == 2, "lost 2");

  cond = create_cond(&dummy_cond_class, nullptr);
  ok(cond == nullptr, "no cond");
  ok(global_cond_container.m_lost == 1, "lost 1");
  cond = create_cond(&dummy_cond_class, nullptr);
  ok(cond == nullptr, "no cond");
  ok(global_cond_container.m_lost == 2, "lost 2");

  thread = create_thread(&dummy_thread_class, 0, nullptr, 0);
  ok(thread == nullptr, "no thread");
  ok(global_thread_container.m_lost == 1, "lost 1");
  thread = create_thread(&dummy_thread_class, 0, nullptr, 0);
  ok(thread == nullptr, "no thread");
  ok(global_thread_container.m_lost == 2, "lost 2");

  PFS_thread fake_thread;
  fake_thread.m_filename_hash_pins = nullptr;

  file = find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file == nullptr, "no file");
  ok(global_file_container.m_lost == 1, "lost 1");
  file = find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file == nullptr, "no file");
  ok(global_file_container.m_lost == 2, "lost 2");

  init_file_hash(&param);

  file = find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file == nullptr, "no file");
  ok(global_file_container.m_lost == 3, "lost 3");
  file = find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file == nullptr, "no file");
  ok(global_file_container.m_lost == 4, "lost 4");

  char long_file_name[10000];
  int size = sizeof(long_file_name);
  memset(long_file_name, 'X', size);

  file = find_or_create_file(&fake_thread, &dummy_file_class, long_file_name,
                             size, true);
  ok(file == nullptr, "no file");
  ok(global_file_container.m_lost == 5, "lost 5");

  table = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table == nullptr, "no table");
  ok(global_table_container.m_lost == 1, "lost 1");
  table = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table == nullptr, "no table");
  ok(global_table_container.m_lost == 2, "lost 2");

  socket = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket == nullptr, "no socket");
  ok(global_socket_container.m_lost == 1, "lost 1");
  socket = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket == nullptr, "no socket");
  ok(global_socket_container.m_lost == 2, "lost 2");

  /* No result to test, just make sure it does not crash */
  reset_events_waits_by_instance();
  reset_events_waits_by_thread();

  cleanup_file_hash();
  cleanup_instruments();
}

static void test_with_instances() {
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

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 1;
  param.m_rwlock_class_sizing = 1;
  param.m_cond_class_sizing = 1;
  param.m_thread_class_sizing = 1;
  param.m_table_share_sizing = 1;
  param.m_file_class_sizing = 1;
  param.m_socket_class_sizing = 1;
  param.m_mutex_sizing = 2;
  param.m_rwlock_sizing = 2;
  param.m_cond_sizing = 2;
  param.m_thread_sizing = 2;
  param.m_table_sizing = 2;
  param.m_file_sizing = 2;
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
  param.m_error_sizing = 0;

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

  dummy_cond_class.m_event_name_index = 2;
  dummy_cond_class.m_flags = 0;
  dummy_cond_class.m_enabled = true;
  dummy_cond_class.m_timed = true;
  dummy_cond_class.m_volatility = PSI_VOLATILITY_UNKNOWN;

  dummy_thread_class.m_enabled = 0;
  dummy_thread_class.m_flags = 0;
  dummy_thread_class.m_singleton = nullptr;
  dummy_thread_class.m_history = 0;
  snprintf(dummy_thread_class.m_os_name, PFS_MAX_OS_NAME_LENGTH, "OS_NAME");

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

  mutex_1 = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex_1 != nullptr, "mutex");
  ok(global_mutex_container.get_lost_counter() == 0, "not lost");
  mutex_2 = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex_2 != nullptr, "mutex");
  ok(global_mutex_container.get_lost_counter() == 0, "not lost");
  mutex_2 = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex_2 == nullptr, "no mutex");
  ok(global_mutex_container.get_lost_counter() == 1, "lost 1");
  destroy_mutex(mutex_1);
  mutex_2 = create_mutex(&dummy_mutex_class, nullptr);
  ok(mutex_2 != nullptr, "mutex");
  ok(global_mutex_container.get_lost_counter() == 1, "no new loss");

  rwlock_1 = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock_1 != nullptr, "rwlock");
  ok(global_rwlock_container.m_lost == 0, "not lost");
  rwlock_2 = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock_2 != nullptr, "rwlock");
  ok(global_rwlock_container.m_lost == 0, "not lost");
  rwlock_2 = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock_2 == nullptr, "no rwlock");
  ok(global_rwlock_container.m_lost == 1, "lost 1");
  destroy_rwlock(rwlock_1);
  rwlock_2 = create_rwlock(&dummy_rwlock_class, nullptr);
  ok(rwlock_2 != nullptr, "rwlock");
  ok(global_rwlock_container.m_lost == 1, "no new loss");

  cond_1 = create_cond(&dummy_cond_class, nullptr);
  ok(cond_1 != nullptr, "cond");
  ok(global_cond_container.m_lost == 0, "not lost");
  cond_2 = create_cond(&dummy_cond_class, nullptr);
  ok(cond_2 != nullptr, "cond");
  ok(global_cond_container.m_lost == 0, "not lost");
  cond_2 = create_cond(&dummy_cond_class, nullptr);
  ok(cond_2 == nullptr, "no cond");
  ok(global_cond_container.m_lost == 1, "lost 1");
  destroy_cond(cond_1);
  cond_2 = create_cond(&dummy_cond_class, nullptr);
  ok(cond_2 != nullptr, "cond");
  ok(global_cond_container.m_lost == 1, "no new loss");

  thread_1 = create_thread(&dummy_thread_class, 0, nullptr, 0);
  ok(thread_1 != nullptr, "thread");
  ok(global_thread_container.m_lost == 0, "not lost");
  thread_2 = create_thread(&dummy_thread_class, 0, nullptr, 0);
  ok(thread_2 != nullptr, "thread");
  ok(global_thread_container.m_lost == 0, "not lost");
  thread_2 = create_thread(&dummy_thread_class, 0, nullptr, 0);
  ok(thread_2 == nullptr, "no thread");
  ok(global_thread_container.m_lost == 1, "lost 1");
  destroy_thread(thread_1);
  thread_2 = create_thread(&dummy_thread_class, 0, nullptr, 0);
  ok(thread_2 != nullptr, "thread");
  ok(global_thread_container.m_lost == 1, "no new loss");

  PFS_thread fake_thread;
  fake_thread.m_filename_hash_pins = nullptr;

  file_1 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file_1 == nullptr, "no file");
  ok(global_file_container.m_lost == 1, "lost 1");
  file_1 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy", 5, true);
  ok(file_1 == nullptr, "no file");
  ok(global_file_container.m_lost == 2, "lost 2");

  init_file_hash(&param);
  global_file_container.m_lost = 0;

  file_1 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy_A", 7, true);
  ok(file_1 != nullptr, "file");
  ok(file_1->m_file_stat.m_open_count == 1, "open count 1");
  ok(global_file_container.m_lost == 0, "not lost");
  file_2 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy_A", 7, true);
  ok(file_1 == file_2, "same file");
  ok(file_1->m_file_stat.m_open_count == 2, "open count 2");
  ok(global_file_container.m_lost == 0, "not lost");
  release_file(file_2);
  ok(file_1->m_file_stat.m_open_count == 1, "open count 1");
  file_2 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy_B", 7, true);
  ok(file_2 != nullptr, "file");
  ok(global_file_container.m_lost == 0, "not lost");
  file_2 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy_C", 7, true);
  ok(file_2 == nullptr, "no file");
  ok(global_file_container.m_lost == 1, "lost");
  release_file(file_1);
  /* the file still exists, not destroyed */
  ok(file_1->m_file_stat.m_open_count == 0, "open count 0");
  file_2 =
      find_or_create_file(&fake_thread, &dummy_file_class, "dummy_D", 7, true);
  ok(file_2 == nullptr, "no file");
  ok(global_file_container.m_lost == 2, "lost");

  socket_1 = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket_1 != nullptr, "socket");
  ok(global_socket_container.m_lost == 0, "not lost");
  socket_2 = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket_2 != nullptr, "socket");
  ok(global_socket_container.m_lost == 0, "not lost");
  socket_2 = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket_2 == nullptr, "no socket");
  ok(global_socket_container.m_lost == 1, "lost 1");
  destroy_socket(socket_1);
  socket_2 = create_socket(&dummy_socket_class, nullptr, nullptr, 0);
  ok(socket_2 != nullptr, "socket");
  ok(global_socket_container.m_lost == 1, "no new loss");

  table_1 = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table_1 != nullptr, "table");
  ok(global_table_container.m_lost == 0, "not lost");
  table_2 = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table_2 != nullptr, "table");
  ok(global_table_container.m_lost == 0, "not lost");
  table_2 = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table_2 == nullptr, "no table");
  ok(global_table_container.m_lost == 1, "lost 1");
  destroy_table(table_1);
  table_2 = create_table(&dummy_table_share, &fake_thread, nullptr);
  ok(table_2 != nullptr, "table");
  ok(global_table_container.m_lost == 1, "no new loss");

  // TODO: test that cleanup works
  reset_events_waits_by_instance();
  reset_events_waits_by_thread();

  cleanup_file_hash();
  cleanup_instruments();
}

static void do_all_tests() {
  flag_global_instrumentation = true;
  flag_thread_instrumentation = true;

  test_no_instruments();
  test_no_instances();
  test_with_instances();
}

int main(int, char **) {
  plan(103);
  MY_INIT("pfs_instr-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
