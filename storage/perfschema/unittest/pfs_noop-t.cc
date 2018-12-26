/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <pfs_server.h>
#include <pfs_instr_class.h>
#include <pfs_instr.h>
#include <pfs_global.h>
#include <tap.h>

#include <string.h>
#include <memory.h>

#include "stub_print_error.h"
#include "stub_pfs_defaults.h"

void test_noop()
{
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

  PSI_server->register_mutex(NULL, NULL, 0);
  PSI_server->register_rwlock(NULL, NULL, 0);
  PSI_server->register_cond(NULL, NULL, 0);
  PSI_server->register_thread(NULL, NULL, 0);
  PSI_server->register_file(NULL, NULL, 0);
  PSI_server->register_stage(NULL, NULL, 0);
  PSI_server->register_statement(NULL, NULL, 0);
  PSI_server->register_socket(NULL, NULL, 0);

  ok(true, "register");
  mutex= PSI_server->init_mutex(1, NULL);
  ok(mutex == NULL, "no mutex");
  PSI_server->destroy_mutex(NULL);
  rwlock= PSI_server->init_rwlock(1, NULL);
  ok(rwlock == NULL, "no rwlock");
  PSI_server->destroy_rwlock(NULL);
  cond= PSI_server->init_cond(1, NULL);
  ok(cond == NULL, "no cond");
  PSI_server->destroy_cond(NULL);
  socket= PSI_server->init_socket(1, NULL, NULL, 0);
  ok(socket == NULL, "no socket");
  PSI_server->destroy_socket(NULL);
  table_share= PSI_server->get_table_share(false, NULL);
  ok(table_share == NULL, "no table_share");
  PSI_server->release_table_share(NULL);
  PSI_server->drop_table_share(false, NULL, 0, NULL, 0);
  table= PSI_server->open_table(NULL, NULL);
  ok(table == NULL, "no table");
  PSI_server->unbind_table(NULL);
  table= PSI_server->rebind_table(NULL, NULL, NULL);
  ok(table == NULL, "no table");
  PSI_server->close_table(NULL, NULL);
  PSI_server->create_file(1, NULL, 2);
  /* TODO: spawn thread */
  thread= PSI_server->new_thread(1, NULL, 2);
  ok(thread == NULL, "no thread");
  PSI_server->set_thread_id(NULL, 1);
  thread= PSI_server->get_thread();
  ok(thread == NULL, "no thread");
  PSI_server->set_thread_user(NULL, 0);
  PSI_server->set_thread_account(NULL, 0, NULL, 0);
  PSI_server->set_thread_db(NULL, 0);
  PSI_server->set_thread_command(1);
  PSI_server->set_thread_start_time(1);
  PSI_server->set_thread_state(NULL);
  PSI_server->set_thread_info(NULL, 0);
  PSI_server->set_thread(NULL);
  PSI_server->delete_current_thread();
  PSI_server->delete_thread(NULL);
  file_locker= PSI_server->get_thread_file_name_locker(NULL, 1, PSI_FILE_OPEN, NULL, NULL);
  ok(file_locker == NULL, "no file_locker");
  file_locker= PSI_server->get_thread_file_stream_locker(NULL, NULL, PSI_FILE_OPEN);
  ok(file_locker == NULL, "no file_locker");
  file_locker= PSI_server->get_thread_file_descriptor_locker(NULL, 0, PSI_FILE_OPEN);
  ok(file_locker == NULL, "no file_locker");
  PSI_server->unlock_mutex(NULL);
  PSI_server->unlock_rwlock(NULL);
  PSI_server->signal_cond(NULL);
  PSI_server->broadcast_cond(NULL);
  idle_locker= PSI_server->start_idle_wait(NULL, NULL, 0);
  ok(idle_locker == NULL, "no idle_locker");
  PSI_server->end_idle_wait(NULL);
  mutex_locker= PSI_server->start_mutex_wait(NULL, NULL, PSI_MUTEX_LOCK, NULL, 0);
  ok(mutex_locker == NULL, "no mutex_locker");
  PSI_server->end_mutex_wait(NULL, 0);
  rwlock_locker= PSI_server->start_rwlock_rdwait(NULL, NULL, PSI_RWLOCK_READLOCK, NULL, 0);
  ok(rwlock_locker == NULL, "no rwlock_locker");
  PSI_server->end_rwlock_rdwait(NULL, 0);
  rwlock_locker= PSI_server->start_rwlock_wrwait(NULL, NULL, PSI_RWLOCK_WRITELOCK, NULL, 0);
  ok(rwlock_locker == NULL, "no rwlock_locker");
  PSI_server->end_rwlock_wrwait(NULL, 0);
  cond_locker= PSI_server->start_cond_wait(NULL, NULL, NULL, PSI_COND_WAIT, NULL, 0);
  ok(cond_locker == NULL, "no cond_locker");
  PSI_server->end_cond_wait(NULL, 0);
  table_locker= PSI_server->start_table_io_wait(NULL, NULL, PSI_TABLE_FETCH_ROW, 0, NULL, 0);
  ok(table_locker == NULL, "no table_locker");
  PSI_server->end_table_io_wait(NULL, 0);
  table_locker= PSI_server->start_table_lock_wait(NULL, NULL, PSI_TABLE_LOCK, 0, NULL, 0);
  ok(table_locker == NULL, "no table_locker");
  PSI_server->end_table_lock_wait(NULL);
  PSI_server->start_file_open_wait(NULL, NULL, 0);
  file= PSI_server->end_file_open_wait(NULL, NULL);
  ok(file == NULL, "no file");
  PSI_server->end_file_open_wait_and_bind_to_descriptor(NULL, 0);
  PSI_server->start_file_wait(NULL, 0, NULL, 0);
  PSI_server->end_file_wait(NULL, 0);
  PSI_server->start_file_close_wait(NULL, NULL, 0);
  PSI_server->end_file_close_wait(NULL, 0);
  PSI_server->end_file_rename_wait(NULL, NULL, NULL, 0);
  PSI_server->start_stage(1, NULL, 0);

  PSI_stage_progress *progress;
  progress= PSI_server->get_current_stage_progress();
  ok(progress == NULL, "no progress");

  PSI_server->end_stage();
  statement_locker= PSI_server->get_thread_statement_locker(NULL, 1, NULL, NULL);
  ok(statement_locker == NULL, "no statement_locker");
  statement_locker= PSI_server->refine_statement(NULL, 1);
  ok(statement_locker == NULL, "no statement_locker");
  PSI_server->start_statement(NULL, NULL, 0, NULL, 0);
  PSI_server->set_statement_text(NULL, NULL, 0);
  PSI_server->set_statement_lock_time(NULL, 0);
  PSI_server->set_statement_rows_sent(NULL, 0);
  PSI_server->set_statement_rows_examined(NULL, 0);
  PSI_server->inc_statement_created_tmp_disk_tables(NULL, 0);
  PSI_server->inc_statement_created_tmp_tables(NULL, 0);
  PSI_server->inc_statement_select_full_join(NULL, 0);
  PSI_server->inc_statement_select_full_range_join(NULL, 0);
  PSI_server->inc_statement_select_range(NULL, 0);
  PSI_server->inc_statement_select_range_check(NULL, 0);
  PSI_server->inc_statement_select_scan(NULL, 0);
  PSI_server->inc_statement_sort_merge_passes(NULL, 0);
  PSI_server->inc_statement_sort_range(NULL, 0);
  PSI_server->inc_statement_sort_rows(NULL, 0);
  PSI_server->inc_statement_sort_scan(NULL, 0);
  PSI_server->set_statement_no_index_used(NULL);
  PSI_server->set_statement_no_good_index_used(NULL);
  PSI_server->end_statement(NULL, NULL);
  socket_locker= PSI_server->start_socket_wait(NULL, NULL, PSI_SOCKET_SEND, 1, NULL, 0);
  ok(socket_locker == NULL, "no socket_locker");
  PSI_server->end_socket_wait(NULL, 0);
  PSI_server->set_socket_state(NULL, PSI_SOCKET_STATE_IDLE);
  PSI_server->set_socket_info(NULL, NULL, NULL, 0);
  PSI_server->set_socket_thread_owner(NULL);
  digest_locker= PSI_server->digest_start(NULL);
  ok(digest_locker == NULL, "no digest_locker");
  PSI_server->digest_end(NULL, NULL);
  sp_locker= PSI_server->start_sp(NULL, NULL);
  ok(sp_locker == NULL, "no sp_locker");
  PSI_server->end_sp(NULL);
  PSI_server->drop_sp(0, NULL, 0, NULL, 0);
  sp_share= PSI_server->get_sp_share(0, NULL, 0, NULL, 0);
  ok(sp_share == NULL, "no sp_share");
  PSI_server->release_sp_share(NULL);
  PSI_server->register_memory(NULL, NULL, 0);
  memory_key= PSI_server->memory_alloc(0, 0, & owner);
  ok(memory_key == PSI_NOT_INSTRUMENTED, "no memory_key");
  memory_key= PSI_server->memory_realloc(0, 0, 0, & owner);
  ok(memory_key == PSI_NOT_INSTRUMENTED, "no memory_key");
  PSI_server->memory_free(0, 0, NULL);
  PSI_server->unlock_table(NULL);
  metadata_lock= PSI_server->create_metadata_lock(NULL, NULL, 1, 2, 3, NULL, 0);
  ok(metadata_lock == NULL, "no metadata_lock");
  PSI_server->set_metadata_lock_status(NULL, 0);
  PSI_server->destroy_metadata_lock(NULL);
  metadata_locker= PSI_server->start_metadata_wait(NULL, NULL, NULL, 0);
  ok(metadata_locker == NULL, "no metadata_locker");
  PSI_server->end_metadata_wait(NULL, 0);
  
  transaction_locker= PSI_server->get_thread_transaction_locker(NULL, NULL, NULL, 1, false, 1);
  ok(transaction_locker == NULL, "no transaction_locker");
  PSI_server->start_transaction(NULL, NULL, 0);
  PSI_server->end_transaction(NULL, true);

  PSI_server->set_transaction_gtid(NULL, NULL, NULL);
  PSI_server->set_transaction_trxid(NULL, NULL);
  PSI_server->set_transaction_xa_state(NULL, 1);
  PSI_server->set_transaction_xid(NULL, NULL, 1);
  PSI_server->inc_transaction_release_savepoint(NULL, 1);
  PSI_server->inc_transaction_rollback_to_savepoint(NULL, 1);
  PSI_server->inc_transaction_savepoints(NULL, 1);

  PSI_server->set_thread_THD(NULL, NULL);

  ok(true, "all noop api called");
}

int main(int, char **)
{
  plan(34);

  MY_INIT("pfs_noop-t");
  test_noop();
  return (exit_status());
}

