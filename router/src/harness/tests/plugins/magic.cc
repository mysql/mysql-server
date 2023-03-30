/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "magic.h"

#include <cstdlib>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

using mysql_harness::logging::log_info;

#if defined(_MSC_VER) && defined(routertestplugin_magic_EXPORTS)
/* We are building this library */
#define MAGIC_API __declspec(dllexport)
#else
#define MAGIC_API
#endif

const mysql_harness::AppInfo *g_info;
const mysql_harness::ConfigSection *g_section;

static void init(mysql_harness::PluginFuncEnv *env) {
  g_info = get_app_info(env);
}

extern "C" void MAGIC_API do_magic() {
  auto const &section = g_info->config->get("routertestplugin_magic", "");
  auto const &message = section.get("message");
  log_info("%s", message.c_str());
}

static void start(mysql_harness::PluginFuncEnv *env) {
  const auto *section = get_config_section(env);
  try {
    if (section->has("suki") && section->get("suki") == "bad") {
      set_error(env, mysql_harness::kRuntimeError,
                "The suki was bad, please throw away");
    }
  } catch (mysql_harness::bad_option &) {
  }

  if (section->has("do_magic")) do_magic();
}

extern "C" {
mysql_harness::Plugin MAGIC_API harness_plugin_routertestplugin_magic = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "A magic plugin",                        // name
    VERSION_NUMBER(1, 2, 3),
    // required
    0,
    nullptr,
    // conflicts
    0,
    nullptr,
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
    false,    // declares_readiness
    0,
    nullptr,
};
}
