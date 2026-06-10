/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TELEMETRY_LOG_H_INCLUDED
#define TELEMETRY_LOG_H_INCLUDED

#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

namespace telemetry {

extern const char *component_name;

class Log {
 public:
  static void init(SERVICE_TYPE(log_builtins) * log_bi_srv,
                   SERVICE_TYPE(log_builtins_string) * log_bs_srv);

  static void log_message(const char *src_file, int src_line, long long level,
                          long long code, const char *msg, ...)
      MY_ATTRIBUTE((format(printf, 5, 0)));

  static void log_message_va(const char *src_file, int src_line,
                             long long level, long long code, const char *msg,
                             va_list args)
      MY_ATTRIBUTE((format(printf, 5, 0))) {
    LogEvent()
        .no_telemetry()
        .prio(level)
        .errcode(code)
        .subsys(LOG_SUBSYSTEM_TAG)
        .source_line(src_line)
        .source_file(src_file)
        .function(__FUNCTION__)
        .component(LOG_COMPONENT_TAG)
        .messagev(msg, args);
  }

  template <typename... Args>
  static void log_message_lu(const char *src_file, int src_line,
                             long long level, long long code, Args... args) {
    LogEvent()
        .no_telemetry()
        .prio(level)
        .errcode(code)
        .subsys(LOG_SUBSYSTEM_TAG)
        .source_line(src_line)
        .source_file(src_file)
        .function(__FUNCTION__)
        .component(LOG_COMPONENT_TAG)
        .lookup(code, args...);
  }
};

}  // namespace telemetry

#define log_info(msg, ...)                                                   \
  Log::log_message(__FILE__, __LINE__, INFORMATION_LEVEL, ER_TELEMETRY_INFO, \
                   msg, ##__VA_ARGS__)

#define log_warning(msg, ...)                                               \
  Log::log_message(__FILE__, __LINE__, WARNING_LEVEL, ER_TELEMETRY_WARNING, \
                   msg, ##__VA_ARGS__)

#define log_error(msg, ...)                                                  \
  Log::log_message(__FILE__, __LINE__, ERROR_LEVEL, ER_TELEMETRY_ERROR, msg, \
                   ##__VA_ARGS__)

#define log_warn_usage(msgno, ...) \
  Log::log_message_lu(__FILE__, __LINE__, WARNING_LEVEL, msgno, ##__VA_ARGS__)

#endif /* TELEMETRY_LOG_H_INCLUDED */
