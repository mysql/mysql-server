/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef PFS_BUILTIN_MEMORY_H
#define PFS_BUILTIN_MEMORY_H

#include <sys/types.h>

#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

/**
  @file storage/perfschema/pfs_builtin_memory.h
  Performance schema instruments metadata (declarations).
*/

typedef uint PFS_builtin_memory_key;

struct PFS_builtin_memory_class {
  PFS_memory_class m_class;
  PFS_memory_shared_stat m_stat;

  inline void count_alloc(size_t size) { m_stat.count_builtin_alloc(size); }

  inline void count_free(size_t size) { m_stat.count_builtin_free(size); }
};

void init_all_builtin_memory_class();

PFS_builtin_memory_class *find_builtin_memory_class(PFS_builtin_memory_key);

extern PFS_builtin_memory_class builtin_memory_mutex;
extern PFS_builtin_memory_class builtin_memory_rwlock;
extern PFS_builtin_memory_class builtin_memory_cond;
extern PFS_builtin_memory_class builtin_memory_file;
extern PFS_builtin_memory_class builtin_memory_socket;
extern PFS_builtin_memory_class builtin_memory_mdl;
extern PFS_builtin_memory_class builtin_memory_file_handle;

extern PFS_builtin_memory_class builtin_memory_account;
extern PFS_builtin_memory_class builtin_memory_account_waits;
extern PFS_builtin_memory_class builtin_memory_account_stages;
extern PFS_builtin_memory_class builtin_memory_account_statements;
extern PFS_builtin_memory_class builtin_memory_account_transactions;
extern PFS_builtin_memory_class builtin_memory_account_errors;
extern PFS_builtin_memory_class builtin_memory_account_memory;

extern PFS_builtin_memory_class builtin_memory_global_stages;
extern PFS_builtin_memory_class builtin_memory_global_statements;
extern PFS_builtin_memory_class builtin_memory_global_memory;
extern PFS_builtin_memory_class builtin_memory_global_errors;

extern PFS_builtin_memory_class builtin_memory_host;
extern PFS_builtin_memory_class builtin_memory_host_waits;
extern PFS_builtin_memory_class builtin_memory_host_stages;
extern PFS_builtin_memory_class builtin_memory_host_statements;
extern PFS_builtin_memory_class builtin_memory_host_transactions;
extern PFS_builtin_memory_class builtin_memory_host_errors;
extern PFS_builtin_memory_class builtin_memory_host_memory;

extern PFS_builtin_memory_class builtin_memory_thread;
extern PFS_builtin_memory_class builtin_memory_thread_waits;
extern PFS_builtin_memory_class builtin_memory_thread_stages;
extern PFS_builtin_memory_class builtin_memory_thread_statements;
extern PFS_builtin_memory_class builtin_memory_thread_transactions;
extern PFS_builtin_memory_class builtin_memory_thread_errors;
extern PFS_builtin_memory_class builtin_memory_thread_memory;

extern PFS_builtin_memory_class builtin_memory_thread_waits_history;
extern PFS_builtin_memory_class builtin_memory_thread_stages_history;
extern PFS_builtin_memory_class builtin_memory_thread_statements_history;
extern PFS_builtin_memory_class builtin_memory_thread_statements_history_tokens;
extern PFS_builtin_memory_class
    builtin_memory_thread_statements_history_sqltext;
extern PFS_builtin_memory_class builtin_memory_thread_statements_stack;
extern PFS_builtin_memory_class builtin_memory_thread_statements_stack_tokens;
extern PFS_builtin_memory_class builtin_memory_thread_statements_stack_sqltext;
extern PFS_builtin_memory_class builtin_memory_thread_transaction_history;
extern PFS_builtin_memory_class builtin_memory_thread_session_connect_attrs;

extern PFS_builtin_memory_class builtin_memory_user;
extern PFS_builtin_memory_class builtin_memory_user_waits;
extern PFS_builtin_memory_class builtin_memory_user_stages;
extern PFS_builtin_memory_class builtin_memory_user_statements;
extern PFS_builtin_memory_class builtin_memory_user_transactions;
extern PFS_builtin_memory_class builtin_memory_user_errors;
extern PFS_builtin_memory_class builtin_memory_user_memory;

extern PFS_builtin_memory_class builtin_memory_mutex_class;
extern PFS_builtin_memory_class builtin_memory_rwlock_class;
extern PFS_builtin_memory_class builtin_memory_cond_class;
extern PFS_builtin_memory_class builtin_memory_thread_class;
extern PFS_builtin_memory_class builtin_memory_file_class;
extern PFS_builtin_memory_class builtin_memory_socket_class;
extern PFS_builtin_memory_class builtin_memory_stage_class;
extern PFS_builtin_memory_class builtin_memory_statement_class;
extern PFS_builtin_memory_class builtin_memory_memory_class;

extern PFS_builtin_memory_class builtin_memory_meter_class;
extern PFS_builtin_memory_class builtin_memory_meter;
extern PFS_builtin_memory_class builtin_memory_metric_class;
extern PFS_builtin_memory_class builtin_memory_metric;
extern PFS_builtin_memory_class builtin_memory_logger_class;

extern PFS_builtin_memory_class builtin_memory_setup_actor;
extern PFS_builtin_memory_class builtin_memory_setup_object;

extern PFS_builtin_memory_class builtin_memory_digest;
extern PFS_builtin_memory_class builtin_memory_digest_tokens;
extern PFS_builtin_memory_class builtin_memory_digest_sample_sqltext;

extern PFS_builtin_memory_class builtin_memory_stages_history_long;
extern PFS_builtin_memory_class builtin_memory_statements_history_long;
extern PFS_builtin_memory_class builtin_memory_statements_history_long_tokens;
extern PFS_builtin_memory_class builtin_memory_statements_history_long_sqltext;
extern PFS_builtin_memory_class builtin_memory_transactions_history_long;
extern PFS_builtin_memory_class builtin_memory_waits_history_long;

extern PFS_builtin_memory_class builtin_memory_table;
extern PFS_builtin_memory_class builtin_memory_table_share;
extern PFS_builtin_memory_class builtin_memory_table_share_index;
extern PFS_builtin_memory_class builtin_memory_table_share_lock;

extern PFS_builtin_memory_class builtin_memory_program;
extern PFS_builtin_memory_class builtin_memory_prepared_stmt;

extern PFS_builtin_memory_class builtin_memory_scalable_buffer;

extern PFS_builtin_memory_class builtin_memory_data_container;

#endif
