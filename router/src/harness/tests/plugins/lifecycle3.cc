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

////////////////////////////////////////////////////////////////////////////////
//
// This test plugin is used to test Harness' handling of plugin lifecycle.
// The plugin exposes 2 lifecycle functions (init, deinit), which do nothing.
//
// This plugin is an (artificial) dependency of another test plugin, "lifecycle"
// - it is useful in testing correctness of plugin initialisation and
// deinitialisation.
//
////////////////////////////////////////////////////////////////////////////////

#include "mysql/harness/plugin.h"

namespace mysql_harness {
class PluginFuncEnv;
}

#if defined(_MSC_VER) && defined(routertestplugin_lifecycle3_EXPORTS)
/* We are building this library */
#define LIFECYCLE3_API __declspec(dllexport)
#else
#define LIFECYCLE3_API
#endif

static void init(mysql_harness::PluginFuncEnv *) {}
static void deinit(mysql_harness::PluginFuncEnv *) {}

extern "C" {
mysql_harness::Plugin LIFECYCLE3_API
    harness_plugin_routertestplugin_lifecycle3 = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "Logging functions",                     // anme
        VERSION_NUMBER(0, 0, 1),
        // requires
        0, nullptr,
        // conflicts
        0, nullptr,
        init,     // init
        deinit,   // deinit
        nullptr,  // start
        nullptr,  // stop
        false,    // declares_readiness
        0, nullptr,
        nullptr,  // expose_configuration
};
}
