/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "pfs_global.h"
#include "pfs_builtin_memory.h"

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
PFS_builtin_memory_class builtin_memory_account_memory;

PFS_builtin_memory_class builtin_memory_global_stages;
PFS_builtin_memory_class builtin_memory_global_statements;
PFS_builtin_memory_class builtin_memory_global_memory;

PFS_builtin_memory_class builtin_memory_host;
PFS_builtin_memory_class builtin_memory_host_waits;
PFS_builtin_memory_class builtin_memory_host_stages;
PFS_builtin_memory_class builtin_memory_host_statements;
PFS_builtin_memory_class builtin_memory_host_transactions;
PFS_builtin_memory_class builtin_memory_host_memory;

PFS_builtin_memory_class builtin_memory_thread;
PFS_builtin_memory_class builtin_memory_thread_waits;
PFS_builtin_memory_class builtin_memory_thread_stages;
PFS_builtin_memory_class builtin_memory_thread_statements;
PFS_builtin_memory_class builtin_memory_thread_transactions;
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

static void init_builtin_memory_class(PFS_builtin_memory_class *klass, const char* name)
{
  klass->m_class.m_type= PFS_CLASS_MEMORY;
  klass->m_class.m_enabled= true; /* Immutable */
  klass->m_class.m_timed= false; /* Immutable */
  klass->m_class.m_flags= PSI_FLAG_GLOBAL;
  klass->m_class.m_event_name_index= 0;
  strncpy(klass->m_class.m_name, name, sizeof(klass->m_class.m_name));
  klass->m_class.m_name_length= strlen(name);
  DBUG_ASSERT(klass->m_class.m_name_length < sizeof(klass->m_class.m_name));
  klass->m_class.m_timer= NULL;

  klass->m_stat.reset();
}

void init_all_builtin_memory_class()
{
  init_builtin_memory_class( & builtin_memory_mutex,
                             "memory/performance_schema/mutex_instances");
  init_builtin_memory_class( & builtin_memory_rwlock,
                             "memory/performance_schema/rwlock_instances");
  init_builtin_memory_class( & builtin_memory_cond,
                             "memory/performance_schema/cond_instances");
  init_builtin_memory_class( & builtin_memory_file,
                             "memory/performance_schema/file_instances");
  init_builtin_memory_class( & builtin_memory_socket,
                             "memory/performance_schema/socket_instances");
  init_builtin_memory_class( & builtin_memory_mdl,
                             "memory/performance_schema/metadata_locks");
  init_builtin_memory_class( & builtin_memory_file_handle,
                             "memory/performance_schema/file_handle");

  init_builtin_memory_class( & builtin_memory_account,
                             "memory/performance_schema/accounts");
  init_builtin_memory_class( & builtin_memory_account_waits,
                             "memory/performance_schema/events_waits_summary_by_account_by_event_name");
  init_builtin_memory_class( & builtin_memory_account_stages,
                             "memory/performance_schema/events_stages_summary_by_account_by_event_name");
  init_builtin_memory_class( & builtin_memory_account_statements,
                             "memory/performance_schema/events_statements_summary_by_account_by_event_name");
  init_builtin_memory_class( & builtin_memory_account_transactions,
                             "memory/performance_schema/events_transactions_summary_by_account_by_event_name");
  init_builtin_memory_class( & builtin_memory_account_memory,
                             "memory/performance_schema/memory_summary_by_account_by_event_name");

  init_builtin_memory_class( & builtin_memory_global_stages,
                             "memory/performance_schema/events_stages_summary_global_by_event_name");
  init_builtin_memory_class( & builtin_memory_global_statements,
                             "memory/performance_schema/events_statements_summary_global_by_event_name");
  init_builtin_memory_class( & builtin_memory_global_memory,
                             "memory/performance_schema/memory_summary_global_by_event_name");

  init_builtin_memory_class( & builtin_memory_host,
                             "memory/performance_schema/hosts");
  init_builtin_memory_class( & builtin_memory_host_waits,
                             "memory/performance_schema/events_waits_summary_by_host_by_event_name");
  init_builtin_memory_class( & builtin_memory_host_stages,
                             "memory/performance_schema/events_stages_summary_by_host_by_event_name");
  init_builtin_memory_class( & builtin_memory_host_statements,
                             "memory/performance_schema/events_statements_summary_by_host_by_event_name");
  init_builtin_memory_class( & builtin_memory_host_transactions,
                             "memory/performance_schema/events_transactions_summary_by_host_by_event_name");
  init_builtin_memory_class( & builtin_memory_host_memory,
                             "memory/performance_schema/memory_summary_by_host_by_event_name");

  init_builtin_memory_class( & builtin_memory_thread,
                             "memory/performance_schema/threads");
  init_builtin_memory_class( & builtin_memory_thread_waits,
                             "memory/performance_schema/events_waits_summary_by_thread_by_event_name");
  init_builtin_memory_class( & builtin_memory_thread_stages,
                             "memory/performance_schema/events_stages_summary_by_thread_by_event_name");
  init_builtin_memory_class( & builtin_memory_thread_statements,
                             "memory/performance_schema/events_statements_summary_by_thread_by_event_name");
  init_builtin_memory_class( & builtin_memory_thread_transactions,
                             "memory/performance_schema/events_transactions_summary_by_thread_by_event_name");
  init_builtin_memory_class( & builtin_memory_thread_memory,
                             "memory/performance_schema/memory_summary_by_thread_by_event_name");

  init_builtin_memory_class( & builtin_memory_thread_waits_history,
                             "memory/performance_schema/events_waits_history");
  init_builtin_memory_class( & builtin_memory_thread_stages_history,
                             "memory/performance_schema/events_stages_history");
  init_builtin_memory_class( & builtin_memory_thread_statements_history,
                             "memory/performance_schema/events_statements_history");
  init_builtin_memory_class( & builtin_memory_thread_statements_history_tokens,
                             "memory/performance_schema/events_statements_history.tokens");
  init_builtin_memory_class( & builtin_memory_thread_statements_history_sqltext,
                             "memory/performance_schema/events_statements_history.sqltext");
  init_builtin_memory_class( & builtin_memory_thread_statements_stack,
                             "memory/performance_schema/events_statements_current");
  init_builtin_memory_class( & builtin_memory_thread_statements_stack_tokens,
                             "memory/performance_schema/events_statements_current.tokens");
  init_builtin_memory_class( & builtin_memory_thread_statements_stack_sqltext,
                             "memory/performance_schema/events_statements_current.sqltext");
  init_builtin_memory_class( & builtin_memory_thread_transaction_history,
                             "memory/performance_schema/events_transactions_history");
  init_builtin_memory_class( & builtin_memory_thread_session_connect_attrs,
                             "memory/performance_schema/session_connect_attrs");

  init_builtin_memory_class( & builtin_memory_user,
                             "memory/performance_schema/users");
  init_builtin_memory_class( & builtin_memory_user_waits,
                             "memory/performance_schema/events_waits_summary_by_user_by_event_name");
  init_builtin_memory_class( & builtin_memory_user_stages,
                             "memory/performance_schema/events_stages_summary_by_user_by_event_name");
  init_builtin_memory_class( & builtin_memory_user_statements,
                             "memory/performance_schema/events_statements_summary_by_user_by_event_name");
  init_builtin_memory_class( & builtin_memory_user_transactions,
                             "memory/performance_schema/events_transactions_summary_by_user_by_event_name");
  init_builtin_memory_class( & builtin_memory_user_memory,
                             "memory/performance_schema/memory_summary_by_user_by_event_name");

  init_builtin_memory_class( & builtin_memory_mutex_class,
                             "memory/performance_schema/mutex_class");
  init_builtin_memory_class( & builtin_memory_rwlock_class,
                             "memory/performance_schema/rwlock_class");
  init_builtin_memory_class( & builtin_memory_cond_class,
                             "memory/performance_schema/cond_class");
  init_builtin_memory_class( & builtin_memory_thread_class,
                             "memory/performance_schema/thread_class");
  init_builtin_memory_class( & builtin_memory_file_class,
                             "memory/performance_schema/file_class");
  init_builtin_memory_class( & builtin_memory_socket_class,
                             "memory/performance_schema/socket_class");
  init_builtin_memory_class( & builtin_memory_stage_class,
                             "memory/performance_schema/stage_class");
  init_builtin_memory_class( & builtin_memory_statement_class,
                             "memory/performance_schema/statement_class");
  init_builtin_memory_class( & builtin_memory_memory_class,
                             "memory/performance_schema/memory_class");

  init_builtin_memory_class( & builtin_memory_setup_actor,
                             "memory/performance_schema/setup_actors");
  init_builtin_memory_class( & builtin_memory_setup_object,
                             "memory/performance_schema/setup_objects");

  init_builtin_memory_class( & builtin_memory_digest,
                             "memory/performance_schema/events_statements_summary_by_digest");
  init_builtin_memory_class( & builtin_memory_digest_tokens,
                             "memory/performance_schema/events_statements_summary_by_digest.tokens");

  init_builtin_memory_class( & builtin_memory_stages_history_long,
                             "memory/performance_schema/events_stages_history_long");
  init_builtin_memory_class( & builtin_memory_statements_history_long,
                             "memory/performance_schema/events_statements_history_long");
  init_builtin_memory_class( & builtin_memory_statements_history_long_tokens,
                             "memory/performance_schema/events_statements_history_long.tokens");
  init_builtin_memory_class( & builtin_memory_statements_history_long_sqltext,
                             "memory/performance_schema/events_statements_history_long.sqltext");
  init_builtin_memory_class( & builtin_memory_transactions_history_long,
                             "memory/performance_schema/events_transactions_history_long");
  init_builtin_memory_class( & builtin_memory_waits_history_long,
                             "memory/performance_schema/events_waits_history_long");

  init_builtin_memory_class( & builtin_memory_table,
                             "memory/performance_schema/table_handles");
  init_builtin_memory_class( & builtin_memory_table_share,
                             "memory/performance_schema/table_shares");
  init_builtin_memory_class( & builtin_memory_table_share_index,
                             "memory/performance_schema/table_io_waits_summary_by_index_usage");
  init_builtin_memory_class( & builtin_memory_table_share_lock,
                             "memory/performance_schema/table_lock_waits_summary_by_table");

  init_builtin_memory_class( & builtin_memory_program,
                             "memory/performance_schema/events_statements_summary_by_program");
  init_builtin_memory_class( & builtin_memory_prepared_stmt,
                             "memory/performance_schema/prepared_statements_instances");

  init_builtin_memory_class( & builtin_memory_scalable_buffer,
                             "memory/performance_schema/scalable_buffer");
}

static PFS_builtin_memory_class* all_builtin_memory[]=
{
  & builtin_memory_mutex,
  & builtin_memory_rwlock,
  & builtin_memory_cond,
  & builtin_memory_file,
  & builtin_memory_socket,
  & builtin_memory_mdl,
  & builtin_memory_file_handle,

  & builtin_memory_account,
  & builtin_memory_account_waits,
  & builtin_memory_account_stages,
  & builtin_memory_account_statements,
  & builtin_memory_account_transactions,
  & builtin_memory_account_memory,

  & builtin_memory_global_stages,
  & builtin_memory_global_statements,
  & builtin_memory_global_memory,

  & builtin_memory_host,
  & builtin_memory_host_waits,
  & builtin_memory_host_stages,
  & builtin_memory_host_statements,
  & builtin_memory_host_transactions,
  & builtin_memory_host_memory,

  & builtin_memory_thread,
  & builtin_memory_thread_waits,
  & builtin_memory_thread_stages,
  & builtin_memory_thread_statements,
  & builtin_memory_thread_transactions,
  & builtin_memory_thread_memory,

  & builtin_memory_thread_waits_history,
  & builtin_memory_thread_stages_history,
  & builtin_memory_thread_statements_history,
  & builtin_memory_thread_statements_history_tokens,
  & builtin_memory_thread_statements_history_sqltext,
  & builtin_memory_thread_statements_stack,
  & builtin_memory_thread_statements_stack_tokens,
  & builtin_memory_thread_statements_stack_sqltext,
  & builtin_memory_thread_transaction_history,
  & builtin_memory_thread_session_connect_attrs,

  & builtin_memory_user,
  & builtin_memory_user_waits,
  & builtin_memory_user_stages,
  & builtin_memory_user_statements,
  & builtin_memory_user_transactions,
  & builtin_memory_user_memory,

  & builtin_memory_mutex_class,
  & builtin_memory_rwlock_class,
  & builtin_memory_cond_class,
  & builtin_memory_thread_class,
  & builtin_memory_file_class,
  & builtin_memory_socket_class,
  & builtin_memory_stage_class,
  & builtin_memory_statement_class,
  & builtin_memory_memory_class,

  & builtin_memory_setup_actor,
  & builtin_memory_setup_object,

  & builtin_memory_digest,
  & builtin_memory_digest_tokens,

  & builtin_memory_stages_history_long,
  & builtin_memory_statements_history_long,
  & builtin_memory_statements_history_long_tokens,
  & builtin_memory_statements_history_long_sqltext,
  & builtin_memory_transactions_history_long,
  & builtin_memory_waits_history_long,

  & builtin_memory_table,
  & builtin_memory_table_share,
  & builtin_memory_table_share_index,
  & builtin_memory_table_share_lock,

  & builtin_memory_program,
  & builtin_memory_prepared_stmt,

  & builtin_memory_scalable_buffer,

  NULL
};


PFS_builtin_memory_class *find_builtin_memory_class(PFS_builtin_memory_key key)
{
  if (key == 0)
    return NULL;

  return all_builtin_memory[key - 1];
}

