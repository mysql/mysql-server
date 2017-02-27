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

/**
  @file mysys/psi_noop.cc
  Always provide the noop performance interface, for plugins.
*/

#define USE_PSI_V1
#define HAVE_PSI_INTERFACE

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <time.h>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"  // IWYU pragma: keep
#include "my_thread.h"
#include "mysql/psi/psi_base.h"
#include "mysql/psi/psi_cond.h"
#include "mysql/psi/psi_data_lock.h"
#include "mysql/psi/psi_error.h"
#include "mysql/psi/psi_file.h"
#include "mysql/psi/psi_idle.h"
#include "mysql/psi/psi_mdl.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/psi/psi_mutex.h"
#include "mysql/psi/psi_rwlock.h"
#include "mysql/psi/psi_socket.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/psi/psi_statement.h"
#include "mysql/psi/psi_table.h"
#include "mysql/psi/psi_thread.h"
#include "mysql/psi/psi_transaction.h"

class THD;
struct MDL_key;

C_MODE_START

#define NNN MY_ATTRIBUTE((unused))

// ===========================================================================

static void register_thread_noop(const char *category NNN,
                                 PSI_thread_info *info NNN,
                                 int count NNN)
{
  return;
}

static int spawn_thread_noop(PSI_thread_key key NNN,
                             my_thread_handle *thread,
                             const my_thread_attr_t *attr,
                             my_start_routine start_routine, void *arg)
{
  return my_thread_create(thread, attr, start_routine, arg);
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

static void set_thread_THD_noop(PSI_thread *thread NNN, THD *thd NNN)
{
  return;
}

static void set_thread_os_id_noop(PSI_thread *thread NNN)
{
  return;
}

static PSI_thread*
get_thread_noop(void)
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

static void set_connection_type_noop(opaque_vio_type conn_type NNN)
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

static int
set_thread_connect_attrs_noop(const char *buffer MY_ATTRIBUTE((unused)),
                             uint length  MY_ATTRIBUTE((unused)),
                             const void *from_cs MY_ATTRIBUTE((unused)))
{
  return 0;
}

static void get_thread_event_id_noop(ulonglong *thread_internal_id,
                                     ulonglong *event_id)
{
  *thread_internal_id= 0;
  *event_id= 0;
}

static PSI_thread_service_t psi_thread_noop=
{
  register_thread_noop,
  spawn_thread_noop,
  new_thread_noop,
  set_thread_id_noop,
  set_thread_THD_noop,
  set_thread_os_id_noop,
  get_thread_noop,
  set_thread_user_noop,
  set_thread_user_host_noop,
  set_thread_db_noop,
  set_thread_command_noop,
  set_connection_type_noop,
  set_thread_start_time_noop,
  set_thread_state_noop,
  set_thread_info_noop,
  set_thread_noop,
  delete_current_thread_noop,
  delete_thread_noop,
  set_thread_connect_attrs_noop,
  get_thread_event_id_noop
};

struct PSI_thread_bootstrap *psi_thread_hook= NULL;
PSI_thread_service_t *psi_thread_service= & psi_thread_noop;

void set_psi_thread_service(PSI_thread_service_t *psi)
{
  psi_thread_service= psi;
}

// ===========================================================================

static void register_mutex_noop(const char *category NNN,
                                PSI_mutex_info *info NNN,
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

static void unlock_mutex_noop(PSI_mutex *mutex NNN)
{
  return;
}

static PSI_mutex_service_t psi_mutex_noop=
{
  register_mutex_noop,
  init_mutex_noop,
  destroy_mutex_noop,
  start_mutex_wait_noop,
  end_mutex_wait_noop,
  unlock_mutex_noop
};

struct PSI_mutex_bootstrap *psi_mutex_hook= NULL;
PSI_mutex_service_t *psi_mutex_service= & psi_mutex_noop;

void set_psi_mutex_service(PSI_mutex_service_t *psi)
{
  psi_mutex_service= psi;
}

// ===========================================================================

static void register_rwlock_noop(const char *category NNN,
                                 PSI_rwlock_info *info NNN,
                                 int count NNN)
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

static void unlock_rwlock_noop(PSI_rwlock *rwlock NNN)
{
  return;
}

static PSI_rwlock_service_t psi_rwlock_noop=
{
  register_rwlock_noop,
  init_rwlock_noop,
  destroy_rwlock_noop,
  start_rwlock_rdwait_noop,
  end_rwlock_rdwait_noop,
  start_rwlock_wrwait_noop,
  end_rwlock_wrwait_noop,
  unlock_rwlock_noop
};

struct PSI_rwlock_bootstrap *psi_rwlock_hook= NULL;
PSI_rwlock_service_t *psi_rwlock_service= & psi_rwlock_noop;

void set_psi_rwlock_service(PSI_rwlock_service_t *psi)
{
  psi_rwlock_service= psi;
}

// ===========================================================================

static void register_cond_noop(const char *category NNN,
                               PSI_cond_info *info NNN,
                               int count NNN)
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

static void signal_cond_noop(PSI_cond* cond NNN)
{
  return;
}

static void broadcast_cond_noop(PSI_cond* cond NNN)
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

static PSI_cond_service_t psi_cond_noop=
{
  register_cond_noop,
  init_cond_noop,
  destroy_cond_noop,
  signal_cond_noop,
  broadcast_cond_noop,
  start_cond_wait_noop,
  end_cond_wait_noop
};

struct PSI_cond_bootstrap *psi_cond_hook= NULL;
PSI_cond_service_t *psi_cond_service= & psi_cond_noop;

void set_psi_cond_service(PSI_cond_service_t *psi)
{
  psi_cond_service= psi;
}

// ===========================================================================

static void register_file_noop(const char *category NNN,
                               PSI_file_info *info NNN,
                               int count NNN)
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

static void create_file_noop(PSI_file_key key NNN,
                             const char *name NNN, File file NNN)
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

static void end_temp_file_open_wait_and_bind_to_descriptor_noop
  (PSI_file_locker *locker NNN, File file NNN, const char *filaneme NNN)
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

static PSI_file_service_t psi_file_noop=
{
  register_file_noop,
  create_file_noop,
  get_thread_file_name_locker_noop,
  get_thread_file_stream_locker_noop,
  get_thread_file_descriptor_locker_noop,
  start_file_open_wait_noop,
  end_file_open_wait_noop,
  end_file_open_wait_and_bind_to_descriptor_noop,
  end_temp_file_open_wait_and_bind_to_descriptor_noop,
  start_file_wait_noop,
  end_file_wait_noop,
  start_file_close_wait_noop,
  end_file_close_wait_noop
};

struct PSI_file_bootstrap *psi_file_hook= NULL;
PSI_file_service_t *psi_file_service= & psi_file_noop;

void set_psi_file_service(PSI_file_service_t *psi)
{
  psi_file_service= psi;
}

// ===========================================================================

static void register_socket_noop(const char *category NNN,
                                 PSI_socket_info *info NNN,
                                 int count NNN)
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

static PSI_socket_service_t psi_socket_noop=
{
  register_socket_noop,
  init_socket_noop,
  destroy_socket_noop,
  start_socket_wait_noop,
  end_socket_wait_noop,
  set_socket_state_noop,
  set_socket_info_noop,
  set_socket_thread_owner_noop
};

struct PSI_socket_bootstrap *psi_socket_hook= NULL;
PSI_socket_service_t *psi_socket_service= & psi_socket_noop;

void set_psi_socket_service(PSI_socket_service_t *psi)
{
  psi_socket_service= psi;
}

// ===========================================================================

static PSI_table_share*
get_table_share_noop(bool temporary NNN, struct TABLE_SHARE *share NNN)
{
  return NULL;
}

static void release_table_share_noop(PSI_table_share* share NNN)
{
  return;
}

static void
drop_table_share_noop(bool temporary NNN, const char *schema_name NNN,
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

static void close_table_noop(struct TABLE_SHARE *share NNN,
                             PSI_table *table NNN)
{
  return;
}

static struct PSI_table_locker*
start_table_io_wait_noop(struct PSI_table_locker_state *state NNN,
                         struct PSI_table *table NNN,
                         enum PSI_table_io_operation op NNN,
                         uint index NNN,
                         const char *src_file NNN, uint src_line NNN)
{
  return NULL;
}

static void end_table_io_wait_noop(PSI_table_locker* locker NNN,
                                   ulonglong numrows NNN)
{
  return;
}

static struct PSI_table_locker*
start_table_lock_wait_noop(struct PSI_table_locker_state *state NNN,
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

static void unlock_table_noop(PSI_table *table NNN)
{
  return;
}

static PSI_table_service_t psi_table_noop=
{
  get_table_share_noop,
  release_table_share_noop,
  drop_table_share_noop,
  open_table_noop,
  unbind_table_noop,
  rebind_table_noop,
  close_table_noop,
  start_table_io_wait_noop,
  end_table_io_wait_noop,
  start_table_lock_wait_noop,
  end_table_lock_wait_noop,
  unlock_table_noop
};

struct PSI_table_bootstrap *psi_table_hook= NULL;
PSI_table_service_t *psi_table_service= & psi_table_noop;

void set_psi_table_service(PSI_table_service_t *psi)
{
  psi_table_service= psi;
}

// ===========================================================================

static PSI_metadata_lock *
create_metadata_lock_noop(void *identity NNN,
                          const MDL_key *mdl_key NNN,
                          opaque_mdl_type mdl_type NNN,
                          opaque_mdl_duration mdl_duration NNN,
                          opaque_mdl_status mdl_status NNN,
                          const char *src_file NNN,
                          uint src_line NNN)
{
  return NULL;
}

static void
set_metadata_lock_status_noop(PSI_metadata_lock* lock NNN,
                              opaque_mdl_status mdl_status NNN)
{
}

static void
destroy_metadata_lock_noop(PSI_metadata_lock* lock NNN)
{
}

static PSI_metadata_locker *
start_metadata_wait_noop(PSI_metadata_locker_state *state NNN,
                         PSI_metadata_lock *mdl NNN,
                         const char *src_file NNN,
                         uint src_line NNN)
{
  return NULL;
}

static void
end_metadata_wait_noop(PSI_metadata_locker *locker NNN,
                       int rc NNN)
{
}

static PSI_mdl_service_t psi_mdl_noop=
{
  create_metadata_lock_noop,
  set_metadata_lock_status_noop,
  destroy_metadata_lock_noop,
  start_metadata_wait_noop,
  end_metadata_wait_noop
};

struct PSI_mdl_bootstrap *psi_mdl_hook= NULL;
PSI_mdl_service_t *psi_mdl_service= & psi_mdl_noop;

void set_psi_mdl_service(PSI_mdl_service_t *psi)
{
  psi_mdl_service= psi;
}

// ===========================================================================

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

static PSI_idle_service_t psi_idle_noop=
{
  start_idle_wait_noop,
  end_idle_wait_noop
};

struct PSI_idle_bootstrap *psi_idle_hook= NULL;
PSI_idle_service_t *psi_idle_service= & psi_idle_noop;

void set_psi_idle_service(PSI_idle_service_t *psi)
{
  psi_idle_service= psi;
}

// ===========================================================================

static void register_stage_noop(const char *category NNN,
                                PSI_stage_info **info_array NNN,
                                int count NNN)
{
  return;
}

static PSI_stage_progress*
start_stage_noop(PSI_stage_key key NNN,
                 const char *src_file NNN, int src_line NNN)
{
  return NULL;
}

static PSI_stage_progress*
get_current_stage_progress_noop()
{
  return NULL;
}

static void end_stage_noop(void)
{
  return;
}

static PSI_stage_service_t psi_stage_noop=
{
  register_stage_noop,
  start_stage_noop,
  get_current_stage_progress_noop,
  end_stage_noop
};

struct PSI_stage_bootstrap *psi_stage_hook= NULL;
PSI_stage_service_t *psi_stage_service= & psi_stage_noop;

void set_psi_stage_service(PSI_stage_service_t *psi)
{
  psi_stage_service= psi;
}

// ===========================================================================

static void register_statement_noop(const char *category NNN,
                                    PSI_statement_info *info NNN,
                                    int count NNN)
{
  return;
}

static PSI_statement_locker*
get_thread_statement_locker_noop(PSI_statement_locker_state *state NNN,
                                 PSI_statement_key key NNN,
                                 const void *charset NNN,
                                 PSI_sp_share *sp_share NNN)
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

static PSI_prepared_stmt*
create_prepared_stmt_noop(void *identity NNN, uint stmt_id NNN,
                          PSI_statement_locker *locker NNN,
                          const char *stmt_name NNN, size_t stmt_name_length NNN,
                          const char *name NNN, size_t length NNN)
{
  return NULL;
}

static void
destroy_prepared_stmt_noop(PSI_prepared_stmt *prepared_stmt NNN)
{
  return;
}

static void
reprepare_prepared_stmt_noop(PSI_prepared_stmt *prepared_stmt NNN)
{
  return;
}

static void
execute_prepared_stmt_noop(PSI_statement_locker *locker NNN,
                           PSI_prepared_stmt *prepared_stmt NNN)
{
  return;
}

static struct PSI_digest_locker*
digest_start_noop(PSI_statement_locker *locker NNN)
{
  return NULL;
}

static void
digest_end_noop(PSI_digest_locker *locker NNN,
                const struct sql_digest_storage *digest NNN)
{
  return;
}

static PSI_sp_share*
get_sp_share_noop(uint object_type NNN,
                  const char *schema_name NNN, uint schema_name_length NNN,
                  const char *object_name NNN, uint object_name_length NNN)
{
  return NULL;
}

static void
release_sp_share_noop(PSI_sp_share *sp_share NNN)
{
  return;
}

static PSI_sp_locker*
start_sp_noop(PSI_sp_locker_state *state NNN, PSI_sp_share *sp_share NNN)
{
  return NULL;
}

static void end_sp_noop(PSI_sp_locker *locker NNN)
{
  return;
}

static void
drop_sp_noop(uint object_type NNN,
             const char *schema_name NNN, uint schema_name_length NNN,
             const char *object_name NNN, uint object_name_length NNN)
{
  return;
}

static PSI_statement_service_t psi_statement_noop=
{
  register_statement_noop,
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
  create_prepared_stmt_noop,
  destroy_prepared_stmt_noop,
  reprepare_prepared_stmt_noop,
  execute_prepared_stmt_noop,
  digest_start_noop,
  digest_end_noop,
  get_sp_share_noop,
  release_sp_share_noop,
  start_sp_noop,
  end_sp_noop,
  drop_sp_noop
};

struct PSI_statement_bootstrap *psi_statement_hook= NULL;
PSI_statement_service_t *psi_statement_service= & psi_statement_noop;

void set_psi_statement_service(PSI_statement_service_t *psi)
{
  psi_statement_service= psi;
}

// ===========================================================================

static PSI_transaction_locker*
get_thread_transaction_locker_noop(PSI_transaction_locker_state *state NNN,
                                   const void *xid NNN,
                                   const ulonglong *trxid NNN,
                                   int isolation_level NNN,
                                   bool read_only NNN,
                                   bool autocommit NNN)
{
  return NULL;
}

static void start_transaction_noop(PSI_transaction_locker *locker NNN,
                                   const char *src_file NNN, uint src_line NNN)
{
  return;
}

static void set_transaction_xid_noop(PSI_transaction_locker *locker NNN,
                                     const void *xid NNN,
                                     int xa_state NNN)
{
  return;
}

static void set_transaction_xa_state_noop(PSI_transaction_locker *locker NNN,
                                          int xa_state NNN)
{
  return;
}

static void set_transaction_gtid_noop(PSI_transaction_locker *locker NNN,
                                      const void *sid NNN,
                                      const void *gtid_spec NNN)
{
  return;
}

static void set_transaction_trxid_noop(PSI_transaction_locker *locker NNN,
                                       const ulonglong *trxid NNN)
{
  return;
}

static void inc_transaction_savepoints_noop(PSI_transaction_locker *locker NNN,
                                            ulong count NNN)
{
  return;
}

static void inc_transaction_rollback_to_savepoint_noop(PSI_transaction_locker *locker NNN,
                                                       ulong count NNN)
{
  return;
}

static void inc_transaction_release_savepoint_noop(PSI_transaction_locker *locker NNN,
                                                   ulong count NNN)
{
  return;
}

static void end_transaction_noop(PSI_transaction_locker *locker NNN,
                                 bool commit NNN)
{
  return;
}

static PSI_transaction_service_t psi_transaction_noop=
{
  get_thread_transaction_locker_noop,
  start_transaction_noop,
  set_transaction_xid_noop,
  set_transaction_xa_state_noop,
  set_transaction_gtid_noop,
  set_transaction_trxid_noop,
  inc_transaction_savepoints_noop,
  inc_transaction_rollback_to_savepoint_noop,
  inc_transaction_release_savepoint_noop,
  end_transaction_noop
};

struct PSI_transaction_bootstrap *psi_transaction_hook= NULL;
PSI_transaction_service_t *psi_transaction_service= & psi_transaction_noop;

void set_psi_transaction_service(PSI_transaction_service_t *psi)
{
  psi_transaction_service= psi;
}

// ===========================================================================

static void
log_error_noop(unsigned int error_num NNN, PSI_error_operation error_operation NNN)
{
}


static PSI_error_service_t psi_error_noop=
{
  log_error_noop
};

struct PSI_error_bootstrap *psi_error_hook= NULL;
PSI_error_service_t *psi_error_service= & psi_error_noop;

void set_psi_error_service(PSI_error_service_t *psi)
{
  psi_error_service= psi;
}

// ===========================================================================

static void register_memory_noop(const char *category NNN,
                                 PSI_memory_info *info NNN,
                                 int count NNN)
{
  return;
}

static PSI_memory_key memory_alloc_noop(PSI_memory_key key NNN, size_t size NNN, struct PSI_thread ** owner NNN)
{
  *owner= NULL;
  return PSI_NOT_INSTRUMENTED;
}

static PSI_memory_key memory_realloc_noop(PSI_memory_key key NNN, size_t old_size NNN, size_t new_size NNN, struct PSI_thread ** owner NNN)
{
  *owner= NULL;
  return PSI_NOT_INSTRUMENTED;
}

static PSI_memory_key memory_claim_noop(PSI_memory_key key NNN, size_t size NNN, struct PSI_thread ** owner)
{
  *owner= NULL;
  return PSI_NOT_INSTRUMENTED;
}

static void memory_free_noop(PSI_memory_key key NNN, size_t size NNN, struct PSI_thread * owner NNN)
{
  return;
}

static PSI_memory_service_t psi_memory_noop=
{
  register_memory_noop,
  memory_alloc_noop,
  memory_realloc_noop,
  memory_claim_noop,
  memory_free_noop
};

struct PSI_memory_bootstrap *psi_memory_hook= NULL;
PSI_memory_service_t *psi_memory_service= & psi_memory_noop;

void set_psi_memory_service(PSI_memory_service_t *psi)
{
  psi_memory_service= psi;
}

C_MODE_END

// ===========================================================================

static void register_data_lock_noop(PSI_engine_data_lock_inspector *inspector NNN)
{
  return;
}

static void unregister_data_lock_noop(PSI_engine_data_lock_inspector *inspector NNN)
{
  return;
}

static PSI_data_lock_service_t psi_data_lock_noop=
{
  register_data_lock_noop,
  unregister_data_lock_noop
};

struct PSI_data_lock_bootstrap *psi_data_lock_hook= NULL;
PSI_data_lock_service_t *psi_data_lock_service= & psi_data_lock_noop;

void set_psi_data_lock_service(PSI_data_lock_service_t *psi)
{
  psi_data_lock_service= psi;
}

// ===========================================================================

