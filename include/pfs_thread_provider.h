/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/psi/psi_thread.h"

#define PSI_THREAD_CALL(M) pfs_ ## M ## _v1

C_MODE_START

void pfs_register_thread_v1(const char *category,
                            PSI_thread_info_v1 *info,
                            int count);

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

int pfs_set_thread_connect_attrs_v1(const char *buffer, uint length,
                                      const void *from_cs);

C_MODE_END

#endif /* EMBEDDED_LIBRARY */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_THREAD_INTERFACE */

#endif

