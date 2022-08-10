/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_THREAD_PROVIDER_H
#define PFS_THREAD_PROVIDER_H

/**
  @file include/pfs_thread_provider.h
  Performance schema instrumentation (declarations).
*/

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#ifdef HAVE_PSI_THREAD_INTERFACE
#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
#ifndef MYSQL_DYNAMIC_PLUGIN
#ifndef WITH_LOCK_ORDER

#include <sys/types.h>
#include <time.h>

#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/psi/psi_thread.h"

#ifdef __cplusplus
class THD;
#endif /* __cplusplus */

/*
  Naming current apis as _vc (version 'current'),
  to avoid changing the names every time
  psi_thread_v<N> is replaced by psi_thread_v<N+1>.
*/

#define PSI_THREAD_CALL(M) pfs_##M##_vc

void pfs_register_thread_vc(const char *category, PSI_thread_info *info,
                            int count);

int pfs_spawn_thread_vc(PSI_thread_key key, unsigned int sequence_number,
                        my_thread_handle *thread, const my_thread_attr_t *attr,
                        void *(*start_routine)(void *), void *arg);

PSI_thread *pfs_new_thread_vc(PSI_thread_key key, unsigned int sequence_number,
                              const void *identity, ulonglong processlist_id);

void pfs_set_thread_id_vc(PSI_thread *thread, ulonglong processlist_id);

ulonglong pfs_get_current_thread_internal_id_vc();

ulonglong pfs_get_thread_internal_id_vc(PSI_thread *thread);

PSI_thread *pfs_get_thread_by_id_vc(ulonglong processlist_id);

#ifdef __cplusplus
void pfs_set_thread_THD_vc(PSI_thread *thread, THD *thd);
#endif /* __cplusplus */

void pfs_set_thread_os_id_vc(PSI_thread *thread);

PSI_thread *pfs_get_thread_vc(void);

void pfs_set_mem_cnt_THD_vc(THD *thd, THD **backup_thd);

void pfs_set_thread_user_vc(const char *user, int user_len);

void pfs_set_thread_account_vc(const char *user, int user_len, const char *host,
                               int host_len);

void pfs_set_thread_db_vc(const char *db, int db_len);

void pfs_set_thread_command_vc(int command);

void pfs_set_thread_start_time_vc(time_t start_time);

void pfs_set_thread_state_vc(const char *state);

void pfs_set_connection_type_vc(opaque_vio_type conn_type);

void pfs_set_thread_info_vc(const char *info, uint info_len);

void pfs_set_thread_secondary_engine_vc(bool secondary);

int pfs_set_thread_resource_group_vc(const char *group_name, int group_name_len,
                                     void *user_data);

int pfs_set_thread_resource_group_by_id_vc(PSI_thread *thread,
                                           ulonglong thread_id,
                                           const char *group_name,
                                           int group_name_len, void *user_data);

void pfs_set_thread_vc(PSI_thread *thread);

void pfs_set_thread_peer_port_vc(PSI_thread *thread, uint port);

void pfs_aggregate_thread_status_vc(PSI_thread *thread);

void pfs_delete_current_thread_vc(void);

void pfs_delete_thread_vc(PSI_thread *thread);

int pfs_set_thread_connect_attrs_vc(const char *buffer, uint length,
                                    const void *from_cs);

void pfs_get_current_thread_event_id_vc(ulonglong *internal_thread_id,
                                        ulonglong *event_id);

void pfs_get_thread_event_id_vc(PSI_thread *thread,
                                ulonglong *internal_thread_id,
                                ulonglong *event_id);

int pfs_get_thread_system_attrs_vc(PSI_thread_attrs *thread_attrs);

int pfs_get_thread_system_attrs_by_id_vc(PSI_thread *thread,
                                         ulonglong thread_id,
                                         PSI_thread_attrs *thread_attrs);

int pfs_register_notification_vc(const PSI_notification *callbacks,
                                 bool with_ref_count);

int pfs_unregister_notification_vc(int handle);

void pfs_notify_session_connect_vc(PSI_thread *thread);

void pfs_notify_session_disconnect_vc(PSI_thread *thread);

void pfs_notify_session_change_user_vc(PSI_thread *thread);

#endif /* WITH_LOCK_ORDER */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER || PFS_DIRECT_CALL */
#endif /* HAVE_PSI_THREAD_INTERFACE */

#endif
