/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include <sstream>

#include "consolelog_plugin.h"
#include "dim.h"
#include "filelog_plugin.h"
#include "mysql/harness/logging/supported_logger_options.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/utility/string.h"  // join

#ifdef _WIN32
#include "mysql/harness/logging/eventlog_plugin.h"
#else
#include "syslog_plugin.h"
#endif

using mysql_harness::DIM;
using mysql_harness::LoaderConfig;
using mysql_harness::logging::Handler;
using mysql_harness::logging::log_level_from_string;
using mysql_harness::logging::log_timestamp_precision_from_string;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::LogTimestampPrecision;
IMPORT_LOG_FUNCTIONS()

using HandlerPtr = std::shared_ptr<mysql_harness::logging::Handler>;
using LoggerHandlersList = std::vector<std::pair<std::string, HandlerPtr>>;

#ifdef WIN32
#define NULL_DEVICE_NAME "NUL"
#define STDOUT_DEVICE_NAME "CON"
// no equivalent for STDERR_DEVICE_NAME
#define LEGAL_DESTINATION_DEVICE_NAMES NULL_DEVICE_NAME ", " STDOUT_DEVICE_NAME
#else
#define NULL_DEVICE_NAME "/dev/null"
#define STDOUT_DEVICE_NAME "/dev/stdout"
#define STDERR_DEVICE_NAME "/dev/stderr"
#define LEGAL_DESTINATION_DEVICE_NAMES \
  NULL_DEVICE_NAME ", " STDOUT_DEVICE_NAME ", " STDERR_DEVICE_NAME
#endif

static inline bool legal_consolelog_destination(
    const std::string &destination) {
  if ((destination != NULL_DEVICE_NAME) &&
#ifndef WIN32
      (destination != STDERR_DEVICE_NAME) &&
#endif
      (destination != STDOUT_DEVICE_NAME))
    return false;

  return true;
}
namespace {

HandlerPtr create_logging_sink(
    const std::string &sink_name, const mysql_harness::LoaderConfig &config,
    const std::string &default_log_filename,
    const mysql_harness::logging::LogLevel default_log_level,
    const mysql_harness::logging::LogTimestampPrecision
        default_log_timestamp_precision) {
  using mysql_harness::Path;
  using mysql_harness::logging::FileHandler;
  using mysql_harness::logging::get_default_log_level;
  using mysql_harness::logging::get_default_logger_stream;
  using mysql_harness::logging::NullHandler;
  using mysql_harness::logging::StreamHandler;

  constexpr const char *kLogLevel =
      mysql_harness::logging::kConfigOptionLogLevel;
  constexpr const char *kLogTimestampPrecision =
      mysql_harness::logging::kConfigOptionLogTimestampPrecision;
  constexpr const char *kLogFilename =
      mysql_harness::logging::kConfigOptionLogFilename;
  constexpr const char *kDestination =
      mysql_harness::logging::kConfigOptionLogDestination;
#ifdef _WIN32
  // application defined event logger source name
  constexpr const char *kConfigEventSourceName = "event_source_name";
#endif

  HandlerPtr result;

#ifdef _WIN32
  const std::string kSystemLogPluginName = kEventlogPluginName;
#else
  const std::string kSystemLogPluginName = kSyslogPluginName;
#endif

  // check if the sink has a dedicated section in the configuration and if so if
  // it contain the log level. If it does then use it, otherwise we go with the
  // default one. Similar check is applied for timestamp precision.
  auto log_level = default_log_level;
  auto log_filename = default_log_filename;
  auto log_timestamp_precision = default_log_timestamp_precision;
  bool to_nullhandler = false;
  std::string destination;

  if (config.has(sink_name)) {
    const auto &section = config.get(sink_name, "");
    if (section.has(kLogLevel)) {
      const auto level_name = section.get(kLogLevel);

      log_level = log_level_from_string(level_name);
    }
    if (section.has(kLogTimestampPrecision)) {
      const auto precision_name = section.get(kLogTimestampPrecision);

      // reject timestamp_precision set for syslog/eventlog sinks
      if (sink_name.compare(kSystemLogPluginName) == 0) {
        throw std::runtime_error("timestamp_precision not valid for '" +
                                 kSystemLogPluginName + "'");
      }

      log_timestamp_precision =
          log_timestamp_precision_from_string(precision_name);
    }
    if (sink_name == kConsolelogPluginName) {
      // consolelog shall log to specified destination when specified
      // Limit to the null device depending on platform
      if (section.has(kDestination) && !section.get(kDestination).empty()) {
        destination = section.get(kDestination);
        if (!destination.empty() &&
            !legal_consolelog_destination(destination)) {
          throw std::runtime_error(
              "Illegal destination '" + destination + "' for '" +
              kConsolelogPluginName +
              "'. Legal values are " LEGAL_DESTINATION_DEVICE_NAMES
              ", or empty");
        }
        if (destination == NULL_DEVICE_NAME) {
          to_nullhandler = true;
        }
      }
    } else {
      // illegal default filename shall throw error, even if overridden
      if (!default_log_filename.empty()) {
        // tmp_path = /path/to/file.log ?
        std::string tmp_path(default_log_filename);
        size_t pos = tmp_path.find_last_of('/');
        if (pos != std::string::npos) {
          tmp_path.erase(pos);  // tmp_path = /path/to
          // absolute filename /file.log will not be empty, but still illegal
          if (!tmp_path.empty() || Path(default_log_filename).is_absolute()) {
            throw std::runtime_error("logger filename '" +
                                     default_log_filename +
                                     "' must be a filename, not a path");
          }
        }
      }
      if (section.has(kLogFilename) && !section.get(kLogFilename).empty()) {
        log_filename = section.get(kLogFilename);
      }
    }
  }

  if (sink_name == kConsolelogPluginName) {
    if (to_nullhandler) {
      result.reset(new NullHandler(true, log_level, log_timestamp_precision));
    } else {
      std::ostream *os = (destination == STDOUT_DEVICE_NAME)
                             ? &std::cout
                             : get_default_logger_stream();
      result.reset(
          new StreamHandler(*os, true, log_level, log_timestamp_precision));
    }
  } else if (sink_name == kFilelogPluginName) {
    const std::string logging_folder = config.get_default("logging_folder");

    if (logging_folder.empty()) {
      throw std::runtime_error(
          "filelog sink configured but the logging_folder is empty");
    }
    if (log_filename.empty()) {
      throw std::runtime_error(
          "filelog sink configured but the filename is empty");
    }

    std::string tmp_path(log_filename);  // tmp_path = /path/to/file.log ?
    size_t pos = tmp_path.find_last_of('/');
    if (pos != std::string::npos) {
      tmp_path.erase(pos);  // tmp_path = /path/to
      if (!tmp_path.empty()) {
        throw std::runtime_error("filelog sink configured but the filename '" +
                                 log_filename +
                                 "' must be a filename, not a path");
      }
    }

    Path log_file(log_filename);
    if (!log_file.is_absolute()) {
      log_file = Path(logging_folder).join(log_filename);
    }

    result.reset(
        new FileHandler(log_file, true, log_level, log_timestamp_precision));
  } else if (sink_name == kSystemLogPluginName) {
#ifdef _WIN32
    std::string ev_src_name = config.get_default(kConfigEventSourceName);
    if (ev_src_name.empty()) ev_src_name = std::string(kDefaultEventSourceName);
    result.reset(new EventlogHandler(true, log_level, true, ev_src_name));
#else
    result.reset(new SyslogHandler(true, log_level));
#endif
  } else {
    throw std::runtime_error("Unsupported logger sink type: '" + sink_name +
                             "'");
  }

  return result;
}

}  // namespace

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
  using mysql_harness::logging::get_default_log_filename;
  using mysql_harness::logging::get_default_log_level;
  using mysql_harness::logging::get_default_timestamp_precision;
  logger_handlers.clear();

  // we don't expect any keys for our section
  const auto &section = config.get(kLoggerPluginName, "");

  const auto default_log_level = get_default_log_level(config);
  // an illegal loglevel in the handler configuration has already been
  // caught earlier during startup. Need to catch an illegal timestamp
  // precision and filename here
  std::string default_log_filename;
  try {
    default_log_filename = get_default_log_filename(config);
  } catch (const std::exception &exc) {
    log_error("%s", exc.what());
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
    return false;
  }
  LogTimestampPrecision default_log_timestamp_precision;
  try {
    default_log_timestamp_precision = get_default_timestamp_precision(config);
  } catch (const std::exception &exc) {
    log_error("%s", exc.what());
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
    return false;
  }

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
          sink, create_logging_sink(sink, config, default_log_filename,
                                    default_log_level,
                                    default_log_timestamp_precision)));
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
  auto registry = std::make_unique<mysql_harness::logging::Registry>();

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
  bool new_config_has_consolelog{false};
  for (const auto &handler : logger_handlers) {
    registry->add_handler(handler.first, handler.second);
    attach_handler_to_all_loggers(*registry, handler.first);

    if (handler.first == kConsolelogPluginName) {
      new_config_has_consolelog = true;
    }
  }

  // in case we switched away from the default consolelog and something was
  // already logged to the console, log that we are now switching away
  if (!new_config_has_consolelog) {
    auto &reg = DIM::instance().get_LoggingRegistry();
    try {
      // there may be no main_console_handler.
      auto handler =
          reg.get_handler(mysql_harness::logging::kMainConsoleHandler);

      if (handler->has_logged()) {
        std::vector<std::string> handler_names;
        for (const auto &handler : logger_handlers) {
          handler_names.push_back(handler.first);
        }

        log_info("stopping to log to the console. Continuing to log to %s",
                 mysql_harness::join(handler_names, ", ").c_str());
      }
    } catch (const std::exception &) {
      // not found.
    }
  }

  // nothing threw - we're good. Now let's replace the new registry with the
  // old one
  DIM::instance().set_LoggingRegistry(
      [&registry]() { return registry.release(); },
      std::default_delete<mysql_harness::logging::Registry>());
  DIM::instance().reset_LoggingRegistry();

  // set timestamp precision
  auto precision =
      mysql_harness::logging::get_default_timestamp_precision(config);
  mysql_harness::logging::set_timestamp_precision_for_all_loggers(
      DIM::instance().get_LoggingRegistry(), precision);

  // flag that the new loggers are ready for use
  DIM::instance().get_LoggingRegistry().set_ready();
}

static void init(mysql_harness::PluginFuncEnv *env) {
  LoggerHandlersList logger_handlers;

  auto &config = DIM::instance().get_Config();

  bool res = init_handlers(env, config, logger_handlers);
  // something went wrong; the init_handlers called set_error() so we just
  // stop progress further and let Loader deal with it
  if (!res) return;

  switch_to_loggers_in_config(config, logger_handlers);
}

mysql_harness::Plugin harness_plugin_logger = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Logger",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // Requires
    0,
    nullptr,  // Conflicts
    init,     // init
    nullptr,  // deinit
    nullptr,  // start
    nullptr,  // stop
    false,    // declares_readiness
    logger_supported_options.size(),
    logger_supported_options.data(),
};
