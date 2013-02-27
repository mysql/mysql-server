/* Copyright (c) 2011, 2013 Oracle and/or its affiliates. All rights reserved.

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

C_MODE_START

#define NNN __attribute__((unused))

static void register_mutex_noop(const char *category NNN,
                                PSI_mutex_info *info NNN,
                                int count NNN)
{
  return;
}

static void register_rwlock_noop(const char *category NNN,
                                 PSI_rwlock_info *info NNN,
                                 int count NNN)
{
  return;
}

static void register_cond_noop(const char *category NNN,
                               PSI_cond_info *info NNN,
                               int count NNN)
{
  return;
}

static void register_thread_noop(const char *category NNN,
                                 PSI_thread_info *info NNN,
                                 int count NNN)
{
  return;
}

static void register_file_noop(const char *category NNN,
                               PSI_file_info *info NNN,
                               int count NNN)
{
  return;
}

static void register_stage_noop(const char *category NNN,
                                PSI_stage_info **info_array NNN,
                                int count NNN)
{
  return;
}

static void register_statement_noop(const char *category NNN,
                                    PSI_statement_info *info NNN,
                                    int count NNN)
{
  return;
}

static void register_socket_noop(const char *category NNN,
                                 PSI_socket_info *info NNN,
                                 int count NNN)
{
  return;
}

static PSI_mutex*
init_mutex_noop(PSI_mutex_key key NNN, const void *identity NNN)
{
  return NULL;
}

static void destroy_mutex_noop(PSI_mutex* mutex NNN)
{
  return;
}

static PSI_rwlock*
init_rwlock_noop(PSI_rwlock_key key NNN, const void *identity NNN)
{
  return NULL;
}

static void destroy_rwlock_noop(PSI_rwlock* rwlock NNN)
{
  return;
}

static PSI_cond*
init_cond_noop(PSI_cond_key key NNN, const void *identity NNN)
{
  return NULL;
}

static void destroy_cond_noop(PSI_cond* cond NNN)
{
  return;
}

static PSI_socket*
init_socket_noop(PSI_socket_key key NNN, const my_socket *fd NNN,
                 const struct sockaddr *addr NNN, socklen_t addr_len NNN)
{
  return NULL;
}

static void destroy_socket_noop(PSI_socket* socket NNN)
{
  return;
}

static PSI_table_share*
get_table_share_noop(my_bool temporary NNN, struct TABLE_SHARE *share NNN)
{
  return NULL;
}

static void release_table_share_noop(PSI_table_share* share NNN)
{
  return;
}

static void
drop_table_share_noop(my_bool temporary NNN, const char *schema_name NNN,
                      int schema_name_length NNN, const char *table_name NNN,
                      int table_name_length NNN)
{
  return;
}

static PSI_table*
open_table_noop(PSI_table_share *share NNN, const void *identity NNN)
{
  return NULL;
}

static void unbind_table_noop(PSI_table *table NNN)
{
  return;
}

static PSI_table*
rebind_table_noop(PSI_table_share *share NNN,
                  const void *identity NNN,
                  PSI_table *table NNN)
{
  return NULL;
}

static void close_table_noop(PSI_table *table NNN)
{
  return;
}

static void create_file_noop(PSI_file_key key NNN,
                             const char *name NNN, File file NNN)
{
  return;
}

static int spawn_thread_noop(PSI_thread_key key NNN,
                             pthread_t *thread NNN,
                             const pthread_attr_t *attr NNN,
                             void *(*start_routine)(void*) NNN, void *arg NNN)
{
  return pthread_create(thread, attr, start_routine, arg);
}

static PSI_thread*
new_thread_noop(PSI_thread_key key NNN,
                const void *identity NNN, ulonglong thread_id NNN)
{
  return NULL;
}

static void set_thread_id_noop(PSI_thread *thread NNN, ulonglong id NNN)
{
  return;
}

static PSI_thread*
get_thread_noop(void NNN)
{
  return NULL;
}

static void set_thread_user_noop(const char *user NNN, int user_len NNN)
{
  return;
}

static void set_thread_user_host_noop(const char *user NNN, int user_len NNN,
                                      const char *host NNN, int host_len NNN)
{
  return;
}

static void set_thread_db_noop(const char* db NNN, int db_len NNN)
{
  return;
}

static void set_thread_command_noop(int command NNN)
{
  return;
}

static void set_thread_start_time_noop(time_t start_time NNN)
{
  return;
}

static void set_thread_state_noop(const char* state NNN)
{
  return;
}

static void set_thread_info_noop(const char* info NNN, uint info_len NNN)
{
  return;
}

static void set_thread_noop(PSI_thread* thread NNN)
{
  return;
}

static void delete_current_thread_noop(void)
{
  return;
}

static void delete_thread_noop(PSI_thread *thread NNN)
{
  return;
}

static PSI_file_locker*
get_thread_file_name_locker_noop(PSI_file_locker_state *state NNN,
                                 PSI_file_key key NNN,
                                 enum PSI_file_operation op NNN,
                                 const char *name NNN, const void *identity NNN)
{
  return NULL;
}

static PSI_file_locker*
get_thread_file_stream_locker_noop(PSI_file_locker_state *state NNN,
                                   PSI_file *file NNN,
                                   enum PSI_file_operation op NNN)
{
  return NULL;
}


static PSI_file_locker*
get_thread_file_descriptor_locker_noop(PSI_file_locker_state *state NNN,
                                       File file NNN,
                                       enum PSI_file_operation op NNN)
{
  return NULL;
}

static void unlock_mutex_noop(PSI_mutex *mutex NNN)
{
  return;
}

static void unlock_rwlock_noop(PSI_rwlock *rwlock NNN)
{
  return;
}

static void signal_cond_noop(PSI_cond* cond NNN)
{
  return;
}

static void broadcast_cond_noop(PSI_cond* cond NNN)
{
  return;
}

static PSI_idle_locker*
start_idle_wait_noop(PSI_idle_locker_state* state NNN,
                     const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_idle_wait_noop(PSI_idle_locker* locker NNN)
{
  return;
}

static PSI_mutex_locker*
start_mutex_wait_noop(PSI_mutex_locker_state *state NNN,
                      PSI_mutex *mutex NNN,
                      PSI_mutex_operation op NNN,
                      const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_mutex_wait_noop(PSI_mutex_locker* locker NNN, int rc NNN)
{
  return;
}


static PSI_rwlock_locker*
start_rwlock_rdwait_noop(struct PSI_rwlock_locker_state_v1 *state NNN,
                         struct PSI_rwlock *rwlock NNN,
                         enum PSI_rwlock_operation op NNN,
                         const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_rwlock_rdwait_noop(PSI_rwlock_locker* locker NNN, int rc NNN)
{
  return;
}

static struct PSI_rwlock_locker*
start_rwlock_wrwait_noop(struct PSI_rwlock_locker_state_v1 *state NNN,
                         struct PSI_rwlock *rwlock NNN,
                         enum PSI_rwlock_operation op NNN,
                         const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_rwlock_wrwait_noop(PSI_rwlock_locker* locker NNN, int rc NNN)
{
  return;
}

static struct PSI_cond_locker*
start_cond_wait_noop(struct PSI_cond_locker_state_v1 *state NNN,
                     struct PSI_cond *cond NNN,
                     struct PSI_mutex *mutex NNN,
                     enum PSI_cond_operation op NNN,
                     const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_cond_wait_noop(PSI_cond_locker* locker NNN, int rc NNN)
{
  return;
}

static struct PSI_table_locker*
start_table_io_wait_noop(struct PSI_table_locker_state_v1 *state NNN,
                         struct PSI_table *table NNN,
                         enum PSI_table_io_operation op NNN,
                         uint index NNN,
                         const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_table_io_wait_noop(PSI_table_locker* locker NNN)
{
  return;
}

static struct PSI_table_locker*
start_table_lock_wait_noop(struct PSI_table_locker_state_v1 *state NNN,
                           struct PSI_table *table NNN,
                           enum PSI_table_lock_operation op NNN,
                           ulong flags NNN,
                           const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_table_lock_wait_noop(PSI_table_locker* locker NNN)
{
  return;
}

static void start_file_open_wait_noop(PSI_file_locker *locker NNN,
                                      const char *src_file NNN,
                                      uint src_line NNN)
{
  return;
}

static PSI_file* end_file_open_wait_noop(PSI_file_locker *locker NNN,
                                         void *result NNN)
{
  return NULL;
}

static void end_file_open_wait_and_bind_to_descriptor_noop
  (PSI_file_locker *locker NNN, File file NNN)
{
  return;
}

static void start_file_wait_noop(PSI_file_locker *locker NNN,
                                 size_t count NNN,
                                 const char *src_file NNN,
                                 uint src_line NNN)
{
  return;
}

static void end_file_wait_noop(PSI_file_locker *locker NNN,
                               size_t count NNN)
{
  return;
}

static void start_file_close_wait_noop(PSI_file_locker *locker NNN,
                                       const char *src_file NNN,
                                       uint src_line NNN)
{
  return;
}

static void end_file_close_wait_noop(PSI_file_locker *locker NNN,
                                     int result NNN)
{
  return;
}

static void start_stage_noop(PSI_stage_key key NNN,
                             const char *src_file NNN, int src_line NNN)
{
  return;
}

static void end_stage_noop(void)
{
  return;
}

static PSI_statement_locker*
get_thread_statement_locker_noop(PSI_statement_locker_state *state NNN,
                                 PSI_statement_key key NNN,
                                 const void *charset NNN)
{
  return NULL;
}

static PSI_statement_locker*
refine_statement_noop(PSI_statement_locker *locker NNN,
                      PSI_statement_key key NNN)
{
  return NULL;
}

static void start_statement_noop(PSI_statement_locker *locker NNN,
                                 const char *db NNN, uint db_len NNN,
                                 const char *src_file NNN, uint src_line NNN)
{
  return;
}

static void set_statement_text_noop(PSI_statement_locker *locker NNN,
                                    const char *text NNN, uint text_len NNN)
{
  return;
}

static void set_statement_lock_time_noop(PSI_statement_locker *locker NNN,
                                         ulonglong count NNN)
{
  return;
}

static void set_statement_rows_sent_noop(PSI_statement_locker *locker NNN,
                                         ulonglong count NNN)
{
  return;
}

static void set_statement_rows_examined_noop(PSI_statement_locker *locker NNN,
                                             ulonglong count NNN)
{
  return;
}

static void inc_statement_created_tmp_disk_tables_noop(PSI_statement_locker *locker NNN,
                                                       ulong count NNN)
{
  return;
}

static void inc_statement_created_tmp_tables_noop(PSI_statement_locker *locker NNN,
                                                  ulong count NNN)
{
  return;
}

static void inc_statement_select_full_join_noop(PSI_statement_locker *locker NNN,
                                                ulong count NNN)
{
  return;
}

static void inc_statement_select_full_range_join_noop(PSI_statement_locker *locker NNN,
                                                      ulong count NNN)
{
  return;
}

static void inc_statement_select_range_noop(PSI_statement_locker *locker NNN,
                                            ulong count NNN)
{
  return;
}

static void inc_statement_select_range_check_noop(PSI_statement_locker *locker NNN,
                                                  ulong count NNN)
{
  return;
}

static void inc_statement_select_scan_noop(PSI_statement_locker *locker NNN,
                                           ulong count NNN)
{
  return;
}

static void inc_statement_sort_merge_passes_noop(PSI_statement_locker *locker NNN,
                                                 ulong count NNN)
{
  return;
}

static void inc_statement_sort_range_noop(PSI_statement_locker *locker NNN,
                                          ulong count NNN)
{
  return;
}

static void inc_statement_sort_rows_noop(PSI_statement_locker *locker NNN,
                                         ulong count NNN)
{
  return;
}

static void inc_statement_sort_scan_noop(PSI_statement_locker *locker NNN,
                                         ulong count NNN)
{
  return;
}

static void set_statement_no_index_used_noop(PSI_statement_locker *locker NNN)
{
  return;
}

static void set_statement_no_good_index_used_noop(PSI_statement_locker *locker NNN)
{
  return;
}

static void end_statement_noop(PSI_statement_locker *locker NNN,
                               void *stmt_da NNN)
{
  return;
}

static PSI_socket_locker*
start_socket_wait_noop(PSI_socket_locker_state *state NNN,
                       PSI_socket *socket NNN,
                       PSI_socket_operation op NNN,
                       size_t count NNN,
                       const char *src_file NNN,
                       uint src_line NNN)
{
  return NULL;
}

static void end_socket_wait_noop(PSI_socket_locker *locker NNN,
                                 size_t count NNN)
{
  return;
}

static void set_socket_state_noop(PSI_socket *socket NNN,
                                  enum PSI_socket_state state NNN)
{
  return;
}

static void set_socket_info_noop(PSI_socket *socket NNN,
                                 const my_socket *fd NNN,
                                 const struct sockaddr *addr NNN,
                                 socklen_t addr_len NNN)
{
  return;
}

static void set_socket_thread_owner_noop(PSI_socket *socket NNN)
{
  return;
}

static struct PSI_digest_locker*
digest_start_noop(PSI_statement_locker *locker NNN)
{
  return NULL;
}

static PSI_digest_locker*
digest_add_token_noop(PSI_digest_locker *locker NNN,
                      uint token NNN,
                      struct OPAQUE_LEX_YYSTYPE *yylval NNN)
{
  return NULL;
}

static int
set_thread_connect_attrs_noop(const char *buffer __attribute__((unused)),
                             uint length  __attribute__((unused)),
                             const void *from_cs __attribute__((unused)))
{
  return 0;
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
  register_socket_noop,
  init_mutex_noop,
  destroy_mutex_noop,
  init_rwlock_noop,
  destroy_rwlock_noop,
  init_cond_noop,
  destroy_cond_noop,
  init_socket_noop,
  destroy_socket_noop,
  get_table_share_noop,
  release_table_share_noop,
  drop_table_share_noop,
  open_table_noop,
  unbind_table_noop,
  rebind_table_noop,
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
  get_thread_file_name_locker_noop,
  get_thread_file_stream_locker_noop,
  get_thread_file_descriptor_locker_noop,
  unlock_mutex_noop,
  unlock_rwlock_noop,
  signal_cond_noop,
  broadcast_cond_noop,
  start_idle_wait_noop,
  end_idle_wait_noop,
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
  start_file_close_wait_noop,
  end_file_close_wait_noop,
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
  end_statement_noop,
  start_socket_wait_noop,
  end_socket_wait_noop,
  set_socket_state_noop,
  set_socket_info_noop,
  set_socket_thread_owner_noop,
  digest_start_noop,
  digest_add_token_noop,
  set_thread_connect_attrs_noop
};

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

