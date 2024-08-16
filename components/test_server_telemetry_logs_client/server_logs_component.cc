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
#include <mysql/psi/mysql_telemetry_logs_client.h>
#include <chrono>
#include <map>
#include "mysqld_error.h"
#include "server_logs_helpers.h"

/* test_server_telemetry_logs_client_component requires/uses the following
 * services.
 */
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_registration_srv);
REQUIRES_SERVICE_PLACEHOLDER(mysql_server_telemetry_logs_client);

/**
  @page TEST_SERVER_LOGS_LOGS_COMPONENT A test component for PS server
  telemetry logs service (client API part)

  Component Name  : test_server_telemetry_logs_client \n
  Source location : components/test_server_telemetry_logs_client

  This file contains a definition of the test_server_telemetry_logs_client
  component.
*/

static FileLogger g_log("test_server_telemetry_logs_component.log");

PSI_logger_key test1_logger_key = 0;
PSI_logger_key test2_logger_key = 0;
PSI_logger_key test3_logger_key = 0;
PSI_logger_key test4_logger_key = 0;

PSI_logger_info_v1 g_loggers[] = {
    {"test1", "Test logger #1", 0, &test1_logger_key},
    {"test2", "Test logger #2", 0, &test2_logger_key}};
PSI_logger_info_v1 g_loggers1[] = {
    {"test3", "Test logger #3", 0, &test3_logger_key},
    {"test4", "Test logger #4", 0, &test4_logger_key}};

static int register_loggers() {
  mysql_log_client_register(g_loggers, std::size(g_loggers),
                            "test_logs_componentA");

  // do not block component load on possible logger registration failures
  // (we might have limited number of instrument slots), just log
  for (auto &logger : g_loggers) {
    if (*logger.m_key == 0) {
      g_log.write("register_loggers() failed to register: %s\n",
                  logger.m_logger_name);
    }
  }
  return 0;
}

static void unregister_loggers() {
  mysql_log_client_unregister(g_loggers, std::size(g_loggers));
  mysql_log_client_unregister(g_loggers1, std::size(g_loggers1));
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
   Implements test_emit_log UDF. This function generates and
   emits a single test log record.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_emit_log(UDF_INIT *init [[maybe_unused]],
                               UDF_ARGS *args [[maybe_unused]],
                               unsigned char *null_value [[maybe_unused]],
                               unsigned char *error [[maybe_unused]]) {
  if (args && args->arg_count == 2 && args->arg_type[0] == STRING_RESULT &&
      args->arg_type[1] == STRING_RESULT) {
    const char *level = args->args[0];
    const char *message = args->args[1];

    OTELLogLevel severity;
    if (parse_log_level(level, severity)) {
      g_log.write("test_emit_log: Invalid level '%s' for message '%s'\n", level,
                  message);
      return -1;
    }

    PSI_logger *logger =
        mysql_log_client_check_enabled(test1_logger_key, severity);
    if (logger != nullptr) {
      mysql_log_client_log(logger, severity, message, time(nullptr), nullptr,
                           0);
    } else {
      g_log.write("test_emit_log: log level=%s not enabled! (message='%s')\n",
                  level, message);
    }
    return 0;
  }
  return -1;
}

/**
   Implements test_emit_log_with_attributes UDF. This function generates and
   emits a single test log record with some test attributes.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_emit_log_with_attributes(UDF_INIT *init [[maybe_unused]],
                                               UDF_ARGS *args [[maybe_unused]],
                                               unsigned char *null_value
                                               [[maybe_unused]],
                                               unsigned char *error
                                               [[maybe_unused]]) {
  if (args && args->arg_count == 2 && args->arg_type[0] == STRING_RESULT &&
      args->arg_type[1] == STRING_RESULT) {
    const char *level = args->args[0];
    const char *message = args->args[1];

    OTELLogLevel severity;
    if (parse_log_level(level, severity)) {
      g_log.write(
          "test_emit_log_with_attributes: Invalid level '%s' for message "
          "'%s'\n",
          level, message);
      return -1;
    }

    PSI_logger *logger =
        mysql_log_client_check_enabled(test1_logger_key, severity);
    if (logger == nullptr) {
      g_log.write(
          "test_emit_log_with_attributes: log level=%s not enabled! "
          "(message='%s')\n",
          level, message);
      return -1;
    }

    // emit complex log with attributes, one of each type
    char str_view_buffer[] = "string_within_long_text";
    log_attribute_t attrs[8];
    attrs[0].set_string("attr_string", "value");
    attrs[1].set_double("attr_double", 0.5);
    attrs[2].set_int64("attr_int64", -1001);
    attrs[3].set_uint64("attr_uint64", 42);
    attrs[4].set_bool("attr_bool", true);
    attrs[5].set_int32("attr_int32", -15);
    attrs[6].set_uint32("attr_uint32", 0);
    attrs[7].set_string_view("attr_str_view", str_view_buffer, 6);

    mysql_log_client_log(logger, severity, message, time(nullptr), attrs,
                         std::size(attrs));
    return 0;
  }
  return -1;
}

/**
   Register/unregister loggers multiple times leaving "holes" in array of logger
   instruments, so we can test if performance_schema.setup_loggers table
   correctly handles such case.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_log_registration(UDF_INIT *init [[maybe_unused]],
                                       UDF_ARGS *args [[maybe_unused]],
                                       unsigned char *null_value
                                       [[maybe_unused]],
                                       unsigned char *error [[maybe_unused]]) {
  mysql_log_client_register(g_loggers1, std::size(g_loggers1),
                            "test_logs_componentB");
  // unregister only 1st entry, leaving the hole
  mysql_log_client_unregister(g_loggers1, 1);

  return 0;
}

/**
   Test C++ wrappers around telemetry logging API.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_log_wrappers(UDF_INIT *init [[maybe_unused]],
                                   UDF_ARGS *args [[maybe_unused]],
                                   unsigned char *null_value [[maybe_unused]],
                                   unsigned char *error [[maybe_unused]]) {
  // optional repetitions parameter for benchmarking
  size_t count = 1;
  const char *message = "";
  if (args && args->arg_count >= 1 && args->arg_type[0] == INT_RESULT) {
    count = *((long long *)args->args[0]);

    if (args->arg_count > 1 && args->arg_type[1] == STRING_RESULT) {
      message = args->args[1];
    }
  }

  const auto start = std::chrono::steady_clock::now();

  for (size_t i = 0; i < count; ++i) {
    // test PSI_SimpleLogger
    const PSI_SimpleLogger log(test1_logger_key);
    log.warn("Simple warning #1");
    log.debug("Simple debug #2");

    // test PSI_LogRecord
    PSI_LogRecord rec(test1_logger_key, OTELLogLevel::TLOG_ERROR,
                      "Complex warning #1");
    if (rec.check_enabled()) {
      rec.add_attribute_string("string", "some value");
      rec.add_attribute_uint64("uint64", (uint64_t)1020232);
      rec.add_attribute_double("double", 3.14);
      rec.emit();
    }
  }

  if (count > 1) {
    const auto diff = std::chrono::steady_clock::now() - start;
    const size_t usecs =
        std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
    g_log.write("test_log_wrappers(%s - %ld) microbenchmark: %ld us\n", message,
                count, usecs);
  }

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
  // register duplicate logger (same name+category) should fail
  PSI_logger_key test1_duplicate_logger_key = 0;
  PSI_logger_info_v1 duplicate_logger[] = {
      {"test1", "Test logger #1", 0, &test1_duplicate_logger_key}};
  mysql_log_client_register(duplicate_logger, std::size(duplicate_logger),
                            "test_logs_componentA");
  if (test1_duplicate_logger_key == 0) {
    g_log.write("Correct handling of duplicated loggers.\n");
  }

  // register invalid logger (name+category too long) should fail
  PSI_logger_key test1_invalid_logger_key = 0;
  PSI_logger_info_v1 invalid_logger[] = {
      {"1234567890123456789012345678901234567890123456789012345678901234567890",
       "Test logger #1", 0, &test1_invalid_logger_key}};
  mysql_log_client_register(
      invalid_logger, std::size(invalid_logger),
      "test_logs_123456789012345678901234567890123456789012345678901234567890");
  if (test1_invalid_logger_key == 0) {
    g_log.write(
        "Correct handling of invalid loggers (name+category too long).\n");
  }

  // log record that was never emitted was correctly dicarded
  const PSI_LogRecord rec(test1_logger_key, OTELLogLevel::TLOG_ERROR,
                          "Test message");

  // test that you can not add more than 64 attributes
  PSI_LogRecord rec1(test1_logger_key, OTELLogLevel::TLOG_ERROR,
                     "Test message");
  char key[128][20];
  for (int i = 0; i < 128; ++i) {
    sprintf(key[i], "name#%02d", i + 1);
    rec1.add_attribute_string(key[i], "a");
  }
  rec1.emit();

  return 0;
}

/**
   Test that MySQL error log entries are also emitted as telemetry logs.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long test_error_log(UDF_INIT *init [[maybe_unused]],
                                UDF_ARGS *args [[maybe_unused]],
                                unsigned char *null_value [[maybe_unused]],
                                unsigned char *error [[maybe_unused]]) {
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(WARNING_LEVEL)
      .errcode(ER_LOG_PRINTF_MSG)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component("component: test_server_telemetry_logs")
      .source_line(12345)
      .source_file("server_logs_component.cc")
      .function("test_error_log")
      .message("Test message #15");
  return 0;
}

static void unregister_udf() {
  int was_present = 0;

  udf_registration_srv->udf_unregister("test_emit_log", &was_present);
  udf_registration_srv->udf_unregister("test_emit_log_with_attributes",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_log_registration", &was_present);
  udf_registration_srv->udf_unregister("test_log_wrappers", &was_present);
  udf_registration_srv->udf_unregister("test_invalid_operations", &was_present);
  udf_registration_srv->udf_unregister("test_error_log", &was_present);
}

static bool register_udf() {
  if (udf_registration_srv->udf_register("test_emit_log", INT_RESULT,
                                         (Udf_func_any)test_emit_log, nullptr,
                                         nullptr))
    return true;

  if (udf_registration_srv->udf_register(
          "test_emit_log_with_attributes", INT_RESULT,
          (Udf_func_any)test_emit_log_with_attributes, nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_log_registration", INT_RESULT,
                                         (Udf_func_any)test_log_registration,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_log_wrappers", INT_RESULT,
                                         (Udf_func_any)test_log_wrappers,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_invalid_operations", INT_RESULT,
                                         (Udf_func_any)test_invalid_operations,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_error_log", INT_RESULT,
                                         (Udf_func_any)test_error_log, nullptr,
                                         nullptr)) {
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
 *    - Register telemetry logs loggers
 *    - Register UDFs
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_logs_component_init() {
  mysql_service_status_t result = 0;

  g_log.write("test_server_telemetry_logs_client_component_init init:\n");

  log_service_init();
  g_log.write(" - Initialized log service.\n");

  if (register_loggers()) {
    g_log.write("Error returned from register_loggers()\n");
    result = true;
    goto error;
  }
  g_log.write(" - Telemetry logs loggers registered.\n");

  if (register_udf()) {
    register_loggers();
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
 *   - Unregister telemetry logs loggers
 *   - Unregister UDFs
 *   - Deinitialize log service
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_logs_component_deinit() {
  g_log.write("test_server_telemetry_logs_client_component_deinit:\n");

  unregister_loggers();
  g_log.write(" - Telemetry logs loggers unregistered.\n");

  unregister_udf();
  g_log.write(" - UDFs unregistered.\n");

  log_service_deinit();
  g_log.write(" - Deinitialized log service.\n");

  g_log.write("End of deinit\n");
  return 0;
}

/* test_server_telemetry_logs component doesn't provide any service */
BEGIN_COMPONENT_PROVIDES(test_server_telemetry_logs_client)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_server_telemetry_logs_client)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(mysql_server_telemetry_logs_client),
    REQUIRES_SERVICE_AS(udf_registration, udf_registration_srv),

    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_server_telemetry_logs_client)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_server_telemetry_logs_client,
                  "mysql:test_server_telemetry_logs_client")
test_server_telemetry_logs_component_init,
    test_server_telemetry_logs_component_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_server_telemetry_logs_client)
    END_DECLARE_LIBRARY_COMPONENTS
