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

#ifndef MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED
#define MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED

#include "harness_export.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/logging.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>

namespace mysql_harness {

namespace logging {

// log level is stored in this hacky global variable; see place where it's
// set for exaplanation
HARNESS_EXPORT
extern std::string g_HACK_default_log_level;

class Handler;

class HARNESS_EXPORT Registry {
 public:
  const static std::map<std::string, LogLevel> kLogLevels;

  Registry() = default;
  Registry(const Registry &) = delete;
  Registry &operator=(const Registry &) = delete;

  ~Registry() = default;

  //----[ logger CRUD
  //]-----------------------------------------------------------

  /**
   * Create a logger in the internal registry
   *
   * @param name Logger id (log domain it services)
   * @param level Log level for logger
   *
   * @throws std::logic_error if there is a logger already registered with given
   *         module name
   */
  void create_logger(const std::string &name,
                     LogLevel level = LogLevel::kNotSet);

  /**
   * Remove a named logger from the internal registry
   *
   * @param name Logger id (log domain it services)
   *
   * @throws std::logic_error if there is no logger registered with given
   *         module name
   */
  void remove_logger(const std::string &name);

  /**
   * Return logger for particular module
   *
   * The reason why this function returns by value is thread-safety.
   *
   * @param name Logger id (log domain it services)
   *
   * @throws std::logic_error if no logger is registered for given module name
   */
  Logger get_logger(const std::string &name) const;

  /**
   * Update logger for particular module
   *
   * This function provides a thread-safe way of updating the Logger object in
   * the registry.
   *
   * @param name Logger id (log domain it services)
   * @param logger Logger object
   *
   * @throws std::logic_error if no logger is registered for given module name
   */
  void update_logger(const std::string &name, const Logger &logger);

  /**
   * Get the logger names (id's) from the internal registry
   */
  std::set<std::string> get_logger_names() const;

  //----[ handler CRUD
  //]----------------------------------------------------------

  /**
   * Add a handler to the internal registry
   *
   * @param name Handler id
   * @param handler Shared pointer to handler
   *
   * @throws std::logic_error if there is a handler already registered with
   * given module name
   */
  void add_handler(std::string name, std::shared_ptr<Handler> handler);

  /**
   * Remove handler from the internal registry
   *
   * @param name Handler id
   *
   * @throws std::logic_error if no handler is registered for given name
   */
  void remove_handler(std::string name);

  /**
   * Return handler in the internal registry
   *
   * @param name Handler id
   *
   * @throws std::logic_error if no handler is registered for given name
   */
  std::shared_ptr<Handler> get_handler(std::string name) const;

  /**
   * Get the handler names from the internal registry
   */
  std::set<std::string> get_handler_names() const;

  /**
   * Flag that the registry has been initialized
   *
   * This method should be called after log initialization is complete to
   * flag that logging facility is now available. Note that this is a
   * convenience flag - it does not directly affect the operation of Registry.
   * However, a logging function (i.e. log_message()) might want to query
   * this flag when called and do whatever it deems appropriate.
   */
  void set_ready() noexcept { ready_ = true; }

  /**
   * Query if logging facility is ready to use
   *
   * The exact meaning of this flag is not defined here, see description in
   * set_ready()
   */
  bool is_ready() const noexcept { return ready_; }

 private:
  mutable std::mutex mtx_;
  std::map<std::string, Logger> loggers_;  // key = log domain
  std::map<std::string, std::shared_ptr<Handler>>
      handlers_;  // key = handler id
  std::atomic<bool> ready_{false};

};  // class Registry

////////////////////////////////////////////////////////////////////////////////
//
// high-level utility functions
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Get default log level
 *
 * Fetches default log level set in the configuration file
 *
 * @param config Configuration items from configuration file
 *
 * @throws std::invalid_argument if [logger].level in configuration is invalid
 */
HARNESS_EXPORT
LogLevel get_default_log_level(const Config &config);

/**
 * Attach handler to all loggers
 *
 * @param registry Registry object, typically managed by DIM
 * @param name Logger id (log domain it services)
 */
HARNESS_EXPORT
void attach_handler_to_all_loggers(Registry &registry, std::string name);

/**
 * Set log levels for all the loggers to specified value
 *
 * @param registry Registry object, typically managed by DIM
 * @param level Log level for logger
 */
HARNESS_EXPORT
void set_log_level_for_all_loggers(Registry &registry, LogLevel level);

/**
 * Clear registry
 *
 * Removes all Loggers and removes all references to Handlers (they're held
 * as shared pointers, which may mean they will also be deleted)
 *
 * @param registry Registry object, typically managed by DIM
 */
HARNESS_EXPORT
void clear_registry(Registry &registry);

/**
 * Initialize logging facility
 *
 * Initializes logging facility by registering a logger for each given module.
 * Loggers will have their log level set to default log level ([logger].level)
 * set in the Router configuration .
 *
 * @note Loggers will not have any handlers attached, this needs to be done
 *       separately (see `create_main_logfile_handler()`)
 *
 * @param registry Registry object, typically managed by DIM
 * @param config Configuration items from configuration file
 * @param modules List of plugin names loaded
 * @param main_app_log_domain Log domain (logger id) to be used as the main
 *                            program logger. This logger must exist, because
 *                            log_*() functions might fail
 *
 * @throws std::invalid_argument (derived from logic_error) on invalid
 *         [logger].level
 * @throws std::logic_error on other error
 */
HARNESS_EXPORT
void init_loggers(Registry &registry, const mysql_harness::Config &config,
                  const std::list<std::string> &modules,
                  const std::string &main_app_log_domain);

/*
 * Initialize a logger and register it in the Registry..
 *
 * Register a logger for a given name and given log level.
 *
 * @param registry Registry object, typically managed by DIM
 * @param logger_name The name under which the logger is registered
 * @param log_level The log level.
 *
 * @throws std::logic_error
 */
HARNESS_EXPORT
void init_logger(Registry &registry, const mysql_harness::Config &config,
                 const std::string &logger_name);

/**
 * Initialize logfile handler
 *
 * Initializes handler which will handle application's log. This handler
 * will be attached to all currently-registered loggers. If `logging_folder`
 * is empty, handler will log messages to console, otherwise, logfile will be
 * used and its path and filename will be derived from `program` and
 * `logging_folder` parameters.
 *
 * @param registry Registry object, typically managed by DIM
 * @param program Name of the main program (Router)
 * @param logging_folder logging_folder provided in configuration file
 * @param format_messages If set to true, log messages will be formatted
 *        (prefixed with log level, timestamp, etc) before logging
 *
 * @throws std::runtime_error if opening log file fails
 */
HARNESS_EXPORT
void create_main_logfile_handler(Registry &registry, const std::string &program,
                                 const std::string &logging_folder,
                                 bool format_messages);

////////////////////////////////////////////////////////////////////////////////
//
// These functions are simple proxies that can be used by logger plugins
// to register their logging services. Note that they can only be called after
// logging facility has been initialized; but by the time the plugins are
// loaded, logging facility is already operational, so this is fine for plugin
// use.
//
////////////////////////////////////////////////////////////////////////////////

/** Set log level for all registered loggers. */
HARNESS_EXPORT
void set_log_level_for_all_loggers(LogLevel level);

/**
 * Register handler for all plugins.
 *
 * This will register a handler for all plugins that have been
 * registered with the logging subsystem (normally all plugins that
 * have been loaded by `Loader`).
 *
 * @param name The name under which handler is registered
 * @param handler Shared pointer to dynamically allocated handler.
 *
 * For example, to register a custom handler from a plugin, you would
 * do the following:
 *
 * @code
 * void init() {
 *   ...
 *   register_handler(std::make_shared<MyHandler>(...));
 *   ...
 * }
 * @endcode
 */
HARNESS_EXPORT
void register_handler(std::string name, std::shared_ptr<Handler> handler);

/**
 * Unregister a handler.
 *
 * This will unregister a previously registered handler.
 *
 * @param name name of registered handler.
 */
HARNESS_EXPORT
void unregister_handler(std::string name);

/**
 * Returns pointer to the default logger sink stream.
 */
HARNESS_EXPORT
std::ostream *get_default_logger_stream();

}  // namespace logging

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED */
