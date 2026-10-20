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

#define LOG_COMPONENT_TAG "Router"

#include "mysqld_error.h"

#include "logging/componentlog_plugin.h"

#include "mysql/harness/logging/supported_logger_options.h"
#include "mysql/harness/plugin.h"

SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

bool ComponentlogHandler::s_inited = false;

void ComponentlogHandler::init_log_service(SERVICE_TYPE(log_builtins) *
                                               log_builtins_ptr,
                                           SERVICE_TYPE(log_builtins_string) *
                                               log_builtins_string_ptr) {
  if (s_inited) return;

  log_bi = log_builtins_ptr;
  log_bs = log_builtins_string_ptr;

  s_inited = true;
}

using mysql_harness::logging::LogLevel;

loglevel ComponentlogHandler::log_level_to_component_log_level(
    LogLevel level) noexcept {
  /*
    enum loglevel {
      SYSTEM_LEVEL = 0,
      ERROR_LEVEL = 1,
      WARNING_LEVEL = 2,
      INFORMATION_LEVEL = 3
  };

    Mapping of LogLevel values to component loglevel enums and
    log_error_verbosity levels:
    +------------------+----------------------+------------------------+
    | Router LogLevel  | loglevel Enum Value  | log_error_verbosity    |
    +------------------+----------------------+------------------------+
    | SYSTEM           | SYSTEM_LEVEL (0)     | Always                 |
    | FATAL            | ERROR_LEVEL (1)      | 1, 2, 3                |
    | ERROR            | ERROR_LEVEL (1)      | 1, 2, 3                |
    | WARNING          | WARNING_LEVEL (2)    | 2, 3                   |
    | INFO             | INFORMATION_LEVEL(3) | 3                      |
    | NOTE             | INFORMATION_LEVEL(3) | 3                      |
    | DEBUG            | INFORMATION_LEVEL(3) | 3                      |
    | default (INFO)   | INFORMATION_LEVEL(3) | 3                      |
    +------------------+----------------------+------------------------+
   */
  switch (level) {
    // SYSTEM_LEVEL
    case LogLevel::kSystem:
      return loglevel::SYSTEM_LEVEL;
    // ERROR_LEVEL
    case LogLevel::kFatal:
      [[fallthrough]];
    case LogLevel::kError:
      return loglevel::ERROR_LEVEL;
    // WARNING_LEVEL
    case LogLevel::kWarning:
      return loglevel::WARNING_LEVEL;
    // INFORMATION_LEVEL
    default:
      assert(level == LogLevel::kInfo || level == LogLevel::kNote ||
             level == LogLevel::kDebug || level == LogLevel::kNotSet);
      return loglevel::INFORMATION_LEVEL;
  }
}

void ComponentlogHandler::do_log(
    const mysql_harness::logging::Record &record) noexcept {
  assert(is_initialized() && "componentlog sink not initialized");

  LogEvent()
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .prio(log_level_to_component_log_level(record.level))
      .lookup(ER_MRS_COMPONENT_MSG, record.message.c_str());
}

extern "C" {
mysql_harness::Plugin harness_plugin_componentlog = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Logging using component logging primitives",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // Requires
    0,
    nullptr,  // Conflicts
    nullptr,  // init
    nullptr,  // deinit,
    nullptr,  // start,
    nullptr,  // stop
    false,    // declares_readiness
    logger_sink_supported_options.size(),
    logger_sink_supported_options.data(),
    nullptr,  // expose_configuration
};
}
