/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_builtin_memory.cc
  Performance schema built in memory instrumentation.
*/

#include "storage/perfschema/pfs_builtin_memory.h"

#include <assert.h>

#include "storage/perfschema/pfs_global.h"

PFS_builtin_memory_class builtin_memory_mutex;
PFS_builtin_memory_class builtin_memory_rwlock;
PFS_builtin_memory_class builtin_memory_cond;
PFS_builtin_memory_class builtin_memory_file;
PFS_builtin_memory_class builtin_memory_socket;
PFS_builtin_memory_class builtin_memory_mdl;
PFS_builtin_memory_class builtin_memory_file_handle;

PFS_builtin_memory_class builtin_memory_account;
PFS_builtin_memory_class builtin_memory_account_waits;
PFS_builtin_memory_class builtin_memory_account_stages;
PFS_builtin_memory_class builtin_memory_account_statements;
PFS_builtin_memory_class builtin_memory_account_transactions;
PFS_builtin_memory_class builtin_memory_account_errors;
PFS_builtin_memory_class builtin_memory_account_memory;

PFS_builtin_memory_class builtin_memory_global_stages;
PFS_builtin_memory_class builtin_memory_global_statements;
PFS_builtin_memory_class builtin_memory_global_memory;
PFS_builtin_memory_class builtin_memory_global_errors;

PFS_builtin_memory_class builtin_memory_host;
PFS_builtin_memory_class builtin_memory_host_waits;
PFS_builtin_memory_class builtin_memory_host_stages;
PFS_builtin_memory_class builtin_memory_host_statements;
PFS_builtin_memory_class builtin_memory_host_transactions;
PFS_builtin_memory_class builtin_memory_host_errors;
PFS_builtin_memory_class builtin_memory_host_memory;

PFS_builtin_memory_class builtin_memory_thread;
PFS_builtin_memory_class builtin_memory_thread_waits;
PFS_builtin_memory_class builtin_memory_thread_stages;
PFS_builtin_memory_class builtin_memory_thread_statements;
PFS_builtin_memory_class builtin_memory_thread_transactions;
PFS_builtin_memory_class builtin_memory_thread_errors;
PFS_builtin_memory_class builtin_memory_thread_memory;

PFS_builtin_memory_class builtin_memory_thread_waits_history;
PFS_builtin_memory_class builtin_memory_thread_stages_history;
PFS_builtin_memory_class builtin_memory_thread_statements_history;
PFS_builtin_memory_class builtin_memory_thread_statements_history_tokens;
PFS_builtin_memory_class builtin_memory_thread_statements_history_sqltext;
PFS_builtin_memory_class builtin_memory_thread_statements_stack;
PFS_builtin_memory_class builtin_memory_thread_statements_stack_tokens;
PFS_builtin_memory_class builtin_memory_thread_statements_stack_sqltext;
PFS_builtin_memory_class builtin_memory_thread_transaction_history;
PFS_builtin_memory_class builtin_memory_thread_session_connect_attrs;

PFS_builtin_memory_class builtin_memory_user;
PFS_builtin_memory_class builtin_memory_user_waits;
PFS_builtin_memory_class builtin_memory_user_stages;
PFS_builtin_memory_class builtin_memory_user_statements;
PFS_builtin_memory_class builtin_memory_user_transactions;
PFS_builtin_memory_class builtin_memory_user_errors;
PFS_builtin_memory_class builtin_memory_user_memory;

PFS_builtin_memory_class builtin_memory_mutex_class;
PFS_builtin_memory_class builtin_memory_rwlock_class;
PFS_builtin_memory_class builtin_memory_cond_class;
PFS_builtin_memory_class builtin_memory_thread_class;
PFS_builtin_memory_class builtin_memory_file_class;
PFS_builtin_memory_class builtin_memory_socket_class;
PFS_builtin_memory_class builtin_memory_stage_class;
PFS_builtin_memory_class builtin_memory_statement_class;
PFS_builtin_memory_class builtin_memory_memory_class;

PFS_builtin_memory_class builtin_memory_setup_actor;
PFS_builtin_memory_class builtin_memory_setup_object;

PFS_builtin_memory_class builtin_memory_digest;
PFS_builtin_memory_class builtin_memory_digest_tokens;
PFS_builtin_memory_class builtin_memory_digest_sample_sqltext;

PFS_builtin_memory_class builtin_memory_stages_history_long;
PFS_builtin_memory_class builtin_memory_statements_history_long;
PFS_builtin_memory_class builtin_memory_statements_history_long_tokens;
PFS_builtin_memory_class builtin_memory_statements_history_long_sqltext;
PFS_builtin_memory_class builtin_memory_transactions_history_long;
PFS_builtin_memory_class builtin_memory_waits_history_long;

PFS_builtin_memory_class builtin_memory_table;
PFS_builtin_memory_class builtin_memory_table_share;
PFS_builtin_memory_class builtin_memory_table_share_index;
PFS_builtin_memory_class builtin_memory_table_share_lock;

PFS_builtin_memory_class builtin_memory_program;
PFS_builtin_memory_class builtin_memory_prepared_stmt;

PFS_builtin_memory_class builtin_memory_scalable_buffer;

static void init_builtin_memory_class(PFS_builtin_memory_class *klass,
                                      const char *name,
                                      const char *documentation) {
  klass->m_class.m_type = PFS_CLASS_MEMORY;
  klass->m_class.m_enabled = true; /* Immutable */
  klass->m_class.m_timed = false;  /* N/A */
  klass->m_class.m_flags = PSI_FLAG_ONLY_GLOBAL_STAT;
  klass->m_class.m_volatility = PSI_VOLATILITY_PERMANENT;
  klass->m_class.m_documentation = const_cast<char *>(documentation);
  klass->m_class.m_event_name_index = 0;
  klass->m_class.m_name.set(PFS_CLASS_MEMORY, name);
  assert(klass->m_class.m_name.length() <= klass->m_class.m_name.max_length);

  klass->m_stat.reset();
}

#define PREFIX "memory/performance_schema/"
#define TABLE_DOC(X) PREFIX X, "Memory used for table performance_schema." X
#define COL_DOC(X, Y) \
  PREFIX X "." Y, "Memory used for table performance_schema." X ", column " Y
#define GEN_DOC(X, Y) PREFIX X, "Memory used for " Y

/* clang-format off */
void
init_all_builtin_memory_class()
{
  init_builtin_memory_class(&builtin_memory_mutex,
                            TABLE_DOC("mutex_instances"));

  init_builtin_memory_class(&builtin_memory_rwlock,
                            TABLE_DOC("rwlock_instances"));

  init_builtin_memory_class(&builtin_memory_cond,
                            TABLE_DOC("cond_instances"));

  init_builtin_memory_class(&builtin_memory_file,
                            TABLE_DOC("file_instances"));

  init_builtin_memory_class(&builtin_memory_socket,
                            TABLE_DOC("socket_instances"));

  init_builtin_memory_class(&builtin_memory_mdl,
                            TABLE_DOC("metadata_locks"));

  init_builtin_memory_class(&builtin_memory_file_handle,
                            TABLE_DOC("file_handle"));

  init_builtin_memory_class(&builtin_memory_account,
                            TABLE_DOC("accounts"));

  init_builtin_memory_class(&builtin_memory_account_waits,
                            TABLE_DOC("events_waits_summary_by_account_by_event_name"));

  init_builtin_memory_class(&builtin_memory_account_stages,
                            TABLE_DOC("events_stages_summary_by_account_by_event_name"));

  init_builtin_memory_class(&builtin_memory_account_statements,
                            TABLE_DOC("events_statements_summary_by_account_by_event_name"));

  init_builtin_memory_class(&builtin_memory_account_transactions,
                            TABLE_DOC("events_transactions_summary_by_account_by_event_name"));

  init_builtin_memory_class(&builtin_memory_account_errors,
                            TABLE_DOC("events_errors_summary_by_account_by_error"));

  init_builtin_memory_class(&builtin_memory_account_memory,
                            TABLE_DOC("memory_summary_by_account_by_event_name"));

  init_builtin_memory_class(&builtin_memory_global_stages,
                            TABLE_DOC("events_stages_summary_global_by_event_name"));

  init_builtin_memory_class(&builtin_memory_global_statements,
                            TABLE_DOC("events_statements_summary_global_by_event_name"));

  init_builtin_memory_class(&builtin_memory_global_memory,
                            TABLE_DOC("memory_summary_global_by_event_name"));

  init_builtin_memory_class(&builtin_memory_global_errors,
                            TABLE_DOC("events_errors_summary_global_by_error"));

  init_builtin_memory_class(&builtin_memory_host,
                            TABLE_DOC("hosts"));

  init_builtin_memory_class(&builtin_memory_host_waits,
                            TABLE_DOC("events_waits_summary_by_host_by_event_name"));

  init_builtin_memory_class(&builtin_memory_host_stages,
                            TABLE_DOC("events_stages_summary_by_host_by_event_name"));

  init_builtin_memory_class(&builtin_memory_host_statements,
                            TABLE_DOC("events_statements_summary_by_host_by_event_name"));

  init_builtin_memory_class(&builtin_memory_host_transactions,
                            TABLE_DOC("events_transactions_summary_by_host_by_event_name"));

  init_builtin_memory_class(&builtin_memory_host_errors,
                            TABLE_DOC("events_errors_summary_by_host_by_error"));

  init_builtin_memory_class(&builtin_memory_host_memory,
                            TABLE_DOC("memory_summary_by_host_by_event_name"));

  init_builtin_memory_class(&builtin_memory_thread,
                            TABLE_DOC("threads"));

  init_builtin_memory_class(&builtin_memory_thread_waits,
                            TABLE_DOC("events_waits_summary_by_thread_by_event_name"));

  init_builtin_memory_class(&builtin_memory_thread_stages,
                            TABLE_DOC("events_stages_summary_by_thread_by_event_name"));

  init_builtin_memory_class(&builtin_memory_thread_statements,
                            TABLE_DOC("events_statements_summary_by_thread_by_event_name"));

  init_builtin_memory_class(&builtin_memory_thread_transactions,
                            TABLE_DOC("events_transactions_summary_by_thread_by_event_name"));

  init_builtin_memory_class(&builtin_memory_thread_errors,
                            TABLE_DOC("events_errors_summary_by_thread_by_error"));

  init_builtin_memory_class(&builtin_memory_thread_memory,
                            TABLE_DOC("memory_summary_by_thread_by_event_name"));

  init_builtin_memory_class(&builtin_memory_thread_waits_history,
                            TABLE_DOC("events_waits_history"));

  init_builtin_memory_class(&builtin_memory_thread_stages_history,
                            TABLE_DOC("events_stages_history"));

  init_builtin_memory_class(&builtin_memory_thread_statements_history,
                            TABLE_DOC("events_statements_history"));

  init_builtin_memory_class(&builtin_memory_thread_statements_history_tokens,
                            COL_DOC("events_statements_history", "digest_text"));

  init_builtin_memory_class(&builtin_memory_thread_statements_history_sqltext,
                            COL_DOC("events_statements_history", "sql_text"));

  init_builtin_memory_class(&builtin_memory_thread_statements_stack,
                            TABLE_DOC("events_statements_current"));

  init_builtin_memory_class(&builtin_memory_thread_statements_stack_tokens,
                            COL_DOC("events_statements_current", "digest_text"));

  init_builtin_memory_class(&builtin_memory_thread_statements_stack_sqltext,
                            COL_DOC("events_statements_current", "sql_text"));

  init_builtin_memory_class(&builtin_memory_thread_transaction_history,
                            TABLE_DOC("events_transactions_history"));

  init_builtin_memory_class(&builtin_memory_thread_session_connect_attrs,
                            TABLE_DOC("session_connect_attrs"));

  init_builtin_memory_class(&builtin_memory_user,
                            TABLE_DOC("users"));

  init_builtin_memory_class(&builtin_memory_user_waits,
                            TABLE_DOC("events_waits_summary_by_user_by_event_name"));

  init_builtin_memory_class(&builtin_memory_user_stages,
                            TABLE_DOC("events_stages_summary_by_user_by_event_name"));

  init_builtin_memory_class(&builtin_memory_user_statements,
                            TABLE_DOC("events_statements_summary_by_user_by_event_name"));

  init_builtin_memory_class(&builtin_memory_user_transactions,
                            TABLE_DOC("events_transactions_summary_by_user_by_event_name"));

  init_builtin_memory_class(&builtin_memory_user_errors,
                            TABLE_DOC("events_errors_summary_by_user_by_error"));

  init_builtin_memory_class(&builtin_memory_user_memory,
                            TABLE_DOC("memory_summary_by_user_by_event_name"));

  init_builtin_memory_class(&builtin_memory_mutex_class,
                            GEN_DOC("mutex_class", "mutex instrument classes"));

  init_builtin_memory_class(&builtin_memory_rwlock_class,
                            GEN_DOC("rwlock_class", "rwlock instrument classes"));

  init_builtin_memory_class(&builtin_memory_cond_class,
                            GEN_DOC("cond_class", "cond instrument classes"));

  init_builtin_memory_class(&builtin_memory_thread_class,
                            GEN_DOC("thread_class", "thread instrument classes"));

  init_builtin_memory_class(&builtin_memory_file_class,
                            GEN_DOC("file_class", "file instrument classes"));

  init_builtin_memory_class(&builtin_memory_socket_class,
                            GEN_DOC("socket_class", "socket instrument classes"));

  init_builtin_memory_class(&builtin_memory_stage_class,
                            GEN_DOC("stage_class", "stage instrument classes"));

  init_builtin_memory_class(&builtin_memory_statement_class,
                            GEN_DOC("statement_class", "statement instrument classes"));

  init_builtin_memory_class(&builtin_memory_memory_class,
                            GEN_DOC("memory_class", "memory instrument classes"));

  init_builtin_memory_class(&builtin_memory_setup_actor,
                            TABLE_DOC("setup_actors"));

  init_builtin_memory_class(&builtin_memory_setup_object,
                            TABLE_DOC("setup_objects"));

  init_builtin_memory_class(&builtin_memory_digest,
                            TABLE_DOC("events_statements_summary_by_digest"));

  init_builtin_memory_class(&builtin_memory_digest_tokens,
                            COL_DOC("events_statements_summary_by_digest", "digest_text"));

  init_builtin_memory_class(&builtin_memory_stages_history_long,
                            TABLE_DOC("events_stages_history_long"));

  init_builtin_memory_class(&builtin_memory_statements_history_long,
                            TABLE_DOC("events_statements_history_long"));

  init_builtin_memory_class(&builtin_memory_statements_history_long_tokens,
                            COL_DOC("events_statements_history_long", "digest_text"));

  init_builtin_memory_class(&builtin_memory_statements_history_long_sqltext,
                            COL_DOC("events_statements_history_long", "sql_text"));

  init_builtin_memory_class(&builtin_memory_transactions_history_long,
                            TABLE_DOC("events_transactions_history_long"));

  init_builtin_memory_class(&builtin_memory_waits_history_long,
                            TABLE_DOC("events_waits_history_long"));

  init_builtin_memory_class(&builtin_memory_table,
                            TABLE_DOC("table_handles"));

  init_builtin_memory_class(&builtin_memory_table_share,
                            TABLE_DOC("table_shares"));

  init_builtin_memory_class(&builtin_memory_table_share_index,
                            TABLE_DOC("table_io_waits_summary_by_index_usage"));

  init_builtin_memory_class(&builtin_memory_table_share_lock,
                            TABLE_DOC("table_lock_waits_summary_by_table"));

  init_builtin_memory_class(&builtin_memory_program,
                            TABLE_DOC("events_statements_summary_by_program"));

  init_builtin_memory_class(&builtin_memory_prepared_stmt,
                            TABLE_DOC("prepared_statements_instances"));

  init_builtin_memory_class(&builtin_memory_scalable_buffer,
                            GEN_DOC("scalable_buffer", "scalable buffers"));
}
/* clang-format off */

static PFS_builtin_memory_class* all_builtin_memory[] = {
  &builtin_memory_mutex,
  &builtin_memory_rwlock,
  &builtin_memory_cond,
  &builtin_memory_file,
  &builtin_memory_socket,
  &builtin_memory_mdl,
  &builtin_memory_file_handle,

  &builtin_memory_account,
  &builtin_memory_account_waits,
  &builtin_memory_account_stages,
  &builtin_memory_account_statements,
  &builtin_memory_account_transactions,
  &builtin_memory_account_errors,
  &builtin_memory_account_memory,

  &builtin_memory_global_stages,
  &builtin_memory_global_statements,
  &builtin_memory_global_memory,
  &builtin_memory_global_errors,

  &builtin_memory_host,
  &builtin_memory_host_waits,
  &builtin_memory_host_stages,
  &builtin_memory_host_statements,
  &builtin_memory_host_transactions,
  &builtin_memory_host_errors,
  &builtin_memory_host_memory,

  &builtin_memory_thread,
  &builtin_memory_thread_waits,
  &builtin_memory_thread_stages,
  &builtin_memory_thread_statements,
  &builtin_memory_thread_transactions,
  &builtin_memory_thread_errors,
  &builtin_memory_thread_memory,

  &builtin_memory_thread_waits_history,
  &builtin_memory_thread_stages_history,
  &builtin_memory_thread_statements_history,
  &builtin_memory_thread_statements_history_tokens,
  &builtin_memory_thread_statements_history_sqltext,
  &builtin_memory_thread_statements_stack,
  &builtin_memory_thread_statements_stack_tokens,
  &builtin_memory_thread_statements_stack_sqltext,
  &builtin_memory_thread_transaction_history,
  &builtin_memory_thread_session_connect_attrs,

  &builtin_memory_user,
  &builtin_memory_user_waits,
  &builtin_memory_user_stages,
  &builtin_memory_user_statements,
  &builtin_memory_user_transactions,
  &builtin_memory_user_errors,
  &builtin_memory_user_memory,

  &builtin_memory_mutex_class,
  &builtin_memory_rwlock_class,
  &builtin_memory_cond_class,
  &builtin_memory_thread_class,
  &builtin_memory_file_class,
  &builtin_memory_socket_class,
  &builtin_memory_stage_class,
  &builtin_memory_statement_class,
  &builtin_memory_memory_class,

  &builtin_memory_setup_actor,
  &builtin_memory_setup_object,

  &builtin_memory_digest,
  &builtin_memory_digest_tokens,

  &builtin_memory_stages_history_long,
  &builtin_memory_statements_history_long,
  &builtin_memory_statements_history_long_tokens,
  &builtin_memory_statements_history_long_sqltext,
  &builtin_memory_transactions_history_long,
  &builtin_memory_waits_history_long,

  &builtin_memory_table,
  &builtin_memory_table_share,
  &builtin_memory_table_share_index,
  &builtin_memory_table_share_lock,

  &builtin_memory_program,
  &builtin_memory_prepared_stmt,

  &builtin_memory_scalable_buffer,

  /*
    MAINTAINER:
    When changing builtin memory,
    make sure to adjust pfs_show_status() as well.
  */

  nullptr};

PFS_builtin_memory_class*
find_builtin_memory_class(PFS_builtin_memory_key key)
{
  if (key == 0)
  {
    return nullptr;
  }

  return all_builtin_memory[key - 1];
}
