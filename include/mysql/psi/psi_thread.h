/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_THREAD_H
#define MYSQL_PSI_THREAD_H

/**
  @file include/mysql/psi/psi_thread.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_thread Thread Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "mysql/components/services/psi_thread_bits.h"

C_MODE_START

/**
  @def PSI_THREAD_VERSION_1
  Performance Schema Thread Interface number for version 1.
  This version is supported.
*/
#define PSI_THREAD_VERSION_1 1

/**
  @def PSI_CURRENT_THREAD_VERSION
  Performance Schema Thread Interface number for the most recent version.
  The most current version is @c PSI_THREAD_VERSION_1
*/
#define PSI_CURRENT_THREAD_VERSION 1

/** Entry point for the performance schema interface. */
struct PSI_thread_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_THREAD_VERSION_1
    @sa PSI_CURRENT_THREAD_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_thread_bootstrap PSI_thread_bootstrap;

#ifdef HAVE_PSI_THREAD_INTERFACE

/**
  Performance Schema Thread Interface, version 1.
  @since PSI_IDLE_VERSION_1
*/
struct PSI_thread_service_v1
{
  /** @sa register_thread_v1_t. */
  register_thread_v1_t register_thread;
  /** @sa spawn_thread_v1_t. */
  spawn_thread_v1_t spawn_thread;
  /** @sa new_thread_v1_t. */
  new_thread_v1_t new_thread;
  /** @sa set_thread_id_v1_t. */
  set_thread_id_v1_t set_thread_id;
  /** @sa set_thread_THD_v1_t. */
  set_thread_THD_v1_t set_thread_THD;
  /** @sa set_thread_os_id_v1_t. */
  set_thread_os_id_v1_t set_thread_os_id;
  /** @sa get_thread_v1_t. */
  get_thread_v1_t get_thread;
  /** @sa set_thread_user_v1_t. */
  set_thread_user_v1_t set_thread_user;
  /** @sa set_thread_account_v1_t. */
  set_thread_account_v1_t set_thread_account;
  /** @sa set_thread_db_v1_t. */
  set_thread_db_v1_t set_thread_db;
  /** @sa set_thread_command_v1_t. */
  set_thread_command_v1_t set_thread_command;
  /** @sa set_connection_type_v1_t. */
  set_connection_type_v1_t set_connection_type;
  /** @sa set_thread_start_time_v1_t. */
  set_thread_start_time_v1_t set_thread_start_time;
  /** @sa set_thread_state_v1_t. */
  set_thread_state_v1_t set_thread_state;
  /** @sa set_thread_info_v1_t. */
  set_thread_info_v1_t set_thread_info;
  /** @sa set_thread_resource_group_v1_t. */
  set_thread_resource_group_v1_t set_thread_resource_group;
  /** @sa set_thread_resource_group_by_id_v1_t. */
  set_thread_resource_group_by_id_v1_t set_thread_resource_group_by_id;
  /** @sa set_thread_v1_t. */
  set_thread_v1_t set_thread;
  /** @sa delete_current_thread_v1_t. */
  delete_current_thread_v1_t delete_current_thread;
  /** @sa delete_thread_v1_t. */
  delete_thread_v1_t delete_thread;
  /** @sa set_thread_connect_attrs_v1_t. */
  set_thread_connect_attrs_v1_t set_thread_connect_attrs;
  /** @sa get_thread_event_id_v1_t. */
  get_thread_event_id_v1_t get_thread_event_id;
  /** @sa get_thread_system_attrs_v1_t. */
  get_thread_system_attrs_v1_t get_thread_system_attrs;
  /** @sa get_thread_system_attrs_by_id_v1_t. */
  get_thread_system_attrs_by_id_v1_t get_thread_system_attrs_by_id;
  /** @sa register_notification_v1_t. */
  register_notification_v1_t register_notification;
  /** @sa unregister_notification_v1_t. */
  unregister_notification_v1_t unregister_notification;
  /** @sa notify_session_connect_v1_t. */
  notify_session_connect_v1_t notify_session_connect;
  /** @sa notify_session_disconnect_v1_t. */
  notify_session_disconnect_v1_t notify_session_disconnect;
  /** @sa notify_session_change_user_v1_t. */
  notify_session_change_user_v1_t notify_session_change_user;
};

typedef struct PSI_thread_service_v1 PSI_thread_service_t;

extern MYSQL_PLUGIN_IMPORT PSI_thread_service_t *psi_thread_service;

#endif /* HAVE_PSI_THREAD_INTERFACE */

/** @} (end of group psi_abi_thread) */

C_MODE_END

#endif /* MYSQL_PSI_THREAD_H */
