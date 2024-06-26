/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#include <string.h> /* memset */
#include <sys/types.h>

#include "my_thread.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_defaults.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_global.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_pfs_tls_channel.h"
#include "storage/perfschema/unittest/stub_server_logs.h"
#include "storage/perfschema/unittest/stub_server_telemetry.h"
#include "storage/perfschema/unittest/stub_telemetry_metrics.h"
#include "unittest/mytap/tap.h"

PFS_thread pfs_thread;

static void initialize_performance_schema_helper(PFS_global_param *param) {
  stub_alloc_always_fails = false;
  stub_alloc_fails_after_count = 1000;

  param->m_enabled = true;
  param->m_thread_class_sizing = 10;
  param->m_thread_sizing = 1000;
  param->m_account_sizing = 1000;
  transaction_class_max = 0;

  pfs_thread.m_account_hash_pins = nullptr;

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
}

static void test_oom() {
  PFS_global_param param;
  const PFS_account *pfs_account;
  PFS_user_name username;
  PFS_host_name hostname;

  username.set("username", strlen("username"));
  hostname.set("hostname", strlen("hostname"));

  /* Account. */
  memset(&param, 0, sizeof(param));
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 1;
  pfs_account = find_or_create_account(&pfs_thread, &username, &hostname);
  ok(pfs_account == nullptr, "oom (account)");
  ok(global_account_container.m_lost == 1, "lost (account)");
  shutdown_performance_schema();

  /* Account waits. */
  memset(&param, 0, sizeof(param));
  param.m_mutex_class_sizing = 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 2;
  pfs_account = find_or_create_account(&pfs_thread, &username, &hostname);
  ok(pfs_account == nullptr, "oom (account waits)");
  ok(global_account_container.m_lost == 1, "lost (account waits)");
  shutdown_performance_schema();

  /* Account stages. */
  memset(&param, 0, sizeof(param));
  param.m_stage_class_sizing = 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  pfs_account = find_or_create_account(&pfs_thread, &username, &hostname);
  ok(pfs_account == nullptr, "oom (account stages)");
  ok(global_account_container.m_lost == 1, "lost (account stages)");
  shutdown_performance_schema();

  /* Account statements. */
  memset(&param, 0, sizeof(param));
  param.m_statement_class_sizing = 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  pfs_account = find_or_create_account(&pfs_thread, &username, &hostname);
  ok(pfs_account == nullptr, "oom (account statements)");
  ok(global_account_container.m_lost == 1, "lost (account statements)");
  shutdown_performance_schema();

  /* Account transactions. */
  memset(&param, 0, sizeof(param));
  initialize_performance_schema_helper(&param);
  transaction_class_max = 1;
  stub_alloc_fails_after_count = 3;
  pfs_account = find_or_create_account(&pfs_thread, &username, &hostname);
  ok(pfs_account == nullptr, "oom (account transactions)");
  ok(global_account_container.m_lost == 1, "lost (account transactions)");
  shutdown_performance_schema();

  /* Account memory. */
  memset(&param, 0, sizeof(param));
  param.m_memory_class_sizing = 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count = 3;
  pfs_account = find_or_create_account(&pfs_thread, &username, &hostname);
  ok(pfs_account == nullptr, "oom (account memory)");
  ok(global_account_container.m_lost == 1, "lost (account memory)");
  shutdown_performance_schema();
}

static void do_all_tests() { test_oom(); }

int main(int, char **) {
  plan(12);
  MY_INIT("pfs_account-oom-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
