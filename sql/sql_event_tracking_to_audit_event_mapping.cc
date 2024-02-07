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

#include "sql_event_tracking_to_audit_event_mapping.h"

#define STRINGIFY(x) #x
#define MAPIFY(x) std::make_pair(x, #x)

const char *plugin_event_names[] = {
    STRINGIFY(MYSQL_AUDIT_AUTHENTICATION_CLASS),
    STRINGIFY(MYSQL_AUDIT_COMMAND_CLASS),
    STRINGIFY(MYSQL_AUDIT_CONNECTION_CLASS),
    STRINGIFY(MYSQL_AUDIT_GENERAL_CLASS),
    STRINGIFY(MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS),
    STRINGIFY(MYSQL_AUDIT_MESSAGE_CLASS),
    STRINGIFY(MYSQL_AUDIT_PARSE_CLASS),
    STRINGIFY(MYSQL_AUDIT_QUERY_CLASS),
    STRINGIFY(MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS),
    STRINGIFY(MYSQL_AUDIT_SERVER_STARTUP_CLASS),
    STRINGIFY(MYSQL_AUDIT_STORED_PROGRAM_CLASS),
    STRINGIFY(MYSQL_AUDIT_TABLE_ACCESS_CLASS),
    nullptr};

const char *event_tracking_names[] = {"event_tracking_authentication",
                                      "event_tracking_command",
                                      "event_tracking_connection",
                                      "event_tracking_general",
                                      "event_tracking_global_variable",
                                      "event_tracking_message",
                                      "event_tracking_parse",
                                      "event_tracking_query",
                                      "event_tracking_lifecycle",
                                      "event_tracking_lifecycle",
                                      "event_tracking_stored_program",
                                      "event_tracking_table_access",
                                      ""};

Singleton_event_tracking_service_to_plugin_mapping
    *Singleton_event_tracking_service_to_plugin_mapping::instance = nullptr;

Singleton_event_tracking_service_to_plugin_mapping::
    Singleton_event_tracking_service_to_plugin_mapping()
    : event_tracking_to_plugin_event_map_() {
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::AUTHENTICATION)] = MYSQL_AUDIT_AUTHENTICATION_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::COMMAND)] = MYSQL_AUDIT_COMMAND_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::CONNECTION)] = MYSQL_AUDIT_CONNECTION_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::GENERAL)] = MYSQL_AUDIT_GENERAL_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::GLOBAL_VARIABLE)] =
      MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::MESSAGE)] = MYSQL_AUDIT_MESSAGE_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::PARSE)] = MYSQL_AUDIT_PARSE_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::QUERY)] = MYSQL_AUDIT_QUERY_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::SHUTDOWN)] = MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::STARTUP)] = MYSQL_AUDIT_SERVER_STARTUP_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::STORED_PROGRAM)] = MYSQL_AUDIT_STORED_PROGRAM_CLASS;
  event_tracking_to_plugin_event_map_[static_cast<size_t>(
      Event_tracking_class::TABLE_ACCESS)] = MYSQL_AUDIT_TABLE_ACCESS_CLASS;
}

mysql_event_class_t
Singleton_event_tracking_service_to_plugin_mapping::plugin_event_class(
    Event_tracking_class event_tracking_class) {
  try {
    return event_tracking_to_plugin_event_map_.at(
        static_cast<size_t>(event_tracking_class));
  } catch (...) {
    return MYSQL_AUDIT_CLASS_MASK_SIZE;
  }
}