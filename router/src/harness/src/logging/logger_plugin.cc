/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#define MYSQL_ROUTER_LOG_DOMAIN "logger"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/string_utils.h"

#include <sstream>

#include "consolelog_plugin.h"
#include "dim.h"
#include "filelog_plugin.h"

#ifdef _WIN32
#include "mysql/harness/logging/eventlog_plugin.h"
#else
#include "syslog_plugin.h"
#endif

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::DIM;
using mysql_harness::LoaderConfig;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::logging::Handler;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::log_level_from_string;

IMPORT_LOG_FUNCTIONS()

using HandlerPtr = std::shared_ptr<mysql_harness::logging::Handler>;
using LoggerHandlersList = std::vector<std::pair<std::string, HandlerPtr>>;

static HandlerPtr create_logging_sink(
    const std::string &sink_name, const mysql_harness::LoaderConfig &config,
    const mysql_harness::logging::LogLevel default_log_level) {
  using mysql_harness::Path;
  using mysql_harness::logging::FileHandler;
  using mysql_harness::logging::StreamHandler;
  using mysql_harness::logging::get_default_log_level;
  using mysql_harness::logging::get_default_logger_stream;

  constexpr const char *kLogLevel = "level";

  HandlerPtr result;

  // check if the sink has a dedicated section in the configuration and if so if
  // it contain the log level. If it does use it, otherwise we go with the
  // default one.
  auto log_level = default_log_level;
  if (config.has(sink_name)) {
    const auto &section = config.get(sink_name, "");
    if (section.has(kLogLevel)) {
      const auto level_name = section.get(kLogLevel);

      log_level = log_level_from_string(level_name);
    }
  }
#ifdef _WIN32
  const std::string kSystemLogPluginName = kEventlogPluginName;
#else
  const std::string kSystemLogPluginName = kSyslogPluginName;
#endif

  if (sink_name == kConsolelogPluginName) {
    result.reset(
        new StreamHandler(*get_default_logger_stream(), true, log_level));
  } else if (sink_name == kFilelogPluginName) {
    const std::string logging_folder = config.get_default("logging_folder");

    if (logging_folder.empty()) {
      throw std::runtime_error(
          "filelog sink configured but the logging_folder is empty");
    }

    Path log_file = Path::make_path(logging_folder, "mysqlrouter", "log");

    result.reset(new FileHandler(log_file, true, log_level));
  } else if (sink_name == kSystemLogPluginName) {
#ifdef _WIN32
    result.reset(new EventlogHandler(true, log_level));
#else
    result.reset(new SyslogHandler(true, log_level));
#endif
  } else {
    throw std::runtime_error("Unsupported logger sink type: '" + sink_name +
                             "'");
  }

  return result;
}

void create_plugin_loggers(const mysql_harness::LoaderConfig &config,
                           mysql_harness::logging::Registry &registry,
                           const mysql_harness::logging::LogLevel level) {
  // put together a list of plugins to be loaded. config.section_names()
  // provides a list of plugin instances (one per each [section:key]), while we
  // need a list of plugin names (each entry has to be unique).
  std::set<std::string> modules;
  std::list<mysql_harness::Config::SectionKey> plugins = config.section_names();
  for (const mysql_harness::Config::SectionKey &sk : plugins)
    modules.emplace(sk.first);

  // create loggers for all modules (plugins)
  // we set their log level to "debug" as we want the handlers to decide
  // about the log level
  std::list<std::string> log_domains(modules.begin(), modules.end());
  mysql_harness::logging::create_module_loggers(  // throws
                                                  // std::invalid_argument,
                                                  // std::logic_error
      registry, level, log_domains, ::mysql_harness::logging::kMainLogger);

  // take all the handlers that exist, and attach them to all new loggers.
  for (const std::string &h : registry.get_handler_names())
    attach_handler_to_all_loggers(registry, h);
}

static bool init_handlers(mysql_harness::PluginFuncEnv *env,
                          const mysql_harness::LoaderConfig &config,
                          LoggerHandlersList &logger_handlers) {
  using mysql_harness::logging::get_default_log_level;
  logger_handlers.clear();

  // we don't expect any keys for our secion
  const auto &section = config.get(kLoggerPluginName, "");

  const auto default_log_level = get_default_log_level(config);

  constexpr const char *kSinksOption = "sinks";
  auto sinks_str = section.has(kSinksOption) ? section.get(kSinksOption) : "";
  auto sinks = mysql_harness::split_string(sinks_str, ',', true);

  if (sinks.empty()) {
    if (section.has(kSinksOption)) {
      std::string err_msg =
          std::string(kSinksOption) +
          " option does not contain any valid sink name, was '" +
          section.get(kSinksOption) + "'";
      set_error(env, mysql_harness::kConfigInvalidArgument, "%s",
                err_msg.c_str());
      return false;
    }
    // if there is no sinks configured we go with either filelog or consolelog
    // depending on logging_path being present in the default section or not
    const std::string default_handler =
        config.logging_to_file() ? kFilelogPluginName : kConsolelogPluginName;
    sinks.push_back(default_handler);
  }

  logger_handlers.clear();
  // for each sink create a handler
  for (const auto &sink : sinks) {
    try {
      logger_handlers.push_back(std::make_pair(
          sink, create_logging_sink(sink, config, default_log_level)));
    } catch (const std::exception &exc) {
      log_error("%s", exc.what());
      set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
      return false;
    }
  }
  return true;
}

static void switch_to_loggers_in_config(
    const mysql_harness::LoaderConfig &config,
    const LoggerHandlersList &logger_handlers) {
  // REMINDER: If something threw beyond this point, but before we managed to
  //           re-initialize the logger (registry), we would be in a world of
  //           pain: throwing with a non- functioning logger may cascade to a
  //           place where the error is logged and... BOOM!) So we deal with the
  //           above problem by working on a new logger registry object, and
  //           only if nothing throws, we replace the current registry with the
  //           new one at the very end.

  // our new logger registry, it will replace the current one if all goes well
  std::unique_ptr<mysql_harness::logging::Registry> registry(
      new mysql_harness::logging::Registry());

  // register loggers for all modules + main exec (throws std::logic_error,
  // std::invalid_argument)
  // we use debug level for the loggers as we want the handlers (sinks) to
  // decide independently
  constexpr auto min_log_level = mysql_harness::logging::LogLevel::kDebug;
  mysql_harness::logging::create_module_loggers(
      *registry, min_log_level, {::mysql_harness::logging::kMainLogger},
      ::mysql_harness::logging::kMainLogger);
  create_plugin_loggers(config, *registry, min_log_level);

  // register logger for sql domain
  mysql_harness::logging::create_logger(*registry, min_log_level, "sql");

  // attach all loggers to the handlers (throws std::runtime_error)
  for (const auto &handler : logger_handlers) {
    registry->add_handler(handler.first, handler.second);
    attach_handler_to_all_loggers(*registry, handler.first);
  }

  // nothing threw - we're good. Now let's replace the new registry with the
  // old one
  DIM::instance().set_LoggingRegistry(
      [&registry]() { return registry.release(); },
      std::default_delete<mysql_harness::logging::Registry>());
  DIM::instance().reset_LoggingRegistry();

  // flag that the new loggers are ready for use
  DIM::instance().get_LoggingRegistry().set_ready();
}

static void init(mysql_harness::PluginFuncEnv *env) {
  using mysql_harness::logging::get_default_log_level;
  LoggerHandlersList logger_handlers;

  auto &config = DIM::instance().get_Config();

  bool res = init_handlers(env, config, logger_handlers);
  // something went wrong; the init_handlers called set_error() so we just stop
  // progress further and let Loader deal with it
  if (!res) return;

  log_info(
      "logging facility initialized, switching logging to loggers specified in "
      "configuration");
  switch_to_loggers_in_config(config, logger_handlers);
}

Plugin harness_plugin_logger = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Logging using eventlog",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // Requires
    0,
    nullptr,  // Conflicts
    init,     // init
    nullptr,  // deinit
    nullptr,  // start
    nullptr,  // stop
};
