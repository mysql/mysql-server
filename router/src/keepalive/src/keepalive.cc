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

/**
 * Keepalive Plugin
 *
 * Keepalive plugin is simply sending a message every, by default,
 * 8 seconds and running until Router is shut down.
 *
 * [keepalive]
 * interval = 2
 * runs = 3
 */

#include <chrono>
#include <iostream>
#include <thread>

// Harness interface include files
#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::ConfigSection;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::PluginFuncEnv;
using mysql_harness::logging::log_info;

// Keep symbols with external linkage away from global scope so that
// they do not clash with other symbols.
namespace {

const int kInterval = 60;  // in seconds
const int kRuns = 0;       // 0 means for ever

}  // namespace

static void init(PluginFuncEnv *) {}

static void start(PluginFuncEnv *env) {
  const ConfigSection *section = get_config_section(env);
  int interval = kInterval;
  try {
    interval = std::stoi(section->get("interval"));
  } catch (...) {
    // Anything invalid will result in using the default.
  }

  int runs = kRuns;
  try {
    runs = std::stoi(section->get("runs"));
  } catch (...) {
    // Anything invalid will result in using the default.
  }

  std::string name = section->name;
  if (!section->key.empty()) {
    name += " " + section->key;
  }

  log_info("%s started with interval %d", name.c_str(), interval);
  if (runs) {
    log_info("%s will run %d time(s)", name.c_str(), runs);
  }

  for (int total_runs = 0; runs == 0 || total_runs < runs; ++total_runs) {
    log_info("%s", name.c_str());
    if (wait_for_stop(env, static_cast<uint32_t>(interval * 1000))) break;
  }
}

#if defined(_MSC_VER) && defined(keepalive_EXPORTS)
/* We are building this library */
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" {
Plugin DLLEXPORT harness_plugin_keepalive = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Keepalive Plugin",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,
    0,
    nullptr,  // conflicts
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
};
}
