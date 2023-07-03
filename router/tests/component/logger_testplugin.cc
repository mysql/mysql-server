/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
 * @file
 *
 * plugin for routertest_component_logging.
 *
 * - ensures logger is initialized early.
 * - writes a message for each log-level
 * - notifies the test-runner the plugin is ready.
 * - waits to be stopped.
 */

#include <array>

#include "mysql/harness/loader.h"  // PluginFuncEnv
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/supported_logger_options.h"
#include "mysql/harness/plugin.h"

IMPORT_LOG_FUNCTIONS()

static const std::array<const char *, 1> required = {{
    // This plugin do not exist
    "logger",
}};

static void run(mysql_harness::PluginFuncEnv *env) {
  log_debug("I'm a debug message");
  log_note("I'm a note message");
  log_info("I'm an info message");
  log_warning("I'm a warning message");
  log_error("I'm an error message");
  log_system("I'm a system message");

  on_service_ready(env);
  env->wait_for_stop(0);
}

#if defined(_MSC_VER) && defined(routertestplugin_logger_EXPORTS)
/* We are building this library */
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API
#endif

extern "C" {
mysql_harness::Plugin PLUGIN_API harness_plugin_routertestplugin_logger = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "Logger",                                // name
    VERSION_NUMBER(1, 0, 0),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,  //
    nullptr,  // init
    nullptr,  // deinit
    run,      // start
    nullptr,  // stop
    true,     // declares_readiness
    logger_sink_supported_options.size(),
    logger_sink_supported_options.data(),
};
}
