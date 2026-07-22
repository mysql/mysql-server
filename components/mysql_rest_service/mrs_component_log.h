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

#ifndef MRS_COMPONENT_LOG_H
#define MRS_COMPONENT_LOG_H

#include "mysql/components/services/log_builtins.h"

namespace mrs_component {

/**
  Emits MRS messages through the MySQL Server logging services.
*/
class Log {
 public:
  void log_message(enum_log_type log_type, long long level, long long code,
                   const char *msg, ...) const
      MY_ATTRIBUTE((format(printf, 5, 0)));

  void log_message_va(enum_log_type log_type, long long level, long long code,
                      const char *msg, va_list args) const
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

  Log() = default;
  ~Log() = default;
  Log(const Log &) = delete;
  Log &operator=(const Log &) = delete;
  Log(Log &&) = delete;
  Log &operator=(Log &&) = delete;
};

}  // namespace mrs_component

// Logging helper macros
#define MRS_LOG_SYSTEM(msg, ...)                                              \
  do {                                                                        \
    mrs_component().log().log_message(LOG_TYPE_ERROR, SYSTEM_LEVEL,           \
                                      ER_MRS_COMPONENT_MSG, "%s: " msg,       \
                                      mrs_component().name(), ##__VA_ARGS__); \
  } while (0)

#define MRS_LOG_INFO(msg, ...)                                                \
  do {                                                                        \
    mrs_component().log().log_message(LOG_TYPE_ERROR, INFORMATION_LEVEL,      \
                                      ER_MRS_COMPONENT_INFO, "%s: " msg,      \
                                      mrs_component().name(), ##__VA_ARGS__); \
  } while (0)

#define MRS_LOG_WARNING(msg, ...)                                             \
  do {                                                                        \
    mrs_component().log().log_message(LOG_TYPE_ERROR, WARNING_LEVEL,          \
                                      ER_MRS_COMPONENT_WARNING, "%s: " msg,   \
                                      mrs_component().name(), ##__VA_ARGS__); \
  } while (0)

#define MRS_LOG_ERROR(msg, ...)                                               \
  do {                                                                        \
    mrs_component().log().log_message(LOG_TYPE_ERROR, ERROR_LEVEL,            \
                                      ER_MRS_COMPONENT_ERROR, "%s: " msg,     \
                                      mrs_component().name(), ##__VA_ARGS__); \
  } while (0)

#ifdef COMPONENT_MRS_LOG_DEBUG
#define MRS_LOG_DEBUG(msg, ...)                                                \
  do {                                                                         \
    mrs_component().log().log_message(LOG_TYPE_ERROR, INFORMATION_LEVEL,       \
                                      ER_MRS_COMPONENT_MSG, "%s(DEBUG): " msg, \
                                      mrs_component().name(), ##__VA_ARGS__);  \
  } while (0)
#else
#define MRS_LOG_DEBUG(...) \
  do {                     \
  } while (0)
#endif  // COMPONENT_MRS_LOG_DEBUG

#endif  // MRS_COMPONENT_LOG_H
