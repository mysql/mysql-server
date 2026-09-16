/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <string>

#include "component_log.h"
#include "health_monitor.h"
#include "mysqld_error.h"

namespace hm {

class Log {
 public:
  void log_message(enum_log_type log_type, long long level, long long code,
                   const char *msg, ...) MY_ATTRIBUTE((format(printf, 5, 0)));

  // cppcheck-suppress functionStatic
  void log_message_va(enum_log_type log_type, long long level, long long code,
                      const char *msg, va_list args)
      MY_ATTRIBUTE((format(printf, 5, 0))) {
    LogEvent()
        .prio(level)
        .errcode(code)
        .subsys(LOG_SUBSYSTEM_TAG)
        .source_line(__LINE__)
        .source_file(MY_BASENAME)
        .function(__FUNCTION__)
        .component(LOG_COMPONENT_TAG)
        .type(log_type)
        .messagev(msg, args);
  }

  template <typename... Args>
  // cppcheck-suppress functionStatic
  void log_message_lu(long long level, long long code, Args... args) {
    LogEvent()
        .prio(level)
        .errcode(code)
        .subsys(LOG_SUBSYSTEM_TAG)
        .source_line(__LINE__)
        .source_file(MY_BASENAME)
        .function(__FUNCTION__)
        .component(LOG_COMPONENT_TAG)
        .lookup(code, args...);
  }

  Log();
  ~Log() = default;
  Log(const Log &) = delete;
  Log &operator=(const Log &) = delete;
  Log(Log &&) = delete;
  Log &operator=(Log &&) = delete;
};

// Logging helper macros
#define log_diag(msg, ...)                                                   \
  ::hm::Health_Monitor::log()->log_message(LOG_TYPE_DIAG, INFORMATION_LEVEL, \
                                           ER_DIAG_LOG_STRING, msg,          \
                                           ##__VA_ARGS__)

#define log_info(msg, ...)                                                    \
  ::hm::Health_Monitor::log()->log_message(LOG_TYPE_ERROR, INFORMATION_LEVEL, \
                                           ER_HEALTH_INFO, msg, ##__VA_ARGS__)

#define log_warning(msg, ...)               \
  ::hm::Health_Monitor::log()->log_message( \
      LOG_TYPE_ERROR, WARNING_LEVEL, ER_HEALTH_WARNING, msg, ##__VA_ARGS__)

#define log_error(msg, ...)                 \
  ::hm::Health_Monitor::log()->log_message( \
      LOG_TYPE_ERROR, ERROR_LEVEL, ER_HEALTH_ERROR, msg, ##__VA_ARGS__)

#define log_warn_usage(msgno, ...)                                  \
  ::hm::Health_Monitor::log()->log_message_lu(WARNING_LEVEL, msgno, \
                                              ##__VA_ARGS__)

// Logging helper functions
std::string bool_to_string(bool value, std::string &if_true,
                           std::string &if_false);
const std::string eq{"="};
std::string on_off(bool value);
std::string true_false(bool value);

}  // namespace hm
