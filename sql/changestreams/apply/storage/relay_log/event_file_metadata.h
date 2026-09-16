// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_STORAGE_RELAY_LOG_EVENT_FILE_METADATA_H
#define MYSQL_CSA_STORAGE_RELAY_LOG_EVENT_FILE_METADATA_H

#include <functional>
#include <memory>
#include <string>
#include "sql/binlog_reader.h"

namespace mysql::csa {

/// @brief Represents basic event metadata: type and length
class Event_file_metadata {
 public:
  using Type = mysql::binlog::event::Log_event_type;
  Event_file_metadata() = default;
  /// @brief Construct
  Event_file_metadata(const Event_metadata &ev_meta,
                      const std::string &file_name, std::size_t file_position,
                      Log_event *ev)
      : m_event_metadata(ev_meta),
        m_file_name(file_name),
        m_file_postion(file_position),
        m_event(ev) {}
  Event_file_metadata(const Event_metadata &ev_meta,
                      const std::string &file_name, std::size_t file_position,
                      Log_event *ev, std::optional<Event_payload> data)
      : m_event_metadata(ev_meta),
        m_file_name(file_name),
        m_file_postion(file_position),
        m_event(ev),
        m_data(data) {}
  /// @brief Construct
  Event_file_metadata(const Event_metadata &ev_meta,
                      const std::string &file_name, std::size_t file_position,
                      const std::string &query, bool is_atomic_ddl,
                      Log_event *ev)
      : m_event_metadata(ev_meta),
        m_file_name(file_name),
        m_file_postion(file_position),
        m_query(query),
        m_is_atomic_ddl(is_atomic_ddl),
        m_event(ev) {}
  /// @brief Event length accessor
  /// @return Event length
  std::size_t get_length() const { return m_event_metadata.get_length(); }
  /// @brief Event type accessor
  /// @return Event type
  Type get_type() const { return m_event_metadata.get_type(); }
  /// @brief Event location accessor
  /// @return Log event file location: file name
  std::string get_file_name() const { return m_file_name; }
  /// @brief Event location accessor
  /// @return Log event file location: file position
  std::size_t get_file_pos() const { return m_file_postion; }
  /// @brief Returns information on whether this event is ignorable
  /// @return True if event may be ignored
  bool is_ignorable() const { return m_event_metadata.is_ignorable(); }
  /// @brief Returns query string if type is QUERY_LOG_EVENT, otherwise,
  /// empty string
  /// @return query string if type is QUERY_LOG_EVENT, otherwise, empty string
  const std::string &get_query() const { return m_query; }
  /// @brief Is atomic DDL flag if any
  bool is_atomic_ddl() const { return m_is_atomic_ddl; }
  /// @brief We allow to construct empty objects, this function returns true
  /// in case object was initialized with data
  /// @return True in case object was initialized with data, false otherwise
  bool is_valid() const { return m_event_metadata.is_valid(); }
  /// @brief Compares two events and returns true if they are placed in
  /// the same location
  /// @return True in case files are in the same location, false otherwise
  bool is_in_same_file(const Event_file_metadata &arg) const {
    return is_valid() && arg.is_valid() &&
           (arg.m_file_name == this->m_file_name);
  }

  bool has_payload() const { return m_data.has_value(); }
  bool has_event() const { return m_event.operator bool(); }
  const Event_payload &get_payload() const { return m_data.value(); }

  std::shared_ptr<Log_event> get_event() { return m_event; }

  void clear_file_metadata() {
    m_file_name = "";
    m_file_postion = 0;
  }

 private:
  /// @brief Log event metadata
  Event_metadata m_event_metadata;
  /// @brief Log event file location: file name
  std::string m_file_name;
  /// @brief Log event file location: file position
  std::size_t m_file_postion;
  /// Query is needed to check transaction boundary,
  /// filled if type is QUERY_LOG_EVENT
  std::string m_query{""};
  /// "is atomic DDL" flag, filled if type is QUERY_LOG_EVENT
  bool m_is_atomic_ddl{false};
  /// Decoded event, if available
  std::shared_ptr<Log_event> m_event;
  /// Payload raw data, if any
  std::optional<Event_payload> m_data;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_RELAY_LOG_DATA_SOURCE_H
