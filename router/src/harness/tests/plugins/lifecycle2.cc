/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

////////////////////////////////////////////////////////////////////////////////
//
// This test plugin is used to test Harness' handling of plugin lifecycle.
// The plugin exposes all 4 lifecycle functions (init, start, stop and deinit),
// which do nothing except log that they ran. start() persists until stop()
// makes it exit.
//
// The notable feature of this plugin is its (artificial) dependency on another
// test plugin, "lifecycle".  This is useful in testing correctness of plugin
// initialisation and deinitialisation.
//
////////////////////////////////////////////////////////////////////////////////

#include "mysql/harness/plugin.h"
namespace mysql_harness {
class PluginFuncEnv;
}

#include <array>
#include <thread>

// debug printer, keep it disabled unless developing this code
#if 0
#include <iostream>
  void trace(const char* message) {
    std::cerr << "===>" << message << std::endl;
  }
#else
void trace(const char *, ...) {}
#endif

#if defined(_MSC_VER) && defined(routertestplugin_lifecycle2_EXPORTS)
/* We are building this library */
#define LIFECYCLE2_API __declspec(dllexport)
#else
#define LIFECYCLE2_API
#endif

// (artificial) dependency on "lifecycle" plugin (lifecycle.cc):
// At CMake level we don't specify this requirement, because truly, this plugin
// doesn't depend on lifecycle. However, to ensure that it is always initialized
// after lifecycle in unit tests, we set this dependency here to enforce this.
static const std::array<const char *, 1> required = {
    "routertestplugin_lifecycle",
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const auto *info = get_app_info(env);

  // nullptr is special - it's a hack to tell the plugin to reset state
  if (info != nullptr) {
    trace("lifecycle2 init()");
  }
}

static void start(mysql_harness::PluginFuncEnv *env) {
  trace("lifecycle2 start():sleeping");

  while (is_running(env)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  trace("lifecycle2 start():done");
}

static void stop(mysql_harness::PluginFuncEnv *) { trace("lifecycle2 stop()"); }

static void deinit(mysql_harness::PluginFuncEnv *) {
  trace("lifecycle2 deinit()");
}

extern "C" {
mysql_harness::Plugin LIFECYCLE2_API
    harness_plugin_routertestplugin_lifecycle2 = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "Lifecycle2 test plugin",                // name
        VERSION_NUMBER(1, 0, 0),
        // requires
        required.size(), required.data(),
        // conflicts
        0, nullptr,
        init,    // init
        deinit,  // deinit
        start,   // start
        stop,    // stop
        false,   // declares_readiness
        0, nullptr,
        nullptr,  // expose_configuration
};
}
