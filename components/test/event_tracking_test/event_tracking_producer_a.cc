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

#include "event_tracking_producer.h"

#include <cstring>
#include <iostream>
#include <string>

#include "mysql/components/my_service.h"
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
#include "mysql/components/services/registry.h"

#define CSTRING_WITH_LENGTH(x) \
  { x, x ? strlen(x) : 0 }
#define EVENT_NAME(x) #x

extern REQUIRES_SERVICE_PLACEHOLDER(registry);

namespace event_tracking_producer {

const char *component_name = "event_tracking_producer_a";
Event_producer *g_event_producer = nullptr;

void print_info(const std::string &event) {
  std::cout << "-------------------------------------------------------------"
            << std::endl;
  std::cout << "Component: " << event_tracking_producer::component_name
            << ". Event : " << event << "." << std::endl;
}

void print_result(const std::string &event, bool result) {
  std::string retval = result ? "Error." : "Success.";
  std::cout << "Component: " << event_tracking_producer::component_name
            << ". Event: " << event << ". Consumer returned: " << retval
            << std::endl;
  std::cout << "-------------------------------------------------------------"
            << std::endl;
}

bool Event_producer::generate_events() {
  auto generate_event = [](auto &service, auto data,
                           const char *event_name) -> bool {
    bool result = true;
    print_info(event_name);
    result = service->notify(data);
    print_result(event_name, result);
    return result;
  };

  /* Generate CONNECTION events */
  {
    mysql_event_tracking_connection_data connection_data;
    connection_data.connection_id = 1;
    connection_data.connection_type = 0;
    connection_data.database = CSTRING_WITH_LENGTH("demo");
    connection_data.external_user = CSTRING_WITH_LENGTH("external_user");
    connection_data.host = CSTRING_WITH_LENGTH("example.com");
    connection_data.ip = CSTRING_WITH_LENGTH("10.10.10.10");
    connection_data.priv_user = CSTRING_WITH_LENGTH("priv_user");
    connection_data.proxy_user = CSTRING_WITH_LENGTH("proxy_user");
    connection_data.status = 0;
    connection_data.user = CSTRING_WITH_LENGTH("user");

    my_service<SERVICE_TYPE(event_tracking_connection)> connection_service(
        "event_tracking_connection", mysql_service_registry);

    if (connection_service.is_valid()) {
      connection_data.event_subclass =
          EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE;
      if (generate_event(
              connection_service, &connection_data,
              EVENT_NAME(EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE)))
        return true;

      connection_data.event_subclass = EVENT_TRACKING_CONNECTION_CONNECT;
      if (generate_event(connection_service, &connection_data,
                         EVENT_NAME(EVENT_TRACKING_CONNECTION_CONNECT)))
        return true;

      connection_data.event_subclass = EVENT_TRACKING_CONNECTION_CHANGE_USER;
      if (generate_event(connection_service, &connection_data,
                         EVENT_NAME(EVENT_TRACKING_CONNECTION_CHANGE_USER)))
        return true;

      connection_data.event_subclass = EVENT_TRACKING_CONNECTION_DISCONNECT;
      if (generate_event(connection_service, &connection_data,
                         EVENT_NAME(EVENT_TRACKING_CONNECTION_DISCONNECT)))
        return true;
    }
  }

  /* Generate COMMAND events */
  {
    mysql_event_tracking_command_data command_data;
    command_data.command = CSTRING_WITH_LENGTH("COM_QUERY");
    command_data.connection_id = 1;
    command_data.status = 0;

    my_service<SERVICE_TYPE(event_tracking_command)> command_service(
        "event_tracking_command", mysql_service_registry);

    if (command_service.is_valid()) {
      command_data.event_subclass = EVENT_TRACKING_COMMAND_START;
      if (generate_event(command_service, &command_data,
                         EVENT_NAME(EVENT_TRACKING_COMMAND_START)))
        return true;

      command_data.event_subclass = EVENT_TRACKING_COMMAND_END;
      if (generate_event(command_service, &command_data,
                         EVENT_NAME(EVENT_TRACKING_COMMAND_END)))
        return true;
    }
  }

  /* Generate GENERAL events */
  {
    mysql_event_tracking_general_data general_data;
    general_data.connection_id = 1;
    general_data.error_code = 0;
    general_data.host = CSTRING_WITH_LENGTH("example.com");
    general_data.ip = CSTRING_WITH_LENGTH("10.10.10.10");
    general_data.user = CSTRING_WITH_LENGTH("user");

    my_service<SERVICE_TYPE(event_tracking_general)> general_service(
        "event_tracking_general", mysql_service_registry);

    if (general_service.is_valid()) {
      general_data.event_subclass = EVENT_TRACKING_GENERAL_ERROR;
      if (generate_event(general_service, &general_data,
                         EVENT_NAME(EVENT_TRACKING_GENERAL_ERROR)))
        return true;

      general_data.event_subclass = EVENT_TRACKING_GENERAL_LOG;
      if (generate_event(general_service, &general_data,
                         EVENT_NAME(EVENT_TRACKING_GENERAL_LOG)))
        return true;

      general_data.event_subclass = EVENT_TRACKING_GENERAL_RESULT;
      if (generate_event(general_service, &general_data,
                         EVENT_NAME(EVENT_TRACKING_GENERAL_RESULT)))
        return true;

      general_data.event_subclass = EVENT_TRACKING_GENERAL_STATUS;
      if (generate_event(general_service, &general_data,
                         EVENT_NAME(EVENT_TRACKING_GENERAL_STATUS)))
        return true;
    }
  }

  /* Generate QUERY events */
  {
    mysql_event_tracking_query_data query_data;
    query_data.connection_id = 1;
    query_data.query = CSTRING_WITH_LENGTH("SELECT * FROM demodb.demo_table;");
    query_data.query_charset = "UTF8MB4";
    query_data.sql_command = "SQLCOM_SELECT";
    query_data.status = 0;

    my_service<SERVICE_TYPE(event_tracking_query)> query_service(
        "event_tracking_query", mysql_service_registry);

    if (query_service.is_valid()) {
      query_data.event_subclass = EVENT_TRACKING_QUERY_START;
      if (generate_event(query_service, &query_data,
                         EVENT_NAME(EVENT_TRACKING_QUERY_START)))
        return true;

      query_data.event_subclass = EVENT_TRACKING_QUERY_NESTED_START;
      if (generate_event(query_service, &query_data,
                         EVENT_NAME(EVENT_TRACKING_QUERY_NESTED_START)))
        return true;

      query_data.event_subclass = EVENT_TRACKING_QUERY_NESTED_STATUS_END;
      if (generate_event(query_service, &query_data,
                         EVENT_NAME(EVENT_TRACKING_QUERY_NESTED_STATUS_END)))
        return true;

      query_data.event_subclass = EVENT_TRACKING_QUERY_STATUS_END;
      if (generate_event(query_service, &query_data,
                         EVENT_NAME(EVENT_TRACKING_QUERY_STATUS_END)))
        return true;
    }
  }

  /* Generate TABLE ACCESS events */
  {
    mysql_event_tracking_table_access_data table_access_data;
    table_access_data.connection_id = 1;
    table_access_data.table_database = CSTRING_WITH_LENGTH("demodb");
    table_access_data.table_name = CSTRING_WITH_LENGTH("demo_table");

    my_service<SERVICE_TYPE(event_tracking_table_access)> table_access_service(
        "event_tracking_table_access", mysql_service_registry);

    if (table_access_service.is_valid()) {
      table_access_data.event_subclass = EVENT_TRACKING_TABLE_ACCESS_DELETE;
      if (generate_event(table_access_service, &table_access_data,
                         EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_DELETE)))
        return true;

      table_access_data.event_subclass = EVENT_TRACKING_TABLE_ACCESS_INSERT;
      if (generate_event(table_access_service, &table_access_data,
                         EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_INSERT)))
        return true;

      table_access_data.event_subclass = EVENT_TRACKING_TABLE_ACCESS_READ;
      if (generate_event(table_access_service, &table_access_data,
                         EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_READ)))
        return true;

      table_access_data.event_subclass = EVENT_TRACKING_TABLE_ACCESS_UPDATE;
      if (generate_event(table_access_service, &table_access_data,
                         EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_UPDATE)))
        return true;
    }
  }

  /* Generate AUTHENTICATION events */
  {
    mysql_event_tracking_authentication_data authentication_data;
    authentication_data.connection_id = 1;
    authentication_data.host = CSTRING_WITH_LENGTH("example.com");
    authentication_data.status = 1;
    authentication_data.user = CSTRING_WITH_LENGTH("user");

    my_service<SERVICE_TYPE(event_tracking_authentication)>
        authentication_service("event_tracking_authentication",
                               mysql_service_registry);

    if (authentication_service.is_valid()) {
      authentication_data.event_subclass =
          EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE;
      if (generate_event(
              authentication_service, &authentication_data,
              EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE)))
        return true;

      authentication_data.event_subclass =
          EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP;
      if (generate_event(authentication_service, &authentication_data,
                         EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP)))
        return true;

      authentication_data.event_subclass =
          EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME;
      if (generate_event(
              authentication_service, &authentication_data,
              EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME)))
        return true;

      authentication_data.event_subclass =
          EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE;
      if (generate_event(
              authentication_service, &authentication_data,
              EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE)))
        return true;

      authentication_data.event_subclass = EVENT_TRACKING_AUTHENTICATION_FLUSH;
      if (generate_event(authentication_service, &authentication_data,
                         EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_FLUSH)))
        return true;
    }
  }

  /* Generate LIFECYCLE events */
  {
    mysql_event_tracking_startup_data startup_data;
    startup_data.argc = 1;
    startup_data.argv = nullptr;

    my_service<SERVICE_TYPE(event_tracking_lifecycle)> lifecycle_service(
        "event_tracking_lifecycle", mysql_service_registry);

    mysql_event_tracking_shutdown_data shutdown_data;
    if (lifecycle_service.is_valid()) {
      bool result;
      startup_data.event_subclass = EVENT_TRACKING_STARTUP_STARTUP;
      print_info(EVENT_NAME(EVENT_TRACKING_STARTUP_STARTUP));
      result = lifecycle_service->notify_startup(&startup_data);
      print_result(EVENT_NAME(EVENT_TRACKING_STARTUP_STARTUP), result);
      if (result) return true;

      shutdown_data.exit_code = 0;
      shutdown_data.reason = EVENT_TRACKING_SHUTDOWN_REASON_SHUTDOWN;
      shutdown_data.event_subclass = EVENT_TRACKING_SHUTDOWN_SHUTDOWN;
      print_info(EVENT_NAME(EVENT_TRACKING_SHUTDOWN_SHUTDOWN));
      result = lifecycle_service->notify_shutdown(&shutdown_data);
      print_result(EVENT_NAME(EVENT_TRACKING_SHUTDOWN_SHUTDOWN), result);
      if (result) return true;

      shutdown_data.exit_code = 1;
      shutdown_data.reason = EVENT_TRACKING_SHUTDOWN_REASON_ABORT;
      print_info(EVENT_NAME(EVENT_TRACKING_SHUTDOWN_SHUTDOWN));
      result = lifecycle_service->notify_shutdown(&shutdown_data);
      print_result(EVENT_NAME(EVENT_TRACKING_SHUTDOWN_SHUTDOWN), result);
      if (result) return true;
    }
  }

  /* Generate MESSAGE events */
  {
    mysql_event_tracking_message_data message_data;
    message_data.component =
        CSTRING_WITH_LENGTH("component_test_event_tracking_producer_a");
    message_data.connection_id = 1;
    message_data.key_value_map = nullptr;
    message_data.key_value_map_length = 0;
    message_data.message = CSTRING_WITH_LENGTH("test_message");
    message_data.producer = CSTRING_WITH_LENGTH("event_tracking_producer_a");

    my_service<SERVICE_TYPE(event_tracking_message)> message_service(
        "event_tracking_message", mysql_service_registry);

    if (message_service.is_valid()) {
      message_data.event_subclass = EVENT_TRACKING_MESSAGE_INTERNAL;
      if (generate_event(message_service, &message_data,
                         EVENT_NAME(EVENT_TRACKING_MESSAGE_INTERNAL)))
        return true;

      message_data.event_subclass = EVENT_TRACKING_MESSAGE_USER;
      if (generate_event(message_service, &message_data,
                         EVENT_NAME(EVENT_TRACKING_MESSAGE_INTERNAL)))
        return true;
    }
  }

  /* Generate PARSE events */
  {
    mysql_event_tracking_parse_data parse_data;
    parse_data.connection_id = 1;
    parse_data.flags = nullptr;
    parse_data.query = CSTRING_WITH_LENGTH("SELECT * FROM demodb.demo_table");

    my_service<SERVICE_TYPE(event_tracking_parse)> parse_service(
        "event_tracking_parse", mysql_service_registry);

    if (parse_service.is_valid()) {
      parse_data.event_subclass = EVENT_TRACKING_PARSE_PREPARSE;
      if (generate_event(parse_service, &parse_data,
                         EVENT_NAME(EVENT_TRACKING_PARSE_PREPARSE)))
        return true;

      parse_data.event_subclass = EVENT_TRACKING_PARSE_POSTPARSE;
      if (generate_event(parse_service, &parse_data,
                         EVENT_NAME(EVENT_TRACKING_PARSE_POSTPARSE)))
        return true;
    }
  }

  /* Generate STORED PROGRAM events */
  {
    mysql_event_tracking_stored_program_data stored_program_data;
    stored_program_data.connection_id = 1;
    stored_program_data.database = CSTRING_WITH_LENGTH("demodb");
    stored_program_data.name = CSTRING_WITH_LENGTH("demo_program");
    stored_program_data.parameters = nullptr;

    my_service<SERVICE_TYPE(event_tracking_stored_program)>
        stored_program_service("event_tracking_stored_program",
                               mysql_service_registry);

    if (stored_program_service.is_valid()) {
      stored_program_data.event_subclass =
          EVENT_TRACKING_STORED_PROGRAM_EXECUTE;
      if (generate_event(stored_program_service, &stored_program_data,
                         EVENT_NAME(EVENT_TRACKING_STORED_PROGRAM_EXECUTE)))
        return true;
    }
  }

  /* Generate GLOBAL VARIABLE events */
  {
    mysql_event_tracking_global_variable_data global_variable_data;
    global_variable_data.connection_id = 1;
    global_variable_data.variable_name = CSTRING_WITH_LENGTH("demo_variable");
    global_variable_data.variable_value = CSTRING_WITH_LENGTH("demo_value");

    my_service<SERVICE_TYPE(event_tracking_global_variable)>
        global_variable_service("event_tracking_global_variable",
                                mysql_service_registry);

    if (global_variable_service.is_valid()) {
      global_variable_data.sql_command = "SQLCOM_SELECT";
      global_variable_data.event_subclass = EVENT_TRACKING_GLOBAL_VARIABLE_GET;
      if (generate_event(global_variable_service, &global_variable_data,
                         EVENT_NAME(EVENT_TRACKING_GLOBAL_VARIABLE_GET)))
        return true;

      global_variable_data.sql_command = "SQLCOM_SET";
      global_variable_data.event_subclass = EVENT_TRACKING_GLOBAL_VARIABLE_SET;
      if (generate_event(global_variable_service, &global_variable_data,
                         EVENT_NAME(EVENT_TRACKING_GLOBAL_VARIABLE_SET)))
        return true;
    }
  }

  return false;
}

static mysql_service_status_t init() {
  g_event_producer = new (std::nothrow) Event_producer();
  if (!g_event_producer || g_event_producer->generate_events()) return true;
  return false;
}

static mysql_service_status_t deinit() {
  if (g_event_producer) delete g_event_producer;
  g_event_producer = nullptr;
  return false;
}
}  // namespace event_tracking_producer

BEGIN_COMPONENT_PROVIDES(event_tracking_producer_a)
/* Nothing */
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(event_tracking_producer_a)
/* Nothing */
END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(event_tracking_producer_a)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(event_tracking_producer_a,
                  event_tracking_producer::component_name)
event_tracking_producer::init,
    event_tracking_producer::deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(event_tracking_producer_a)
    END_DECLARE_LIBRARY_COMPONENTS
