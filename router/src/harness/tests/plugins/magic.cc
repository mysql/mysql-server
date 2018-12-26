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

#include "magic.h"

#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include <cstdlib>
#include <iostream>

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::ConfigSection;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::PluginFuncEnv;
using mysql_harness::bad_option;
using mysql_harness::logging::log_info;

#if defined(_MSC_VER) && defined(magic_EXPORTS)
/* We are building this library */
#define MAGIC_API __declspec(dllexport)
#else
#define MAGIC_API
#endif

const AppInfo *g_info;
const ConfigSection *g_section;

static void init(PluginFuncEnv *env) { g_info = get_app_info(env); }

extern "C" void MAGIC_API do_magic() {
  auto &&section = g_info->config->get("magic", "");
  auto &&message = section.get("message");
  log_info("%s", message.c_str());
}

static void start(PluginFuncEnv *env) {
  const ConfigSection *section = get_config_section(env);
  try {
    if (section->has("suki") && section->get("suki") == "bad") {
      set_error(env, mysql_harness::kRuntimeError,
                "The suki was bad, please throw away");
    }
  } catch (bad_option &) {
  }

  if (section->has("do_magic")) do_magic();
}

extern "C" {
Plugin MAGIC_API harness_plugin_magic = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "A magic plugin",
    VERSION_NUMBER(1, 2, 3),
    0,
    nullptr,
    0,
    nullptr,
    init,
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
};
}
