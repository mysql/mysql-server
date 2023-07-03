/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/unittest/pfs_unit_test_conf.h"

#include <string.h> /* memset */

#include "my_thread.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_global.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_pfs_tls_channel.h"
#include "storage/perfschema/unittest/stub_server_telemetry.h"
#include "unittest/mytap/tap.h"

static void test_oom() {
  int rc;
  PFS_global_param param;
  PSI_thread_service_t *thread_service;
  PSI_thread_bootstrap *thread_boot;
  PSI_mutex_bootstrap *mutex_boot;
  PSI_rwlock_bootstrap *rwlock_boot;
  PSI_cond_bootstrap *cond_boot;
  PSI_file_bootstrap *file_boot;
  PSI_socket_bootstrap *socket_boot;
  PSI_table_bootstrap *table_boot;
  PSI_mdl_bootstrap *mdl_boot;
  PSI_idle_bootstrap *idle_boot;
  PSI_stage_bootstrap *stage_boot;
  PSI_statement_bootstrap *statement_boot;
  PSI_transaction_bootstrap *transaction_boot;
  PSI_memory_bootstrap *memory_boot;
  PSI_error_bootstrap *error_boot;
  PSI_data_lock_bootstrap *data_lock_boot;
  PSI_system_bootstrap *system_boot;
  PSI_tls_channel_bootstrap *tls_channel_boot;

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 0;
  param.m_rwlock_class_sizing = 0;
  param.m_cond_class_sizing = 0;
  param.m_thread_class_sizing = 10;
  param.m_table_share_sizing = 0;
  param.m_file_class_sizing = 0;
  param.m_socket_class_sizing = 0;
  param.m_mutex_sizing = 0;
  param.m_rwlock_sizing = 0;
  param.m_cond_sizing = 0;
  param.m_thread_sizing = 1000;
  param.m_table_sizing = 0;
  param.m_file_sizing = 0;
  param.m_file_handle_sizing = 0;
  param.m_socket_sizing = 0;
  param.m_events_waits_history_sizing = 10;
  param.m_events_waits_history_long_sizing = 0;
  param.m_setup_actor_sizing = 0;
  param.m_setup_object_sizing = 0;
  param.m_host_sizing = 0;
  param.m_user_sizing = 1000;
  param.m_account_sizing = 0;
  param.m_stage_class_sizing = 50;
  param.m_events_stages_history_sizing = 0;
  param.m_events_stages_history_long_sizing = 0;
  param.m_statement_class_sizing = 50;
  param.m_events_statements_history_sizing = 0;
  param.m_events_statements_history_long_sizing = 0;
  param.m_events_transactions_history_sizing = 0;
  param.m_events_transactions_history_long_sizing = 0;
  param.m_digest_sizing = 0;
  param.m_session_connect_attrs_sizing = 0;
  param.m_program_sizing = 0;
  param.m_statement_stack_sizing = 0;
  param.m_memory_class_sizing = 10;
  param.m_metadata_lock_sizing = 0;
  param.m_max_digest_length = 0;
  param.m_max_sql_text_length = 0;
  param.m_error_sizing = 0;
  param.m_consumer_events_stages_current_enabled = false;
  param.m_consumer_events_stages_history_enabled = false;
  param.m_consumer_events_stages_history_long_enabled = false;
  param.m_consumer_events_statements_cpu_enabled = false;
  param.m_consumer_events_statements_current_enabled = false;
  param.m_consumer_events_statements_history_enabled = false;
  param.m_consumer_events_statements_history_long_enabled = false;
  param.m_consumer_events_transactions_current_enabled = false;
  param.m_consumer_events_transactions_history_enabled = false;
  param.m_consumer_events_transactions_history_long_enabled = false;
  param.m_consumer_events_waits_current_enabled = false;
  param.m_consumer_events_waits_history_enabled = false;
  param.m_consumer_events_waits_history_long_enabled = false;
  param.m_consumer_global_instrumentation_enabled = false;
  param.m_consumer_thread_instrumentation_enabled = false;
  param.m_consumer_statement_digest_enabled = false;

  /* Setup */

  stub_alloc_always_fails = false;
  stub_alloc_fails_after_count = 1000;

  pre_initialize_performance_schema();
  rc = initialize_performance_schema(
      &param, &thread_boot, &mutex_boot, &rwlock_boot, &cond_boot, &file_boot,
      &socket_boot, &table_boot, &mdl_boot, &idle_boot, &stage_boot,
      &statement_boot, &transaction_boot, &memory_boot, &error_boot,
      &data_lock_boot, &system_boot, &tls_channel_boot);
  ok(rc == 0, "init ok");
  thread_service = (PSI_thread_service_t *)thread_boot->get_interface(
      PSI_CURRENT_THREAD_VERSION);

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", "T-1", 0, 0, ""}};
  thread_service->register_thread("test", all_thread, 1);

  PSI_thread *thread_1 =
      thread_service->new_thread(thread_key_1, 0, nullptr, 0);
  thread_service->set_thread(thread_1);

  /* Tests */

  int first_fail = 1;
  stub_alloc_fails_after_count = first_fail;
  thread_service->set_thread_account("user1", 5, "", 0);
  ok(global_user_container.m_lost == 1, "oom (user)");

  stub_alloc_fails_after_count = first_fail + 1;
  thread_service->set_thread_account("user2", 5, "", 0);
  ok(global_user_container.m_lost == 2, "oom (user waits)");

  stub_alloc_fails_after_count = first_fail + 2;
  thread_service->set_thread_account("user3", 5, "", 0);
  ok(global_user_container.m_lost == 3, "oom (user stages)");

  stub_alloc_fails_after_count = first_fail + 3;
  thread_service->set_thread_account("user4", 5, "", 0);
  ok(global_user_container.m_lost == 4, "oom (user statements)");

  stub_alloc_fails_after_count = first_fail + 4;
  thread_service->set_thread_account("user5", 5, "", 0);
  ok(global_user_container.m_lost == 5, "oom (user transactions)");

  stub_alloc_fails_after_count = first_fail + 5;
  thread_service->set_thread_account("user6", 5, "", 0);
  ok(global_user_container.m_lost == 6, "oom (user memory)");

  shutdown_performance_schema();
}

static void do_all_tests() { test_oom(); }

int main(int, char **) {
  plan(7);
  MY_INIT("pfs_user-oom-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
