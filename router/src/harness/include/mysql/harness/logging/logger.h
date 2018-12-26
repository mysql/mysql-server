/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_LOGGER_LOGGER_INCLUDED
#define MYSQL_HARNESS_LOGGER_LOGGER_INCLUDED

#include "harness_export.h"
#include "mysql/harness/logging/logging.h"

#include <set>
#include <string>

namespace mysql_harness {

namespace logging {

class Registry;

/**
 * Logger class.
 *
 * The logger class handles the logging for one or more logging
 * handlers. Each logger class instance keeps state for logging for one
 * module or subsystem. You can add handlers to a logger which will
 * then be used for all logging to that subsystem.
 */
class HARNESS_EXPORT Logger {
 public:
  explicit Logger(Registry &registry, LogLevel level = kDefaultLogLevel);

  // such null object is useless, however we need the ability to create an
  // object and populate it later inside of guarded scope (e.g: std::lock_guard)
  Logger() : level_(LogLevel::kNotSet) {}

  void attach_handler(std::string name);
  void detach_handler(std::string name, bool handler_must_exist = true);
  void handle(const Record &record);
  const std::set<std::string> &get_handler_names() const { return handlers_; }

  void set_level(LogLevel level) { level_ = level; }
  LogLevel get_level() const { return level_; }

 private:
  LogLevel level_;
  std::set<std::string> handlers_;
  const Registry *registry_;  // owner backreference (we don't own Registry,
                              // Registry owns us)
};

}  // namespace logging

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOGGER_LOGGER_INCLUDED */
