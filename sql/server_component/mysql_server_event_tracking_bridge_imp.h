/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_SERVER_COMPONENT_MYSQL_SERVER_EVENT_TRACKING_BRIDGE_IMP
#define SQL_SERVER_COMPONENT_MYSQL_SERVER_EVENT_TRACKING_BRIDGE_IMP

#include <cstddef>
#include <string>
#include <unordered_map>

#include "mysql/components/service_implementation.h"
#include "mysql/components/services/event_tracking_authentication_service.h"
#include "mysql/components/services/event_tracking_command_service.h"
#include "mysql/components/services/event_tracking_connection_service.h"
#include "mysql/components/services/event_tracking_general_service.h"
#include "mysql/components/services/event_tracking_global_variable_service.h"
#include "mysql/components/services/event_tracking_lifecycle_service.h"
#include "mysql/components/services/event_tracking_message_service.h"
#include "mysql/components/services/event_tracking_parse_service.h"
#include "mysql/components/services/event_tracking_query_service.h"
#include "mysql/components/services/event_tracking_stored_program_service.h"
#include "mysql/components/services/event_tracking_table_access_service.h"
#include "mysql/plugin_audit.h"

#include "sql/reference_caching_setup.h"
#include "sql/sql_event_tracking_to_audit_event_mapping.h"

extern SERVICE_TYPE(event_tracking_authentication) *
    srv_event_tracking_authentication;
extern SERVICE_TYPE(event_tracking_command) * srv_event_tracking_command;
extern SERVICE_TYPE(event_tracking_connection) * srv_event_tracking_connection;
extern SERVICE_TYPE(event_tracking_general) * srv_event_tracking_general;
extern SERVICE_TYPE(event_tracking_global_variable) *
    srv_event_tracking_global_variable;
extern SERVICE_TYPE(event_tracking_lifecycle) * srv_event_tracking_lifecycle;
extern SERVICE_TYPE(event_tracking_message) * srv_event_tracking_message;
extern SERVICE_TYPE(event_tracking_parse) * srv_event_tracking_parse;
extern SERVICE_TYPE(event_tracking_query) * srv_event_tracking_query;
extern SERVICE_TYPE(event_tracking_stored_program) *
    srv_event_tracking_stored_program;
extern SERVICE_TYPE(event_tracking_table_access) *
    srv_event_tracking_table_access;

class Event_general_bridge_implementation final {
 public:
  /**
    Process an general audit event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
*/
  static DEFINE_BOOL_METHOD(notify,
                            (const mysql_event_tracking_general_data *data));
};

class Event_connection_bridge_implementation final {
 public:
  /**
  Process a connection event

  @param [in] data   Event specific data

  @returns Status of processing the event
    @retval false Success
    @retval true  Error
*/
  static DEFINE_BOOL_METHOD(notify,
                            (const mysql_event_tracking_connection_data *data));
};

class Event_parse_bridge_implementation final {
 public:
  /**
  Process a parse event

  @param [in] data   Event specific data

  @returns Status of processing the event
    @retval false Success
    @retval true  Error
*/
  static DEFINE_BOOL_METHOD(notify, (mysql_event_tracking_parse_data * data));
};

class Event_table_access_bridge_implementation final {
 public:
  /**
  Process a table access event

  @param [in] data   Event specific data

  @returns Status of processing the event
    @retval false Success
    @retval true  Error
*/
  static DEFINE_BOOL_METHOD(
      notify, (const mysql_event_tracking_table_access_data *data));
};

class Event_global_variable_bridge_implementation {
 public:
  /**
    Process a global_variables event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(
      notify, (const mysql_event_tracking_global_variable_data *data));
};

class Event_lifecycle_bridge_implementation final {
 public:
  /**
    Process a start-up event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(notify_startup,
                            (const mysql_event_tracking_startup_data *data));

  /**
    Process a shutdown event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(notify_shutdown,
                            (const mysql_event_tracking_shutdown_data *data));
};

class Event_command_bridge_implementation final {
 public:
  /**
    Process a command event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(notify,
                            (const mysql_event_tracking_command_data *data));
};

class Event_query_bridge_implementation final {
 public:
  /**
    Process a query event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(notify,
                            (const mysql_event_tracking_query_data *data));
};

class Event_stored_program_bridge_implementation final {
 public:
  /**
    Process a stored program event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(
      notify, (const mysql_event_tracking_stored_program_data *data));
};

class Event_authentication_bridge_implementation final {
 public:
  /**
    Process a authentication event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(
      notify, (const mysql_event_tracking_authentication_data *data));
};

class Event_message_bridge_implementation final {
 public:
  /**
    Process a message event

    @param [in] data   Event specific data

    @returns Status of processing the event
      @retval false Success
      @retval true  Error
  */
  static DEFINE_BOOL_METHOD(notify,
                            (const mysql_event_tracking_message_data *data));
};
#endif  // !SQL_SERVER_COMPONENT_MYSQL_SERVER_EVENT_TRACKING_BRIDGE_IMP
