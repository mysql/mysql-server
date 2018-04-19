/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <memory.h>
#include <string.h>

#include "my_thread.h"
#include "mysql/psi/psi_transaction.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_server.h"
#include "storage/perfschema/unittest/stub_pfs_defaults.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_print_error.h"
#include "unittest/mytap/tap.h"

static void test_noop() {
  PSI_mutex *mutex;
  PSI_rwlock *rwlock;
  PSI_cond *cond;
  PSI_socket *socket;
  PSI_table_share *table_share;
  PSI_table *table;
  PSI_file *file;
  PSI_thread *thread;
  PSI_file_locker *file_locker;
  PSI_idle_locker *idle_locker;
  PSI_mutex_locker *mutex_locker;
  PSI_rwlock_locker *rwlock_locker;
  PSI_cond_locker *cond_locker;
  PSI_table_locker *table_locker;
  PSI_statement_locker *statement_locker;
  PSI_transaction_locker *transaction_locker;
  PSI_socket_locker *socket_locker;
  PSI_digest_locker *digest_locker;
  PSI_sp_locker *sp_locker;
  PSI_sp_share *sp_share;
  PSI_memory_key memory_key;
  PSI_metadata_lock *metadata_lock;
  PSI_metadata_locker *metadata_locker;
  PSI_thread *owner;

  diag("test_noop");

  psi_mutex_service->register_mutex(NULL, NULL, 0);
  psi_rwlock_service->register_rwlock(NULL, NULL, 0);
  psi_cond_service->register_cond(NULL, NULL, 0);
  psi_thread_service->register_thread(NULL, NULL, 0);
  psi_file_service->register_file(NULL, NULL, 0);
  psi_stage_service->register_stage(NULL, NULL, 0);
  psi_statement_service->register_statement(NULL, NULL, 0);
  psi_socket_service->register_socket(NULL, NULL, 0);

  ok(true, "register");
  mutex = psi_mutex_service->init_mutex(1, NULL);
  ok(mutex == NULL, "no mutex");
  psi_mutex_service->destroy_mutex(NULL);
  rwlock = psi_rwlock_service->init_rwlock(1, NULL);
  ok(rwlock == NULL, "no rwlock");
  psi_rwlock_service->destroy_rwlock(NULL);
  cond = psi_cond_service->init_cond(1, NULL);
  ok(cond == NULL, "no cond");
  psi_cond_service->destroy_cond(NULL);
  socket = psi_socket_service->init_socket(1, NULL, NULL, 0);
  ok(socket == NULL, "no socket");
  psi_socket_service->destroy_socket(NULL);
  table_share = psi_table_service->get_table_share(false, NULL);
  ok(table_share == NULL, "no table_share");
  psi_table_service->release_table_share(NULL);
  psi_table_service->drop_table_share(false, NULL, 0, NULL, 0);
  table = psi_table_service->open_table(NULL, NULL);
  ok(table == NULL, "no table");
  psi_table_service->unbind_table(NULL);
  table = psi_table_service->rebind_table(NULL, NULL, NULL);
  ok(table == NULL, "no table");
  psi_table_service->close_table(NULL, NULL);
  psi_file_service->create_file(1, NULL, 2);
  /* TODO: spawn thread */
  thread = psi_thread_service->new_thread(1, NULL, 2);
  ok(thread == NULL, "no thread");
  psi_thread_service->set_thread_id(NULL, 1);
  thread = psi_thread_service->get_thread();
  ok(thread == NULL, "no thread");
  psi_thread_service->set_thread_user(NULL, 0);
  psi_thread_service->set_thread_account(NULL, 0, NULL, 0);
  psi_thread_service->set_thread_db(NULL, 0);
  psi_thread_service->set_thread_command(1);
  psi_thread_service->set_thread_start_time(1);
  psi_thread_service->set_thread_state(NULL);
  psi_thread_service->set_thread_info(NULL, 0);
  psi_thread_service->set_thread(NULL);
  psi_thread_service->delete_current_thread();
  psi_thread_service->delete_thread(NULL);
  file_locker = psi_file_service->get_thread_file_name_locker(
      NULL, 1, PSI_FILE_OPEN, NULL, NULL);
  ok(file_locker == NULL, "no file_locker");
  file_locker = psi_file_service->get_thread_file_stream_locker(NULL, NULL,
                                                                PSI_FILE_OPEN);
  ok(file_locker == NULL, "no file_locker");
  file_locker = psi_file_service->get_thread_file_descriptor_locker(
      NULL, 0, PSI_FILE_OPEN);
  ok(file_locker == NULL, "no file_locker");
  psi_mutex_service->unlock_mutex(NULL);
  psi_rwlock_service->unlock_rwlock(NULL);
  psi_cond_service->signal_cond(NULL);
  psi_cond_service->broadcast_cond(NULL);
  idle_locker = psi_idle_service->start_idle_wait(NULL, NULL, 0);
  ok(idle_locker == NULL, "no idle_locker");
  psi_idle_service->end_idle_wait(NULL);
  mutex_locker =
      psi_mutex_service->start_mutex_wait(NULL, NULL, PSI_MUTEX_LOCK, NULL, 0);
  ok(mutex_locker == NULL, "no mutex_locker");
  psi_mutex_service->end_mutex_wait(NULL, 0);
  rwlock_locker = psi_rwlock_service->start_rwlock_rdwait(
      NULL, NULL, PSI_RWLOCK_READLOCK, NULL, 0);
  ok(rwlock_locker == NULL, "no rwlock_locker");
  psi_rwlock_service->end_rwlock_rdwait(NULL, 0);
  rwlock_locker = psi_rwlock_service->start_rwlock_wrwait(
      NULL, NULL, PSI_RWLOCK_WRITELOCK, NULL, 0);
  ok(rwlock_locker == NULL, "no rwlock_locker");
  psi_rwlock_service->end_rwlock_wrwait(NULL, 0);
  cond_locker = psi_cond_service->start_cond_wait(NULL, NULL, NULL,
                                                  PSI_COND_WAIT, NULL, 0);
  ok(cond_locker == NULL, "no cond_locker");
  psi_cond_service->end_cond_wait(NULL, 0);
  table_locker = psi_table_service->start_table_io_wait(
      NULL, NULL, PSI_TABLE_FETCH_ROW, 0, NULL, 0);
  ok(table_locker == NULL, "no table_locker");
  psi_table_service->end_table_io_wait(NULL, 0);
  table_locker = psi_table_service->start_table_lock_wait(
      NULL, NULL, PSI_TABLE_LOCK, 0, NULL, 0);
  ok(table_locker == NULL, "no table_locker");
  psi_table_service->end_table_lock_wait(NULL);
  psi_file_service->start_file_open_wait(NULL, NULL, 0);
  file = psi_file_service->end_file_open_wait(NULL, NULL);
  ok(file == NULL, "no file");
  psi_file_service->end_file_open_wait_and_bind_to_descriptor(NULL, 0);
  psi_file_service->start_file_wait(NULL, 0, NULL, 0);
  psi_file_service->end_file_wait(NULL, 0);
  psi_file_service->start_file_close_wait(NULL, NULL, 0);
  psi_file_service->end_file_close_wait(NULL, 0);
  psi_file_service->end_file_rename_wait(NULL, NULL, NULL, 0);
  psi_stage_service->start_stage(1, NULL, 0);

  PSI_stage_progress *progress;
  progress = psi_stage_service->get_current_stage_progress();
  ok(progress == NULL, "no progress");

  psi_stage_service->end_stage();
  statement_locker =
      psi_statement_service->get_thread_statement_locker(NULL, 1, NULL, NULL);
  ok(statement_locker == NULL, "no statement_locker");
  statement_locker = psi_statement_service->refine_statement(NULL, 1);
  ok(statement_locker == NULL, "no statement_locker");
  psi_statement_service->start_statement(NULL, NULL, 0, NULL, 0);
  psi_statement_service->set_statement_text(NULL, NULL, 0);
  psi_statement_service->set_statement_lock_time(NULL, 0);
  psi_statement_service->set_statement_rows_sent(NULL, 0);
  psi_statement_service->set_statement_rows_examined(NULL, 0);
  psi_statement_service->inc_statement_created_tmp_disk_tables(NULL, 0);
  psi_statement_service->inc_statement_created_tmp_tables(NULL, 0);
  psi_statement_service->inc_statement_select_full_join(NULL, 0);
  psi_statement_service->inc_statement_select_full_range_join(NULL, 0);
  psi_statement_service->inc_statement_select_range(NULL, 0);
  psi_statement_service->inc_statement_select_range_check(NULL, 0);
  psi_statement_service->inc_statement_select_scan(NULL, 0);
  psi_statement_service->inc_statement_sort_merge_passes(NULL, 0);
  psi_statement_service->inc_statement_sort_range(NULL, 0);
  psi_statement_service->inc_statement_sort_rows(NULL, 0);
  psi_statement_service->inc_statement_sort_scan(NULL, 0);
  psi_statement_service->set_statement_no_index_used(NULL);
  psi_statement_service->set_statement_no_good_index_used(NULL);
  psi_statement_service->end_statement(NULL, NULL);
  socket_locker = psi_socket_service->start_socket_wait(
      NULL, NULL, PSI_SOCKET_SEND, 1, NULL, 0);
  ok(socket_locker == NULL, "no socket_locker");
  psi_socket_service->end_socket_wait(NULL, 0);
  psi_socket_service->set_socket_state(NULL, PSI_SOCKET_STATE_IDLE);
  psi_socket_service->set_socket_info(NULL, NULL, NULL, 0);
  psi_socket_service->set_socket_thread_owner(NULL);
  digest_locker = psi_statement_service->digest_start(NULL);
  ok(digest_locker == NULL, "no digest_locker");
  psi_statement_service->digest_end(NULL, NULL);
  sp_locker = psi_statement_service->start_sp(NULL, NULL);
  ok(sp_locker == NULL, "no sp_locker");
  psi_statement_service->end_sp(NULL);
  psi_statement_service->drop_sp(0, NULL, 0, NULL, 0);
  sp_share = psi_statement_service->get_sp_share(0, NULL, 0, NULL, 0);
  ok(sp_share == NULL, "no sp_share");
  psi_statement_service->release_sp_share(NULL);
  psi_memory_service->register_memory(NULL, NULL, 0);
  memory_key = psi_memory_service->memory_alloc(0, 0, &owner);
  ok(memory_key == PSI_NOT_INSTRUMENTED, "no memory_key");
  memory_key = psi_memory_service->memory_realloc(0, 0, 0, &owner);
  ok(memory_key == PSI_NOT_INSTRUMENTED, "no memory_key");
  psi_memory_service->memory_free(0, 0, NULL);
  psi_table_service->unlock_table(NULL);
  metadata_lock =
      psi_mdl_service->create_metadata_lock(NULL, NULL, 1, 2, 3, NULL, 0);
  ok(metadata_lock == NULL, "no metadata_lock");
  psi_mdl_service->set_metadata_lock_status(NULL, 0);
  psi_mdl_service->destroy_metadata_lock(NULL);
  metadata_locker = psi_mdl_service->start_metadata_wait(NULL, NULL, NULL, 0);
  ok(metadata_locker == NULL, "no metadata_locker");
  psi_mdl_service->end_metadata_wait(NULL, 0);

  transaction_locker = psi_transaction_service->get_thread_transaction_locker(
      NULL, NULL, NULL, 1, false, 1);
  ok(transaction_locker == NULL, "no transaction_locker");
  psi_transaction_service->start_transaction(NULL, NULL, 0);
  psi_transaction_service->end_transaction(NULL, true);

  psi_transaction_service->set_transaction_gtid(NULL, NULL, NULL);
  psi_transaction_service->set_transaction_trxid(NULL, NULL);
  psi_transaction_service->set_transaction_xa_state(NULL, 1);
  psi_transaction_service->set_transaction_xid(NULL, NULL, 1);
  psi_transaction_service->inc_transaction_release_savepoint(NULL, 1);
  psi_transaction_service->inc_transaction_rollback_to_savepoint(NULL, 1);
  psi_transaction_service->inc_transaction_savepoints(NULL, 1);

  psi_thread_service->set_thread_THD(NULL, NULL);

  psi_error_service->log_error(0, PSI_ERROR_OPERATION_RAISED);
  ok(true, "no error");

  ok(true, "all noop api called");
}

int main(int, char **) {
  plan(35);

  MY_INIT("pfs_noop-t");
  test_noop();
  my_end(0);
  return (exit_status());
}
