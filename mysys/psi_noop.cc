/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

/*
  Always provide the noop performance interface, for plugins.
*/

#define USE_PSI_V1
#define HAVE_PSI_INTERFACE

#include "my_global.h"
#include "my_pthread.h"
#include "my_sys.h"
#include "mysql/psi/psi.h"

static void register_mutex_noop(const char *category,
                                PSI_mutex_info *info,
                                int count)
{
  return;
}

static void register_rwlock_noop(const char *category,
                                 PSI_rwlock_info *info,
                                 int count)
{
  return;
}

static void register_cond_noop(const char *category,
                               PSI_cond_info *info,
                               int count)
{
  return;
}

static void register_thread_noop(const char *category,
                                 PSI_thread_info *info,
                                 int count)
{
  return;
}

static void register_file_noop(const char *category,
                               PSI_file_info *info,
                               int count)
{
  return;
}

static void register_stage_noop(const char *category,
                                PSI_stage_info **info_array,
                                int count)
{
  return;
}

static void register_statement_noop(const char *category,
                                    PSI_statement_info *info,
                                    int count)
{
  return;
}

static PSI_mutex*
init_mutex_noop(PSI_mutex_key key, const void *identity)
{
  return NULL;
}

static void destroy_mutex_noop(PSI_mutex* mutex)
{
  return;
}

static PSI_rwlock*
init_rwlock_noop(PSI_rwlock_key key, const void *identity)
{
  return NULL;
}

static void destroy_rwlock_noop(PSI_rwlock* rwlock)
{
  return;
}

static PSI_cond*
init_cond_noop(PSI_cond_key key, const void *identity)
{
  return NULL;
}

static void destroy_cond_noop(PSI_cond* cond)
{
  return;
}

static PSI_table_share*
get_table_share_noop(my_bool temporary, TABLE_SHARE *share)
{
  return NULL;
}

static void release_table_share_noop(PSI_table_share* share)
{
  return;
}

static void
drop_table_share_noop(const char *schema_name, int schema_name_length,
                      const char *table_name, int table_name_length)
{
  return;
}

static PSI_table*
open_table_noop(PSI_table_share *share, const void *identity)
{
  return NULL;
}

static void close_table_noop(PSI_table *table)
{
  return;
}

static void create_file_noop(PSI_file_key key, const char *name, File file)
{
  return;
}

static int spawn_thread_noop(PSI_thread_key key,
                             pthread_t *thread, const pthread_attr_t *attr,
                             void *(*start_routine)(void*), void *arg)
{
  return pthread_create(thread, attr, start_routine, arg);
}

static PSI_thread*
new_thread_noop(PSI_thread_key key, const void *identity, ulong thread_id)
{
  return NULL;
}

static void set_thread_id_noop(PSI_thread *thread, unsigned long id)
{
  return;
}

static PSI_thread*
get_thread_noop(void)
{
  return NULL;
}

static void set_thread_user_noop(const char *user, int user_len)
{
  return;
}

static void set_thread_user_host_noop(const char *user, int user_len,
                                    const char *host, int host_len)
{
  return;
}

static void set_thread_db_noop(const char* db, int db_len)
{
  return;
}

static void set_thread_command_noop(int command)
{
  return;
}

static void set_thread_start_time_noop(time_t start_time)
{
  return;
}

static void set_thread_state_noop(const char* state)
{
  return;
}

static void set_thread_info_noop(const char* info, int info_len)
{
  return;
}

static void set_thread_noop(PSI_thread* thread)
{
  return;
}

static void delete_current_thread_noop(void)
{
  return;
}

static void delete_thread_noop(PSI_thread *thread)
{
  return;
}

static PSI_mutex_locker*
get_thread_mutex_locker_noop(PSI_mutex_locker_state *state,
                             PSI_mutex *mutex, PSI_mutex_operation op)
{
  return NULL;
}

static PSI_rwlock_locker*
get_thread_rwlock_locker_noop(PSI_rwlock_locker_state *state,
                              PSI_rwlock *rwlock, PSI_rwlock_operation op)
{
  return NULL;
}

static PSI_cond_locker*
get_thread_cond_locker_noop(PSI_cond_locker_state *state,
                            PSI_cond *cond, PSI_mutex *mutex,
                            PSI_cond_operation op)
{
  return NULL;
}

static PSI_table_locker*
get_thread_table_io_locker_noop(PSI_table_locker_state *state,
                                PSI_table *table, PSI_table_io_operation op, uint index)
{
  return NULL;
}

static PSI_table_locker*
get_thread_table_lock_locker_noop(PSI_table_locker_state *state,
                                  PSI_table *table, PSI_table_lock_operation op, ulong op_flags)
{
  return NULL;
}

static PSI_file_locker*
get_thread_file_name_locker_noop(PSI_file_locker_state *state,
                                 PSI_file_key key,
                                 PSI_file_operation op,
                                 const char *name, const void *identity)
{
  return NULL;
}

static PSI_file_locker*
get_thread_file_stream_locker_noop(PSI_file_locker_state *state,
                                   PSI_file *file, PSI_file_operation op)
{
  return NULL;
}


static PSI_file_locker*
get_thread_file_descriptor_locker_noop(PSI_file_locker_state *state,
                                       File file, PSI_file_operation op)
{
  return NULL;
}

static void unlock_mutex_noop(PSI_mutex *mutex)
{
  return;
}

static void unlock_rwlock_noop(PSI_rwlock *rwlock)
{
  return;
}

static void signal_cond_noop(PSI_cond* cond)
{
  return;
}

static void broadcast_cond_noop(PSI_cond* cond)
{
  return;
}

static void start_mutex_wait_noop(PSI_mutex_locker* locker,
                                  const char *src_file, uint src_line)
{
  return;
}

static void end_mutex_wait_noop(PSI_mutex_locker* locker, int rc)
{
  return;
}


static void start_rwlock_rdwait_noop(PSI_rwlock_locker* locker,
                                     const char *src_file, uint src_line)
{
  return;
}

static void end_rwlock_rdwait_noop(PSI_rwlock_locker* locker, int rc)
{
  return;
}

static void start_rwlock_wrwait_noop(PSI_rwlock_locker* locker,
                                     const char *src_file, uint src_line)
{
  return;
}

static void end_rwlock_wrwait_noop(PSI_rwlock_locker* locker, int rc)
{
  return;
}

static void start_cond_wait_noop(PSI_cond_locker* locker,
                                 const char *src_file, uint src_line)
{
  return;
}

static void end_cond_wait_noop(PSI_cond_locker* locker, int rc)
{
  return;
}

static void start_table_io_wait_noop(PSI_table_locker* locker,
                                     const char *src_file, uint src_line)
{
  return;
}

static void end_table_io_wait_noop(PSI_table_locker* locker)
{
  return;
}

static void start_table_lock_wait_noop(PSI_table_locker* locker,
                                       const char *src_file, uint src_line)
{
  return;
}

static void end_table_lock_wait_noop(PSI_table_locker* locker)
{
  return;
}

static PSI_file* start_file_open_wait_noop(PSI_file_locker *locker,
                                           const char *src_file,
                                           uint src_line)
{
  return NULL;
}

static void end_file_open_wait_noop(PSI_file_locker *locker)
{
  return;
}

static void end_file_open_wait_and_bind_to_descriptor_noop
  (PSI_file_locker *locker, File file)
{
  return;
}

static void start_file_wait_noop(PSI_file_locker *locker,
                                 size_t count,
                                 const char *src_file,
                                 uint src_line)
{
  return;
}

static void end_file_wait_noop(PSI_file_locker *locker,
                               size_t count)
{
  return;
}

static void start_stage_noop(PSI_stage_key key, const char *src_file, int src_line)
{
  return;
}

static void end_stage_noop()
{
  return;
}

static PSI_statement_locker*
get_thread_statement_locker_noop(PSI_statement_locker_state *state,
                                 PSI_statement_key key)
{
  return NULL;
}

static PSI_statement_locker*
refine_statement_noop(PSI_statement_locker *locker,
                      PSI_statement_key key)
{
  return NULL;
}

static void start_statement_noop(PSI_statement_locker *locker,
                                 const char *db, uint db_len,
                                 const char *src_file, uint src_line)
{
  return;
}

static void set_statement_text_noop(PSI_statement_locker *locker,
                                    const char *text, uint text_len)
{
  return;
}

static void set_statement_lock_time_noop(PSI_statement_locker *locker,
                                         ulonglong count)
{
  return;
}

static void set_statement_rows_sent_noop(PSI_statement_locker *locker,
                                         ulonglong count)
{
  return;
}

static void set_statement_rows_examined_noop(PSI_statement_locker *locker,
                                             ulonglong count)
{
  return;
}

static void inc_statement_created_tmp_disk_tables_noop(PSI_statement_locker *locker,
                                                       ulonglong count)
{
  return;
}

static void inc_statement_created_tmp_tables_noop(PSI_statement_locker *locker,
                                                  ulonglong count)
{
  return;
}

static void inc_statement_select_full_join_noop(PSI_statement_locker *locker,
                                                ulonglong count)
{
  return;
}

static void inc_statement_select_full_range_join_noop(PSI_statement_locker *locker,
                                                      ulonglong count)
{
  return;
}

static void inc_statement_select_range_noop(PSI_statement_locker *locker,
                                            ulonglong count)
{
  return;
}

static void inc_statement_select_range_check_noop(PSI_statement_locker *locker,
                                                  ulonglong count)
{
  return;
}

static void inc_statement_select_scan_noop(PSI_statement_locker *locker,
                                           ulonglong count)
{
  return;
}

static void inc_statement_sort_merge_passes_noop(PSI_statement_locker *locker,
                                                 ulonglong count)
{
  return;
}

static void inc_statement_sort_range_noop(PSI_statement_locker *locker,
                                          ulonglong count)
{
  return;
}

static void inc_statement_sort_rows_noop(PSI_statement_locker *locker,
                                         ulonglong count)
{
  return;
}

static void inc_statement_sort_scan_noop(PSI_statement_locker *locker,
                                         ulonglong count)
{
  return;
}

static void set_statement_no_index_used_noop(PSI_statement_locker *locker)
{
  return;
}

static void set_statement_no_good_index_used_noop(PSI_statement_locker *locker)
{
  return;
}

static void end_statement_noop(PSI_statement_locker *locker, void *stmt_da)
{
  return;
}

static PSI PSI_noop=
{
  register_mutex_noop,
  register_rwlock_noop,
  register_cond_noop,
  register_thread_noop,
  register_file_noop,
  register_stage_noop,
  register_statement_noop,
  init_mutex_noop,
  destroy_mutex_noop,
  init_rwlock_noop,
  destroy_rwlock_noop,
  init_cond_noop,
  destroy_cond_noop,
  get_table_share_noop,
  release_table_share_noop,
  drop_table_share_noop,
  open_table_noop,
  close_table_noop,
  create_file_noop,
  spawn_thread_noop,
  new_thread_noop,
  set_thread_id_noop,
  get_thread_noop,
  set_thread_user_noop,
  set_thread_user_host_noop,
  set_thread_db_noop,
  set_thread_command_noop,
  set_thread_start_time_noop,
  set_thread_state_noop,
  set_thread_info_noop,
  set_thread_noop,
  delete_current_thread_noop,
  delete_thread_noop,
  get_thread_mutex_locker_noop,
  get_thread_rwlock_locker_noop,
  get_thread_cond_locker_noop,
  get_thread_table_io_locker_noop,
  get_thread_table_lock_locker_noop,
  get_thread_file_name_locker_noop,
  get_thread_file_stream_locker_noop,
  get_thread_file_descriptor_locker_noop,
  unlock_mutex_noop,
  unlock_rwlock_noop,
  signal_cond_noop,
  broadcast_cond_noop,
  start_mutex_wait_noop,
  end_mutex_wait_noop,
  start_rwlock_rdwait_noop,
  end_rwlock_rdwait_noop,
  start_rwlock_wrwait_noop,
  end_rwlock_wrwait_noop,
  start_cond_wait_noop,
  end_cond_wait_noop,
  start_table_io_wait_noop,
  end_table_io_wait_noop,
  start_table_lock_wait_noop,
  end_table_lock_wait_noop,
  start_file_open_wait_noop,
  end_file_open_wait_noop,
  end_file_open_wait_and_bind_to_descriptor_noop,
  start_file_wait_noop,
  end_file_wait_noop,
  start_stage_noop,
  end_stage_noop,
  get_thread_statement_locker_noop,
  refine_statement_noop,
  start_statement_noop,
  set_statement_text_noop,
  set_statement_lock_time_noop,
  set_statement_rows_sent_noop,
  set_statement_rows_examined_noop,
  inc_statement_created_tmp_disk_tables_noop,
  inc_statement_created_tmp_tables_noop,
  inc_statement_select_full_join_noop,
  inc_statement_select_full_range_join_noop,
  inc_statement_select_range_noop,
  inc_statement_select_range_check_noop,
  inc_statement_select_scan_noop,
  inc_statement_sort_merge_passes_noop,
  inc_statement_sort_range_noop,
  inc_statement_sort_rows_noop,
  inc_statement_sort_scan_noop,
  set_statement_no_index_used_noop,
  set_statement_no_good_index_used_noop,
  end_statement_noop
};

C_MODE_START

/**
  Hook for the instrumentation interface.
  Code implementing the instrumentation interface should register here.
*/
struct PSI_bootstrap *PSI_hook= NULL;

/**
  Instance of the instrumentation interface for the MySQL server.
  @todo This is currently a global variable, which is handy when
  compiling instrumented code that is bundled with the server.
  When dynamic plugin are truly supported, this variable will need
  to be replaced by a macro, so that each XYZ plugin can have it's own
  xyz_psi_server variable, obtained from PSI_bootstrap::get_interface()
  with the version used at compile time for plugin XYZ.
*/

PSI *PSI_server= & PSI_noop;

void set_psi_server(PSI *psi)
{
  PSI_server= psi;
}

C_MODE_END

