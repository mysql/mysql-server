/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_LOGGER_HANDLER_INCLUDED
#define MYSQL_HARNESS_LOGGER_HANDLER_INCLUDED

#include "harness_export.h"
#include "mysql/harness/logging/logging.h"

#include <fstream>
#include <mutex>
#include <ostream>
#include <string>

namespace mysql_harness {

namespace logging {

/**
 * Base class for log message handler.
 *
 * This class is used to implement a log message handler. You need
 * to implement the `do_log` primitive to process the log
 * record. If, for some reason, the implementation is unable to log
 * the record, and exception can be thrown that will be caught by
 * the harness.
 */
class HARNESS_EXPORT Handler {
 public:
  /**
   * Default identifier
   *
   * Every handler provides a default name which could be used as key in
   * registry to uniquely identify it. There is no obligation to use it, it
   * is only supplied for convenience. In case of many instances of the same
   * handler, using a key derived from this default (such as
   * "my_handler:instance1") is suggested.
   *
   * This field should be set in derived classes
   */
  static constexpr const char *kDefaultName = nullptr;

  explicit Handler() = default;
  explicit Handler(const Handler &) = default;
  Handler &operator=(const Handler &) = default;

  virtual ~Handler() = default;

  void handle(const Record &record);

  void set_level(LogLevel level) { level_ = level; }
  LogLevel get_level() const { return level_; }
  void set_timestamp_precision(LogTimestampPrecision precision) {
    precision_ = precision;
  }

  /**
   * Request to reopen underlying log sink. Should be no-op for handlers NOT
   * writing to a file. Useful for log rotation, when the logger got the
   * signal with the request to reopen the file. Provide a destination filename
   * for the old file for file based handlers.
   */
  virtual void reopen(const std::string dst = "") = 0;

  /**
   * check if the handler has logged at least one record.
   *
   * @retval true if at least one record was logged
   * @retval false if no record has been logged yet.
   */
  bool has_logged() const { return has_logged_; }

 protected:
  std::string format(const Record &record) const;

  explicit Handler(bool format_messages, LogLevel level,
                   LogTimestampPrecision timestamp_precision);

  void has_logged(bool v) { has_logged_ = v; }

 private:
  /**
   * Log message handler primitive.
   *
   * This member function is implemented by subclasses to properly log
   * a record wherever it need to be logged.  If it is not possible to
   * log the message properly, an exception should be thrown and will
   * be caught by the caller.
   *
   * @param record Record containing information about the message.
   */
  virtual void do_log(const Record &record) = 0;

  /**
   * Flags if log messages should be formatted (prefixed with log level,
   * timestamp, etc) before logging.
   */
  bool format_messages_;

  /**
   * Log level set for the handler.
   */
  LogLevel level_;

  /**
   * Timestamp precision for logging
   */
  LogTimestampPrecision precision_;

  bool has_logged_{false};
};

/**
 * Handler to write to an output stream.
 *
 * @code
 * Logger logger("my_module");
 * ...
 * logger.add_handler(StreamHandler(std::clog));
 * @endcode
 */
class HARNESS_EXPORT StreamHandler : public Handler {
 public:
  static constexpr const char *kDefaultName = "stream";

  explicit StreamHandler(std::ostream &stream, bool format_messages = true,
                         LogLevel level = LogLevel::kNotSet,
                         LogTimestampPrecision timestamp_precision =
                             LogTimestampPrecision::kNotSet);

  // for the stream handler there is nothing to do
  void reopen(const std::string /*dst*/) override {}

 protected:
  std::ostream &stream_;
  std::mutex stream_mutex_;

 private:
  void do_log(const Record &record) override;
};

/**
 * Handler to write to a null device such as /dev/null (unix) or NUL (windows).
 *
 * This handler produces no output.
 *
 * @code
 * Logger logger("my_module");
 * ...
 * logger.add_handler(NullHandler());
 * @endcode
 */
class HARNESS_EXPORT NullHandler : public Handler {
 public:
  static constexpr const char *kDefaultName = "null";

  explicit NullHandler(bool format_messages = true,
                       LogLevel level = LogLevel::kNotSet,
                       LogTimestampPrecision timestamp_precision =
                           LogTimestampPrecision::kNotSet);

  // for the null handler there is nothing to do
  void reopen(const std::string /*dst*/) override {}

 private:
  void do_log(const Record &record) override;
};

/**
 * Handler that writes to a file.
 *
 * @code
 * Logger logger("my_module");
 * ...
 * logger.add_handler(FileHandler("/var/log/router.log"));
 * @endcode
 */
class HARNESS_EXPORT FileHandler : public StreamHandler {
 public:
  static constexpr const char *kDefaultName = "file";

  explicit FileHandler(const Path &path, bool format_messages = true,
                       LogLevel level = LogLevel::kNotSet,
                       LogTimestampPrecision timestamp_precision =
                           LogTimestampPrecision::kNotSet);
  ~FileHandler() override;

  void reopen(const std::string dst = "") override;

 private:
  void do_log(const Record &record) override;

  const Path file_path_;
  std::ofstream fstream_;
};

}  // namespace logging

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOGGER_HANDLER_INCLUDED */
