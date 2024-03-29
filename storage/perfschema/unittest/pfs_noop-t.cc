/* Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

  psi_mutex_service->register_mutex(nullptr, nullptr, 0);
  psi_rwlock_service->register_rwlock(nullptr, nullptr, 0);
  psi_cond_service->register_cond(nullptr, nullptr, 0);
  psi_thread_service->register_thread(nullptr, nullptr, 0);
  psi_file_service->register_file(nullptr, nullptr, 0);
  psi_stage_service->register_stage(nullptr, nullptr, 0);
  psi_statement_service->register_statement(nullptr, nullptr, 0);
  psi_socket_service->register_socket(nullptr, nullptr, 0);

  ok(true, "register");
  mutex = psi_mutex_service->init_mutex(1, nullptr);
  ok(mutex == nullptr, "no mutex");
  psi_mutex_service->destroy_mutex(nullptr);
  rwlock = psi_rwlock_service->init_rwlock(1, nullptr);
  ok(rwlock == nullptr, "no rwlock");
  psi_rwlock_service->destroy_rwlock(nullptr);
  cond = psi_cond_service->init_cond(1, nullptr);
  ok(cond == nullptr, "no cond");
  psi_cond_service->destroy_cond(nullptr);
  socket = psi_socket_service->init_socket(1, nullptr, nullptr, 0);
  ok(socket == nullptr, "no socket");
  psi_socket_service->destroy_socket(nullptr);
  table_share = psi_table_service->get_table_share(false, nullptr);
  ok(table_share == nullptr, "no table_share");
  psi_table_service->release_table_share(nullptr);
  psi_table_service->drop_table_share(false, nullptr, 0, nullptr, 0);
  table = psi_table_service->open_table(nullptr, nullptr);
  ok(table == nullptr, "no table");
  psi_table_service->unbind_table(nullptr);
  table = psi_table_service->rebind_table(nullptr, nullptr, nullptr);
  ok(table == nullptr, "no table");
  psi_table_service->close_table(nullptr, nullptr);
  psi_file_service->create_file(1, nullptr, 2);
  /* TODO: spawn thread */
  thread = psi_thread_service->new_thread(1, 0, nullptr, 2);
  ok(thread == nullptr, "no thread");
  psi_thread_service->set_thread_id(nullptr, 1);
  thread = psi_thread_service->get_thread();
  ok(thread == nullptr, "no thread");
  psi_thread_service->set_thread_user(nullptr, 0);
  psi_thread_service->set_thread_account(nullptr, 0, nullptr, 0);
  psi_thread_service->set_thread_db(nullptr, 0);
  psi_thread_service->set_thread_command(1);
  psi_thread_service->set_thread_start_time(1);
  psi_thread_service->set_thread_info(nullptr, 0);
  psi_thread_service->set_thread(nullptr);
  psi_thread_service->aggregate_thread_status(nullptr);
  psi_thread_service->delete_current_thread();
  psi_thread_service->delete_thread(nullptr);
  file_locker = psi_file_service->get_thread_file_name_locker(
      nullptr, 1, PSI_FILE_OPEN, nullptr, nullptr);
  ok(file_locker == nullptr, "no file_locker");
  file_locker = psi_file_service->get_thread_file_stream_locker(
      nullptr, nullptr, PSI_FILE_OPEN);
  ok(file_locker == nullptr, "no file_locker");
  file_locker = psi_file_service->get_thread_file_descriptor_locker(
      nullptr, 0, PSI_FILE_OPEN);
  ok(file_locker == nullptr, "no file_locker");
  psi_mutex_service->unlock_mutex(nullptr);
  psi_rwlock_service->unlock_rwlock(nullptr, PSI_RWLOCK_UNLOCK);
  psi_cond_service->signal_cond(nullptr);
  psi_cond_service->broadcast_cond(nullptr);
  idle_locker = psi_idle_service->start_idle_wait(nullptr, nullptr, 0);
  ok(idle_locker == nullptr, "no idle_locker");
  psi_idle_service->end_idle_wait(nullptr);
  mutex_locker = psi_mutex_service->start_mutex_wait(
      nullptr, nullptr, PSI_MUTEX_LOCK, nullptr, 0);
  ok(mutex_locker == nullptr, "no mutex_locker");
  psi_mutex_service->end_mutex_wait(nullptr, 0);
  rwlock_locker = psi_rwlock_service->start_rwlock_rdwait(
      nullptr, nullptr, PSI_RWLOCK_READLOCK, nullptr, 0);
  ok(rwlock_locker == nullptr, "no rwlock_locker");
  psi_rwlock_service->end_rwlock_rdwait(nullptr, 0);
  rwlock_locker = psi_rwlock_service->start_rwlock_wrwait(
      nullptr, nullptr, PSI_RWLOCK_WRITELOCK, nullptr, 0);
  ok(rwlock_locker == nullptr, "no rwlock_locker");
  psi_rwlock_service->end_rwlock_wrwait(nullptr, 0);
  cond_locker = psi_cond_service->start_cond_wait(nullptr, nullptr, nullptr,
                                                  PSI_COND_WAIT, nullptr, 0);
  ok(cond_locker == nullptr, "no cond_locker");
  psi_cond_service->end_cond_wait(nullptr, 0);
  table_locker = psi_table_service->start_table_io_wait(
      nullptr, nullptr, PSI_TABLE_FETCH_ROW, 0, nullptr, 0);
  ok(table_locker == nullptr, "no table_locker");
  psi_table_service->end_table_io_wait(nullptr, 0);
  table_locker = psi_table_service->start_table_lock_wait(
      nullptr, nullptr, PSI_TABLE_LOCK, 0, nullptr, 0);
  ok(table_locker == nullptr, "no table_locker");
  psi_table_service->end_table_lock_wait(nullptr);
  psi_file_service->start_file_open_wait(nullptr, nullptr, 0);
  file = psi_file_service->end_file_open_wait(nullptr, nullptr);
  ok(file == nullptr, "no file");
  psi_file_service->end_file_open_wait_and_bind_to_descriptor(nullptr, 0);
  psi_file_service->start_file_wait(nullptr, 0, nullptr, 0);
  psi_file_service->end_file_wait(nullptr, 0);
  psi_file_service->start_file_close_wait(nullptr, nullptr, 0);
  psi_file_service->end_file_close_wait(nullptr, 0);
  psi_file_service->start_file_rename_wait(nullptr, 0, nullptr, nullptr,
                                           nullptr, 0);
  psi_file_service->end_file_rename_wait(nullptr, nullptr, nullptr, 0);
  psi_stage_service->start_stage(1, nullptr, 0);

  PSI_stage_progress *progress;
  progress = psi_stage_service->get_current_stage_progress();
  ok(progress == nullptr, "no progress");

  psi_stage_service->end_stage();
  statement_locker = psi_statement_service->get_thread_statement_locker(
      nullptr, 1, nullptr, nullptr);
  ok(statement_locker == nullptr, "no statement_locker");
  statement_locker = psi_statement_service->refine_statement(nullptr, 1);
  ok(statement_locker == nullptr, "no statement_locker");
  psi_statement_service->start_statement(nullptr, nullptr, 0, nullptr, 0);
  psi_statement_service->set_statement_text(nullptr, nullptr, 0);
  psi_statement_service->set_statement_lock_time(nullptr, 0);
  psi_statement_service->set_statement_rows_sent(nullptr, 0);
  psi_statement_service->set_statement_rows_examined(nullptr, 0);
  psi_statement_service->inc_statement_created_tmp_disk_tables(nullptr, 0);
  psi_statement_service->inc_statement_created_tmp_tables(nullptr, 0);
  psi_statement_service->inc_statement_select_full_join(nullptr, 0);
  psi_statement_service->inc_statement_select_full_range_join(nullptr, 0);
  psi_statement_service->inc_statement_select_range(nullptr, 0);
  psi_statement_service->inc_statement_select_range_check(nullptr, 0);
  psi_statement_service->inc_statement_select_scan(nullptr, 0);
  psi_statement_service->inc_statement_sort_merge_passes(nullptr, 0);
  psi_statement_service->inc_statement_sort_range(nullptr, 0);
  psi_statement_service->inc_statement_sort_rows(nullptr, 0);
  psi_statement_service->inc_statement_sort_scan(nullptr, 0);
  psi_statement_service->set_statement_no_index_used(nullptr);
  psi_statement_service->set_statement_no_good_index_used(nullptr);
  psi_statement_service->end_statement(nullptr, nullptr);
  socket_locker = psi_socket_service->start_socket_wait(
      nullptr, nullptr, PSI_SOCKET_SEND, 1, nullptr, 0);
  ok(socket_locker == nullptr, "no socket_locker");
  psi_socket_service->end_socket_wait(nullptr, 0);
  psi_socket_service->set_socket_state(nullptr, PSI_SOCKET_STATE_IDLE);
  psi_socket_service->set_socket_info(nullptr, nullptr, nullptr, 0);
  psi_socket_service->set_socket_thread_owner(nullptr);
  digest_locker = psi_statement_service->digest_start(nullptr);
  ok(digest_locker == nullptr, "no digest_locker");
  psi_statement_service->digest_end(nullptr, nullptr);
  sp_locker = psi_statement_service->start_sp(nullptr, nullptr);
  ok(sp_locker == nullptr, "no sp_locker");
  psi_statement_service->end_sp(nullptr);
  psi_statement_service->drop_sp(0, nullptr, 0, nullptr, 0);
  sp_share = psi_statement_service->get_sp_share(0, nullptr, 0, nullptr, 0);
  ok(sp_share == nullptr, "no sp_share");
  psi_statement_service->release_sp_share(nullptr);
  psi_memory_service->register_memory(nullptr, nullptr, 0);
  memory_key = psi_memory_service->memory_alloc(0, 0, &owner);
  ok(memory_key == PSI_NOT_INSTRUMENTED, "no memory_key");
  memory_key = psi_memory_service->memory_realloc(0, 0, 0, &owner);
  ok(memory_key == PSI_NOT_INSTRUMENTED, "no memory_key");
  psi_memory_service->memory_free(0, 0, nullptr);
  psi_table_service->unlock_table(nullptr);
  metadata_lock = psi_mdl_service->create_metadata_lock(nullptr, nullptr, 1, 2,
                                                        3, nullptr, 0);
  ok(metadata_lock == nullptr, "no metadata_lock");
  psi_mdl_service->set_metadata_lock_status(nullptr, 0);
  psi_mdl_service->destroy_metadata_lock(nullptr);
  metadata_locker =
      psi_mdl_service->start_metadata_wait(nullptr, nullptr, nullptr, 0);
  ok(metadata_locker == nullptr, "no metadata_locker");
  psi_mdl_service->end_metadata_wait(nullptr, 0);

  transaction_locker = psi_transaction_service->get_thread_transaction_locker(
      nullptr, nullptr, nullptr, 1, false, true);
  ok(transaction_locker == nullptr, "no transaction_locker");
  psi_transaction_service->start_transaction(nullptr, nullptr, 0);
  psi_transaction_service->end_transaction(nullptr, true);

  psi_transaction_service->set_transaction_gtid(nullptr, nullptr, nullptr);
  psi_transaction_service->set_transaction_trxid(nullptr, nullptr);
  psi_transaction_service->set_transaction_xa_state(nullptr, 1);
  psi_transaction_service->set_transaction_xid(nullptr, nullptr, 1);
  psi_transaction_service->inc_transaction_release_savepoint(nullptr, 1);
  psi_transaction_service->inc_transaction_rollback_to_savepoint(nullptr, 1);
  psi_transaction_service->inc_transaction_savepoints(nullptr, 1);

  psi_thread_service->set_thread_THD(nullptr, nullptr);

  psi_error_service->log_error(0, PSI_ERROR_OPERATION_RAISED);

  psi_thread_service->set_thread_secondary_engine(false);
  psi_statement_service->set_statement_secondary_engine(nullptr, false);
  psi_statement_service->set_prepared_stmt_secondary_engine(nullptr, false);

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
