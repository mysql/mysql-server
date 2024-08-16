/* Copyright (c) 2023, 2024 Oracle and/or its affiliates.

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

#include "server_logs_component.h"
#include <chrono>
#include <map>
#include "mysqld_error.h"
#include "server_logs_helpers.h"

/* test_server_telemetry_logs_export_component requires/uses the following
 * services.
 */
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_logs,
                                telemetry_logs_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                sysvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                sysvar_unregister_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_registration_srv);

// uncomment this when you want to benchmark/profile the core system
// without an impact of the test component callback implementation
// #define EMPTY_CALLBACKS

/**
  @page TEST_SERVER_LOGS_LOGS_COMPONENT A test component for PS server
  telemetry logs service

  Component Name  : test_server_telemetry_logs_export \n
  Source location : components/test_server_telemetry_logs_export

  This file contains a definition of the test_server_telemetry_logs_export
  component.
*/

static FileLogger g_log("test_server_telemetry_logs_component.log");

static int filter_severity_value = 9999;

void dummy_log_cb(const char *logger_name [[maybe_unused]],
                  OTELLogLevel severity [[maybe_unused]],
                  const char *message [[maybe_unused]],
                  time_t timestamp [[maybe_unused]],
                  const log_attribute_t *attr_array [[maybe_unused]],
                  size_t attr_count [[maybe_unused]]) {
  // does nothing
}

void telemetry_log_cb(const char *logger_name [[maybe_unused]],
                      OTELLogLevel severity [[maybe_unused]],
                      const char *message [[maybe_unused]],
                      time_t timestamp [[maybe_unused]],
                      const log_attribute_t *attr_array [[maybe_unused]],
                      size_t attr_count [[maybe_unused]]) {
#ifndef EMPTY_CALLBACKS
  if (severity >= filter_severity_value) {
    g_log.write(
        "telemetry_log_cb(%s): Custom filter message '%s' (level=%d/%s, "
        "filter_level=%d)\n",
        logger_name, message, severity, print_log_level(severity),
        filter_severity_value);
    return;
  }

  std::string attributes;
  for (size_t i = 0; i < attr_count; ++i) {
    // skip logging non-deterministic attributes
    if (0 == strcmp("thread", attr_array[i].name)) continue;

    if (!attributes.empty()) attributes += "; ";
    attributes += "'";
    attributes += attr_array[i].name;
    attributes += "'=";
    switch (attr_array[i].type) {
      case LOG_ATTRIBUTE_BOOLEAN:
        attributes += attr_array[i].value.bool_value ? "true" : "false";
        break;
      case LOG_ATTRIBUTE_INT32:
        attributes += std::to_string(attr_array[i].value.int32_value);
        break;
      case LOG_ATTRIBUTE_UINT32:
        attributes += std::to_string(attr_array[i].value.uint32_value);
        break;
      case LOG_ATTRIBUTE_INT64:
        attributes += std::to_string(attr_array[i].value.int64_value);
        break;
      case LOG_ATTRIBUTE_UINT64:
        attributes += std::to_string(attr_array[i].value.uint64_value);
        break;
      case LOG_ATTRIBUTE_DOUBLE:
        attributes += std::to_string(attr_array[i].value.double_value);
        break;
      case LOG_ATTRIBUTE_STRING:
        attributes += "'";
        attributes.append(attr_array[i].value.string_value);
        attributes += "'";
        break;
      case LOG_ATTRIBUTE_STRING_VIEW:
        attributes += "'";
        attributes.append(attr_array[i].value.string_value,
                          attr_array[i].value.string_length);
        attributes += "'";
        break;
      default:
        assert(false);
    }
  }
  g_log.write(
      "telemetry_log_cb(%s): Log message '%s' (level=%d/%s, attributes: %s)\n",
      logger_name, message, severity, print_log_level(severity),
      attributes.c_str());

  // test that error log with no_telemetry flag does not cause infinite
  // recursion error log -> telemetry log emitted -> telemetry callback -> error
  // log ->
  // ...
  LogEvent()
      .no_telemetry()
      .type(LOG_TYPE_ERROR)
      .prio(WARNING_LEVEL)
      .errcode(ER_LOG_PRINTF_MSG)
      .component("component: test_server_telemetry_logs")
      .message("Test message #33");
#endif  // EMPTY_CALLBACKS
}

static bool register_telemetry_callback() {
  return telemetry_logs_srv->register_logger(telemetry_log_cb);
}

static bool unregister_telemetry_callback() {
  return telemetry_logs_srv->unregister_logger(telemetry_log_cb);
}

static int register_system_variables() {
  INTEGRAL_CHECK_ARG(int) int_arg;
  int_arg.def_val = 9999;  // log everything
  int_arg.min_val = -1;    // no logging
  int_arg.max_val = 9999;
  int_arg.blk_sz = 0;
  if (sysvar_register_srv->register_variable(
          "test_server_telemetry_logs", "filter_severity", PLUGIN_VAR_INT,
          "Filter log output by logging only entries with specified severity "
          "level or lower.",
          nullptr, nullptr, (void *)&int_arg, (void *)&filter_severity_value)) {
    g_log.write("register_variable failed (filter_severity).\n");
    return 1;
  }

  return 0; /* All system variables registered successfully */
}

static void unregister_system_variables() {
  if (sysvar_unregister_srv->unregister_variable("test_server_telemetry_logs",
                                                 "filter_severity")) {
    g_log.write("unregister_variable failed (filter_severity).\n");
  }
}

SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

/**
  logger services initialization method for Component used when
  loading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
static bool log_service_init() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;

  return false;
}

/**
  logger services de-initialization method for Component used when
  unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
static bool log_service_deinit() { return false; }

/**
   Unregister telemetry callback, used for benchmarking comparisons.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_unregister_callback(UDF_INIT *init [[maybe_unused]],
                                          UDF_ARGS *args [[maybe_unused]],
                                          unsigned char *null_value
                                          [[maybe_unused]],
                                          unsigned char *error
                                          [[maybe_unused]]) {
  if (unregister_telemetry_callback())
    g_log.write("test_unregister_callback: unregister failed!\n");
  return 0;
}

/**
   Test error handling by triggering some invalid API calls.
   The call of this function also increases code coverage.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_invalid_operations(UDF_INIT *init [[maybe_unused]],
                                         UDF_ARGS *args [[maybe_unused]],
                                         unsigned char *null_value
                                         [[maybe_unused]],
                                         unsigned char *error
                                         [[maybe_unused]]) {
  // unregistering non-current log exporter callbacks
  if (telemetry_logs_srv->unregister_logger(dummy_log_cb)) {
    g_log.write("Correct handling unregister of non-current log exporter.\n");
  }

  return 0;
}

static void unregister_udf() {
  int was_present = 0;

  udf_registration_srv->udf_unregister("test_unregister_callback",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_invalid_operations_export",
                                       &was_present);
}

static bool register_udf() {
  if (udf_registration_srv->udf_register("test_unregister_callback", INT_RESULT,
                                         (Udf_func_any)test_unregister_callback,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register(
          "test_invalid_operations_export", INT_RESULT,
          (Udf_func_any)test_invalid_operations, nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  return false;
}

/**
 *  Initialize the test_server_telemetry_logs component at server start or
 *  component installation:
 *
 *    - Initialize log service
 *    - Register system variables
 *    - Register telemetry logs callback
 *    - Register UDFs
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_logs_component_init() {
  mysql_service_status_t result = 0;

  g_log.write("test_server_telemetry_logs_export_component_init init:\n");

  log_service_init();
  g_log.write(" - Initialized log service.\n");

  if (register_system_variables()) {
    g_log.write("Error returned from register_system_variables()\n");
    result = true;
    goto error;
  }
  g_log.write(" - System variables registered.\n");

  if (register_telemetry_callback()) {
    unregister_system_variables();
    g_log.write("Error returned from register_telemetry_callback()\n");
    result = true;
    goto error;
  }
  g_log.write(" - Telemetry logs callback registered.\n");

  if (register_udf()) {
    unregister_system_variables();
    unregister_telemetry_callback();
    g_log.write("Error returned from register_udfs()\n");
    result = true;
    goto error;
  }
  g_log.write(" - Telemetry logs UDFs registered.\n");

error:
  g_log.write("End of init\n");
  return result;
}

/**
 *  Terminate the test_server_telemetry_logs_component at server shutdown or
 *  component deinstallation:
 *
 *   - Unregister telemetry logs callback
 *   - Unregister system variables
 *   - Unregister UDFs
 *   - Deinitialize log service
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_logs_component_deinit() {
  g_log.write("test_server_telemetry_logs_export_component_deinit:\n");

  unregister_telemetry_callback();
  g_log.write(" - Telemetry logs callbacks unregistered.\n");

  unregister_system_variables();
  g_log.write(" - System variables unregistered.\n");

  unregister_udf();
  g_log.write(" - UDFs unregistered.\n");

  log_service_deinit();
  g_log.write(" - Deinitialized log service.\n");

  g_log.write("End of deinit\n");
  return 0;
}

/* test_server_telemetry_logs component doesn't provide any service */
BEGIN_COMPONENT_PROVIDES(test_server_telemetry_logs_export)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_server_telemetry_logs_export)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE_AS(mysql_server_telemetry_logs, telemetry_logs_srv),
    REQUIRES_SERVICE_AS(component_sys_variable_register, sysvar_register_srv),
    REQUIRES_SERVICE_AS(component_sys_variable_unregister,
                        sysvar_unregister_srv),
    REQUIRES_SERVICE_AS(udf_registration, udf_registration_srv),

    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_server_telemetry_logs_export)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_server_telemetry_logs_export,
                  "mysql:test_server_telemetry_logs_export")
test_server_telemetry_logs_component_init,
    test_server_telemetry_logs_component_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_server_telemetry_logs_export)
    END_DECLARE_LIBRARY_COMPONENTS
