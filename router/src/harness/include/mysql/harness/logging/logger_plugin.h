/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_LOGGER_PLUGIN_INCLUDED
#define MYSQL_HARNESS_LOGGER_PLUGIN_INCLUDED

#include <array>
#include <functional>

#include "harness_export.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/plugin.h"

constexpr const char *kLoggerPluginName = "logger";
extern mysql_harness::Plugin HARNESS_EXPORT harness_plugin_logger;

/**
 * Creates the logging handler for each plugin from the configuration.
 *
 * @param config    configuration containing the plugin names we should create
 * loggers for
 * @param registry  logging registry where the logging handlers should be
 * created
 * @param level     logging level for the newly create logging handlers
 *
 * @throws std::logic_error
 */
void HARNESS_EXPORT
create_plugin_loggers(const mysql_harness::LoaderConfig &config,
                      mysql_harness::logging::Registry &registry,
                      const mysql_harness::logging::LogLevel level);

using on_switch_to_configured_loggers = std::function<void()>;

/**
 * Registers the callback that the plugin will execute once it is finished with
 * initialization and switches from the default to the configured logger
 * sink(s).
 *
 * @param callback    callback to execute
 */
void HARNESS_EXPORT register_on_switch_to_configured_loggers_callback(
    on_switch_to_configured_loggers callback);

#endif
