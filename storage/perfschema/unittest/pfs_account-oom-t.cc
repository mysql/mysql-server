/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <pfs_defaults.h>
#include <pfs_user.h>
#include <pfs_host.h>
#include <pfs_account.h>
#include <pfs_buffer_container.h>
#include <tap.h>

#include "stub_pfs_global.h"
#include "stub_global_status_var.h"

#include <string.h> /* memset */

PFS_thread pfs_thread;

void initialize_performance_schema_helper(PFS_global_param *param)
{
  stub_alloc_always_fails= false;
  stub_alloc_fails_after_count= 1000;

  param->m_enabled= true;
  param->m_thread_class_sizing= 10;
  param->m_thread_sizing= 1000;
  param->m_account_sizing= 1000;
  transaction_class_max= 0;

  pfs_thread.m_account_hash_pins= NULL;

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
}

void test_oom()
{
  PFS_global_param param;
  PFS_account *pfs_account;
  const char *username= "username";
  const char *hostname= "hostname";

  uint user_len= (uint)strlen(username);
  uint host_len= (uint)strlen(hostname);

  /* Account. */
  memset(&param, 0, sizeof(param));
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 1;
  pfs_account= find_or_create_account(&pfs_thread, username, user_len, hostname, host_len);
  ok(pfs_account == NULL, "oom (account)");
  ok(global_account_container.m_lost == 1, "lost (account)");
  shutdown_performance_schema();

  /* Account waits. */
  memset(&param, 0, sizeof(param));
  param.m_mutex_class_sizing= 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 2;
  pfs_account= find_or_create_account(&pfs_thread, username, user_len, hostname, host_len);
  ok(pfs_account == NULL, "oom (account waits)");
  ok(global_account_container.m_lost == 1, "lost (account waits)");
  shutdown_performance_schema();


  /* Account stages. */
  memset(&param, 0, sizeof(param));
  param.m_stage_class_sizing= 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  pfs_account= find_or_create_account(&pfs_thread, username, user_len, hostname, host_len);
  ok(pfs_account == NULL, "oom (account stages)");
  ok(global_account_container.m_lost == 1, "lost (account stages)");
  shutdown_performance_schema();

  /* Account statements. */
  memset(&param, 0, sizeof(param));
  param.m_statement_class_sizing= 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  pfs_account= find_or_create_account(&pfs_thread, username, user_len, hostname, host_len);
  ok(pfs_account == NULL, "oom (account statements)");
  ok(global_account_container.m_lost == 1, "lost (account statements)");
  shutdown_performance_schema();

  /* Account transactions. */
  memset(&param, 0, sizeof(param));
  initialize_performance_schema_helper(&param);
  transaction_class_max= 1;
  stub_alloc_fails_after_count= 3;
  pfs_account= find_or_create_account(&pfs_thread, username, user_len, hostname, host_len);
  ok(pfs_account == NULL, "oom (account transactions)");
  ok(global_account_container.m_lost == 1, "lost (account transactions)");
  shutdown_performance_schema();

  /* Account memory. */
  memset(&param, 0, sizeof(param));
  param.m_memory_class_sizing= 10;
  initialize_performance_schema_helper(&param);
  stub_alloc_fails_after_count= 3;
  pfs_account= find_or_create_account(&pfs_thread, username, user_len, hostname, host_len);
  ok(pfs_account == NULL, "oom (account memory)");
  ok(global_account_container.m_lost == 1, "lost (account memory)");
  shutdown_performance_schema();
}

void do_all_tests()
{
  test_oom();
}

int main(int, char **)
{
  plan(12);
  MY_INIT("pfs_account-oom-t");
  do_all_tests();
  return (exit_status());
}
