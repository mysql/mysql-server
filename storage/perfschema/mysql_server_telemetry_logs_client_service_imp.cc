/* Copyright (c) 2024 Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/mysql_server_telemetry_logs_client_service_imp.cc
  The performance schema implementation of server telemetry logs client service.
*/

#include "storage/perfschema/mysql_server_telemetry_logs_client_service_imp.h"
#include "storage/perfschema/mysql_server_telemetry_logs_service_imp.h"

#include <mysql/components/services/mysql_server_telemetry_logs_client_service.h>
#include <list>
#include <string>

#include "sql/auth/sql_security_ctx.h"
#include "sql/field.h"
#include "sql/pfs_priv_util.h"
#include "sql/sql_class.h"  // THD

#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

/* clang-format off */
/**

  @page PAGE_MYSQL_SERVER_TELEMETRY_LOGS_CLIENT_SERVICE Server telemetry logs client service
  Performance Schema server telemetry logs client service enables code instrumentation
  in order to emit OpenTelemetry logs in MySQL.

  @subpage TELEMETRY_LOGS_CLIENT_SERVICE_INTRODUCTION

  @subpage TELEMETRY_LOGS_CLIENT_SERVICE_INTERFACE
  
  @subpage TELEMETRY_LOGS_CLIENT_EXAMPLE_PLUGIN_COMPONENT

  @page TELEMETRY_LOGS_CLIENT_SERVICE_INTRODUCTION Service Introduction

  This service is named <i>mysql_server_telemetry_logs_client</i> and it exposes
  methods for instrumented code to:\n
  - @c register_logger_client   : register logger client
  - @c unregister_logger_client : unregister logger client
  - @c check_enabled            : check if log level for a given logger will be emitted
  - @c log_emit                 : emit log record with optional attributes

  @page TELEMETRY_LOGS_CLIENT_SERVICE_INTERFACE Service Interface

  This interface is provided to plugins/components or core server code, using it enables
  code instrumentation in order to generate and emit telemetry log records.

  @page TELEMETRY_LOGS_CLIENT_EXAMPLE_PLUGIN_COMPONENT  Example component

  Instrumented code that emits telemetry log records, can use either simple log interface
  (no attributes attached) or more complex one with string, double or int64 attributes attached
  to the record.
  As an example, see "components/test_server_telemetry_logs" test component source code,
  used to test this service.
*/
/* clang-format on */

BEGIN_SERVICE_IMPLEMENTATION(performance_schema,
                             mysql_server_telemetry_logs_client)
pfs_register_logger_client_v1, pfs_unregister_logger_client_v1,
    pfs_check_enabled_v1, pfs_log_emit_v1, END_SERVICE_IMPLEMENTATION();

#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
bool server_telemetry_logs_client_service_initialized = false;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */

void initialize_mysql_server_telemetry_logs_client_service() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  assert(!server_telemetry_logs_client_service_initialized);
  server_telemetry_logs_client_service_initialized = true;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

void cleanup_mysql_server_telemetry_logs_client_service() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  server_telemetry_logs_client_service_initialized = false;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

// copied from pfs.cc
/**
  Build the prefix name of a class of instruments in a category.
  For example, this function builds the string 'wait/sync/mutex/sql/' from
  a prefix 'wait/sync/mutex' and a category 'sql'.
  This prefix is used later to build each instrument name, such as
  'wait/sync/mutex/sql/LOCK_open'.
  @param prefix               Prefix for this class of instruments
  @param category             Category name
  @param [out] output         Buffer of length PFS_MAX_INFO_NAME_LENGTH.
  @param [out] output_length  Length of the resulting output string.
  @return 0 for success, non zero for errors
*/
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
static int build_prefix(const LEX_CSTRING *prefix, const char *category,
                        char *output, size_t *output_length) {
  const size_t len = strlen(category);
  char *out_ptr = output;
  const size_t prefix_length = prefix->length;

  if (unlikely((prefix_length + len + 2) >= PFS_MAX_FULL_PREFIX_NAME_LENGTH)) {
    pfs_print_error("build_prefix: prefix+category is too long <%s> <%s>\n",
                    prefix->str, category);
    return 1;
  }

  if (unlikely(strchr(category, '/') != nullptr)) {
    pfs_print_error("build_prefix: invalid category <%s>\n", category);
    return 1;
  }

  /* output = prefix + '/' + category + '/' */
  memcpy(out_ptr, prefix->str, prefix_length);
  out_ptr += prefix_length;
  if (len > 0) {
    *out_ptr = '/';
    out_ptr++;
    memcpy(out_ptr, category, len);
    out_ptr += len;
    *out_ptr = '/';
    out_ptr++;
  }
  *output_length = int(out_ptr - output);

  return 0;
}
#endif  // HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE

void pfs_register_logger_client_v1(PSI_logger_info_v1 *info [[maybe_unused]],
                                   size_t count [[maybe_unused]],
                                   const char *category [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  size_t prefix_length;
  if (unlikely(build_prefix(&logger_instrument_prefix, category, formatted_name,
                            &prefix_length)) ||
      !pfs_initialized) {
    for (; count > 0; count--, info++)
      if (info->m_key != nullptr) *(info->m_key) = 0;
    return;
  }

  for (; count > 0; count--, info++) {
    PSI_metric_key key;
    const size_t len = strlen(info->m_logger_name);
    const size_t full_length = prefix_length + len;

    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
      memcpy(formatted_name + prefix_length, info->m_logger_name, len);
      key = register_logger_class(formatted_name, (uint)full_length, info);
      if (key == UINT_MAX) {
        // duplicate detected, _lost count was not increased internally
        key = 0;
        if (pfs_enabled) ++logger_class_lost;
        pfs_print_error(
            "pfs_register_logger_client_v1: duplicate name <%s> <%s>\n",
            category, info->m_logger_name);
      }
    } else {
      key = 0;
      if (pfs_enabled) ++logger_class_lost;
      pfs_print_error(
          "pfs_register_logger_client_v1: name too long <%s> <%s>\n", category,
          info->m_logger_name);
    }
    if (info->m_key != nullptr) *(info->m_key) = key;
  }
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

void pfs_unregister_logger_client_v1(PSI_logger_info_v1 *info [[maybe_unused]],
                                     size_t count [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  for (; count > 0; count--, info++) {
    unregister_logger_class(info);
  }
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

PSI_logger *pfs_check_enabled_v1(PSI_logger_key key [[maybe_unused]],
                                 OTELLogLevel level [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  PFS_logger_class *klass = find_logger_class(key);
  if (klass == nullptr) return nullptr;
  if (level > klass->m_effective_level) return nullptr;
  return reinterpret_cast<PSI_logger *>(klass);
#else
  // Failure
  return nullptr;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

void pfs_log_emit_v1(PSI_logger *logger [[maybe_unused]],
                     OTELLogLevel level [[maybe_unused]],
                     const char *message [[maybe_unused]],
                     time_t timestamp [[maybe_unused]],
                     const log_attribute_t *attr_array [[maybe_unused]],
                     size_t attr_count [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  const auto *klass = reinterpret_cast<PFS_logger_class *>(logger);
  if (klass == nullptr) return;
  pfs_notify_logger_v1(logger, level, message, timestamp, attr_array,
                       attr_count);
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}
