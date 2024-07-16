/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <iostream>  // cerr
#include <sstream>
#include <stdexcept>

#include "my_compiler.h"

#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/stdx/process.h"
#ifdef _WIN32
#include "mysql/harness/logging/eventlog_plugin.h"
#endif

#include "common.h"  // serial_comma
#include "dim.h"
#include "harness_assert.h"

using mysql_harness::logging::Logger;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::Record;

namespace {

using mysql_harness::logging::LogLevel;
using mysql_harness::logging::LogTimestampPrecision;

constexpr auto kLogLevels =
    std::to_array<std::pair<std::string_view, LogLevel>>({
        {"fatal", LogLevel::kFatal},
        {"system", LogLevel::kSystem},
        {"error", LogLevel::kError},
        {"warning", LogLevel::kWarning},
        {"info", LogLevel::kInfo},
        {"note", LogLevel::kNote},
        {"debug", LogLevel::kDebug},
    });

constexpr auto kLogTimestampPrecisions =
    std::to_array<std::pair<std::string_view, LogTimestampPrecision>>({
        {"second", LogTimestampPrecision::kSec},
        {"sec", LogTimestampPrecision::kSec},
        {"s", LogTimestampPrecision::kSec},
        {"millisecond", LogTimestampPrecision::kMilliSec},
        {"msec", LogTimestampPrecision::kMilliSec},
        {"ms", LogTimestampPrecision::kMilliSec},
        {"microsecond", LogTimestampPrecision::kMicroSec},
        {"usec", LogTimestampPrecision::kMicroSec},
        {"us", LogTimestampPrecision::kMicroSec},
        {"nanosecond", LogTimestampPrecision::kNanoSec},
        {"nsec", LogTimestampPrecision::kNanoSec},
        {"ns", LogTimestampPrecision::kNanoSec},
    });
}  // namespace

namespace mysql_harness {

namespace logging {

////////////////////////////////////////////////////////////////////////////////
//
// logger CRUD
//
////////////////////////////////////////////////////////////////////////////////

// throws std::logic_error
void Registry::create_logger(const std::string &name, LogLevel level) {
  std::unique_lock lock(mtx_);
  auto result = loggers_.emplace(name, Logger(*this, level));
  if (result.second == false)
    throw std::logic_error("Duplicate logger '" + name + "'");
}

// throws std::logic_error
void Registry::remove_logger(const std::string &name) {
  std::unique_lock lock(mtx_);
  if (loggers_.erase(name) == 0)
    throw std::logic_error("Removing non-existant logger '" + name + "'");
}

// throws std::logic_error
Logger Registry::get_logger(const std::string &name) const {
  std::shared_lock lock(mtx_);

  auto it = loggers_.find(name);
  if (it == loggers_.end())
    throw std::logic_error("Accessing non-existant logger '" + name + "'");

  return it->second;
}

Logger Registry::get_logger_or_default(const std::string &name,
                                       const std::string &default_name) const {
  std::shared_lock lock(mtx_);

  auto it = loggers_.find(name);
  if (it != loggers_.end()) return it->second;

  it = loggers_.find(default_name);
  if (it != loggers_.end()) return it->second;

  throw std::logic_error("Accessing non-existant logger '" + name + "'");
}

// throws std::logic_error
void Registry::update_logger(const std::string &name, const Logger &logger) {
  // this internally locks mtx_, so we call it before we lock it for good here
  const std::set<std::string> handlers_in_registry = get_handler_names();

  std::unique_lock lock(mtx_);

  // verify logger exists
  auto it = loggers_.find(name);
  if (it == loggers_.end())
    throw std::logic_error("Updating non-existant logger '" + name + "'");

  // verify that all the handlers the new logger brings exist
  for (const std::string &s : logger.get_handler_names())
    if (std::find(handlers_in_registry.begin(), handlers_in_registry.end(),
                  s) == handlers_in_registry.end())
      throw std::logic_error(std::string("Attaching unknown handler '") + s +
                             "'");

  it->second = logger;
}

std::set<std::string> Registry::get_logger_names() const {
  std::shared_lock lock(mtx_);

  std::set<std::string> result;
  for (const auto &pair : loggers_) result.emplace(pair.first);

  return result;
}

void Registry::flush_all_loggers(const std::string &dst) {
  std::unique_lock lock(mtx_);

  for (const auto &handler : handlers_) {
    handler.second->reopen(dst);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// handler CRUD
//
////////////////////////////////////////////////////////////////////////////////

// throws std::logic_error
void Registry::add_handler(std::string name, std::shared_ptr<Handler> handler) {
  std::unique_lock lock(mtx_);

  auto result = handlers_.emplace(name, handler);
  if (!result.second)
    throw std::logic_error("Duplicate handler '" + name + "'");
}

// throws std::logic_error
void Registry::remove_handler(std::string name) {
  std::unique_lock lock(mtx_);

  auto it = handlers_.find(name);
  if (it == handlers_.end())
    throw std::logic_error("Removing non-existant handler '" + name + "'");

  // first remove the handler from all loggers
  for (auto &pair : loggers_) pair.second.detach_handler(name, false);

  handlers_.erase(it);
}

// throws std::logic_error
std::shared_ptr<Handler> Registry::get_handler(const std::string &name) const {
  std::shared_lock lock(mtx_);

  auto it = handlers_.find(name);
  if (it == handlers_.end())
    throw std::logic_error("Accessing non-existant handler '" + name + "'");

  return it->second;
}

std::set<std::string> Registry::get_handler_names() const {
  std::shared_lock lock(mtx_);

  std::set<std::string> result;
  for (const auto &pair : handlers_) result.emplace(pair.first);

  return result;
}

bool Registry::is_handled(LogLevel level) const {
  std::shared_lock lock(mtx_);

  for (const auto &handler_pair : handlers_) {
    if (level <= handler_pair.second->get_level()) return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// high-level functions
//
////////////////////////////////////////////////////////////////////////////////

static std::string g_main_app_log_domain;

Logger Registry::get_logger_or_default(const std::string &name) const {
  return get_logger_or_default(name, g_main_app_log_domain);
}

void attach_handler_to_all_loggers(Registry &registry,
                                   std::string handler_name) {
  for (const std::string &logger_name : registry.get_logger_names()) {
    Logger logger = registry.get_logger(logger_name);
    logger.attach_handler(
        handler_name);  // no-op if handler is already attached
    registry.update_logger(logger_name, logger);
  }
}

void set_log_level_for_all_loggers(Registry &registry, LogLevel level) {
  for (const std::string &logger_name : registry.get_logger_names()) {
    Logger logger = registry.get_logger(logger_name);
    logger.set_level(level);
    registry.update_logger(logger_name, logger);
  }
}

void set_log_level_for_all_handlers(const Registry &registry, LogLevel level) {
  for (const std::string &handler_name : registry.get_handler_names()) {
    std::shared_ptr<Handler> handler = registry.get_handler(handler_name);
    handler->set_level(level);
  }
}

void set_timestamp_precision_for_all_loggers(Registry &registry,
                                             LogTimestampPrecision precision) {
  for (const std::string &logger_name : registry.get_logger_names()) {
    Logger logger = registry.get_logger(logger_name);
    logger.set_timestamp_precision(precision);
    registry.update_logger(logger_name, logger);
  }
}

void clear_registry(Registry &registry) {
  // wipe any existing loggers
  for (const std::string &name : registry.get_logger_names())
    registry.remove_logger(name);  // throws std::logic_error

  // wipe any existing handlers
  for (const std::string &name : registry.get_handler_names())
    registry.remove_handler(name.c_str());  // throws std::logic_error
}

std::ostream *get_default_logger_stream() { return &std::cerr; }

void create_main_log_handler(Registry &registry, const std::string &program,
                             const std::string &logging_folder,
                             bool format_messages,
                             bool use_os_log /*= false*/) {
#ifndef _WIN32
  // currently logging to OS log is only supported on Windows
  // (maybe in the future we'll add Syslog on the Unix side)
  harness_assert(use_os_log == false);
#endif

  // if logging folder is provided, make filelogger our main handler
  if (!logging_folder.empty()) {
    Path log_file = Path::make_path(logging_folder, program, "log");

    // throws std::runtime_error on failure to open file
    registry.add_handler(kMainLogHandler, std::make_shared<FileHandler>(
                                              log_file, format_messages));

    attach_handler_to_all_loggers(registry, kMainLogHandler);
    return;
  }

  // if user wants to log to OS log, make that our main handler
#ifdef _WIN32  // only Windows Eventlog is supported at the moment
  if (use_os_log) {
    // throws std::runtime_error on failure to init Windows Eventlog
    registry.add_handler(
        EventlogHandler::kDefaultName,
        std::make_shared<EventlogHandler>(
            format_messages, mysql_harness::logging::LogLevel::kWarning,
            false));

    attach_handler_to_all_loggers(registry, EventlogHandler::kDefaultName);
    return;
  }
#endif

  // fall back to logging to console
  {
    registry.add_handler(kMainConsoleHandler,
                         std::make_shared<StreamHandler>(
                             *get_default_logger_stream(), format_messages));
    attach_handler_to_all_loggers(registry, kMainConsoleHandler);
  }
}

void create_logger(Registry &registry, const LogLevel level,
                   const std::string &logger_name) {
  registry.create_logger(logger_name, level);
}

void create_module_loggers(Registry &registry, const LogLevel level,
                           const std::list<std::string> &modules,
                           const std::string &main_app) {
  // Create a logger for each module in the logging registry.
  for (const std::string &module : modules)
    registry.create_logger(module, level);  // throws std::logic_error

  // ensure that we have at least 1 logger registered: the main app logger
  g_main_app_log_domain = main_app;
  harness_assert(registry.get_logger_names().size() > 0);
}

LogLevel log_level_from_string(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  // Return its enum representation
  for (const auto &lvl : kLogLevels) {
    if (lvl.first == name) {
      return lvl.second;
    }
  }

  std::stringstream buffer;

  buffer << "Log level '" << name << "' is not valid. Valid values are: ";

  // Print the entries using a serial comma
  std::vector<std::string> alternatives;
  for (const auto &pair : kLogLevels) {
    alternatives.emplace_back(pair.first);
  }

  serial_comma(buffer, alternatives.begin(), alternatives.end());
  throw std::invalid_argument(buffer.str());
}

std::string log_level_to_string(LogLevel log_level) {
  for (const auto &lvl : kLogLevels) {
    if (lvl.second == log_level) {
      return std::string(lvl.first);
    }
  }

  return "unknown";
}

LogLevel get_default_log_level(const Config &config, bool raw_mode) {
  constexpr const char kNone[] = "";

  // aliases with shorter names
  constexpr const char *kLogLevel = mysql_harness::logging::options::kLevel;
  constexpr const char *kLogger = mysql_harness::logging::kConfigSectionLogger;

  std::string level_name;
  // extract log level from [logger] section/log level entry, if it exists
  if (config.has(kLogger) && config.get(kLogger, kNone).has(kLogLevel))
    level_name = config.get(kLogger, kNone).get(kLogLevel);
  // otherwise, set it to default
  else
    level_name = raw_mode ? mysql_harness::logging::kRawLogLevelName
                          : mysql_harness::logging::kDefaultLogLevelName;

  return log_level_from_string(level_name);  // throws std::invalid_argument
}

std::string get_default_log_filename(const Config &config) {
  constexpr const char kNone[] = "";

  // aliases with shorter names
  constexpr const char *kLogFilename =
      mysql_harness::logging::options::kFilename;
  constexpr const char *kLogger = mysql_harness::logging::kConfigSectionLogger;

  std::string log_filename;
  // extract log filename from [logger] section/log filename entry, if it exists
  // and is the legal device name depending on platform
  if (config.has(kLogger) && config.get(kLogger, kNone).has(kLogFilename) &&
      !config.get(kLogger, kNone).get(kLogFilename).empty())
    log_filename = config.get(kLogger, kNone).get(kLogFilename);
  // otherwise, set it to default
  else
    log_filename = mysql_harness::logging::kDefaultLogFilename;

  return log_filename;
}

LogTimestampPrecision log_timestamp_precision_from_string(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  // Return its enum representation
  for (const auto &prec : kLogTimestampPrecisions) {
    if (prec.first == name) return prec.second;
  }

  std::stringstream buffer;

  buffer << "Timestamp precision '" << name
         << "' is not valid. Valid values are: ";

  // Print the entries using a serial comma
  std::vector<std::string> alternatives;
  for (const auto &pair : kLogTimestampPrecisions) {
    alternatives.emplace_back(pair.first);
  }
  serial_comma(buffer, alternatives.begin(), alternatives.end());
  throw std::invalid_argument(buffer.str());
}

std::string log_timestamp_precision_to_string(LogTimestampPrecision tsp) {
  // Return its enum representation
  for (const auto &prec : kLogTimestampPrecisions) {
    if (prec.second == tsp) return std::string(prec.first);
  }

  return "unknown";
}

LogTimestampPrecision get_default_timestamp_precision(const Config &config) {
  constexpr const char kNone[] = "";

  // aliases with shorter names
  constexpr const char *kLogTimestampPrecision =
      mysql_harness::logging::options::kTimestampPrecision;
  constexpr const char *kLogger = mysql_harness::logging::kConfigSectionLogger;

  std::string precision;
  // extract precision from [logger] section/log, if it exists
  if (config.has(kLogger) &&
      config.get(kLogger, kNone).has(kLogTimestampPrecision))
    precision = config.get(kLogger, kNone).get(kLogTimestampPrecision);
  else
    precision = "second";

  return log_timestamp_precision_from_string(
      precision);  // throws std::invalid_argument
}

////////////////////////////////////////////////////////////////////////////////
//
// These functions are simple proxies that can be used by logger plugins
// to register their logging services. Note that they can only be called after
// logging facility has been initialized; but by the time the plugins are
// loaded, logging facility is already operational, so this is fine for plugin
// use.
//
////////////////////////////////////////////////////////////////////////////////

void register_handler(std::string name, std::shared_ptr<Handler> handler) {
  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  registry.add_handler(name, handler);
  attach_handler_to_all_loggers(registry, name);
}

void unregister_handler(std::string name) {
  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  registry.remove_handler(name);
}

void set_log_level_for_all_loggers(LogLevel level) {
  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  set_log_level_for_all_loggers(registry, level);
}

void set_log_level_for_all_handlers(LogLevel level) {
  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  set_log_level_for_all_handlers(registry, level);
}

bool log_level_is_handled(LogLevel level, const char *module) {
  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();

  Logger logger;
  try {
    logger = registry.get_logger(module);
  } catch (const std::logic_error &) {
    logger = registry.get_logger(mysql_harness::logging::g_main_app_log_domain);
  }

  return logger.is_handled(level);
}

void set_timestamp_precision_for_all_loggers(LogTimestampPrecision precision) {
  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  set_timestamp_precision_for_all_loggers(registry, precision);
}

}  // namespace logging

}  // namespace mysql_harness

////////////////////////////////////////////////////////////////
// Logging functions for use by plugins.

// We want to hide log_message(), because instead we want plugins to call
// log_error(), log_warning(), etc. However, those functions must be inline
// and are therefore defined in the header file - which means log_message()
// must have external linkage. So to solve this visibility conflict, we declare
// log_message() locally, inside of log_error(), log_warning(), etc.
//
// Normally, this would only leave us with having to define log_message() here.
// However, since we are building a DLL/DSO with this file, and since VS only
// allows __declspec(dllimport/dllexport) in function declarations, we must
// provide both declaration and definition.
extern "C" MY_ATTRIBUTE((format(printf, 3, 0))) void HARNESS_EXPORT
    log_message(LogLevel level, const char *module, const char *fmt,
                va_list ap);

extern "C" void log_message(LogLevel level, const char *module, const char *fmt,
                            va_list ap) {
  harness_assert(level <= LogLevel::kDebug);

  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  harness_assert(registry.is_ready());

  // Find the logger for the module
  // NOTE that we copy the logger. Even if some other thread removes this
  //      logger from registry, our call will still be valid. As for the
  //      case of handlers getting removed in the meantime, Logger::handle()
  //      handles this properly.
  Logger logger = registry.get_logger_or_default(
      module, mysql_harness::logging::g_main_app_log_domain);

  if (!logger.is_handled(level)) return;

  const auto now = std::chrono::system_clock::now();

  // Build the message
  char message[mysql_harness::logging::kLogMessageMaxSize];
  vsnprintf(message, sizeof(message), fmt, ap);

  // Build the record for the handler.
  Record record{level, stdx::this_process::get_id(), now, module, message};

  // Pass the record to the correct logger. The record should be
  // passed to only one logger since otherwise the handler can get
  // multiple calls, resulting in multiple log records.
  logger.handle(record);
}
