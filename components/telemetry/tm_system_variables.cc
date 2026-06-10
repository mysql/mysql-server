/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tm_system_variables.h"
#include "tls_ciphers.h"
#include "tm_control.h"

#include "tm_log.h"
#include "tm_required_services.h"
#include "tm_setup_otel.h"

namespace telemetry {

struct System_variable {
  const char *m_name;
  const char *m_desc;
  int m_flags;
  mysql_sys_var_check_func m_check_func;
  mysql_sys_var_update_func m_update_func;
  void *m_check_args;
  void *m_val_ptr;
  bool m_registered;
};

static const char *sysvar_prefix = "telemetry";

// TODO: cleanup in components/services/component_sys_var_service.h
// - BOOL_CHECK_ARG(type)
// - STR_CHECK_ARG(type)
// - INTEGRAL_CHECK_ARG(type)
// - ENUM_CHECK_ARG(type)

struct bool_check_arg_s {
  bool def_val;
};

struct str_check_arg_s {
  const char *def_val;
};

struct long_check_arg_s {
  long def_val;
  long min_val;
  long max_val;
  long blk_sz;
};

struct ulong_check_arg_s {
  ulong def_val;
  ulong min_val;
  ulong max_val;
  ulong blk_sz;
};

struct enum_check_arg_s {
  unsigned long def_val;
  TYPE_LIB *typelib;
};

/*
 * MAINTAINER:
 * Please also check file:
 * plugin/telemetry_client/tm_client_options.cc
 */
static int validate_tls_cipher(const char *actual, const char *supported,
                               std::vector<std::string> &all_illegal) {
  int found = 0;

  all_illegal.clear();

  /* "FOO", "FOO:BAR" */
  std::string ciphers{actual};

  /* ":AAA:BBB:CCC:" */
  std::string haystack{":"};
  haystack.append(supported);
  haystack.append(":");

  /* "FOO", "BAR" */
  std::string needle;
  /* ":FOO:", ":BAR:" */
  std::string delimited_needle;

  std::string::size_type delim;

  while (!ciphers.empty()) {
    delim = ciphers.find(':');

    if (delim != std::string::npos) {
      needle = ciphers.substr(0, delim);
      ciphers = ciphers.substr(delim + 1);
    } else {
      needle = ciphers;
      ciphers.clear();
    }

    delimited_needle = ":";
    delimited_needle.append(needle);
    delimited_needle.append(":");

    /*
     * IMPORTANT:
     * We search for delimited needle ":XXX-YYY:"
     * inside delimited haystack ":AAA-BBB-CCC:XXX-YYY-ZZZ:",
     * because we do not want to be confused by "XXX-YYY" found in
     * "XXX-YYY-ZZZ".
     */
    if (haystack.find(delimited_needle) == std::string::npos) {
      all_illegal.push_back(needle);
    } else {
      found++;
    }
  }

  if (found == 0) {
    all_illegal.push_back("(empty)");
  }

  if (!all_illegal.empty()) {
    return 1;
  }

  return 0;
}

static int check_tls_cipher(const char *tls_version, const char *actual,
                            const char *expected) {
  std::vector<std::string> all_illegal;
  int rc;
  rc = validate_tls_cipher(actual, expected, all_illegal);
  if (rc != 0) {
    for (const std::string &illegal : all_illegal) {
      log_error("%s: Illegal %s cipher found '%s'", component_name, tls_version,
                illegal.c_str());
    }
  }

  return rc;
}

static int check_tls12_cipher(MYSQL_THD /* thd */, SYS_VAR * /* var */,
                              void *save, struct st_mysql_value *value) {
  char buffer[1];  // unused
  const char *actual;
  int len = 0;

  actual = value->val_str(value, buffer, &len);

  auto *save_string = static_cast<const char **>(save);
  *save_string = actual;

  return check_tls_cipher("TLS 1.2", actual, default_tls12_ciphers);
}

static int check_tls13_cipher_suite(MYSQL_THD /* thd */, SYS_VAR * /* var */,
                                    void *save, struct st_mysql_value *value) {
  char buffer[1];  // unused
  const char *actual;
  int len = 0;

  actual = value->val_str(value, buffer, &len);

  auto *save_string = static_cast<const char **>(save);
  *save_string = actual;

  return check_tls_cipher("TLS 1.3", actual, default_tls13_ciphers);
}

static const char *otlp_tls[] = {
    /* 0 */ "default", /* OTLP_TLS_DEFAULT */
    /* 1 */ "1.2",     /* OTLP_TLS_12 */
    /* 2 */ "1.3",     /* OTLP_TLS_13 */
    /* EOF */ nullptr};

static TYPE_LIB otlp_tls_typelib = {3, "otlp_tls", otlp_tls, nullptr};

static enum_check_arg_s check_args_exporter_otlp_tls = {OTLP_TLS_DEFAULT,
                                                        &otlp_tls_typelib};

static bool_check_arg_s check_args_trace_enabled = {false};

bool sv_trace_enabled = false;

System_variable sysvar_trace_enabled = {
    /* Name */
    "trace_enabled",
    /* Description */
    "Emit server telemetry trace data",
    /* Flags */
    PLUGIN_VAR_BOOL,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_trace_enabled,
    /* Value */
    &sv_trace_enabled,
    /* Registered */
    false};

static bool_check_arg_s check_args_metrics_enabled = {false};

bool sv_metrics_enabled = false;

/*
  Note about PLUGIN_VAR_READONLY.

  Installing / uninstalling the meter providers,
  with the associated metric readers and exporters,
  is an heavy operation.

  Enabling / disabling the whole metrics sub system dynamically
  is not supported: instead, UNINSTALL then INSTALL the
  component again, with different configuration options.

  Note that individual meters can be enabled / disabled
  dynamically instead, by using:

  UPDATE performance_schema.setup_meters
    SET ENABLED = ...
*/
System_variable sysvar_metrics_enabled = {
    /* Name */
    "metrics_enabled",
    /* Description */
    "Emit server telemetry metrics data",
    /* Flags */
    PLUGIN_VAR_BOOL | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_metrics_enabled,
    /* Value */
    &sv_metrics_enabled,
    /* Registered */
    false};

static bool_check_arg_s check_args_log_enabled = {false};

bool sv_log_enabled = false;

static void update_log_enabled(MYSQL_THD /* thd */, SYS_VAR * /* var */,
                               void * /* save */, const void *value) {
  const auto *typed_value_ptr = reinterpret_cast<const bool *>(value);
  sv_log_enabled = *typed_value_ptr;
  emit_control_log(sv_trace_enabled, sv_metrics_enabled, sv_log_enabled,
                   k_reason_configuration);
}

System_variable sysvar_log_enabled = {
    /* Name */
    "log_enabled",
    /* Description */
    "Emit server telemetry log data",
    /* Flags */
    PLUGIN_VAR_BOOL,
    /* Check */
    nullptr,
    /* Update */
    update_log_enabled,
    /* Check args */
    &check_args_log_enabled,
    /* Value */
    &sv_log_enabled,
    /* Registered */
    false};

static bool_check_arg_s check_args_query_text_enabled = {true};

bool sv_query_text_enabled = false;

System_variable sysvar_query_text_enabled = {
    /* Name */
    "query_text_enabled",
    /* Description */
    "Capture the query text in the server telemetry trace data",
    /* Flags */
    PLUGIN_VAR_BOOL,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_query_text_enabled,
    /* Value */
    &sv_query_text_enabled,
    /* Registered */
    false};

static const char *otel_log_level_enums[] = {
    /* 0 */ "silent",
    /* 1 */ "error",
    /* 2 */ "warning",
    /* 3 */ "info",
    /* 4 */ "debug",
    /* EOF */ nullptr};

static TYPE_LIB otel_log_level_typelib = {5, "otel_log_level_typelib",
                                          otel_log_level_enums, nullptr};

static enum_check_arg_s check_args_otel_log_level = {OTEL_LOG_LEVEL_INFO,
                                                     &otel_log_level_typelib};

ulong sv_otel_log_level = OTEL_LOG_LEVEL_INFO;

static void update_log_level(MYSQL_THD /* thd */, SYS_VAR * /* var */,
                             void * /* save */, const void *value) {
  const auto *typed_value_ptr = reinterpret_cast<const ulong *>(value);
  sv_otel_log_level = *typed_value_ptr;
  setup_internal_logger_level(sv_otel_log_level);
}

/** OTEL_EXPORTER_OTLP_TRACES_PROTOCOL */
System_variable sysvar_otel_log_level = {
    /* Name */
    "otel_log_level",
    /* Description */
    "telemetry log level: silent, error, warning, info or debug",
    /* Flags */
    PLUGIN_VAR_ENUM,
    /* Check */
    nullptr,
    /* Update */
    update_log_level,
    /* Check args */
    &check_args_otel_log_level,
    /* Value */
    &sv_otel_log_level,
    /* Registered */
    false};

static str_check_arg_s check_args_resource_attributes = {""};

char *sv_otel_resource_attributes = nullptr;

/** OTEL_RESOURCE_ATTRIBUTES */
System_variable sysvar_otel_resource_attributes = {
    /* Name */
    "otel_resource_attributes",
    /* Description */
    "Key-value pairs, in W3C Baggage format, to identify the MySQL server "
    "instance",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_resource_attributes,
    /* Value */
    &sv_otel_resource_attributes,
    /* Registered */
    false};

static const char *otlp_protocol_enums[] = {
    /* 0 */ "http/protobuf",
    /* 1 */ "http/json",
    /* EOF */ nullptr};

static TYPE_LIB otlp_protocol_typelib = {2, "otlp_protocol_typelib",
                                         otlp_protocol_enums, nullptr};

static enum_check_arg_s check_args_exporter_otlp_protocol = {
    OTLP_PROTOCOL_HTTP_PROTOBUF, &otlp_protocol_typelib};

ulong sv_otel_exporter_otlp_traces_protocol = 0;

/** OTEL_EXPORTER_OTLP_TRACES_PROTOCOL */
System_variable sysvar_otel_exporter_otlp_traces_protocol = {
    /* Name */
    "otel_exporter_otlp_traces_protocol",
    /* Description */
    "http/protobuf or http/json",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_protocol,
    /* Value */
    &sv_otel_exporter_otlp_traces_protocol,
    /* Registered */
    false};

// OTLP HTTP collector
static str_check_arg_s check_args_exporter_otlp_traces_endpoint = {""};

char *sv_otel_exporter_otlp_traces_endpoint = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_ENDPOINT */
System_variable sysvar_otel_exporter_otlp_traces_endpoint = {
    /* Name */
    "otel_exporter_otlp_traces_endpoint",
    /* Description */
    "Target URL to send server telemetry spans to",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_endpoint,
    /* Value */
    &sv_otel_exporter_otlp_traces_endpoint,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_network_namespace = {""};

char *sv_otel_exporter_otlp_traces_network_namespace = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_NETWORK_NAMESPACE */
System_variable sysvar_otel_exporter_otlp_traces_network_namespace = {
    /* Name */
    "otel_exporter_otlp_traces_network_namespace",
    /* Description */
    "Network namespace to use to send server telemetry spans",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_network_namespace,
    /* Value */
    &sv_otel_exporter_otlp_traces_network_namespace,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_certificates = {""};

char *sv_otel_exporter_otlp_traces_certificates = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CERTIFICATE */
System_variable sysvar_otel_exporter_otlp_traces_certificates = {
    /* Name */
    "otel_exporter_otlp_traces_certificates",
    /* Description */
    "Path to SSL CA certificates",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_certificates,
    /* Value */
    &sv_otel_exporter_otlp_traces_certificates,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_client_key = {""};

char *sv_otel_exporter_otlp_traces_client_key = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CLIENT_KEY */
System_variable sysvar_otel_exporter_otlp_traces_client_key = {
    /* Name */
    "otel_exporter_otlp_traces_client_key",
    /* Description */
    "Path to SSL client key",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_client_key,
    /* Value */
    &sv_otel_exporter_otlp_traces_client_key,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_client_certificates = {
    ""};

char *sv_otel_exporter_otlp_traces_client_certificates = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CLIENT_CERTIFICATE */
System_variable sysvar_otel_exporter_otlp_traces_client_certificates = {
    /* Name */
    "otel_exporter_otlp_traces_client_certificates",
    /* Description */
    "Path to SSL client certificates",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_client_certificates,
    /* Value */
    &sv_otel_exporter_otlp_traces_client_certificates,
    /* Registered */
    false};

unsigned long sv_otel_exporter_otlp_traces_min_tls = OTLP_TLS_DEFAULT;

/** OTEL_EXPORTER_OTLP_TRACES_MIN_TLS */
System_variable sysvar_otel_exporter_otlp_traces_min_tls = {
    /* Name */
    "otel_exporter_otlp_traces_min_tls",
    /* Description */
    "Minimum TLS version",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_tls,
    /* Value */
    &sv_otel_exporter_otlp_traces_min_tls,
    /* Registered */
    false};

unsigned long sv_otel_exporter_otlp_traces_max_tls = OTLP_TLS_DEFAULT;

/** OTEL_EXPORTER_OTLP_TRACES_MAX_TLS */
System_variable sysvar_otel_exporter_otlp_traces_max_tls = {
    /* Name */
    "otel_exporter_otlp_traces_max_tls",
    /* Description */
    "Maximum TLS version",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_tls,
    /* Value */
    &sv_otel_exporter_otlp_traces_max_tls,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_cipher = {
    default_tls12_ciphers};

char *sv_otel_exporter_otlp_traces_cipher = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CIPHER */
System_variable sysvar_otel_exporter_otlp_traces_cipher = {
    /* Name */
    "otel_exporter_otlp_traces_cipher",
    /* Description */
    "TLS Cipher (for TLS 1.2)",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    check_tls12_cipher,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_cipher,
    /* Value */
    &sv_otel_exporter_otlp_traces_cipher,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_cipher_suite = {
    default_tls13_ciphers};

char *sv_otel_exporter_otlp_traces_cipher_suite = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CIPHER_SUITE */
System_variable sysvar_otel_exporter_otlp_traces_cipher_suite = {
    /* Name */
    "otel_exporter_otlp_traces_cipher_suite",
    /* Description */
    "TLS Cipher (for TLS 1.3)",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    check_tls13_cipher_suite,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_cipher_suite,
    /* Value */
    &sv_otel_exporter_otlp_traces_cipher_suite,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_headers = {""};

char *sv_otel_exporter_otlp_traces_headers = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_HEADERS */
System_variable sysvar_otel_exporter_otlp_traces_headers = {
    /* Name */
    "otel_exporter_otlp_traces_headers",
    /* Description */
    "Key-value pairs as header associated with http requests",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_headers,
    /* Value */
    &sv_otel_exporter_otlp_traces_headers,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_traces_secret_headers = {""};

char *sv_otel_exporter_otlp_traces_secret_headers = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_SECRET_HEADERS */
System_variable sysvar_otel_exporter_otlp_traces_secret_headers = {
    /* Name */
    "otel_exporter_otlp_traces_secret_headers",
    /* Description */
    "Named secret, to find key-value pairs as header associated with http "
    "requests",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_secret_headers,
    /* Value */
    &sv_otel_exporter_otlp_traces_secret_headers,
    /* Registered */
    false};

static const char *otlp_compression[] = {
    /* 0 */ "none",
    /* 1 */ "gzip",
    /* EOF */ nullptr};

static TYPE_LIB otlp_compression_typelib = {2, "otlp_compression",
                                            otlp_compression, nullptr};

static enum_check_arg_s check_args_exporter_otlp_compression = {
    OTLP_COMPRESSION_NONE, &otlp_compression_typelib};

ulong sv_otel_exporter_otlp_traces_compression = 0;

/** OTEL_EXPORTER_OTLP_TRACES_COMPRESSION */
System_variable sysvar_otel_exporter_otlp_traces_compression = {
    /* Name */
    "otel_exporter_otlp_traces_compression",
    /* Description */
    "none or gzip",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_compression,
    /* Value */
    &sv_otel_exporter_otlp_traces_compression,
    /* Registered */
    false};

static long_check_arg_s check_args_exporter_otlp_traces_timeout = {
    /* default 10 sec */
    10000,
    /* min 1 sec */
    1000,
    /* max 5 min */
    300000,
    /* block size 1 sec */
    1000};

long sv_otel_exporter_otlp_traces_timeout = 0;

/** OTEL_EXPORTER_OTLP_TRACES_TIMEOUT */
System_variable sysvar_otel_exporter_otlp_traces_timeout = {
    /* Name */
    "otel_exporter_otlp_traces_timeout",
    /* Description */
    "Export trace timeout, in milliseconds",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_traces_timeout,
    /* Value */
    &sv_otel_exporter_otlp_traces_timeout,
    /* Registered */
    false};

static long_check_arg_s check_args_otel_bsp_schedule_delay = {
    /* default 5 sec */
    5000,
    /* min 1 sec */
    1000,
    /* max 60 sec */
    60000,
    /* block size 1 sec */
    1000};

long sv_otel_bsp_schedule_delay = 0;

/** OTEL_BSP_SCHEDULE_DELAY */
System_variable sysvar_otel_bsp_schedule_delay = {
    /* Name */
    "otel_bsp_schedule_delay",
    /* Description */
    "Delay interval between two consecutive exports, in milliseconds",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_otel_bsp_schedule_delay,
    /* Value */
    &sv_otel_bsp_schedule_delay,
    /* Registered */
    false};

static long_check_arg_s check_args_otel_bsp_max_queue_size = {
    /* default */
    2048,
    /* min */
    128,
    /* max */
    32768,
    /* block size */
    128};

long sv_otel_bsp_max_queue_size = 0;

/** OTEL_BSP_MAX_QUEUE_SIZE */
System_variable sysvar_otel_bsp_max_queue_size = {
    /* Name */
    "otel_bsp_max_queue_size",
    /* Description */
    "Maximum queue size",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_otel_bsp_max_queue_size,
    /* Value */
    &sv_otel_bsp_max_queue_size,
    /* Registered */
    false};

static long_check_arg_s check_args_otel_bsp_max_export_batch_size = {
    /* default */
    512,
    /* min */
    16,
    /* max */
    2048,
    /* block size */
    1};

long sv_otel_bsp_max_export_batch_size = 0;

/** OTEL_BSP_MAX_EXPORT_BATCH_SIZE */
System_variable sysvar_otel_bsp_max_export_batch_size = {
    /* Name */
    "otel_bsp_max_export_batch_size",
    /* Description */
    "Maximum batch size",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_otel_bsp_max_export_batch_size,
    /* Value */
    &sv_otel_bsp_max_export_batch_size,
    /* Registered */
    false};

/* ============================= */

unsigned long sv_otel_exporter_otlp_metrics_protocol;

/** OTEL_EXPORTER_OTLP_METRICS_PROTOCOL */
System_variable sysvar_otel_exporter_otlp_metrics_protocol = {
    /* Name */
    "otel_exporter_otlp_metrics_protocol",
    /* Description */
    "http/protobuf or http/json",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_protocol,
    /* Value */
    &sv_otel_exporter_otlp_metrics_protocol,
    /* Registered */
    false};

// OTLP HTTP collector
static str_check_arg_s check_args_exporter_otlp_metrics_endpoint = {""};

char *sv_otel_exporter_otlp_metrics_endpoint = nullptr;

/** OTEL_EXPORTER_OTLP_METRICS_ENDPOINT */
System_variable sysvar_otel_exporter_otlp_metrics_endpoint = {
    /* Name */
    "otel_exporter_otlp_metrics_endpoint",
    /* Description */
    "Target URL to send server telemetry metrics to",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_endpoint,
    /* Value */
    &sv_otel_exporter_otlp_metrics_endpoint,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_network_namespace = {
    ""};

char *sv_otel_exporter_otlp_metrics_network_namespace = nullptr;

/** OTEL_EXPORTER_OTLP_METRICS_NETWORK_NAMESPACE */
System_variable sysvar_otel_exporter_otlp_metrics_network_namespace = {
    /* Name */
    "otel_exporter_otlp_metrics_network_namespace",
    /* Description */
    "Network namespace to use to send server telemetry metrics",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_network_namespace,
    /* Value */
    &sv_otel_exporter_otlp_metrics_network_namespace,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_certificates = {""};

char *sv_otel_exporter_otlp_metrics_certificates = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CERTIFICATE */
System_variable sysvar_otel_exporter_otlp_metrics_certificates = {
    /* Name */
    "otel_exporter_otlp_metrics_certificates",
    /* Description */
    "Path to SSL certificates",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_certificates,
    /* Value */
    &sv_otel_exporter_otlp_metrics_certificates,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_client_key = {""};

char *sv_otel_exporter_otlp_metrics_client_key = nullptr;

/** OTEL_EXPORTER_OTLP_METRICS_CLIENT_KEY */
System_variable sysvar_otel_exporter_otlp_metrics_client_key = {
    /* Name */
    "otel_exporter_otlp_metrics_client_key",
    /* Description */
    "Path to SSL client key",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_client_key,
    /* Value */
    &sv_otel_exporter_otlp_metrics_client_key,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_client_certificates = {
    ""};

char *sv_otel_exporter_otlp_metrics_client_certificates = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CLIENT_CERTIFICATE */
System_variable sysvar_otel_exporter_otlp_metrics_client_certificates = {
    /* Name */
    "otel_exporter_otlp_metrics_client_certificates",
    /* Description */
    "Path to SSL client certificates",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_client_certificates,
    /* Value */
    &sv_otel_exporter_otlp_metrics_client_certificates,
    /* Registered */
    false};

unsigned long sv_otel_exporter_otlp_metrics_min_tls = OTLP_TLS_DEFAULT;

/** OTEL_EXPORTER_OTLP_METRICS_MIN_TLS */
System_variable sysvar_otel_exporter_otlp_metrics_min_tls = {
    /* Name */
    "otel_exporter_otlp_metrics_min_tls",
    /* Description */
    "Minimum TLS version",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_tls,
    /* Value */
    &sv_otel_exporter_otlp_metrics_min_tls,
    /* Registered */
    false};

unsigned long sv_otel_exporter_otlp_metrics_max_tls = OTLP_TLS_DEFAULT;

/** OTEL_EXPORTER_OTLP_METRICS_MAX_TLS */
System_variable sysvar_otel_exporter_otlp_metrics_max_tls = {
    /* Name */
    "otel_exporter_otlp_metrics_max_tls",
    /* Description */
    "Maximum TLS version",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_tls,
    /* Value */
    &sv_otel_exporter_otlp_metrics_max_tls,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_cipher = {
    default_tls12_ciphers};

char *sv_otel_exporter_otlp_metrics_cipher = nullptr;

/** OTEL_EXPORTER_OTLP_METRICS_CIPHER */
System_variable sysvar_otel_exporter_otlp_metrics_cipher = {
    /* Name */
    "otel_exporter_otlp_metrics_cipher",
    /* Description */
    "TLS Cipher (for TLS 1.2)",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    check_tls12_cipher,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_cipher,
    /* Value */
    &sv_otel_exporter_otlp_metrics_cipher,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_cipher_suite = {
    default_tls13_ciphers};

char *sv_otel_exporter_otlp_metrics_cipher_suite = nullptr;

/** OTEL_EXPORTER_OTLP_TRACES_CIPHER_SUITE */
System_variable sysvar_otel_exporter_otlp_metrics_cipher_suite = {
    /* Name */
    "otel_exporter_otlp_metrics_cipher_suite",
    /* Description */
    "TLS Cipher (for TLS 1.3)",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    check_tls13_cipher_suite,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_cipher_suite,
    /* Value */
    &sv_otel_exporter_otlp_metrics_cipher_suite,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_headers = {""};

char *sv_otel_exporter_otlp_metrics_headers = nullptr;

/** OTEL_EXPORTER_OTLP_METRICS_HEADERS */
System_variable sysvar_otel_exporter_otlp_metrics_headers = {
    /* Name */
    "otel_exporter_otlp_metrics_headers",
    /* Description */
    "Key-value pairs as header associated with http requests.",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_headers,
    /* Value */
    &sv_otel_exporter_otlp_metrics_headers,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_metrics_secret_headers = {""};

char *sv_otel_exporter_otlp_metrics_secret_headers = nullptr;

/** OTEL_EXPORTER_OTLP_METRICS_SECRET_HEADERS */
System_variable sysvar_otel_exporter_otlp_metrics_secret_headers = {
    /* Name */
    "otel_exporter_otlp_metrics_secret_headers",
    /* Description */
    "Named secret, to find key-value pairs as header associated with http "
    "requests",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_secret_headers,
    /* Value */
    &sv_otel_exporter_otlp_metrics_secret_headers,
    /* Registered */
    false};

ulong sv_otel_exporter_otlp_metrics_compression = 0;

/** OTEL_EXPORTER_OTLP_METRICS_COMPRESSION */
System_variable sysvar_otel_exporter_otlp_metrics_compression = {
    /* Name */
    "otel_exporter_otlp_metrics_compression",
    /* Description */
    "none or gzip",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_compression,
    /* Value */
    &sv_otel_exporter_otlp_metrics_compression,
    /* Registered */
    false};

static long_check_arg_s check_args_exporter_otlp_metrics_timeout = {
    /* default 10 sec */
    10000,
    /* min 1 sec */
    1000,
    /* max 5 min */
    300000,
    /* block size 1 sec */
    1000};

long sv_otel_exporter_otlp_metrics_timeout = 0;

/** OTEL_EXPORTER_OTLP_METRICS_TIMEOUT */
System_variable sysvar_otel_exporter_otlp_metrics_timeout = {
    /* Name */
    "otel_exporter_otlp_metrics_timeout",
    /* Description */
    "Export metrics timeout, in milliseconds",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_metrics_timeout,
    /* Value */
    &sv_otel_exporter_otlp_metrics_timeout,
    /* Registered */
    false};

ulong sv_metrics_reader_frequency_1 = 0;

static ulong_check_arg_s check_args_metrics_reader_frequency_1 = {
    /* default 10 sec */
    10,
    /* min 1 sec */
    1,
    /* no max */
    ULONG_MAX,
    /* block size 1 sec */
    1};

/** METRICS_READER_FREQUENCY_1 */
System_variable sysvar_metrics_reader_frequency_1 = {
    /* Name */
    "metrics_reader_frequency_1",
    /* Description */
    "Export frequency, in seconds, for metrics data",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_metrics_reader_frequency_1,
    /* Value */
    &sv_metrics_reader_frequency_1,
    /* Registered */
    false};

ulong sv_metrics_reader_frequency_2 = 0;

static ulong_check_arg_s check_args_metrics_reader_frequency_2 = {
    /* default 60 sec */
    60,
    /* 0 = disable */
    0,
    /* no max */
    ULONG_MAX,
    /* block size 1 sec */
    1};

/** METRICS_READER_FREQUENCY_2 */
System_variable sysvar_metrics_reader_frequency_2 = {
    /* Name */
    "metrics_reader_frequency_2",
    /* Description */
    "Optional additional export frequency, in seconds, for metrics data. "
    "Use 0 to disable",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_metrics_reader_frequency_2,
    /* Value */
    &sv_metrics_reader_frequency_2,
    /* Registered */
    false};

ulong sv_metrics_reader_frequency_3 = 0;

static ulong_check_arg_s check_args_metrics_reader_frequency_3 = {
    /* disabled by default */
    0,
    /* 0 = disable */
    0,
    /* no max */
    ULONG_MAX,
    /* block size 1 sec */
    1};

/** METRICS_READER_FREQUENCY_3 */
System_variable sysvar_metrics_reader_frequency_3 = {
    /* Name */
    "metrics_reader_frequency_3",
    /* Description */
    "Optional additional export frequency, in seconds, for metrics data. "
    "Use 0 to disable",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_metrics_reader_frequency_3,
    /* Value */
    &sv_metrics_reader_frequency_3,
    /* Registered */
    false};

/* ============================= */

unsigned long sv_otel_exporter_otlp_logs_protocol;

/** OTEL_EXPORTER_OTLP_LOGS_PROTOCOL */
System_variable sysvar_otel_exporter_otlp_logs_protocol = {
    /* Name */
    "otel_exporter_otlp_logs_protocol",
    /* Description */
    "http/protobuf or http/json",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_protocol,
    /* Value */
    &sv_otel_exporter_otlp_logs_protocol,
    /* Registered */
    false};

// OTLP HTTP collector
static str_check_arg_s check_args_exporter_otlp_logs_endpoint = {""};

char *sv_otel_exporter_otlp_logs_endpoint = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_ENDPOINT */
System_variable sysvar_otel_exporter_otlp_logs_endpoint = {
    /* Name */
    "otel_exporter_otlp_logs_endpoint",
    /* Description */
    "Target URL to send server telemetry logs to",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_endpoint,
    /* Value */
    &sv_otel_exporter_otlp_logs_endpoint,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_network_namespace = {""};

char *sv_otel_exporter_otlp_logs_network_namespace = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_NETWORK_NAMESPACE */
System_variable sysvar_otel_exporter_otlp_logs_network_namespace = {
    /* Name */
    "otel_exporter_otlp_logs_network_namespace",
    /* Description */
    "Network namespace to use to send server telemetry logs",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_network_namespace,
    /* Value */
    &sv_otel_exporter_otlp_logs_network_namespace,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_certificates = {""};

char *sv_otel_exporter_otlp_logs_certificates = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_CERTIFICATE */
System_variable sysvar_otel_exporter_otlp_logs_certificates = {
    /* Name */
    "otel_exporter_otlp_logs_certificates",
    /* Description */
    "Path to SSL certificates",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_certificates,
    /* Value */
    &sv_otel_exporter_otlp_logs_certificates,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_client_key = {""};

char *sv_otel_exporter_otlp_logs_client_key = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_CLIENT_KEY */
System_variable sysvar_otel_exporter_otlp_logs_client_key = {
    /* Name */
    "otel_exporter_otlp_logs_client_key",
    /* Description */
    "Path to SSL client key",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_client_key,
    /* Value */
    &sv_otel_exporter_otlp_logs_client_key,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_client_certificates = {""};

char *sv_otel_exporter_otlp_logs_client_certificates = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_CLIENT_CERTIFICATE */
System_variable sysvar_otel_exporter_otlp_logs_client_certificates = {
    /* Name */
    "otel_exporter_otlp_logs_client_certificates",
    /* Description */
    "Path to SSL client certificates",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_client_certificates,
    /* Value */
    &sv_otel_exporter_otlp_logs_client_certificates,
    /* Registered */
    false};

unsigned long sv_otel_exporter_otlp_logs_min_tls = OTLP_TLS_DEFAULT;

/** OTEL_EXPORTER_OTLP_LOGS_MIN_TLS */
System_variable sysvar_otel_exporter_otlp_logs_min_tls = {
    /* Name */
    "otel_exporter_otlp_logs_min_tls",
    /* Description */
    "Minimum TLS version",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_tls,
    /* Value */
    &sv_otel_exporter_otlp_logs_min_tls,
    /* Registered */
    false};

unsigned long sv_otel_exporter_otlp_logs_max_tls = OTLP_TLS_DEFAULT;

/** OTEL_EXPORTER_OTLP_LOGS_MAX_TLS */
System_variable sysvar_otel_exporter_otlp_logs_max_tls = {
    /* Name */
    "otel_exporter_otlp_logs_max_tls",
    /* Description */
    "Maximum TLS version",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_tls,
    /* Value */
    &sv_otel_exporter_otlp_logs_max_tls,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_cipher = {
    default_tls12_ciphers};

char *sv_otel_exporter_otlp_logs_cipher = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_CIPHER */
System_variable sysvar_otel_exporter_otlp_logs_cipher = {
    /* Name */
    "otel_exporter_otlp_logs_cipher",
    /* Description */
    "TLS Cipher (for TLS 1.2)",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    check_tls12_cipher,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_cipher,
    /* Value */
    &sv_otel_exporter_otlp_logs_cipher,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_cipher_suite = {
    default_tls13_ciphers};

char *sv_otel_exporter_otlp_logs_cipher_suite = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_CIPHER_SUITE */
System_variable sysvar_otel_exporter_otlp_logs_cipher_suite = {
    /* Name */
    "otel_exporter_otlp_logs_cipher_suite",
    /* Description */
    "TLS Cipher (for TLS 1.3)",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    check_tls13_cipher_suite,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_cipher_suite,
    /* Value */
    &sv_otel_exporter_otlp_logs_cipher_suite,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_headers = {""};

char *sv_otel_exporter_otlp_logs_headers = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_HEADERS */
System_variable sysvar_otel_exporter_otlp_logs_headers = {
    /* Name */
    "otel_exporter_otlp_logs_headers",
    /* Description */
    "Key-value pairs as header associated with http requests.",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_headers,
    /* Value */
    &sv_otel_exporter_otlp_logs_headers,
    /* Registered */
    false};

static str_check_arg_s check_args_exporter_otlp_logs_secret_headers = {""};

char *sv_otel_exporter_otlp_logs_secret_headers = nullptr;

/** OTEL_EXPORTER_OTLP_LOGS_SECRET_HEADERS */
System_variable sysvar_otel_exporter_otlp_logs_secret_headers = {
    /* Name */
    "otel_exporter_otlp_logs_secret_headers",
    /* Description */
    "Named secret, to find key-value pairs as header associated with http "
    "requests",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_secret_headers,
    /* Value */
    &sv_otel_exporter_otlp_logs_secret_headers,
    /* Registered */
    false};

ulong sv_otel_exporter_otlp_logs_compression = 0;

/** OTEL_EXPORTER_OTLP_LOGS_COMPRESSION */
System_variable sysvar_otel_exporter_otlp_logs_compression = {
    /* Name */
    "otel_exporter_otlp_logs_compression",
    /* Description */
    "none or gzip",
    /* Flags */
    PLUGIN_VAR_ENUM | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_compression,
    /* Value */
    &sv_otel_exporter_otlp_logs_compression,
    /* Registered */
    false};

static long_check_arg_s check_args_exporter_otlp_logs_timeout = {
    /* default 10 sec */
    10000,
    /* min 1 sec */
    1000,
    /* max 5 min */
    300000,
    /* block size 1 sec */
    1000};

long sv_otel_exporter_otlp_logs_timeout = 0;

/** OTEL_EXPORTER_OTLP_LOGS_TIMEOUT */
System_variable sysvar_otel_exporter_otlp_logs_timeout = {
    /* Name */
    "otel_exporter_otlp_logs_timeout",
    /* Description */
    "Export logs timeout, in milliseconds",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_exporter_otlp_logs_timeout,
    /* Value */
    &sv_otel_exporter_otlp_logs_timeout,
    /* Registered */
    false};

static long_check_arg_s check_args_otel_blrp_schedule_delay = {
    /* default 5 sec */
    5000,
    /* min 1 sec */
    1000,
    /* max 60 sec */
    60000,
    /* block size 1 sec */
    1000};

long sv_otel_blrp_schedule_delay = 0;

/** OTEL_BLRP_SCHEDULE_DELAY */
System_variable sysvar_otel_blrp_schedule_delay = {
    /* Name */
    "otel_blrp_schedule_delay",
    /* Description */
    "Delay interval between two consecutive log exports, in milliseconds",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_otel_blrp_schedule_delay,
    /* Value */
    &sv_otel_blrp_schedule_delay,
    /* Registered */
    false};

static long_check_arg_s check_args_otel_blrp_max_queue_size = {
    /* default */
    2048,
    /* min */
    128,
    /* max */
    32768,
    /* block size */
    128};

long sv_otel_blrp_max_queue_size = 0;

/** OTEL_BLRP_MAX_QUEUE_SIZE */
System_variable sysvar_otel_blrp_max_queue_size = {
    /* Name */
    "otel_blrp_max_queue_size",
    /* Description */
    "Maximum queue size",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_otel_blrp_max_queue_size,
    /* Value */
    &sv_otel_blrp_max_queue_size,
    /* Registered */
    false};

static long_check_arg_s check_args_otel_blrp_max_export_batch_size = {
    /* default */
    512,
    /* min */
    16,
    /* max */
    2048,
    /* block size */
    1};

long sv_otel_blrp_max_export_batch_size = 0;

/** OTEL_BLRP_MAX_EXPORT_BATCH_SIZE */
System_variable sysvar_otel_blrp_max_export_batch_size = {
    /* Name */
    "otel_blrp_max_export_batch_size",
    /* Description */
    "Maximum batch size",
    /* Flags */
    PLUGIN_VAR_LONG | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_otel_blrp_max_export_batch_size,
    /* Value */
    &sv_otel_blrp_max_export_batch_size,
    /* Registered */
    false};

static str_check_arg_s check_args_resource_provider = {""};

char *sv_resource_provider = nullptr;

/** RESOURCE_PROVIDER */
System_variable sysvar_resource_provider = {
    /* Name */
    "resource_provider",
    /* Description */
    "Optional, name of the component that provides the resource provider "
    "service",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_resource_provider,
    /* Value */
    &sv_resource_provider,
    /* Registered */
    false};

static str_check_arg_s check_args_secret_provider = {""};

char *sv_secret_provider = nullptr;

/** RESOURCE_PROVIDER */
System_variable sysvar_secret_provider = {
    /* Name */
    "secret_provider",
    /* Description */
    "Optional, name of the component that provides the secret provider service",
    /* Flags */
    PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
    /* Check */
    nullptr,
    /* Update */
    nullptr,
    /* Check args */
    &check_args_secret_provider,
    /* Value */
    &sv_secret_provider,
    /* Registered */
    false};

System_variable *all_sys_vars[] = {
    &sysvar_trace_enabled,
    &sysvar_metrics_enabled,
    &sysvar_log_enabled,
    &sysvar_query_text_enabled,
    &sysvar_otel_log_level,
    &sysvar_otel_resource_attributes,
    &sysvar_otel_exporter_otlp_traces_protocol,
    &sysvar_otel_exporter_otlp_traces_endpoint,
    &sysvar_otel_exporter_otlp_traces_network_namespace,
    &sysvar_otel_exporter_otlp_traces_certificates,
    &sysvar_otel_exporter_otlp_traces_client_key,
    &sysvar_otel_exporter_otlp_traces_client_certificates,
    &sysvar_otel_exporter_otlp_traces_min_tls,
    &sysvar_otel_exporter_otlp_traces_max_tls,
    &sysvar_otel_exporter_otlp_traces_cipher,
    &sysvar_otel_exporter_otlp_traces_cipher_suite,
    &sysvar_otel_exporter_otlp_traces_headers,
    &sysvar_otel_exporter_otlp_traces_secret_headers,
    &sysvar_otel_exporter_otlp_traces_compression,
    &sysvar_otel_exporter_otlp_traces_timeout,
    &sysvar_otel_exporter_otlp_metrics_protocol,
    &sysvar_otel_exporter_otlp_metrics_endpoint,
    &sysvar_otel_exporter_otlp_metrics_network_namespace,
    &sysvar_otel_exporter_otlp_metrics_certificates,
    &sysvar_otel_exporter_otlp_metrics_client_key,
    &sysvar_otel_exporter_otlp_metrics_client_certificates,
    &sysvar_otel_exporter_otlp_metrics_min_tls,
    &sysvar_otel_exporter_otlp_metrics_max_tls,
    &sysvar_otel_exporter_otlp_metrics_cipher,
    &sysvar_otel_exporter_otlp_metrics_cipher_suite,
    &sysvar_otel_exporter_otlp_metrics_headers,
    &sysvar_otel_exporter_otlp_metrics_secret_headers,
    &sysvar_otel_exporter_otlp_metrics_compression,
    &sysvar_otel_exporter_otlp_metrics_timeout,
    &sysvar_otel_exporter_otlp_logs_protocol,
    &sysvar_otel_exporter_otlp_logs_endpoint,
    &sysvar_otel_exporter_otlp_logs_network_namespace,
    &sysvar_otel_exporter_otlp_logs_certificates,
    &sysvar_otel_exporter_otlp_logs_client_key,
    &sysvar_otel_exporter_otlp_logs_client_certificates,
    &sysvar_otel_exporter_otlp_logs_min_tls,
    &sysvar_otel_exporter_otlp_logs_max_tls,
    &sysvar_otel_exporter_otlp_logs_cipher,
    &sysvar_otel_exporter_otlp_logs_cipher_suite,
    &sysvar_otel_exporter_otlp_logs_headers,
    &sysvar_otel_exporter_otlp_logs_secret_headers,
    &sysvar_otel_exporter_otlp_logs_compression,
    &sysvar_otel_exporter_otlp_logs_timeout,
    &sysvar_metrics_reader_frequency_1,
    &sysvar_metrics_reader_frequency_2,
    &sysvar_metrics_reader_frequency_3,
    &sysvar_otel_bsp_schedule_delay,
    &sysvar_otel_bsp_max_queue_size,
    &sysvar_otel_bsp_max_export_batch_size,
    &sysvar_otel_blrp_schedule_delay,
    &sysvar_otel_blrp_max_queue_size,
    &sysvar_otel_blrp_max_export_batch_size,
    &sysvar_resource_provider,
    &sysvar_secret_provider,
    nullptr};

int check_system_variable(System_variable *v);

int register_system_variables() {
  System_variable **cur = &all_sys_vars[0];
  System_variable *v = nullptr;
  int rc;
  while (*cur != nullptr) {
    v = *cur;
    if (!v->m_registered) {
      rc = sysvar_register_srv->register_variable(
          sysvar_prefix, v->m_name,
          v->m_flags | PLUGIN_VAR_PERSIST_AS_READ_ONLY, v->m_desc,
          v->m_check_func, v->m_update_func, v->m_check_args, v->m_val_ptr);
      if (rc != 0) {
        log_error("%s: Failed to register system variable '%s'", component_name,
                  v->m_name);
        return rc;
      }

      v->m_registered = true;

      rc = check_system_variable(v);
      if (rc != 0) {
        log_error("%s: Failed to check system variable '%s'", component_name,
                  v->m_name);
        return rc;
      }
    }

    cur++;
  }

  return 0;
}

void unregister_system_variables() {
  System_variable **cur = &all_sys_vars[0];
  System_variable *v = nullptr;
  int rc;
  while (*cur != nullptr) {
    v = *cur;
    if (v->m_registered) {
      rc = sysvar_unregister_srv->unregister_variable(sysvar_prefix, v->m_name);
      if (rc != 0) {
        log_error("%s: Failed to unregister system variable '%s'",
                  component_name, v->m_name);
      } else {
        v->m_registered = false;
      }
    }

    cur++;
  }
}

/* ============================================================= */

// WORK AROUND FOR THE FOLLOWING BUG:
// Bug#36806546 INSTALL COMPONENT
//   ... SET VARIABLE = VALUE does not invoke check
//
// Because INSTALL COMPONENT "file://component_telemetry"
//   SET otel_exporter_otlp_traces_cipher = 'ILLEGAL-CIPHER';
// is accepted,
// we are invoking the check function manually.
//
// This code is to be removed once the bug is resolved.

/**
 * Implement a st_mysql_value subclass,
 * that retrieves the value of a system variable.
 */
struct value_access : public st_mysql_value {
 public:
  value_access(void *value_ptr) {
    // Data
    m_value_ptr = value_ptr;

    // Callbacks
    // This is just enough to invoke:
    // - check_tls12_cipher()
    // - check_tls13_cipher_suite()
    value_type = nullptr;
    val_str = value_access::val_str_func;
    val_real = nullptr;
    val_int = nullptr;
    is_unsigned = nullptr;
  }

  static const char *val_str_func(st_mysql_value *v, char * /* buffer */,
                                  int * /* len */) {
    auto *va = reinterpret_cast<value_access *>(v);
    const char **str = static_cast<const char **>(va->m_value_ptr);
    assert(str != nullptr);
    return *str;
  }

 private:
  void *m_value_ptr;
};

/**
 * Work around.
 */
int check_system_variable(System_variable *v) {
  int rc;
  const char *save_value_ptr = nullptr;
  mysql_sys_var_check_func checker = v->m_check_func;

  if (checker != nullptr) {
    /* Provide access to the system variable value. */
    value_access value(v->m_val_ptr);
    /* Call the missing check() */
    rc = checker(nullptr, nullptr, &save_value_ptr, &value);
  } else {
    rc = 0;
  }

  return rc;
}

/* ============================================================= */

}  // namespace telemetry
