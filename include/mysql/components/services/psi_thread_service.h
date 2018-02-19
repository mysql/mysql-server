/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENTS_SERVICES_PSI_THREAD_SERVICE_H
#define COMPONENTS_SERVICES_PSI_THREAD_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/psi_thread_bits.h>

BEGIN_SERVICE_DEFINITION(psi_thread_v1)
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
END_SERVICE_DEFINITION(psi_thread_v1)

#define REQUIRES_PSI_THREAD_SERVICE REQUIRES_SERVICE(psi_thread_v1)

#endif /* COMPONENTS_SERVICES_PSI_THREAD_SERVICE_H */
