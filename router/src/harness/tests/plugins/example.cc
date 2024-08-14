/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include <array>
#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::Plugin;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::PluginFuncEnv;
using mysql_harness::logging::log_info;

#ifdef _WIN32
#define EXAMPLE_IMPORT __declspec(dllimport)
#else
#define EXAMPLE_IMPORT
#endif

extern "C" {
extern void EXAMPLE_IMPORT do_magic();
}

#if defined(_MSC_VER) && defined(routertestplugin_example_EXPORTS)
/* We are building this library */
#define EXAMPLE_API __declspec(dllexport)
#else
#define EXAMPLE_API
#endif

static constexpr std::array required{
    "routertestplugin_magic (>>1.0)",
};

static void init(PluginFuncEnv *);
static void deinit(PluginFuncEnv *);
static void start(PluginFuncEnv *);

extern "C" {
Plugin EXAMPLE_API harness_plugin_routertestplugin_example = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "An example plugin",
    VERSION_NUMBER(1, 0, 0),
    required.size(),
    required.data(),
    0,
    nullptr,  // conflicts
    init,     // init
    deinit,   // deinit
    start,    // start
    nullptr,  // stop
    false,    // declares_readiness
    0,
    nullptr,
    nullptr,  // expose_configuration
};
}

static void init(PluginFuncEnv *) { do_magic(); }

static void deinit(PluginFuncEnv *) {}

static void start(PluginFuncEnv *) {
  for (int x = 0; x < 10; ++x) {
    log_info("example <count: %d>", x);
#ifndef _WIN32
    sleep(1);
#else
    Sleep(1000);
#endif
  }
}
