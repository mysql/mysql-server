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

#ifndef MYSQL_SCHEDULER_LOGGER_STREAM_H
#define MYSQL_SCHEDULER_LOGGER_STREAM_H

#include <sstream>

#ifndef STANDALONE_LIBS_MYSQL
#include "my_dbug.h"  // DBUG_PRINT
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"  // ER_*
#else
#include <iostream>
#endif

/// @brief Message severity levels supported by the server
enum class Severity_level { error, warning, info, debug };

#ifndef STANDALONE_LIBS_MYSQL
// default error code: ER_CSA_MSG
inline constexpr int default_error_code = ER_CSA_MSG;
#else
// default error code: 1 (not used)
inline constexpr int default_error_code = 1;
#endif  // STANDALONE_LIBS_MYSQL

/// @brief Logger utility supporting streaming operator
template <Severity_level level, int error_code = default_error_code>
class Logger_stream {
 public:
  using Stream = std::stringstream;

  /// Gets stream reference
  /// Stream reference
  Stream &get_stream() { return m_stream; }

  /// Logs the message
  void log() {
#if defined(NDEBUG)
    if (level == Severity_level::debug) {
      return;
    }
#endif
#ifndef STANDALONE_LIBS_MYSQL
    if constexpr (level == Severity_level::warning) {
      LogErr(WARNING_LEVEL, error_code, m_stream.str().c_str());
    } else if constexpr (level == Severity_level::error) {
      LogErr(ERROR_LEVEL, error_code, m_stream.str().c_str());
    } else if constexpr (level == Severity_level::info) {
      LogErr(INFORMATION_LEVEL, error_code, m_stream.str().c_str());
    } else if constexpr (level == Severity_level::debug) {
      DBUG_PRINT("debug", ("%s", m_stream.str().c_str()));
    } else {
      // do nothing
    }
#else
    std::cout << m_stream.str() << std::endl;
#endif
    m_stream.clear();
  }
  std::stringstream m_stream;
};

struct Eom {};

template <Severity_level level, int code>
Logger_stream<level, code> &operator<<(Logger_stream<level, code> &stream,
                                       Eom &&) {
  stream.log();
  return stream;
}

template <Severity_level level, int code, typename T>
Logger_stream<level, code> &operator<<(Logger_stream<level, code> &stream,
                                       T &&arg) {
  stream.get_stream() << std::forward<T>(arg);
  return stream;
}

template <Severity_level level, int code>
class Logger_stream_inplace {
 private:
  mutable Logger_stream<level, code> m_stream;

 public:
  Logger_stream_inplace() = default;
  ~Logger_stream_inplace() { m_stream << Eom{}; }
  Logger_stream<level, code> &get() const { return m_stream; }
};

template <Severity_level level, int code, typename T>
Logger_stream_inplace<level, code> const &operator<<(
    Logger_stream_inplace<level, code> const &stream,
    [[maybe_unused]] T &&arg) {
#if defined(NDEBUG)
  if (level == Severity_level::debug) {
    return stream;
  }
#endif
  stream.get() << std::forward<T>(arg);
  return stream;
}

#define MYSQL_LIB_LOG_WARN(...) \
  MYSQL_LIB_LOG_WARN_IMPL(default_error_code, __VA_ARGS__)
#define MYSQL_LIB_LOG_WARN_IMPL(code, ...) \
  Logger_stream_inplace<Severity_level::warning, code> {}
#define MYSQL_LIB_LOG_ERR(...) \
  MYSQL_LIB_LOG_ERR_IMPL(default_error_code, __VA_ARGS__)
#define MYSQL_LIB_LOG_ERR_IMPL(code, ...) \
  Logger_stream_inplace<Severity_level::error, code> {}
#define MYSQL_LIB_LOG_INFO(...) \
  MYSQL_LIB_LOG_INFO_IMPL(default_error_code, __VA_ARGS__)
#define MYSQL_LIB_LOG_INFO_IMPL(code, ...) \
  Logger_stream_inplace<Severity_level::info, code> {}
#define MYSQL_LIB_LOG_DEBUG(...) \
  MYSQL_LIB_LOG_DEBUG_IMPL(default_error_code, __VA_ARGS__)
#define MYSQL_LIB_LOG_DEBUG_IMPL(code, ...) \
  Logger_stream_inplace<Severity_level::debug, code> {}

#endif  // MYSQL_SCHEDULER_LOGGER_STREAM_H
