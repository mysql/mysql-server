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

#ifndef SQL_SQL_EVENT_TRACKING_TO_AUDIT_EVENT_MAPPING_H
#define SQL_SQL_EVENT_TRACKING_TO_AUDIT_EVENT_MAPPING_H

#include <array>
#include <string>
#include <unordered_map>

#include "mysql/plugin_audit.h"

extern const char *plugin_event_names[];
extern const char *event_tracking_names[];

/**
  Event tracking classes
  If a new event tracking class is introduced,
  this class should be kept in sync.
*/
enum class Event_tracking_class {
  AUTHENTICATION = 0,
  COMMAND,
  CONNECTION,
  GENERAL,
  GLOBAL_VARIABLE,
  MESSAGE,
  PARSE,
  QUERY,
  SHUTDOWN,
  STARTUP,
  STORED_PROGRAM,
  TABLE_ACCESS,
  /* Add entry above this */
  LAST
};

struct EnumHash {
  template <typename T>
  std::size_t operator()(T t) const {
    return static_cast<std::size_t>(t);
  }
};

class Singleton_event_tracking_service_to_plugin_mapping final {
 public:
  static Singleton_event_tracking_service_to_plugin_mapping *create_instance() {
    if (unlikely(instance == nullptr))
      instance = new (std::nothrow)
          Singleton_event_tracking_service_to_plugin_mapping();
    return instance;
  }

  static void remove_instance() { delete instance; }

  Singleton_event_tracking_service_to_plugin_mapping(
      const Singleton_event_tracking_service_to_plugin_mapping &) = delete;
  void operator=(const Singleton_event_tracking_service_to_plugin_mapping &) =
      delete;
  Singleton_event_tracking_service_to_plugin_mapping(
      Singleton_event_tracking_service_to_plugin_mapping &&) = delete;
  void operator=(Singleton_event_tracking_service_to_plugin_mapping &&) =
      delete;

  ~Singleton_event_tracking_service_to_plugin_mapping() {}

  unsigned long plugin_sub_event(unsigned long subevent) { return subevent; }

  mysql_event_class_t plugin_event_class(
      Event_tracking_class event_tracking_class);

  const std::string event_tracking_names(Event_tracking_class);

 private:
  Singleton_event_tracking_service_to_plugin_mapping();
  std::array<mysql_event_class_t,
             static_cast<size_t>(Event_tracking_class::LAST)>
      event_tracking_to_plugin_event_map_;
  static Singleton_event_tracking_service_to_plugin_mapping *instance;
};

#endif  // !SQL_SQL_EVENT_TRACKING_TO_AUDIT_EVENT_MAPPING_H
