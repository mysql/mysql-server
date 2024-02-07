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

#ifndef MYSQL_HARNESS_SYSLOG_PLUGIN_INCLUDED
#define MYSQL_HARNESS_SYSLOG_PLUGIN_INCLUDED

#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include <syslog.h>
#include <cstring>

constexpr const char *kSyslogPluginName = "syslog";
extern "C" {
extern mysql_harness::Plugin harness_plugin_syslog;
}

using mysql_harness::logging::LogLevel;
using mysql_harness::logging::LogTimestampPrecision;

/**
 * Unix-based systems specific logging handler(sink) that writes the logs to the
 * syslog.
 */
class SyslogHandler final : public mysql_harness::logging::Handler {
 public:
  static constexpr unsigned kMaxIdentSize = 100;

  SyslogHandler(bool format_messages = true, LogLevel level = LogLevel::kNotSet)
      : mysql_harness::logging::Handler(format_messages, level,
                                        LogTimestampPrecision::kSec) {}
  ~SyslogHandler() override { close(); }

  void open(const std::string &ident) noexcept {
    std::strncpy(ident_, ident.c_str(), kMaxIdentSize);
    ident_[kMaxIdentSize - 1] = '\0';
    openlog(ident_, LOG_CONS | LOG_NDELAY, LOG_DAEMON);
  }

  void close() const noexcept { closelog(); }

  // does nothing for the syslog handler
  void reopen(const std::string /*dst*/) override {}

 private:
  void do_log(const mysql_harness::logging::Record &record) noexcept override {
    syslog(log_level_to_syslog(record.level), "%s", record.message.c_str());
  }

  static int log_level_to_syslog(const LogLevel level) noexcept {
    switch (level) {
      case LogLevel::kFatal:
        return LOG_EMERG;
      case LogLevel::kError:
        return LOG_ERR;
      case LogLevel::kWarning:
        return LOG_WARNING;
      case LogLevel::kInfo:
      case LogLevel::kNote:
      case LogLevel::kSystem:
        // Let loglevels NOTE and SYSTEM map to LOG_INFO
        return LOG_INFO;
      default:  // kDebug
        assert(level == LogLevel::kDebug);
        return LOG_DEBUG;
    }
  }

  char ident_[kMaxIdentSize];
};

#endif  // MYSQL_HARNESS_SYSLOG_PLUGIN_INCLUDED
