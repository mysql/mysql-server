/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define getpid GetCurrentProcessId
#endif

#include "my_compiler.h"

#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"

#include "common.h"
#include "dim.h"
#include "utilities.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <sstream>

using mysql_harness::Path;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::Logger;
using mysql_harness::logging::Record;
using mysql_harness::serial_comma;

// TODO one day we'll improve this and move it to a common spot
#define harness_assert(COND) \
  if (!(COND)) abort();

namespace mysql_harness {

namespace logging {

/*static*/
const std::map<std::string, LogLevel> Registry::kLogLevels{
    {"fatal", LogLevel::kFatal},     {"error", LogLevel::kError},
    {"warning", LogLevel::kWarning}, {"info", LogLevel::kInfo},
    {"debug", LogLevel::kDebug},
};

// log level is stored in this hacky global variable; see place where it's
// set for exaplanation
std::string g_HACK_default_log_level;

////////////////////////////////////////////////////////////////////////////////
//
// logger CRUD
//
////////////////////////////////////////////////////////////////////////////////

// throws std::logic_error
void Registry::create_logger(const std::string &name, LogLevel level) {
  std::lock_guard<std::mutex> lock(mtx_);
  auto result = loggers_.emplace(name, Logger(*this, level));
  if (result.second == false)
    throw std::logic_error("Duplicate logger '" + name + "'");
}

// throws std::logic_error
void Registry::remove_logger(const std::string &name) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (loggers_.erase(name) == 0)
    throw std::logic_error("Removing non-existant logger '" + name + "'");
}

// throws std::logic_error
Logger Registry::get_logger(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mtx_);

  auto it = loggers_.find(name);
  if (it == loggers_.end())
    throw std::logic_error("Accessing non-existant logger '" + name + "'");

  return it->second;
}

// throws std::logic_error
void Registry::update_logger(const std::string &name, const Logger &logger) {
  // this internally locks mtx_, so we call it before we lock it for good here
  const std::set<std::string> handlers_in_registry = get_handler_names();

  std::lock_guard<std::mutex> lock(mtx_);

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
  std::lock_guard<std::mutex> lock(mtx_);
  std::set<std::string> result;
  for (const auto &pair : loggers_) result.emplace(pair.first);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// handler CRUD
//
////////////////////////////////////////////////////////////////////////////////

// throws std::logic_error
void Registry::add_handler(std::string name, std::shared_ptr<Handler> handler) {
  std::lock_guard<std::mutex> lock(mtx_);

  auto result = handlers_.emplace(name, handler);
  if (!result.second)
    throw std::logic_error("Duplicate handler '" + name + "'");
}

// throws std::logic_error
void Registry::remove_handler(std::string name) {
  std::lock_guard<std::mutex> lock(mtx_);

  auto it = handlers_.find(name);
  if (it == handlers_.end())
    throw std::logic_error("Removing non-existant handler '" + name + "'");

  // first remove the handler from all loggers
  for (auto &pair : loggers_) pair.second.detach_handler(name, false);

  handlers_.erase(it);
}

// throws std::logic_error
std::shared_ptr<Handler> Registry::get_handler(std::string name) const {
  std::lock_guard<std::mutex> lock(mtx_);

  auto it = handlers_.find(name);
  if (it == handlers_.end())
    throw std::logic_error("Accessing non-existant handler '" + name + "'");

  return it->second;
}

std::set<std::string> Registry::get_handler_names() const {
  std::lock_guard<std::mutex> lock(mtx_);
  std::set<std::string> result;
  for (const auto &pair : handlers_) result.emplace(pair.first);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// high-level functions
//
////////////////////////////////////////////////////////////////////////////////

static std::string g_main_app_log_domain;

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

void clear_registry(Registry &registry) {
  // wipe any existing loggers
  for (const std::string &name : registry.get_logger_names())
    registry.remove_logger(name);  // throws std::logic_error

  // wipe any existing handlers
  for (const std::string &name : registry.get_handler_names())
    registry.remove_handler(name.c_str());  // throws std::logic_error
}

std::ostream *get_default_logger_stream() { return &std::cerr; }

void create_main_logfile_handler(Registry &registry, const std::string &program,
                                 const std::string &logging_folder,
                                 bool format_messages) {
  // Register the console as the handler if the logging folder is
  // undefined. Otherwise, register a file handler.
  if (logging_folder.empty()) {
    registry.add_handler(kMainConsoleHandler,
                         std::make_shared<StreamHandler>(
                             *get_default_logger_stream(), format_messages));
    attach_handler_to_all_loggers(registry, kMainConsoleHandler);
  } else {
    Path log_file = Path::make_path(logging_folder, program, "log");

    // throws std::runtime_error on failure to open file
    registry.add_handler(kMainLogHandler, std::make_shared<FileHandler>(
                                              log_file, format_messages));

    attach_handler_to_all_loggers(registry, kMainLogHandler);
  }
}

void init_logger(Registry &registry, const Config &config,
                 const std::string &logger_name) {
  registry.create_logger(logger_name, get_default_log_level(config));
}

void init_loggers(Registry &registry, const Config &config,
                  const std::list<std::string> &modules,
                  const std::string &main_app) {
  // Create a logger for each module in the logging registry.
  LogLevel level =
      get_default_log_level(config);  // throws std::invalid_argument
  for (const std::string &module : modules)
    registry.create_logger(module, level);  // throws std::logic_error

  // ensure that we have at least 1 logger registered: the main app logger
  g_main_app_log_domain = main_app;
  harness_assert(registry.get_logger_names().size() > 0);
}

LogLevel get_default_log_level(const Config &config) {
  // What we do here is an UGLY HACK. See code where g_HACK_default_log_level
  // is set for explanation.
  //
  // Before introducing this hack in Router v8.0, we used to obtain the default
  // log level from configuration, like so:
  //
  //   // We get the default log level from the configuration.
  //   auto level_name = config.get_default("log_level");
  //
  // Once we have a proper remedy, something analogous should be reinstated.

  // Obtain default log level (through UGLY HACK)
  (void)config;  // after we remove this hack, config will be used once again
  std::string level_name = mysql_harness::logging::g_HACK_default_log_level;

  std::transform(level_name.begin(), level_name.end(), level_name.begin(),
                 ::tolower);

  // Return its enum representation
  try {
    return Registry::kLogLevels.at(level_name);
  } catch (std::out_of_range &exc) {
    std::stringstream buffer;

    buffer << "Log level '" << level_name
           << "' is not valid. Valid values are: ";

    // Print the entries using a serial comma
    std::vector<std::string> alternatives;
    for (const auto &pair : Registry::kLogLevels)
      alternatives.push_back(pair.first);
    serial_comma(buffer, alternatives.begin(), alternatives.end());
    throw std::invalid_argument(buffer.str());
  }
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
extern "C" MY_ATTRIBUTE((format(printf, 3, 0))) void LOGGER_API
    log_message(LogLevel level, const char *module, const char *fmt,
                va_list ap);

extern "C" void log_message(LogLevel level, const char *module, const char *fmt,
                            va_list ap) {
  harness_assert(level <= LogLevel::kDebug);

  // get timestamp
  time_t now;
  time(&now);

  mysql_harness::logging::Registry &registry =
      mysql_harness::DIM::instance().get_LoggingRegistry();
  harness_assert(registry.is_ready());

  // Find the logger for the module
  // NOTE that we copy the logger. Even if some other thread removes this
  //      logger from registry, our call will still be valid. As for the
  //      case of handlers getting removed in the meantime, Logger::handle()
  //      handles this properly.
  Logger logger;
  try {
    logger = registry.get_logger(module);
  } catch (std::logic_error &) {
    // Logger is not registered for this module (log domain), so log as main
    // application domain instead (which should always be available)
    using mysql_harness::logging::g_main_app_log_domain;
    harness_assert(!g_main_app_log_domain.empty());
    try {
      logger = registry.get_logger(g_main_app_log_domain);
    } catch (std::logic_error &) {
      harness_assert(0);
    }

    // Complain that we're logging this elsewhere
    char msg[mysql_harness::logging::kLogMessageMaxSize];
    snprintf(msg, sizeof(msg),
             "Module '%s' not registered with logger - "
             "logging the following message as '%s' instead",
             module, g_main_app_log_domain.c_str());
    logger.handle(
        {LogLevel::kError, getpid(), now, g_main_app_log_domain, msg});

    // And switch log domain to main application domain for the original
    // log message
    module = g_main_app_log_domain.c_str();
  }

  // Build the message
  char message[mysql_harness::logging::kLogMessageMaxSize];
  vsnprintf(message, sizeof(message), fmt, ap);

  // Build the record for the handler.
  Record record{level, getpid(), now, module, message};

  // Pass the record to the correct logger. The record should be
  // passed to only one logger since otherwise the handler can get
  // multiple calls, resulting in multiple log records.
  logger.handle(record);
}
