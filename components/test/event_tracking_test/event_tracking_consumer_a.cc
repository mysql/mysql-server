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

#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"

#include <cstring>
#include <iostream>
#include <string>

#include "mysql/components/util/event_tracking/event_tracking_authentication_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_command_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_connection_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_general_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_global_variable_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_lifecycle_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_message_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_parse_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_query_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_stored_program_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_table_access_consumer_helper.h"

#define EVENT_NAME(x) #x

namespace Event_tracking_consumer_a {
const char *component_name = "event_tracking_consumer_a";

static mysql_service_status_t init() { return false; }

static mysql_service_status_t deinit() { return false; }
}  // namespace Event_tracking_consumer_a

namespace Event_tracking_implementation {

void print_info(const std::string &event, const std::string &event_data) {
  std::cout << "Component: " << Event_tracking_consumer_a::component_name
            << ". Event : " << event << ". Data : " << event_data << "."
            << std::endl;
}

/* START: Service Implementation */

mysql_event_tracking_authentication_subclass_t
    Event_tracking_authentication_implementation::filtered_sub_events = 0;
bool Event_tracking_authentication_implementation::callback(
    const mysql_event_tracking_authentication_data *data) {
  std::string event_name{}, event_data{"["};

  event_data += " User: ";
  event_data += std::string{data->user.str, data->user.length};
  event_data += ", Host: ";
  event_data += std::string{data->host.str, data->host.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_AUTHENTICATION_FLUSH: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_FLUSH));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE: {
      event_name.assign(
          EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE: {
      event_name.assign(
          EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME: {
      event_name.assign(
          EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_command_subclass_t
    Event_tracking_command_implementation::filtered_sub_events = 0;
bool Event_tracking_command_implementation::callback(
    const mysql_event_tracking_command_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " Command: ";
  event_data += std::string{data->command.str, data->command.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_COMMAND_START: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_COMMAND_START));
      break;
    }
    case EVENT_TRACKING_COMMAND_END: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_COMMAND_END));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_connection_subclass_t
    Event_tracking_connection_implementation::filtered_sub_events = 0;
bool Event_tracking_connection_implementation::callback(
    const mysql_event_tracking_connection_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " User: ";
  event_data += std::string{data->user.str, data->user.length};
  event_data += ", Host: ";
  event_data += std::string{data->host.str, data->host.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_CONNECTION_CONNECT: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_CONNECTION_CONNECT));
      break;
    }
    case EVENT_TRACKING_CONNECTION_DISCONNECT: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_CONNECTION_CONNECT));
      break;
    }
    case EVENT_TRACKING_CONNECTION_CHANGE_USER: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_CONNECTION_CHANGE_USER));
      break;
    }
    case EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE));
      break;
    }
    default:
      break;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_general_subclass_t
    Event_tracking_general_implementation::filtered_sub_events = 0;
bool Event_tracking_general_implementation::callback(
    const mysql_event_tracking_general_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " User: ";
  event_data += std::string{data->user.str, data->user.length};
  event_data += ", Host: ";
  event_data += std::string{data->host.str, data->host.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_GENERAL_LOG: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GENERAL_LOG));
      break;
    }
    case EVENT_TRACKING_GENERAL_ERROR: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GENERAL_ERROR));
      break;
    }
    case EVENT_TRACKING_GENERAL_RESULT: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GENERAL_RESULT));
      break;
    }
    case EVENT_TRACKING_GENERAL_STATUS: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GENERAL_STATUS));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_global_variable_subclass_t
    Event_tracking_global_variable_implementation::filtered_sub_events = 0;
bool Event_tracking_global_variable_implementation::callback(
    const mysql_event_tracking_global_variable_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += "Name: ";
  event_data +=
      std::string{data->variable_name.str, data->variable_name.length};
  event_data += ", Value: ";
  event_data +=
      std::string{data->variable_value.str, data->variable_value.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_GLOBAL_VARIABLE_GET: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GLOBAL_VARIABLE_GET));
      break;
    }
    case EVENT_TRACKING_GLOBAL_VARIABLE_SET: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GLOBAL_VARIABLE_SET));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_startup_subclass_t
    Event_tracking_lifecycle_implementation::startup_filtered_sub_events = 0;
bool Event_tracking_lifecycle_implementation::callback(
    const mysql_event_tracking_startup_data *data [[maybe_unused]]) {
  std::string event_name{}, event_data{"["};
  event_data += "Number of arguments: ";
  event_data += std::to_string(data->argc);
  switch (data->event_subclass) {
    case EVENT_TRACKING_STARTUP_STARTUP:
      event_name.assign(EVENT_NAME(EVENT_TRACKING_STARTUP_STARTUP));
      break;
    default:
      return true;
  }
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_shutdown_subclass_t
    Event_tracking_lifecycle_implementation::shutdown_filtered_sub_events = 0;
bool Event_tracking_lifecycle_implementation::callback(
    const mysql_event_tracking_shutdown_data *data [[maybe_unused]]) {
  std::string event_name{}, event_data{"["};
  event_data += " Reason: ";
  switch (data->reason) {
    case EVENT_TRACKING_SHUTDOWN_REASON_SHUTDOWN:
      event_data += EVENT_NAME(EVENT_TRACKING_SHUTDOWN_REASON_SHUTDOWN);
      break;
    case EVENT_TRACKING_SHUTDOWN_REASON_ABORT:
      event_data += EVENT_NAME(EVENT_TRACKING_SHUTDOWN_REASON_ABORT);
      break;
    default:
      return true;
  };

  switch (data->event_subclass) {
    case EVENT_TRACKING_SHUTDOWN_SHUTDOWN:
      event_name.assign(EVENT_NAME(EVENT_TRACKING_SHUTDOWN_SHUTDOWN));
      break;
    default:
      return true;
  }
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_message_subclass_t
    Event_tracking_message_implementation::filtered_sub_events = 0;
bool Event_tracking_message_implementation::callback(
    const mysql_event_tracking_message_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " Component: ";
  event_data += std::string{data->component.str, data->component.length};
  event_data += ", Producer: ";
  event_data += std::string{data->producer.str, data->producer.length};
  event_data += ", Message: ";
  event_data += std::string{data->message.str, data->message.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_MESSAGE_INTERNAL: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_MESSAGE_INTERNAL));
      break;
    }
    case EVENT_TRACKING_MESSAGE_USER: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_MESSAGE_USER));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_parse_subclass_t
    Event_tracking_parse_implementation::filtered_sub_events = 0;
bool Event_tracking_parse_implementation::callback(
    mysql_event_tracking_parse_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " Query: ";
  event_data += std::string{data->query.str, data->query.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_PARSE_PREPARSE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_PARSE_PREPARSE));
      break;
    }
    case EVENT_TRACKING_PARSE_POSTPARSE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_PARSE_POSTPARSE));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_query_subclass_t
    Event_tracking_query_implementation::filtered_sub_events = 0;
bool Event_tracking_query_implementation::callback(
    const mysql_event_tracking_query_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " SQL Command: ";
  event_data += data->sql_command;
  event_data += ", Query: ";
  event_data += std::string{data->query.str, data->query.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_QUERY_START: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_START));
      break;
    }
    case EVENT_TRACKING_QUERY_NESTED_START: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_NESTED_START));
      break;
    }
    case EVENT_TRACKING_QUERY_STATUS_END: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_STATUS_END));
      break;
    }
    case EVENT_TRACKING_QUERY_NESTED_STATUS_END: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_NESTED_STATUS_END));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_stored_program_subclass_t
    Event_tracking_stored_program_implementation::filtered_sub_events = 0;
bool Event_tracking_stored_program_implementation::callback(
    const mysql_event_tracking_stored_program_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " Schema: ";
  event_data += std::string{data->database.str, data->database.length};
  event_data += ", Program: ";
  event_data += std::string{data->name.str, data->name.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_STORED_PROGRAM_EXECUTE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_STORED_PROGRAM_EXECUTE));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

mysql_event_tracking_table_access_subclass_t
    Event_tracking_table_access_implementation::filtered_sub_events = 0;
bool Event_tracking_table_access_implementation::callback(
    const mysql_event_tracking_table_access_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += " Schema: ";
  event_data +=
      std::string{data->table_database.str, data->table_database.length};
  event_data += ", Table: ";
  event_data += std::string{data->table_name.str, data->table_name.length};
  switch (data->event_subclass) {
    case EVENT_TRACKING_TABLE_ACCESS_READ: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_READ));
      break;
    }
    case EVENT_TRACKING_TABLE_ACCESS_INSERT: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_INSERT));
      break;
    }
    case EVENT_TRACKING_TABLE_ACCESS_UPDATE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_UPDATE));
      break;
    }
    case EVENT_TRACKING_TABLE_ACCESS_DELETE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_DELETE));
      break;
    }
    default:
      return true;
  };
  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

/* END: Service Implementation */

}  // namespace Event_tracking_implementation

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
IMPLEMENTS_SERVICE_EVENT_TRACKING_AUTHENTICATION(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_COMMAND(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_CONNECTION(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_GENERAL(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_GLOBAL_VARIABLE(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_LIFECYCLE(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_MESSAGE(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_PARSE(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_QUERY(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_STORED_PROGRAM(event_tracking_consumer_a);
IMPLEMENTS_SERVICE_EVENT_TRACKING_TABLE_ACCESS(event_tracking_consumer_a);

/** This component provides following services */
BEGIN_COMPONENT_PROVIDES(event_tracking_consumer_a)
PROVIDES_SERVICE_EVENT_TRACKING_AUTHENTICATION(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_COMMAND(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_CONNECTION(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_GENERAL(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_GLOBAL_VARIABLE(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_LIFECYCLE(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_MESSAGE(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_PARSE(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_QUERY(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_STORED_PROGRAM(event_tracking_consumer_a),
    PROVIDES_SERVICE_EVENT_TRACKING_TABLE_ACCESS(event_tracking_consumer_a),
    END_COMPONENT_PROVIDES();

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(event_tracking_consumer_a)
/* Nothing */
END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(event_tracking_consumer_a)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("event_tracking_consumer_a", "1"), END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(event_tracking_consumer_a,
                  Event_tracking_consumer_a::component_name)
Event_tracking_consumer_a::init,
    Event_tracking_consumer_a::deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(event_tracking_consumer_a)
    END_DECLARE_LIBRARY_COMPONENTS
