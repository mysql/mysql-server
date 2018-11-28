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

/**
 * @file
 * @brief Logging interface for using and extending the logging subsystem.
 */

#ifndef MYSQL_HARNESS_LOGGING_INCLUDED
#define MYSQL_HARNESS_LOGGING_INCLUDED

#include "harness_export.h"
#include "mysql/harness/compiler_attributes.h"
#include "mysql/harness/filesystem.h"

#include <cstdarg>
#include <fstream>
#include <list>
#include <mutex>
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
typedef int pid_t; /* getpid() */
#endif

namespace mysql_harness {

namespace logging {

/**
 * Max message length that can be logged; if message is longer,
 * it will be truncated to this length.
 */
const size_t kLogMessageMaxSize = 4096;

/**
 * Section name and option name used in config file (and later in configuration
 * object) to specify log level, best explained by example:
 *
 *  vvvvvv------------------ kConfigSectionLogger
 * [logger]
 * level = DEBUG
 * ^^^^^-------------------- kConfigOptionLogLevel
 */
constexpr char kConfigOptionLogLevel[] = "level";
constexpr char kConfigSectionLogger[] = "logger";

/**
 * Special names reserved for "main" program logger. It will use one of the
 * two handlers, depending on whether logging_folder is empty or not.
 */
constexpr char kMainLogger[] = "main";
constexpr char kMainLogHandler[] = "main_log_handler";
constexpr char kMainConsoleHandler[] = "main_console_handler";

constexpr char kSqlLogger[] = "sql";
/**
 * Log level values.
 *
 * Log levels are ordered numerically from most important (lowest
 * value) to least important (highest value).
 */
enum class LogLevel {
  /** Fatal failure. Router usually exits after logging this. */
  kFatal,

  /**
   * Error message. indicate that something is not working properly and
   * actions need to be taken. However, the router continue
   * operating but the particular thread issuing the error message
   * might terminate.
   */
  kError,

  /**
   * Warning message. Indicate a potential problem that could require
   * actions, but does not cause a problem for the continous operation
   * of the router.
   */
  kWarning,

  /**
   * Informational message. Information that can be useful to check
   * the behaviour of the router during normal operation.
   */
  kInfo,

  /**
   * Debug message. Message contain internal details that can be
   * useful for debugging problematic situations, especially regarding
   * the router itself.
   */
  kDebug,

  kNotSet  // Always higher than all other log messages
};

/**
 * Default log level used by the router.
 */
const LogLevel kDefaultLogLevel = LogLevel::kWarning;

/**
 * Log level name for the default log level used by the router
 */
const char *const kDefaultLogLevelName = "warning";

/**
 * Log level name used in raw logging mode
 */
const char *const kRawLogLevelName = "info";

/**
 * Log record containing information collected by the logging
 * system.
 *
 * The log record is passed to the handlers together with message.
 */
struct Record {
  LogLevel level;
  pid_t process_id;
  time_t created;
  std::string domain;
  std::string message;
};

/**
 * Log message for the domain.
 *
 * This will log an error, warning, informational, or debug message
 * for the given domain. The domain have to be be registered before
 * anything is being logged. The `Loader` uses the plugin name as the
 * domain name, so normally you should provide the plugin name as the
 * first argument to this function.
 *
 * @param name Domain name to use when logging message.
 *
 * @param fmt `printf`-style format string, with arguments following.
 */
/** @{ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pre-processor symbol containing the name of the log domain. If not
 * defined explicitly when compiling, it will be an empty string, which
 * means that it logs to the top log domain.
 */

#ifndef MYSQL_ROUTER_LOG_DOMAIN
#define MYSQL_ROUTER_LOG_DOMAIN ""
#endif

/*
 * We need to declare these first, because __attribute__ can only be used in
 * declarations.
 */
static inline void log_error(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_warning(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_info(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_debug(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);

/*
 * Define inline functions that pick up the log domain defined for the module.
 */

static inline void log_error(const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(LogLevel::kError, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

static inline void log_warning(const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(LogLevel::kWarning, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

static inline void log_info(const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(LogLevel::kInfo, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

static inline void log_debug(const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(LogLevel::kDebug, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

  /** @} */

#ifdef __cplusplus
}
#endif

}  // namespace logging

}  // namespace mysql_harness

/**
 * convenience macro to avoid common boilerplate
 */
#define IMPORT_LOG_FUNCTIONS()               \
  using mysql_harness::logging::log_error;   \
  using mysql_harness::logging::log_warning; \
  using mysql_harness::logging::log_info;    \
  using mysql_harness::logging::log_debug;

#endif  // MYSQL_HARNESS_LOGGING_INCLUDED
