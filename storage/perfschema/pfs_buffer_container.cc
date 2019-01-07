/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_buffer_container.cc
  Generic buffer container.
*/

#include "storage/perfschema/pfs_buffer_container.h"

#include "my_compiler.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_error.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_user.h"

PFS_buffer_default_allocator<PFS_mutex> default_mutex_allocator(
    &builtin_memory_mutex);
PFS_mutex_container global_mutex_container(&default_mutex_allocator);

PFS_buffer_default_allocator<PFS_rwlock> default_rwlock_allocator(
    &builtin_memory_rwlock);
PFS_rwlock_container global_rwlock_container(&default_rwlock_allocator);

PFS_buffer_default_allocator<PFS_cond> default_cond_allocator(
    &builtin_memory_cond);
PFS_cond_container global_cond_container(&default_cond_allocator);

PFS_buffer_default_allocator<PFS_file> default_file_allocator(
    &builtin_memory_file);
PFS_file_container global_file_container(&default_file_allocator);

PFS_buffer_default_allocator<PFS_socket> default_socket_allocator(
    &builtin_memory_socket);
PFS_socket_container global_socket_container(&default_socket_allocator);

PFS_buffer_default_allocator<PFS_metadata_lock> default_mdl_allocator(
    &builtin_memory_mdl);
PFS_mdl_container global_mdl_container(&default_mdl_allocator);

PFS_buffer_default_allocator<PFS_setup_actor> default_setup_actor_allocator(
    &builtin_memory_setup_actor);
PFS_setup_actor_container global_setup_actor_container(
    &default_setup_actor_allocator);

PFS_buffer_default_allocator<PFS_setup_object> default_setup_object_allocator(
    &builtin_memory_setup_object);
PFS_setup_object_container global_setup_object_container(
    &default_setup_object_allocator);

PFS_buffer_default_allocator<PFS_table> default_table_allocator(
    &builtin_memory_table);
PFS_table_container global_table_container(&default_table_allocator);

PFS_buffer_default_allocator<PFS_table_share> default_table_share_allocator(
    &builtin_memory_table_share);
PFS_table_share_container global_table_share_container(
    &default_table_share_allocator);

PFS_buffer_default_allocator<PFS_table_share_index>
    default_table_share_index_allocator(&builtin_memory_table_share_index);
PFS_table_share_index_container global_table_share_index_container(
    &default_table_share_index_allocator);

PFS_buffer_default_allocator<PFS_table_share_lock>
    default_table_share_lock_allocator(&builtin_memory_table_share_lock);
PFS_table_share_lock_container global_table_share_lock_container(
    &default_table_share_lock_allocator);

PFS_buffer_default_allocator<PFS_program> default_program_allocator(
    &builtin_memory_program);
PFS_program_container global_program_container(&default_program_allocator);

PFS_buffer_default_allocator<PFS_prepared_stmt> default_prepared_stmt_allocator(
    &builtin_memory_prepared_stmt);
PFS_prepared_stmt_container global_prepared_stmt_container(
    &default_prepared_stmt_allocator);

int PFS_account_allocator::alloc_array(PFS_account_array *array) {
  PFS_account *pfs;
  size_t size = array->m_max;
  size_t index;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  array->m_ptr = NULL;
  array->m_full = true;
  array->m_instr_class_waits_array = NULL;
  array->m_instr_class_stages_array = NULL;
  array->m_instr_class_statements_array = NULL;
  array->m_instr_class_transactions_array = NULL;
  array->m_instr_class_errors_array = NULL;
  array->m_instr_class_memory_array = NULL;

  if (size > 0) {
    array->m_ptr =
        PFS_MALLOC_ARRAY(&builtin_memory_account, size, sizeof(PFS_account),
                         PFS_account, MYF(MY_ZEROFILL));
    if (array->m_ptr == NULL) {
      return 1;
    }
  }

  if (waits_sizing > 0) {
    array->m_instr_class_waits_array = PFS_MALLOC_ARRAY(
        &builtin_memory_account_waits, waits_sizing, sizeof(PFS_single_stat),
        PFS_single_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_waits_array == NULL) {
      return 1;
    }

    for (index = 0; index < waits_sizing; index++) {
      array->m_instr_class_waits_array[index].reset();
    }
  }

  if (stages_sizing > 0) {
    array->m_instr_class_stages_array = PFS_MALLOC_ARRAY(
        &builtin_memory_account_stages, stages_sizing, sizeof(PFS_stage_stat),
        PFS_stage_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_stages_array == NULL) {
      return 1;
    }

    for (index = 0; index < stages_sizing; index++) {
      array->m_instr_class_stages_array[index].reset();
    }
  }

  if (statements_sizing > 0) {
    array->m_instr_class_statements_array = PFS_MALLOC_ARRAY(
        &builtin_memory_account_statements, statements_sizing,
        sizeof(PFS_statement_stat), PFS_statement_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_statements_array == NULL) {
      return 1;
    }

    for (index = 0; index < statements_sizing; index++) {
      array->m_instr_class_statements_array[index].reset();
    }
  }

  if (transactions_sizing > 0) {
    array->m_instr_class_transactions_array = PFS_MALLOC_ARRAY(
        &builtin_memory_account_transactions, transactions_sizing,
        sizeof(PFS_transaction_stat), PFS_transaction_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_transactions_array == NULL) {
      return 1;
    }

    for (index = 0; index < transactions_sizing; index++) {
      array->m_instr_class_transactions_array[index].reset();
    }
  }

  if (errors_sizing > 0) {
    array->m_instr_class_errors_array = PFS_MALLOC_ARRAY(
        &builtin_memory_account_errors, errors_sizing, sizeof(PFS_error_stat),
        PFS_error_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_errors_array == NULL) {
      return 1;
    }

    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].init(
          &builtin_memory_account_errors);
  }

  if (memory_sizing > 0) {
    array->m_instr_class_memory_array =
        PFS_MALLOC_ARRAY(&builtin_memory_account_memory, memory_sizing,
                         sizeof(PFS_memory_shared_stat), PFS_memory_shared_stat,
                         MYF(MY_ZEROFILL));
    if (array->m_instr_class_memory_array == NULL) {
      return 1;
    }

    for (index = 0; index < memory_sizing; index++) {
      array->m_instr_class_memory_array[index].reset();
    }
  }

  for (index = 0; index < size; index++) {
    pfs = &array->m_ptr[index];

    pfs->set_instr_class_waits_stats(
        &array->m_instr_class_waits_array[index * wait_class_max]);
    pfs->set_instr_class_stages_stats(
        &array->m_instr_class_stages_array[index * stage_class_max]);
    pfs->set_instr_class_statements_stats(
        &array->m_instr_class_statements_array[index * statement_class_max]);
    pfs->set_instr_class_transactions_stats(
        &array
             ->m_instr_class_transactions_array[index * transaction_class_max]);
    pfs->set_instr_class_errors_stats(
        (array->m_instr_class_errors_array)
            ? &array->m_instr_class_errors_array[index * error_class_max]
            : NULL);
    pfs->set_instr_class_memory_stats(
        &array->m_instr_class_memory_array[index * memory_class_max]);
  }

  array->m_full = false;
  return 0;
}

void PFS_account_allocator::free_array(PFS_account_array *array) {
  size_t index;
  size_t size = array->m_max;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  PFS_FREE_ARRAY(&builtin_memory_account, size, sizeof(PFS_account),
                 array->m_ptr);
  array->m_ptr = NULL;

  PFS_FREE_ARRAY(&builtin_memory_account_waits, waits_sizing,
                 sizeof(PFS_single_stat), array->m_instr_class_waits_array);
  array->m_instr_class_waits_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_account_stages, stages_sizing,
                 sizeof(PFS_stage_stat), array->m_instr_class_stages_array);
  array->m_instr_class_stages_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_account_statements, statements_sizing,
                 sizeof(PFS_statement_stat),
                 array->m_instr_class_statements_array);
  array->m_instr_class_statements_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_account_transactions, transactions_sizing,
                 sizeof(PFS_transaction_stat),
                 array->m_instr_class_transactions_array);
  array->m_instr_class_transactions_array = NULL;

  if (array->m_instr_class_errors_array != NULL)
    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].cleanup(
          &builtin_memory_account_errors);
  PFS_FREE_ARRAY(&builtin_memory_account_errors, errors_sizing,
                 sizeof(PFS_error_stat), array->m_instr_class_errors_array);
  array->m_instr_class_errors_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_account_memory, memory_sizing,
                 sizeof(PFS_memory_shared_stat),
                 array->m_instr_class_memory_array);
  array->m_instr_class_memory_array = NULL;
}

PFS_account_allocator account_allocator;
PFS_account_container global_account_container(&account_allocator);

int PFS_host_allocator::alloc_array(PFS_host_array *array) {
  size_t size = array->m_max;
  PFS_host *pfs;
  size_t index;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  array->m_ptr = NULL;
  array->m_full = true;
  array->m_instr_class_waits_array = NULL;
  array->m_instr_class_stages_array = NULL;
  array->m_instr_class_statements_array = NULL;
  array->m_instr_class_transactions_array = NULL;
  array->m_instr_class_errors_array = NULL;
  array->m_instr_class_memory_array = NULL;

  if (size > 0) {
    array->m_ptr =
        PFS_MALLOC_ARRAY(&builtin_memory_host, size, sizeof(PFS_host), PFS_host,
                         MYF(MY_ZEROFILL));
    if (array->m_ptr == NULL) {
      return 1;
    }
  }

  if (waits_sizing > 0) {
    array->m_instr_class_waits_array = PFS_MALLOC_ARRAY(
        &builtin_memory_host_waits, waits_sizing, sizeof(PFS_single_stat),
        PFS_single_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_waits_array == NULL) {
      return 1;
    }

    for (index = 0; index < waits_sizing; index++) {
      array->m_instr_class_waits_array[index].reset();
    }
  }

  if (stages_sizing > 0) {
    array->m_instr_class_stages_array = PFS_MALLOC_ARRAY(
        &builtin_memory_host_stages, stages_sizing, sizeof(PFS_stage_stat),
        PFS_stage_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_stages_array == NULL) {
      return 1;
    }

    for (index = 0; index < stages_sizing; index++) {
      array->m_instr_class_stages_array[index].reset();
    }
  }

  if (statements_sizing > 0) {
    array->m_instr_class_statements_array = PFS_MALLOC_ARRAY(
        &builtin_memory_host_statements, statements_sizing,
        sizeof(PFS_statement_stat), PFS_statement_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_statements_array == NULL) {
      return 1;
    }

    for (index = 0; index < statements_sizing; index++) {
      array->m_instr_class_statements_array[index].reset();
    }
  }

  if (transactions_sizing > 0) {
    array->m_instr_class_transactions_array = PFS_MALLOC_ARRAY(
        &builtin_memory_host_transactions, transactions_sizing,
        sizeof(PFS_transaction_stat), PFS_transaction_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_transactions_array == NULL) {
      return 1;
    }

    for (index = 0; index < transactions_sizing; index++) {
      array->m_instr_class_transactions_array[index].reset();
    }
  }

  if (errors_sizing > 0) {
    array->m_instr_class_errors_array = PFS_MALLOC_ARRAY(
        &builtin_memory_host_errors, errors_sizing, sizeof(PFS_error_stat),
        PFS_error_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_errors_array == NULL) {
      return 1;
    }

    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].init(
          &builtin_memory_host_errors);
  }

  if (memory_sizing > 0) {
    array->m_instr_class_memory_array =
        PFS_MALLOC_ARRAY(&builtin_memory_host_memory, memory_sizing,
                         sizeof(PFS_memory_shared_stat), PFS_memory_shared_stat,
                         MYF(MY_ZEROFILL));
    if (array->m_instr_class_memory_array == NULL) {
      return 1;
    }

    for (index = 0; index < memory_sizing; index++) {
      array->m_instr_class_memory_array[index].reset();
    }
  }

  for (index = 0; index < size; index++) {
    pfs = &array->m_ptr[index];

    pfs->set_instr_class_waits_stats(
        &array->m_instr_class_waits_array[index * wait_class_max]);
    pfs->set_instr_class_stages_stats(
        &array->m_instr_class_stages_array[index * stage_class_max]);
    pfs->set_instr_class_statements_stats(
        &array->m_instr_class_statements_array[index * statement_class_max]);
    pfs->set_instr_class_transactions_stats(
        &array
             ->m_instr_class_transactions_array[index * transaction_class_max]);
    pfs->set_instr_class_errors_stats(
        (array->m_instr_class_errors_array)
            ? &array->m_instr_class_errors_array[index * error_class_max]
            : NULL);
    pfs->set_instr_class_memory_stats(
        &array->m_instr_class_memory_array[index * memory_class_max]);
  }

  array->m_full = false;
  return 0;
}

void PFS_host_allocator::free_array(PFS_host_array *array) {
  size_t index;
  size_t size = array->m_max;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  PFS_FREE_ARRAY(&builtin_memory_host, size, sizeof(PFS_host), array->m_ptr);
  array->m_ptr = NULL;

  PFS_FREE_ARRAY(&builtin_memory_host_waits, waits_sizing,
                 sizeof(PFS_single_stat), array->m_instr_class_waits_array);
  array->m_instr_class_waits_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_host_stages, stages_sizing,
                 sizeof(PFS_stage_stat), array->m_instr_class_stages_array);
  array->m_instr_class_stages_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_host_statements, statements_sizing,
                 sizeof(PFS_statement_stat),
                 array->m_instr_class_statements_array);
  array->m_instr_class_statements_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_host_transactions, transactions_sizing,
                 sizeof(PFS_transaction_stat),
                 array->m_instr_class_transactions_array);
  array->m_instr_class_transactions_array = NULL;

  if (array->m_instr_class_errors_array != NULL)
    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].cleanup(
          &builtin_memory_host_errors);
  PFS_FREE_ARRAY(&builtin_memory_host_errors, errors_sizing,
                 sizeof(PFS_error_stat), array->m_instr_class_errors_array);
  array->m_instr_class_errors_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_host_memory, memory_sizing,
                 sizeof(PFS_memory_shared_stat),
                 array->m_instr_class_memory_array);
  array->m_instr_class_memory_array = NULL;
}

PFS_host_allocator host_allocator;
PFS_host_container global_host_container(&host_allocator);

int PFS_thread_allocator::alloc_array(PFS_thread_array *array) {
  size_t size = array->m_max;
  PFS_thread *pfs;
  PFS_events_statements *pfs_stmt;
  unsigned char *pfs_tokens;

  size_t index;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  size_t waits_history_sizing = size * events_waits_history_per_thread;
  size_t stages_history_sizing = size * events_stages_history_per_thread;
  size_t statements_history_sizing =
      size * events_statements_history_per_thread;
  size_t statements_stack_sizing = size * statement_stack_max;
  size_t transactions_history_sizing =
      size * events_transactions_history_per_thread;
  size_t session_connect_attrs_sizing =
      size * session_connect_attrs_size_per_thread;

  size_t current_sqltext_sizing = size * pfs_max_sqltext * statement_stack_max;
  size_t history_sqltext_sizing =
      size * pfs_max_sqltext * events_statements_history_per_thread;
  size_t current_digest_tokens_sizing =
      size * pfs_max_digest_length * statement_stack_max;
  size_t history_digest_tokens_sizing =
      size * pfs_max_digest_length * events_statements_history_per_thread;

  array->m_ptr = NULL;
  array->m_full = true;
  array->m_instr_class_waits_array = NULL;
  array->m_instr_class_stages_array = NULL;
  array->m_instr_class_statements_array = NULL;
  array->m_instr_class_transactions_array = NULL;
  array->m_instr_class_errors_array = NULL;
  array->m_instr_class_memory_array = NULL;

  array->m_waits_history_array = NULL;
  array->m_stages_history_array = NULL;
  array->m_statements_history_array = NULL;
  array->m_statements_stack_array = NULL;
  array->m_transactions_history_array = NULL;
  array->m_session_connect_attrs_array = NULL;

  array->m_current_stmts_text_array = NULL;
  array->m_current_stmts_digest_token_array = NULL;
  array->m_history_stmts_text_array = NULL;
  array->m_history_stmts_digest_token_array = NULL;

  if (size > 0) {
    array->m_ptr =
        PFS_MALLOC_ARRAY(&builtin_memory_thread, size, sizeof(PFS_thread),
                         PFS_thread, MYF(MY_ZEROFILL));
    if (array->m_ptr == NULL) {
      return 1;
    }
  }

  if (waits_sizing > 0) {
    array->m_instr_class_waits_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_waits, waits_sizing, sizeof(PFS_single_stat),
        PFS_single_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_waits_array == NULL) {
      return 1;
    }

    for (index = 0; index < waits_sizing; index++) {
      array->m_instr_class_waits_array[index].reset();
    }
  }

  if (stages_sizing > 0) {
    array->m_instr_class_stages_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_stages, stages_sizing, sizeof(PFS_stage_stat),
        PFS_stage_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_stages_array == NULL) {
      return 1;
    }

    for (index = 0; index < stages_sizing; index++) {
      array->m_instr_class_stages_array[index].reset();
    }
  }

  if (statements_sizing > 0) {
    array->m_instr_class_statements_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_statements, statements_sizing,
        sizeof(PFS_statement_stat), PFS_statement_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_statements_array == NULL) {
      return 1;
    }

    for (index = 0; index < statements_sizing; index++) {
      array->m_instr_class_statements_array[index].reset();
    }
  }

  if (transactions_sizing > 0) {
    array->m_instr_class_transactions_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_transactions, transactions_sizing,
        sizeof(PFS_transaction_stat), PFS_transaction_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_transactions_array == NULL) {
      return 1;
    }

    for (index = 0; index < transactions_sizing; index++) {
      array->m_instr_class_transactions_array[index].reset();
    }
  }

  if (errors_sizing > 0) {
    array->m_instr_class_errors_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_errors, errors_sizing, sizeof(PFS_error_stat),
        PFS_error_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_errors_array == NULL) {
      return 1;
    }

    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].init(
          &builtin_memory_thread_errors);
  }

  if (memory_sizing > 0) {
    array->m_instr_class_memory_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_memory, memory_sizing,
        sizeof(PFS_memory_safe_stat), PFS_memory_safe_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_memory_array == NULL) {
      return 1;
    }

    for (index = 0; index < memory_sizing; index++) {
      array->m_instr_class_memory_array[index].reset();
    }
  }

  if (waits_history_sizing > 0) {
    array->m_waits_history_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_waits_history, waits_history_sizing,
        sizeof(PFS_events_waits), PFS_events_waits, MYF(MY_ZEROFILL));
    if (unlikely(array->m_waits_history_array == NULL)) {
      return 1;
    }
  }

  if (stages_history_sizing > 0) {
    array->m_stages_history_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_stages_history, stages_history_sizing,
        sizeof(PFS_events_stages), PFS_events_stages, MYF(MY_ZEROFILL));
    if (unlikely(array->m_stages_history_array == NULL)) {
      return 1;
    }
  }

  if (statements_history_sizing > 0) {
    array->m_statements_history_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_statements_history, statements_history_sizing,
        sizeof(PFS_events_statements), PFS_events_statements, MYF(MY_ZEROFILL));
    if (unlikely(array->m_statements_history_array == NULL)) {
      return 1;
    }
  }

  if (statements_stack_sizing > 0) {
    array->m_statements_stack_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_statements_stack, statements_stack_sizing,
        sizeof(PFS_events_statements), PFS_events_statements, MYF(MY_ZEROFILL));
    if (unlikely(array->m_statements_stack_array == NULL)) {
      return 1;
    }
  }

  if (transactions_history_sizing > 0) {
    array->m_transactions_history_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_transaction_history, transactions_history_sizing,
        sizeof(PFS_events_transactions), PFS_events_transactions,
        MYF(MY_ZEROFILL));
    if (unlikely(array->m_transactions_history_array == NULL)) {
      return 1;
    }
  }

  if (session_connect_attrs_sizing > 0) {
    array->m_session_connect_attrs_array =
        (char *)pfs_malloc(&builtin_memory_thread_session_connect_attrs,
                           session_connect_attrs_sizing, MYF(MY_ZEROFILL));
    if (unlikely(array->m_session_connect_attrs_array == NULL)) {
      return 1;
    }
  }

  if (current_sqltext_sizing > 0) {
    array->m_current_stmts_text_array =
        (char *)pfs_malloc(&builtin_memory_thread_statements_stack_sqltext,
                           current_sqltext_sizing, MYF(MY_ZEROFILL));
    if (unlikely(array->m_current_stmts_text_array == NULL)) {
      return 1;
    }
  }

  if (history_sqltext_sizing > 0) {
    array->m_history_stmts_text_array =
        (char *)pfs_malloc(&builtin_memory_thread_statements_history_sqltext,
                           history_sqltext_sizing, MYF(MY_ZEROFILL));
    if (unlikely(array->m_history_stmts_text_array == NULL)) {
      return 1;
    }
  }

  if (current_digest_tokens_sizing > 0) {
    array->m_current_stmts_digest_token_array = (unsigned char *)pfs_malloc(
        &builtin_memory_thread_statements_stack_tokens,
        current_digest_tokens_sizing, MYF(MY_ZEROFILL));
    if (unlikely(array->m_current_stmts_digest_token_array == NULL)) {
      return 1;
    }
  }

  if (history_digest_tokens_sizing > 0) {
    array->m_history_stmts_digest_token_array = (unsigned char *)pfs_malloc(
        &builtin_memory_thread_statements_history_tokens,
        history_digest_tokens_sizing, MYF(MY_ZEROFILL));
    if (unlikely(array->m_history_stmts_digest_token_array == NULL)) {
      return 1;
    }
  }

  for (index = 0; index < size; index++) {
    pfs = &array->m_ptr[index];

    pfs->set_instr_class_waits_stats(
        &array->m_instr_class_waits_array[index * wait_class_max]);
    pfs->set_instr_class_stages_stats(
        &array->m_instr_class_stages_array[index * stage_class_max]);
    pfs->set_instr_class_statements_stats(
        &array->m_instr_class_statements_array[index * statement_class_max]);
    pfs->set_instr_class_transactions_stats(
        &array
             ->m_instr_class_transactions_array[index * transaction_class_max]);
    pfs->set_instr_class_errors_stats(
        (array->m_instr_class_errors_array)
            ? &array->m_instr_class_errors_array[index * error_class_max]
            : NULL);
    pfs->set_instr_class_memory_stats(
        &array->m_instr_class_memory_array[index * memory_class_max]);

    pfs->m_waits_history =
        &array->m_waits_history_array[index * events_waits_history_per_thread];
    pfs->m_stages_history =
        &array
             ->m_stages_history_array[index * events_stages_history_per_thread];
    pfs->m_statements_history =
        &array
             ->m_statements_history_array[index *
                                          events_statements_history_per_thread];
    pfs->m_statement_stack =
        &array->m_statements_stack_array[index * statement_stack_max];
    pfs->m_transactions_history =
        &array->m_transactions_history_array
             [index * events_transactions_history_per_thread];
    pfs->m_session_connect_attrs =
        &array->m_session_connect_attrs_array
             [index * session_connect_attrs_size_per_thread];
  }

  for (index = 0; index < statements_stack_sizing; index++) {
    pfs_stmt = &array->m_statements_stack_array[index];

    pfs_stmt->m_sqltext =
        &array->m_current_stmts_text_array[index * pfs_max_sqltext];

    pfs_tokens =
        &array->m_current_stmts_digest_token_array[index *
                                                   pfs_max_digest_length];
    pfs_stmt->m_digest_storage.reset(pfs_tokens, pfs_max_digest_length);
  }

  for (index = 0; index < statements_history_sizing; index++) {
    pfs_stmt = &array->m_statements_history_array[index];

    pfs_stmt->m_sqltext =
        &array->m_history_stmts_text_array[index * pfs_max_sqltext];

    pfs_tokens =
        &array->m_history_stmts_digest_token_array[index *
                                                   pfs_max_digest_length];
    pfs_stmt->m_digest_storage.reset(pfs_tokens, pfs_max_digest_length);
  }

  array->m_full = false;
  return 0;
}

void PFS_thread_allocator::free_array(PFS_thread_array *array) {
  size_t index;
  size_t size = array->m_max;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  size_t waits_history_sizing = size * events_waits_history_per_thread;
  size_t stages_history_sizing = size * events_stages_history_per_thread;
  size_t statements_history_sizing =
      size * events_statements_history_per_thread;
  size_t statements_stack_sizing = size * statement_stack_max;
  size_t transactions_history_sizing =
      size * events_transactions_history_per_thread;
  size_t session_connect_attrs_sizing =
      size * session_connect_attrs_size_per_thread;

  size_t current_sqltext_sizing = size * pfs_max_sqltext * statement_stack_max;
  size_t history_sqltext_sizing =
      size * pfs_max_sqltext * events_statements_history_per_thread;
  size_t current_digest_tokens_sizing =
      size * pfs_max_digest_length * statement_stack_max;
  size_t history_digest_tokens_sizing =
      size * pfs_max_digest_length * events_statements_history_per_thread;

  PFS_FREE_ARRAY(&builtin_memory_thread, size, sizeof(PFS_thread),
                 array->m_ptr);
  array->m_ptr = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_waits, waits_sizing,
                 sizeof(PFS_single_stat), array->m_instr_class_waits_array);
  array->m_instr_class_waits_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_stages, stages_sizing,
                 sizeof(PFS_stage_stat), array->m_instr_class_stages_array);
  array->m_instr_class_stages_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_statements, statements_sizing,
                 sizeof(PFS_statement_stat),
                 array->m_instr_class_statements_array);
  array->m_instr_class_statements_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_transactions, transactions_sizing,
                 sizeof(PFS_transaction_stat),
                 array->m_instr_class_transactions_array);
  array->m_instr_class_transactions_array = NULL;

  if (array->m_instr_class_errors_array != NULL)
    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].cleanup(
          &builtin_memory_thread_errors);
  PFS_FREE_ARRAY(&builtin_memory_thread_errors, errors_sizing,
                 sizeof(PFS_error_stat), array->m_instr_class_errors_array);
  array->m_instr_class_errors_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_memory, memory_sizing,
                 sizeof(PFS_memory_safe_stat),
                 array->m_instr_class_memory_array);
  array->m_instr_class_memory_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_waits_history, waits_history_sizing,
                 sizeof(PFS_events_waits), array->m_waits_history_array);
  array->m_waits_history_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_stages_history, stages_history_sizing,
                 sizeof(PFS_events_stages), array->m_stages_history_array);
  array->m_stages_history_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_statements_history,
                 statements_history_sizing, sizeof(PFS_events_statements),
                 array->m_statements_history_array);
  array->m_statements_history_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_statements_stack,
                 statements_stack_sizing, sizeof(PFS_events_statements),
                 array->m_statements_stack_array);
  array->m_statements_stack_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_thread_transaction_history,
                 transactions_history_sizing, sizeof(PFS_events_transactions),
                 array->m_transactions_history_array);
  array->m_transactions_history_array = NULL;

  pfs_free(&builtin_memory_thread_session_connect_attrs,
           session_connect_attrs_sizing, array->m_session_connect_attrs_array);
  array->m_session_connect_attrs_array = NULL;

  pfs_free(&builtin_memory_thread_statements_stack_sqltext,
           current_sqltext_sizing, array->m_current_stmts_text_array);
  array->m_current_stmts_text_array = NULL;

  pfs_free(&builtin_memory_thread_statements_history_sqltext,
           history_sqltext_sizing, array->m_history_stmts_text_array);
  array->m_history_stmts_text_array = NULL;

  pfs_free(&builtin_memory_thread_statements_stack_tokens,
           current_digest_tokens_sizing,
           array->m_current_stmts_digest_token_array);
  array->m_current_stmts_digest_token_array = NULL;

  pfs_free(&builtin_memory_thread_statements_history_tokens,
           history_digest_tokens_sizing,
           array->m_history_stmts_digest_token_array);
  array->m_history_stmts_digest_token_array = NULL;
}

PFS_thread_allocator thread_allocator;
PFS_thread_container global_thread_container(&thread_allocator);

int PFS_user_allocator::alloc_array(PFS_user_array *array) {
  size_t size = array->m_max;
  PFS_user *pfs;
  size_t index;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  array->m_ptr = NULL;
  array->m_full = true;
  array->m_instr_class_waits_array = NULL;
  array->m_instr_class_stages_array = NULL;
  array->m_instr_class_statements_array = NULL;
  array->m_instr_class_transactions_array = NULL;
  array->m_instr_class_errors_array = NULL;
  array->m_instr_class_memory_array = NULL;

  if (size > 0) {
    array->m_ptr =
        PFS_MALLOC_ARRAY(&builtin_memory_user, size, sizeof(PFS_user), PFS_user,
                         MYF(MY_ZEROFILL));
    if (array->m_ptr == NULL) {
      return 1;
    }
  }

  if (waits_sizing > 0) {
    array->m_instr_class_waits_array = PFS_MALLOC_ARRAY(
        &builtin_memory_user_waits, waits_sizing, sizeof(PFS_single_stat),
        PFS_single_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_waits_array == NULL) {
      return 1;
    }

    for (index = 0; index < waits_sizing; index++) {
      array->m_instr_class_waits_array[index].reset();
    }
  }

  if (stages_sizing > 0) {
    array->m_instr_class_stages_array = PFS_MALLOC_ARRAY(
        &builtin_memory_user_stages, stages_sizing, sizeof(PFS_stage_stat),
        PFS_stage_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_stages_array == NULL) {
      return 1;
    }

    for (index = 0; index < stages_sizing; index++) {
      array->m_instr_class_stages_array[index].reset();
    }
  }

  if (statements_sizing > 0) {
    array->m_instr_class_statements_array = PFS_MALLOC_ARRAY(
        &builtin_memory_user_statements, statements_sizing,
        sizeof(PFS_statement_stat), PFS_statement_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_statements_array == NULL) {
      return 1;
    }

    for (index = 0; index < statements_sizing; index++) {
      array->m_instr_class_statements_array[index].reset();
    }
  }

  if (transactions_sizing > 0) {
    array->m_instr_class_transactions_array = PFS_MALLOC_ARRAY(
        &builtin_memory_user_transactions, transactions_sizing,
        sizeof(PFS_transaction_stat), PFS_transaction_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_transactions_array == NULL) {
      return 1;
    }

    for (index = 0; index < transactions_sizing; index++) {
      array->m_instr_class_transactions_array[index].reset();
    }
  }

  if (errors_sizing > 0) {
    array->m_instr_class_errors_array = PFS_MALLOC_ARRAY(
        &builtin_memory_user_errors, errors_sizing, sizeof(PFS_error_stat),
        PFS_error_stat, MYF(MY_ZEROFILL));
    if (array->m_instr_class_errors_array == NULL) {
      return 1;
    }

    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].init(
          &builtin_memory_user_errors);
  }

  if (memory_sizing > 0) {
    array->m_instr_class_memory_array =
        PFS_MALLOC_ARRAY(&builtin_memory_user_memory, memory_sizing,
                         sizeof(PFS_memory_shared_stat), PFS_memory_shared_stat,
                         MYF(MY_ZEROFILL));
    if (array->m_instr_class_memory_array == NULL) {
      return 1;
    }

    for (index = 0; index < memory_sizing; index++) {
      array->m_instr_class_memory_array[index].reset();
    }
  }

  for (index = 0; index < size; index++) {
    pfs = &array->m_ptr[index];

    pfs->set_instr_class_waits_stats(
        &array->m_instr_class_waits_array[index * wait_class_max]);
    pfs->set_instr_class_stages_stats(
        &array->m_instr_class_stages_array[index * stage_class_max]);
    pfs->set_instr_class_statements_stats(
        &array->m_instr_class_statements_array[index * statement_class_max]);
    pfs->set_instr_class_transactions_stats(
        &array
             ->m_instr_class_transactions_array[index * transaction_class_max]);
    pfs->set_instr_class_errors_stats(
        (array->m_instr_class_errors_array)
            ? &array->m_instr_class_errors_array[index * error_class_max]
            : NULL);
    pfs->set_instr_class_memory_stats(
        &array->m_instr_class_memory_array[index * memory_class_max]);
  }

  array->m_full = false;
  return 0;
}

void PFS_user_allocator::free_array(PFS_user_array *array) {
  size_t index;
  size_t size = array->m_max;
  size_t waits_sizing = size * wait_class_max;
  size_t stages_sizing = size * stage_class_max;
  size_t statements_sizing = size * statement_class_max;
  size_t transactions_sizing = size * transaction_class_max;
  size_t errors_sizing = (max_server_errors != 0) ? size * error_class_max : 0;
  size_t memory_sizing = size * memory_class_max;

  PFS_FREE_ARRAY(&builtin_memory_user, size, sizeof(PFS_user), array->m_ptr);
  array->m_ptr = NULL;

  PFS_FREE_ARRAY(&builtin_memory_user_waits, waits_sizing,
                 sizeof(PFS_single_stat), array->m_instr_class_waits_array);
  array->m_instr_class_waits_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_user_stages, stages_sizing,
                 sizeof(PFS_stage_stat), array->m_instr_class_stages_array);
  array->m_instr_class_stages_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_user_statements, statements_sizing,
                 sizeof(PFS_statement_stat),
                 array->m_instr_class_statements_array);
  array->m_instr_class_statements_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_user_transactions, transactions_sizing,
                 sizeof(PFS_transaction_stat),
                 array->m_instr_class_transactions_array);
  array->m_instr_class_transactions_array = NULL;

  if (array->m_instr_class_errors_array != NULL)
    for (index = 0; index < errors_sizing; index++)
      array->m_instr_class_errors_array[index].cleanup(
          &builtin_memory_user_errors);
  PFS_FREE_ARRAY(&builtin_memory_user_errors, errors_sizing,
                 sizeof(PFS_error_stat), array->m_instr_class_errors_array);
  array->m_instr_class_errors_array = NULL;

  PFS_FREE_ARRAY(&builtin_memory_user_memory, memory_sizing,
                 sizeof(PFS_memory_shared_stat),
                 array->m_instr_class_memory_array);
  array->m_instr_class_memory_array = NULL;
}

PFS_user_allocator user_allocator;
PFS_user_container global_user_container(&user_allocator);
