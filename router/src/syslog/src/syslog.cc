/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/plugin.h"

#include <cstdarg>

#include <syslog.h>

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::logging::LogLevel;

class SyslogHandler final : public mysql_harness::logging::Handler {
 public:
  static constexpr const char *kDefaultName = "syslog";

  SyslogHandler(bool format_messages = true, LogLevel level = LogLevel::kNotSet)
      : mysql_harness::logging::Handler(format_messages, level) {}
  ~SyslogHandler() override { close(); }

  void open(const std::string &ident) {
    openlog(ident.c_str(), LOG_CONS | LOG_NDELAY, LOG_DAEMON);
  }

  void close() { closelog(); }

 private:
  void do_log(const mysql_harness::logging::Record &record) override {
    syslog(static_cast<int>(record.level), "%s", record.message.c_str());
  }
};

std::shared_ptr<SyslogHandler> g_syslog_handler =
    std::make_shared<SyslogHandler>();

static void init(mysql_harness::PluginFuncEnv *env) {
  const AppInfo *info = get_app_info(env);
  using mysql_harness::logging::register_handler;

  g_syslog_handler->open(info->program);
  register_handler(SyslogHandler::kDefaultName, g_syslog_handler);
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  g_syslog_handler->close();
}

extern "C" {
Plugin harness_plugin_syslog = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Logging using syslog",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // Requires
    0,
    nullptr,  // Conflicts
    init,
    deinit,
    nullptr,  // start
    nullptr,  // stop
};
}
