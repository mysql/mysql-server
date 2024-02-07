/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "log_suppressor.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace metadata_cache {
LogSuppressor::~LogSuppressor() = default;

void LogSuppressor::log_message(
    const MessageId id, const std::string &uuid, const std::string &message,
    bool invalid_condition,
    mysql_harness::logging::LogLevel invalid_condition_level,
    mysql_harness::logging::LogLevel valid_condition_level,
    const bool log_initial_valid) {
  const auto key = std::make_pair(uuid, id);
  std::string last_message;
  if (messages_.count(key) != 0) {
    last_message = messages_[key];
  }

  if (last_message == message) {
    // already logged, suppress
    return;
  }

  messages_[key] = message;

  // the caller does not want to log anything in this scenario
  if (message.empty()) {
    return;
  }

  // only log the valid condition message when it previously was invalid if
  // not explicitly told otherwise
  if (!invalid_condition && last_message.empty() && !log_initial_valid) {
    return;
  }

  const mysql_harness::logging::LogLevel log_level =
      invalid_condition ? invalid_condition_level : valid_condition_level;

  log_custom(log_level, "%s", message.c_str());
}

}  // namespace  metadata_cache
