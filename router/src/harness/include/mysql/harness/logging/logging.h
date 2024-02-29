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

/**
 * @file
 * @brief Logging interface for using and extending the logging subsystem.
 */

#ifndef MYSQL_HARNESS_LOGGING_INCLUDED
#define MYSQL_HARNESS_LOGGING_INCLUDED

#include "harness_export.h"
#include "mysql/harness/compiler_attributes.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/stdx/process.h"

#include <chrono>
#include <cstdarg>
#include <fstream>
#include <list>
#include <mutex>
#include <string>

namespace mysql_harness {

namespace logging {

/**
 * Max message length that can be logged; if message is longer,
 * it will be truncated to this length.
 */
const size_t kLogMessageMaxSize = 4096;

/**
 * option name used in config file (and later in configuration
 * object) to specify log level.
 */
namespace options {
constexpr char kFilename[] = "filename";
constexpr char kDestination[] = "destination";
constexpr char kLevel[] = "level";
constexpr char kTimestampPrecision[] = "timestamp_precision";
constexpr char kSinks[] = "sinks";
}  // namespace options

constexpr char kConfigSectionLogger[] = "logger";

constexpr char kNone[] = "";
/**
 * Special names reserved for "main" program logger. It will use one of the
 * two handlers, depending on whether logging_folder is empty or not.
 */
constexpr char kMainLogger[] = "main";
constexpr char kMainLogHandler[] = "main_log_handler";
constexpr char kMainConsoleHandler[] = "main_console_handler";

constexpr char kSqlLogger[] = "sql";
/**
 * Default log filename
 */
constexpr char kDefaultLogFilename[] = "mysqlrouter.log";
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
   * System message. These messages are always logged, such as state changes
   * during startup and shutdown.
   */
  kSystem,

  /**
   * Error message. indicate that something is not working properly and
   * actions need to be taken. However, the router continue
   * operating but the particular thread issuing the error message
   * might terminate.
   */
  kError,

  /**
   * Warning message. Indicate a potential problem that could require
   * actions, but does not cause a problem for the continuous operation
   * of the router.
   */
  kWarning,

  /**
   * Informational message. Information that can be useful to check
   * the behaviour of the router during normal operation.
   */
  kInfo,

  /**
   * Note level contains additional information over the normal informational
   * messages.
   */
  kNote,

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
 * Default log level written by the router to the config file on bootstrap.
 */
const LogLevel kDefaultLogLevelBootstrap = LogLevel::kInfo;

/**
 * Log level name for the default log level used by the router
 */
const char *const kDefaultLogLevelName = "warning";

/**
 * Log level name used in raw logging mode
 */
const char *const kRawLogLevelName = "info";

/**
 * Log timestamp precision values.
 */
enum class LogTimestampPrecision {
  // Second
  kSec = 0,

  // Millisecond
  kMilliSec = 3,

  // Microsecond
  kMicroSec = 6,

  // Nanosecond
  kNanoSec = 9,

  kNotSet  // Always higher than all other log precisions
};

/**
 * Log record containing information collected by the logging
 * system.
 *
 * The log record is passed to the handlers together with message.
 */
struct Record {
  LogLevel level;
  stdx::this_process::pid_type process_id;
  std::chrono::time_point<std::chrono::system_clock> created;
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
static inline void log_system(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_error(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_warning(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_info(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_note(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_debug(const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 1, 2);
static inline void log_custom(const LogLevel log_level, const char *fmt, ...)
    ATTRIBUTE_GCC_FORMAT(printf, 2, 3);

/*
 * Define inline functions that pick up the log domain defined for the module.
 */

static inline void log_system(const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(LogLevel::kSystem, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

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

static inline void log_note(const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(LogLevel::kNote, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
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

static inline void log_custom(const LogLevel log_level, const char *fmt, ...) {
  extern void HARNESS_EXPORT log_message(LogLevel level, const char *module,
                                         const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  log_message(log_level, MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

/** @} */

#ifdef __cplusplus
}
#endif

HARNESS_EXPORT bool log_level_is_handled(LogLevel level, const char *domain);

static inline bool log_level_is_handled(LogLevel level) {
  return log_level_is_handled(level, MYSQL_ROUTER_LOG_DOMAIN);
}

}  // namespace logging

}  // namespace mysql_harness

/**
 * convenience macro to avoid common boilerplate
 */
#define IMPORT_LOG_FUNCTIONS()               \
  using mysql_harness::logging::log_system;  \
  using mysql_harness::logging::log_error;   \
  using mysql_harness::logging::log_warning; \
  using mysql_harness::logging::log_info;    \
  using mysql_harness::logging::log_note;    \
  using mysql_harness::logging::log_debug;   \
  using mysql_harness::logging::log_custom;

#endif  // MYSQL_HARNESS_LOGGING_INCLUDED
