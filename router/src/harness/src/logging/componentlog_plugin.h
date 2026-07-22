/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_COMPONENTLOG_PLUGIN_INCLUDED
#define MYSQL_HARNESS_COMPONENTLOG_PLUGIN_INCLUDED

#include "harness_export.h"

#include <mysql/components/services/log_builtins.h>  // For LogEvent
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

constexpr const char kComponentlogPluginName[] = "componentlog";

extern "C" {
extern mysql_harness::Plugin harness_plugin_componentlog;
}

/**
 * Specific logging handler(sink) that writes the logs using the Component
 * Logging primitives
 */
class HARNESS_EXPORT ComponentlogHandler final
    : public mysql_harness::logging::Handler {
 public:
  explicit ComponentlogHandler(bool format_messages = true,
                               mysql_harness::logging::LogLevel level =
                                   mysql_harness::logging::LogLevel::kNotSet)
      : mysql_harness::logging::Handler(
            format_messages, level,
            mysql_harness::logging::LogTimestampPrecision::kSec) {}
  ~ComponentlogHandler() override = default;

  ComponentlogHandler(const ComponentlogHandler &) = delete;
  ComponentlogHandler(ComponentlogHandler &&) = delete;
  ComponentlogHandler &operator=(const ComponentlogHandler &) = delete;
  ComponentlogHandler &operator=(ComponentlogHandler &&) = delete;

  // does nothing for the componentlog handler
  void reopen(const std::string /*dst*/) override {}

  static void init_log_service(SERVICE_TYPE(log_builtins) * log_builtins_ptr,
                               SERVICE_TYPE(log_builtins_string) *
                                   log_builtins_string_ptr);
  static bool is_initialized() { return s_inited; }

 private:
  void do_log(const mysql_harness::logging::Record &record) noexcept override;
  static loglevel log_level_to_component_log_level(
      mysql_harness::logging::LogLevel level) noexcept;

  static bool s_inited;
};

#endif  // MYSQL_HARNESS_COMPONENTLOG_PLUGIN_INCLUDED
