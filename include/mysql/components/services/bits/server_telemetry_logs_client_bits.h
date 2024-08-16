/* Copyright (c) 2024, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SERVER_TELEMETRY_LOGS_CLIENTS_BITS_H
#define SERVER_TELEMETRY_LOGS_CLIENTS_BITS_H

#include <time.h>   // time_t
#include <cstddef>  // size_t
#include <cstdint>  // uint64_t

#include "server_telemetry_attribute_bits.h"

/** Opaque. */
struct PSI_logger;

/**
  Log levels as supported by opentelemetry-cpp (+ "none"), see:
  api/include/opentelemetry/logs/severity.h
  Some OTEL documentation pages also mention "fatal" level,
  but there is no support for it in the opentelemetry-cpp yet.
  Note that enum integer values may not match those of OTEL.
*/
enum OTELLogLevel { TLOG_NONE, TLOG_ERROR, TLOG_WARN, TLOG_INFO, TLOG_DEBUG };

typedef unsigned int PSI_logger_key;

constexpr size_t MAX_LOGGER_NAME_LEN = 63;
constexpr size_t MAX_LOG_ATTRIBUTES = 64;

/**
  Defines a logger from the side of instrumented code (log API client).
  Logger must be registered under a unique name before use,
  then you can use it to create and emit log records
  (i.e. instrument your code to generate OTEL logs).
  Logger can be enabled/disabled via P_S.setup_loggers table.
*/
struct PSI_logger_info_v1 {
  const char *m_logger_name;
  const char *m_description;
  /** Instrument flags. */
  unsigned int m_flags;
  PSI_logger_key *m_key;
};

/**
  Register telemetry logger client.

  @param info array of logger definitions
  @param count number of loggers in an array
  @param category common category name for set of loggers
*/
typedef void (*register_telemetry_logger_client_v1_t)(PSI_logger_info_v1 *info,
                                                      size_t count,
                                                      const char *category);

/**
  Unregister telemetry logger client.

  @param info array of logger definitions
  @param count array size
*/
typedef void (*unregister_telemetry_logger_client_v1_t)(
    PSI_logger_info_v1 *info, size_t count);

/**
  Check if the logger/log_level combination is currently enabled.

  @param key registered logger key
  @param level log level to be checked
  @retval logger pointer, NULL on failure
*/
typedef PSI_logger *(*check_enabled_telemetry_logger_client_v1_t)(
    PSI_logger_key key, OTELLogLevel level);

/**
  Emit telemetry log record.

  @param logger logger object
  @param level log level
  @param message message string to be logged
  @param timestamp log timestamp
  @param attr_array list of log record attributes (NULL for no attributes)
  @param attr_count size of attributes array (0 for no attributes)
*/
typedef void (*log_emit_telemetry_logger_client_v1_t)(
    PSI_logger *logger, OTELLogLevel level, const char *message,
    time_t timestamp, const log_attribute_t *attr_array, size_t attr_count);

/**
  @def PSI_LOGGER_CLIENT_VERSION_1
  Performance Schema Logger Client Interface number for version 1.
  This version is supported.
*/
#define PSI_LOGGER_CLIENT_VERSION_1 1

/**
  @def PSI_CURRENT_LOGGER_CLIENT_VERSION
  Performance Schema Logger Client Interface number for the most recent version.
  The most current version is @c PSI_METRIC_VERSION_1
*/
#define PSI_CURRENT_LOGGER_CLIENT_VERSION 1

#endif /* SERVER_TELEMETRY_LOGS_BITS_H */
