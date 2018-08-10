/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

////////////////////////////////////////////////////////////////////////////////
//
// This test plugin is used to test Harness' handling of plugin lifecycle.
// The plugin exposes all 4 lifecycle functions (init, start, stop and deinit),
// which do nothing except log that they ran. start() persists until stop()
// makes it exit.
//
// The notable feature of this plugin is its (artificial) depenency on another
// test plugin, "lifecycle".  This is useful in testing correctness of plugin
// initialisation and deinitialisation.
//
////////////////////////////////////////////////////////////////////////////////

#include "mysql/harness/plugin.h"
namespace mysql_harness {
class PluginFuncEnv;
}

#include <thread>

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::PluginFuncEnv;

// debug printer, keep it disabled unless developing this code
#if 0
#include <iostream>
  void trace(const char* message) {
    std::cerr << "===>" << message << std::endl;
  }
#else
void trace(const char *, ...) {}
#endif

#if defined(_MSC_VER) && defined(lifecycle2_EXPORTS)
/* We are building this library */
#define LIFECYCLE2_API __declspec(dllexport)
#else
#define LIFECYCLE2_API
#endif

// (artificial) dependency on "lifecycle" plugin (lifecycle.cc):
// At CMake level we don't specify this requirement, because truly, this plugin
// doesn't depend on lifecycle. However, to ensure that it is always initialized
// after lifecycle in unit tests, we set this dependency here to enforce this.
static const char *requires[] = {
    "lifecycle",
};

static void init(PluginFuncEnv *env) {
  const AppInfo *info = get_app_info(env);

  // nullptr is special - it's a hack to tell the plugin to reset state
  if (info != nullptr) {
    trace("lifecycle2 init()");
  }
}

static void start(PluginFuncEnv *env) {
  trace("lifecycle2 start():sleeping");

  while (is_running(env)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  trace("lifecycle2 start():done");
}

static void stop(PluginFuncEnv *) { trace("lifecycle2 stop()"); }

static void deinit(PluginFuncEnv *) { trace("lifecycle2 deinit()"); }

extern "C" {
Plugin LIFECYCLE2_API harness_plugin_lifecycle2 = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Lifecycle2 test plugin",
    VERSION_NUMBER(1, 0, 0),
    sizeof(requires) / sizeof(*requires),
    requires,
    0,        // \_ conflicts
    nullptr,  // /
    init,     // init
    deinit,   // deinit
    start,    // start
    stop,     // stop
};
}
