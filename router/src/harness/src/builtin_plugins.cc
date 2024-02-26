/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "builtin_plugins.h"
#ifdef _WIN32
#include "mysql/harness/logging/eventlog_plugin.h"
#else
#include "logging/syslog_plugin.h"
#endif
#include "logging/consolelog_plugin.h"
#include "logging/filelog_plugin.h"
#include "mysql/harness/logging/logger_plugin.h"

namespace mysql_harness {

BuiltinPlugins::BuiltinPlugins()
    : plugins_({{kLoggerPluginName, {&harness_plugin_logger, false}},
                {kFilelogPluginName, {&harness_plugin_filelog, true}},
                {kConsolelogPluginName, {&harness_plugin_consolelog, false}},
#ifdef _WIN32
                {kEventlogPluginName, {&harness_plugin_eventlog, false}}
#else
                {kSyslogPluginName, {&harness_plugin_syslog, false}}
#endif
      }) {
}

/* static */
BuiltinPlugins &BuiltinPlugins::instance() {
  static BuiltinPlugins instance;
  return instance;
}

bool BuiltinPlugins::has(const std::string &plugin_name) noexcept {
  return plugins_.count(plugin_name) > 0;
}

void BuiltinPlugins::add(std::string name, PluginInfo plugin_info) {
  plugins_.insert(std::make_pair(name, plugin_info));
}

}  // namespace mysql_harness
