/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/plugin.h"

#include <array>

static const std::array<const char *, 1> required = {{
    // Magic plugin is version 1.2.3, so version does not match and this
    // should fail to load.
    "routertestplugin_magic (>>1.2.3)",
}};

static void init(mysql_harness::PluginFuncEnv *) {}

static void deinit(mysql_harness::PluginFuncEnv *) {}

#if defined(_MSC_VER) && defined(routertestplugin_bad_two_EXPORTS)
/* We are building this library */
#define EXAMPLE_API __declspec(dllexport)
#else
#define EXAMPLE_API
#endif

extern "C" {
mysql_harness::Plugin EXAMPLE_API harness_plugin_routertestplugin_bad_two = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "A bad plugin",                          // anme
    VERSION_NUMBER(1, 0, 0),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,
    init,     // init
    deinit,   // deinit
    nullptr,  // start
    nullptr,  // stop
    false,    // declares_readiness
    0,
    nullptr,
};
}
