
/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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
#include <mysql/components/services/bits/psi_thread_bits.h>

/*
  Version 1.
  Introduced in MySQL 8.0.3
  Deprecated in MySQL 8.0.15
  Abandoned in MySQL 8.0.17
  Status: Removed.
*/

/*
  Version 2.
  Introduced in MySQL 8.0.15
  Abandoned in MySQL 8.0.17
  Status: Removed.
*/

/*
  Version 3.
  Introduced in MySQL 8.0.17
  Abandoned in MySQL 8.0.22
  Status: Removed.
*/

/*
  Version 4.
  Introduced in MySQL 8.0.22
  Deprecated in MySQL 8.0.25
  Status: deprecated, use version 5 instead.
*/

BEGIN_SERVICE_DEFINITION(psi_thread_v4)
/** @sa register_thread_v1_t. */
register_thread_v1_t register_thread;
/** @sa spawn_thread_v1_t. */
spawn_thread_v1_t spawn_thread;
/** @sa new_thread_v1_t. */
new_thread_v1_t new_thread;
/** @sa set_thread_id_v1_t. */
set_thread_id_v1_t set_thread_id;
/** @sa get_current_thread_internal_id_v2_t. */
get_current_thread_internal_id_v2_t get_current_thread_internal_id;
/** @sa get_thread_internal_id_v2_t. */
get_thread_internal_id_v2_t get_thread_internal_id;
/** @sa get_thread_by_id_v2_t. */
get_thread_by_id_v2_t get_thread_by_id;
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
/** @sa set_thread_info_v1_t. */
set_thread_info_v1_t set_thread_info;
/** @sa set_thread_v1_t. */
set_thread_v1_t set_thread;
/** @sa set_thread_peer_port_v4_t. */
set_thread_peer_port_v4_t set_thread_peer_port;
/** @sa aggregate_thread_status_v1_t. */
aggregate_thread_status_v2_t aggregate_thread_status;
/** @sa delete_current_thread_v1_t. */
delete_current_thread_v1_t delete_current_thread;
/** @sa delete_thread_v1_t. */
delete_thread_v1_t delete_thread;
/** @sa set_thread_connect_attrs_v1_t. */
set_thread_connect_attrs_v1_t set_thread_connect_attrs;
/** @sa get_current_thread_event_id_v2_t. */
get_current_thread_event_id_v2_t get_current_thread_event_id;
/** @sa get_thread_event_id_v2_t. */
get_thread_event_id_v2_t get_thread_event_id;
/** @sa get_thread_system_attrs_v3_t. */
get_thread_system_attrs_v3_t get_thread_system_attrs;
/** @sa get_thread_system_attrs_by_id_v3_t. */
get_thread_system_attrs_by_id_v3_t get_thread_system_attrs_by_id;
/** @sa register_notification_v3_t. */
register_notification_v3_t register_notification;
/** @sa unregister_notification_v1_t. */
unregister_notification_v1_t unregister_notification;
/** @sa notify_session_connect_v1_t. */
notify_session_connect_v1_t notify_session_connect;
/** @sa notify_session_disconnect_v1_t. */
notify_session_disconnect_v1_t notify_session_disconnect;
/** @sa notify_session_change_user_v1_t. */
notify_session_change_user_v1_t notify_session_change_user;
END_SERVICE_DEFINITION(psi_thread_v4)

/*
  Version 5.
  Introduced in MySQL 8.0.25
  Deprecated in MySQL 8.0.28
  Status: deprecated, use version 6 instead.
  Changes compared to version 4:
  - register_thread takes an expanded PSI_thread_info_v5 instrumentation,
    which includes a new m_os_name attribute.
  - spawn_thread takes an extra sequence number parameter
  - new_thread takes an extra sequence number parameter
*/

BEGIN_SERVICE_DEFINITION(psi_thread_v5)
/** @sa register_thread_v5_t. */
register_thread_v5_t register_thread;
/** @sa spawn_thread_v5_t. */
spawn_thread_v5_t spawn_thread;
/** @sa new_thread_v5_t. */
new_thread_v5_t new_thread;
/** @sa set_thread_id_v1_t. */
set_thread_id_v1_t set_thread_id;
/** @sa get_current_thread_internal_id_v2_t. */
get_current_thread_internal_id_v2_t get_current_thread_internal_id;
/** @sa get_thread_internal_id_v2_t. */
get_thread_internal_id_v2_t get_thread_internal_id;
/** @sa get_thread_by_id_v2_t. */
get_thread_by_id_v2_t get_thread_by_id;
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
/** @sa set_thread_info_v1_t. */
set_thread_info_v1_t set_thread_info;
/** @sa set_thread_v1_t. */
set_thread_v1_t set_thread;
/** @sa set_thread_peer_port_v4_t. */
set_thread_peer_port_v4_t set_thread_peer_port;
/** @sa aggregate_thread_status_v1_t. */
aggregate_thread_status_v2_t aggregate_thread_status;
/** @sa delete_current_thread_v1_t. */
delete_current_thread_v1_t delete_current_thread;
/** @sa delete_thread_v1_t. */
delete_thread_v1_t delete_thread;
/** @sa set_thread_connect_attrs_v1_t. */
set_thread_connect_attrs_v1_t set_thread_connect_attrs;
/** @sa get_current_thread_event_id_v2_t. */
get_current_thread_event_id_v2_t get_current_thread_event_id;
/** @sa get_thread_event_id_v2_t. */
get_thread_event_id_v2_t get_thread_event_id;
/** @sa get_thread_system_attrs_v3_t. */
get_thread_system_attrs_v3_t get_thread_system_attrs;
/** @sa get_thread_system_attrs_by_id_v3_t. */
get_thread_system_attrs_by_id_v3_t get_thread_system_attrs_by_id;
/** @sa register_notification_v3_t. */
register_notification_v3_t register_notification;
/** @sa unregister_notification_v1_t. */
unregister_notification_v1_t unregister_notification;
/** @sa notify_session_connect_v1_t. */
notify_session_connect_v1_t notify_session_connect;
/** @sa notify_session_disconnect_v1_t. */
notify_session_disconnect_v1_t notify_session_disconnect;
/** @sa notify_session_change_user_v1_t. */
notify_session_change_user_v1_t notify_session_change_user;
END_SERVICE_DEFINITION(psi_thread_v5)

/*
  Version 6.
  Introduced in MySQL 8.0.28
  Deprecated in MySQL 8.0.33
  Status: deprecated, use version 7 instead.
  Changes compared to version 5:
  - added set_thread_secondary_engine
*/

BEGIN_SERVICE_DEFINITION(psi_thread_v6)
/** @sa register_thread_v5_t. */
register_thread_v5_t register_thread;
/** @sa spawn_thread_v5_t. */
spawn_thread_v5_t spawn_thread;
/** @sa new_thread_v5_t. */
new_thread_v5_t new_thread;
/** @sa set_thread_id_v1_t. */
set_thread_id_v1_t set_thread_id;
/** @sa get_current_thread_internal_id_v2_t. */
get_current_thread_internal_id_v2_t get_current_thread_internal_id;
/** @sa get_thread_internal_id_v2_t. */
get_thread_internal_id_v2_t get_thread_internal_id;
/** @sa get_thread_by_id_v2_t. */
get_thread_by_id_v2_t get_thread_by_id;
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
/** @sa set_thread_info_v1_t. */
set_thread_info_v1_t set_thread_info;
/** @sa set_thread_secondary_engine_v6_t. */
set_thread_secondary_engine_v6_t set_thread_secondary_engine;
/** @sa set_thread_v1_t. */
set_thread_v1_t set_thread;
/** @sa set_thread_peer_port_v4_t. */
set_thread_peer_port_v4_t set_thread_peer_port;
/** @sa aggregate_thread_status_v1_t. */
aggregate_thread_status_v2_t aggregate_thread_status;
/** @sa delete_current_thread_v1_t. */
delete_current_thread_v1_t delete_current_thread;
/** @sa delete_thread_v1_t. */
delete_thread_v1_t delete_thread;
/** @sa set_thread_connect_attrs_v1_t. */
set_thread_connect_attrs_v1_t set_thread_connect_attrs;
/** @sa get_current_thread_event_id_v2_t. */
get_current_thread_event_id_v2_t get_current_thread_event_id;
/** @sa get_thread_event_id_v2_t. */
get_thread_event_id_v2_t get_thread_event_id;
/** @sa get_thread_system_attrs_v3_t. */
get_thread_system_attrs_v3_t get_thread_system_attrs;
/** @sa get_thread_system_attrs_by_id_v3_t. */
get_thread_system_attrs_by_id_v3_t get_thread_system_attrs_by_id;
/** @sa register_notification_v3_t. */
register_notification_v3_t register_notification;
/** @sa unregister_notification_v1_t. */
unregister_notification_v1_t unregister_notification;
/** @sa notify_session_connect_v1_t. */
notify_session_connect_v1_t notify_session_connect;
/** @sa notify_session_disconnect_v1_t. */
notify_session_disconnect_v1_t notify_session_disconnect;
/** @sa notify_session_change_user_v1_t. */
notify_session_change_user_v1_t notify_session_change_user;
END_SERVICE_DEFINITION(psi_thread_v6)

/*
  Version 7.
  Introduced in MySQL 8.0.33
  Status: active
  Changes compared to version 6:
  - added detect_telemetry, abort_telemetry
*/

BEGIN_SERVICE_DEFINITION(psi_thread_v7)
/** @sa register_thread_v5_t. */
register_thread_v5_t register_thread;
/** @sa spawn_thread_v5_t. */
spawn_thread_v5_t spawn_thread;
/** @sa new_thread_v5_t. */
new_thread_v5_t new_thread;
/** @sa set_thread_id_v1_t. */
set_thread_id_v1_t set_thread_id;
/** @sa get_current_thread_internal_id_v2_t. */
get_current_thread_internal_id_v2_t get_current_thread_internal_id;
/** @sa get_thread_internal_id_v2_t. */
get_thread_internal_id_v2_t get_thread_internal_id;
/** @sa get_thread_by_id_v2_t. */
get_thread_by_id_v2_t get_thread_by_id;
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
/** @sa set_thread_info_v1_t. */
set_thread_info_v1_t set_thread_info;
/** @sa set_thread_secondary_engine_v6_t. */
set_thread_secondary_engine_v6_t set_thread_secondary_engine;
/** @sa set_thread_v1_t. */
set_thread_v1_t set_thread;
/** @sa set_thread_peer_port_v4_t. */
set_thread_peer_port_v4_t set_thread_peer_port;
/** @sa aggregate_thread_status_v1_t. */
aggregate_thread_status_v2_t aggregate_thread_status;
/** @sa delete_current_thread_v1_t. */
delete_current_thread_v1_t delete_current_thread;
/** @sa delete_thread_v1_t. */
delete_thread_v1_t delete_thread;
/** @sa set_thread_connect_attrs_v1_t. */
set_thread_connect_attrs_v1_t set_thread_connect_attrs;
/** @sa get_current_thread_event_id_v2_t. */
get_current_thread_event_id_v2_t get_current_thread_event_id;
/** @sa get_thread_event_id_v2_t. */
get_thread_event_id_v2_t get_thread_event_id;
/** @sa get_thread_system_attrs_v3_t. */
get_thread_system_attrs_v3_t get_thread_system_attrs;
/** @sa get_thread_system_attrs_by_id_v3_t. */
get_thread_system_attrs_by_id_v3_t get_thread_system_attrs_by_id;
/** @sa register_notification_v3_t. */
register_notification_v3_t register_notification;
/** @sa unregister_notification_v1_t. */
unregister_notification_v1_t unregister_notification;
/** @sa notify_session_connect_v1_t. */
notify_session_connect_v1_t notify_session_connect;
/** @sa notify_session_disconnect_v1_t. */
notify_session_disconnect_v1_t notify_session_disconnect;
/** @sa notify_session_change_user_v1_t. */
notify_session_change_user_v1_t notify_session_change_user;

thread_detect_telemetry_v7_t detect_telemetry;
thread_abort_telemetry_v7_t abort_telemetry;
END_SERVICE_DEFINITION(psi_thread_v7)

#endif /* COMPONENTS_SERVICES_PSI_THREAD_SERVICE_H */
