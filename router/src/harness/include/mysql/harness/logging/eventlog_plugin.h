/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_EVENTLOG_PLUGIN_INCLUDED
#define MYSQL_HARNESS_EVENTLOG_PLUGIN_INCLUDED

#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

constexpr const char *kEventlogPluginName = "eventlog";
constexpr const char *kDefaultEventSourceName = "MySQL Router";
extern "C" mysql_harness::Plugin harness_plugin_eventlog;

/**
 * Windows specific logging handler(sink) that writes the logs to the Windows
 * eventlog.
 */
class EventlogHandler final : public mysql_harness::logging::Handler {
 public:
  static constexpr const char *kDefaultName = "eventlog";

  /** @brief Constructor
   *
   * It adds appropriate entries to the Windows registry and registers the
   * Router as the event log messages source.
   *
   * @param format_messages flag indicating if the logged messages should be
   * formatted
   * @param level minimal log level for the handler
   * @param create_registry_entries If true, initialisation will perform extra
   *        steps (which may potentially fail, thus you might prefer to disable
   *        them for mission-critical usage)
   * @param event_source_name the event source name for event log entries
   *
   * @throw std::runtime_error on WinAPI calls failures
   */
  EventlogHandler(
      bool format_messages, mysql_harness::logging::LogLevel level,
      bool create_registry_entries = true,
      const std::string event_source_name = kDefaultEventSourceName);

  ~EventlogHandler() override;

  // does nothing for the eventlog handler
  void reopen(const std::string dst [[maybe_unused]] = "") override {}

 private:
  void do_log(const mysql_harness::logging::Record &record) noexcept override;

  HANDLE event_src_;

  const std::string event_source_name_;
};

#endif
