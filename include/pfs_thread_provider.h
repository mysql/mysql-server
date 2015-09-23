/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_THREAD_PROVIDER_H
#define PFS_THREAD_PROVIDER_H

/**
  @file include/pfs_thread_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_THREAD_INTERFACE
#ifdef MYSQL_SERVER
#ifndef EMBEDDED_LIBRARY
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "mysql/psi/psi.h"

#define PSI_MUTEX_CALL(M) pfs_ ## M ## _v1
#define PSI_RWLOCK_CALL(M) pfs_ ## M ## _v1
#define PSI_COND_CALL(M) pfs_ ## M ## _v1
#define PSI_THREAD_CALL(M) pfs_ ## M ## _v1

C_MODE_START

void pfs_register_mutex_v1(const char *category,
                           PSI_mutex_info_v1 *info,
                           int count);

void pfs_register_rwlock_v1(const char *category,
                            PSI_rwlock_info_v1 *info,
                            int count);

void pfs_register_cond_v1(const char *category,
                          PSI_cond_info_v1 *info,
                          int count);

void pfs_register_thread_v1(const char *category,
                            PSI_thread_info_v1 *info,
                            int count);

PSI_mutex*
pfs_init_mutex_v1(PSI_mutex_key key, const void *identity);

void pfs_destroy_mutex_v1(PSI_mutex* mutex);

PSI_rwlock*
pfs_init_rwlock_v1(PSI_rwlock_key key, const void *identity);

void pfs_destroy_rwlock_v1(PSI_rwlock* rwlock);

PSI_cond*
pfs_init_cond_v1(PSI_cond_key key, const void *identity);

void pfs_destroy_cond_v1(PSI_cond* cond);

int pfs_spawn_thread_v1(PSI_thread_key key,
                        my_thread_handle *thread, const my_thread_attr_t *attr,
                        void *(*start_routine)(void*), void *arg);

PSI_thread*
pfs_new_thread_v1(PSI_thread_key key, const void *identity, ulonglong processlist_id);

void pfs_set_thread_id_v1(PSI_thread *thread, ulonglong processlist_id);
void pfs_set_thread_THD_v1(PSI_thread *thread, THD *thd);
void pfs_set_thread_os_id_v1(PSI_thread *thread);

PSI_thread*
pfs_get_thread_v1(void);

void pfs_set_thread_user_v1(const char *user, int user_len);

void pfs_set_thread_account_v1(const char *user, int user_len,
                               const char *host, int host_len);

void pfs_set_thread_db_v1(const char* db, int db_len);

void pfs_set_thread_command_v1(int command);

void pfs_set_thread_start_time_v1(time_t start_time);

void pfs_set_thread_state_v1(const char* state);

void pfs_set_connection_type_v1(opaque_vio_type conn_type);

void pfs_set_thread_info_v1(const char* info, uint info_len);

void pfs_set_thread_v1(PSI_thread* thread);

void pfs_delete_current_thread_v1(void);

void pfs_delete_thread_v1(PSI_thread *thread);

PSI_mutex_locker*
pfs_start_mutex_wait_v1(PSI_mutex_locker_state *state,
                        PSI_mutex *mutex, PSI_mutex_operation op,
                        const char *src_file, uint src_line);

PSI_rwlock_locker*
pfs_start_rwlock_rdwait_v1(PSI_rwlock_locker_state *state,
                           PSI_rwlock *rwlock,
                           PSI_rwlock_operation op,
                           const char *src_file, uint src_line);

PSI_rwlock_locker*
pfs_start_rwlock_wrwait_v1(PSI_rwlock_locker_state *state,
                           PSI_rwlock *rwlock,
                           PSI_rwlock_operation op,
                           const char *src_file, uint src_line);

PSI_cond_locker*
pfs_start_cond_wait_v1(PSI_cond_locker_state *state,
                       PSI_cond *cond, PSI_mutex *mutex,
                       PSI_cond_operation op,
                       const char *src_file, uint src_line);

PSI_table_locker*
pfs_start_table_io_wait_v1(PSI_table_locker_state *state,
                           PSI_table *table,
                           PSI_table_io_operation op,
                           uint index,
                           const char *src_file, uint src_line);

PSI_table_locker*
pfs_start_table_lock_wait_v1(PSI_table_locker_state *state,
                             PSI_table *table,
                             PSI_table_lock_operation op,
                             ulong op_flags,
                             const char *src_file, uint src_line);

void pfs_unlock_mutex_v1(PSI_mutex *mutex);

void pfs_unlock_rwlock_v1(PSI_rwlock *rwlock);

void pfs_signal_cond_v1(PSI_cond* cond);

void pfs_broadcast_cond_v1(PSI_cond* cond);

void pfs_end_mutex_wait_v1(PSI_mutex_locker* locker, int rc);

void pfs_end_rwlock_rdwait_v1(PSI_rwlock_locker* locker, int rc);

void pfs_end_rwlock_wrwait_v1(PSI_rwlock_locker* locker, int rc);

void pfs_end_cond_wait_v1(PSI_cond_locker* locker, int rc);

int pfs_set_thread_connect_attrs_v1(const char *buffer, uint length,
                                      const void *from_cs);

C_MODE_END

#endif /* EMBEDDED_LIBRARY */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_THREAD_INTERFACE */

#endif

