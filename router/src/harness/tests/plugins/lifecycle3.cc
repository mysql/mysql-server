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

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::PluginFuncEnv;

#if defined(_MSC_VER) && defined(lifecycle3_EXPORTS)
/* We are building this library */
#define LIFECYCLE3_API __declspec(dllexport)
#else
#define LIFECYCLE3_API
#endif

static void init(PluginFuncEnv *) {}
static void deinit(PluginFuncEnv *) {}

extern "C" {
Plugin LIFECYCLE3_API harness_plugin_lifecycle3 = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Logging functions",
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
