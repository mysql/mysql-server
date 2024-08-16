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

#ifndef MYSQL_TELEMETRY_LOGS_CLIENT_H
#define MYSQL_TELEMETRY_LOGS_CLIENT_H

/**
  @file include/mysql/psi/mysql_telemetry_logs_client.h
  Instrumentation helpers for telemetry logs client.
*/

#include <cstring>  // strlen

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "mysql/components/services/bits/server_telemetry_logs_client_bits.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_LOGS_CALL() as direct call. */
#include "pfs_telemetry_logs_client_provider.h"  // IWYU pragma: keep
#endif

#ifndef PSI_LOGS_CLIENT_CALL
#define PSI_LOGS_CLIENT_CALL(M) \
  SERVICE_PLACEHOLDER(mysql_server_telemetry_logs_client)->M
#endif

/**
  @defgroup psi_api_logs_client Logger Client Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def mysql_log_client_register(P1, P2, P3)
  Registration of logger clients.
*/
#define mysql_log_client_register(P1, P2, P3) \
  inline_mysql_log_client_register(P1, P2, P3)

static inline void inline_mysql_log_client_register(
    PSI_logger_info_v1 *info [[maybe_unused]], size_t count [[maybe_unused]],
    const char *category [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  PSI_LOGS_CLIENT_CALL(register_logger_client)(info, count, category);
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

/**
  @def mysql_log_client_unregister(P1, P2)
  Logger client unregistration.
*/
#define mysql_log_client_unregister(P1, P2) \
  inline_mysql_log_client_unregister(P1, P2)

static inline void inline_mysql_log_client_unregister(PSI_logger_info_v1 *info
                                                      [[maybe_unused]],
                                                      size_t count
                                                      [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  PSI_LOGS_CLIENT_CALL(unregister_logger_client)(info, count);
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

/**
  @def mysql_log_client_check_enabled(P1, P2)
  Logger client check if log level is enabled for this logger.
*/
#define mysql_log_client_check_enabled(P1, P2) \
  inline_mysql_log_client_check_enabled(P1, P2)

static inline PSI_logger *inline_mysql_log_client_check_enabled(
    PSI_logger_key key [[maybe_unused]], OTELLogLevel level [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  return PSI_LOGS_CLIENT_CALL(check_enabled)(key, level);
#else
  return nullptr;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

/**
  @def mysql_log_client_log(P1, P2, P3, P4, P5, P6)
  Emit log record.
*/
#define mysql_log_client_log(P1, P2, P3, P4, P5, P6) \
  inline_mysql_log_client_log(P1, P2, P3, P4, P5, P6)

static inline void inline_mysql_log_client_log(
    PSI_logger *logger [[maybe_unused]], OTELLogLevel level [[maybe_unused]],
    const char *message [[maybe_unused]], time_t timestamp [[maybe_unused]],
    const log_attribute_t *attr_array [[maybe_unused]],
    size_t attr_count [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  PSI_LOGS_CLIENT_CALL(log_emit)
  (logger, level, message, timestamp, attr_array, attr_count);
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

#ifdef __cplusplus

/**
  @class PSI_LogRecord
  C++ wrapper for emitting a telemetry log record.
*/
class PSI_LogRecord {
 public:
  PSI_LogRecord(PSI_logger_key key, OTELLogLevel level, const char *message)
      : m_level(level), m_message(message) {
    m_psi = mysql_log_client_check_enabled(key, level);
  }

  bool check_enabled() const { return m_psi != nullptr; }

  void add_attribute_bool(const char *name, bool value) {
    if (m_attr_count >= MAX_LOG_ATTRIBUTES) return;
    m_attrs[m_attr_count++].set_bool(name, value);
  }

  void add_attribute_string(const char *name, const char *value) {
    if (m_attr_count >= MAX_LOG_ATTRIBUTES) return;
    m_attrs[m_attr_count++].set_string(name, value);
  }

  void add_attribute_string_view(const char *name, const char *value,
                                 size_t len) {
    if (m_attr_count >= MAX_LOG_ATTRIBUTES) return;
    m_attrs[m_attr_count++].set_string_view(name, value, len);
  }

  void add_attribute_double(const char *name, double value) {
    if (m_attr_count >= MAX_LOG_ATTRIBUTES) return;
    m_attrs[m_attr_count++].set_double(name, value);
  }

  void add_attribute_uint64(const char *name, uint64_t value) {
    if (m_attr_count >= MAX_LOG_ATTRIBUTES) return;
    m_attrs[m_attr_count++].set_uint64(name, value);
  }

  void emit() {
    mysql_log_client_log(m_psi, m_level, m_message, time(nullptr), m_attrs,
                         m_attr_count);
  }

 protected:
  PSI_logger *m_psi{nullptr};
  PSI_logger_key m_logger_key;
  OTELLogLevel m_level;
  const char *m_message;
  log_attribute_t m_attrs[MAX_LOG_ATTRIBUTES];
  size_t m_attr_count{0};
};

/**
  @class PSI_SimpleLogger
  C++ wrapper for emitting one or more simple (no attributes)
  telemetry log records.
*/
class PSI_SimpleLogger {
 public:
  explicit PSI_SimpleLogger(PSI_logger_key key) : m_logger_key(key) {}

  void error(const char *message) const {
    const OTELLogLevel level(OTELLogLevel::TLOG_WARN);
    PSI_logger *psi = mysql_log_client_check_enabled(m_logger_key, level);
    if (psi != nullptr)
      mysql_log_client_log(psi, level, message, time(nullptr), nullptr, 0);
  }

  void warn(const char *message) const {
    const OTELLogLevel level(OTELLogLevel::TLOG_WARN);
    PSI_logger *psi = mysql_log_client_check_enabled(m_logger_key, level);
    if (psi != nullptr)
      mysql_log_client_log(psi, level, message, time(nullptr), nullptr, 0);
  }

  void info(const char *message) const {
    const OTELLogLevel level(OTELLogLevel::TLOG_INFO);
    PSI_logger *psi = mysql_log_client_check_enabled(m_logger_key, level);
    if (psi != nullptr)
      mysql_log_client_log(psi, level, message, time(nullptr), nullptr, 0);
  }

  void debug(const char *message) const {
    const OTELLogLevel level(OTELLogLevel::TLOG_DEBUG);
    PSI_logger *psi = mysql_log_client_check_enabled(m_logger_key, level);
    if (psi != nullptr)
      mysql_log_client_log(psi, level, message, time(nullptr), nullptr, 0);
  }

 public:
  PSI_logger_key m_logger_key;
};

#endif

/** @} (end of group psi_api_logs_client) */

#endif
