/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "server_metrics_component.h"
#include <string.h>  // strdup
#include <atomic>
#include <string>  // std::size
#include "server_metrics_helpers.h"

/* test_server_telemetry_metrics_component requires/uses the following services.
 */
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_metrics_v1,
                                metrics_v1_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_registration_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter, string_converter_srv);
REQUIRES_PSI_METRIC_SERVICE_PLACEHOLDER;

/**
  @page TEST_SERVER_TELEMETRY_METRICS_COMPONENT A test component for PS server
  telemetry metrics service

  Component Name  : test_server_telemetry_metrics \n
  Source location : components/test_server_telemetry_metrics

  This file contains a definition of the test_server_telemetry_metrics
  component.
*/

static FileLogger g_log("test_server_telemetry_metrics_component.log");

static void get_metric_dummy_metric(void * /* measurement_context */,
                                    measurement_delivery_callback_t,
                                    void * /* delivery_context */) {
  // dummy measurement callback
}

static bool updown_metric_registered = false;
static std::atomic_int64_t updown_metric_value = 0LL;

static void get_metric_updown_test(void * /* measurement_context */,
                                   measurement_delivery_callback_t delivery,
                                   void *delivery_context) {
  const int64_t value = updown_metric_value.load();
  delivery->value_int64(delivery_context, value);
}

// used to register single test metric
static PSI_metric_info_v1 test_metric_a[] = {
    {"metric_a", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr}};
static PSI_meter_info_v1 test_meter_a = {
    "mysql.test1", "Test metrics",          10, 0, 0,
    test_metric_a, std::size(test_metric_a)};

static PSI_metric_info_v1 test_metric_b[] = {
    {"metric_b", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr}};
static PSI_meter_info_v1 test_meter_b = {
    "mysql.test2", "Test metrics",          10, 0, 0,
    test_metric_b, std::size(test_metric_b)};

// used to register 10k test metrics
static constexpr unsigned int MAX_PERFTEST_METRICS = 10000;
static PSI_metric_info_v1 *perftest_metrics = nullptr;
static PSI_meter_info_v1 perftest_meter = {
    "mysql.test3", "Test metrics", 10, 0, 0, nullptr, 0};

// used to test handling of duplicate meters/metrics registration
static PSI_metric_info_v1 duplicate_metrics1[] = {
    {"metric1", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr},
    {"metric1", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr}};

static PSI_metric_info_v1 duplicate_metrics2[] = {
    {"metric2", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr},
    {"metric2", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr}};

static PSI_meter_info_v1 duplicate_meters[] = {
    {"mysql.test4", "Test metrics", 10, 0, 0, duplicate_metrics1,
     std::size(duplicate_metrics1)},
    {"mysql.test4", "Test metrics", 10, 0, 0, duplicate_metrics2,
     std::size(duplicate_metrics2)}};

// used to test handling of invalid meters/metrics registration
static PSI_metric_info_v1 invalid_metrics[] = {
    {"", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr},
    {"metric_name_tooooooooooooooooooooo_looooooooooooooooooooongggggg", "", "",
     MetricOTELType::ASYNC_UPDOWN_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_dummy_metric, nullptr},
    {"1doesnt_start_with_alpha", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr},
    {"invalid_chars_#?", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_dummy_metric, nullptr}};

static PSI_meter_info_v1 invalid_meters[] = {
    {"mysql.test5", "Test metrics", 10, 0, 0, invalid_metrics,
     std::size(invalid_metrics)},
    {"meter_name_tooooooooooooooooooooo_looooooooooooooooooooonggggggg",
     "Test metrics", 10, 0, 0, invalid_metrics, std::size(invalid_metrics)},
    {"1doesnt_start_with_alpha", "Test metrics", 10, 0, 0, invalid_metrics,
     std::size(invalid_metrics)},
    {"invalid_chars_#?", "Test metrics", 10, 0, 0, invalid_metrics,
     std::size(invalid_metrics)}};

static PSI_metric_info_v1 updown_metric[] = {
    {"test_updown", "", "", MetricOTELType::ASYNC_UPDOWN_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_updown_test, nullptr}};

static PSI_meter_info_v1 updown_meter[] = {{"mysql.test6", "Test metrics", 10,
                                            0, 0, updown_metric,
                                            std::size(updown_metric)}};

/**
   Implements test_report_single_metric UDF. This function returns
   telemetry metric measurement of a single.

   @param init       Unused.
   @param args       Unused.
   @param null_value Unused.
   @param error      Unused.

   @retval 0 This function always returns 0.
*/
static long long report_single_metric(UDF_INIT *init [[maybe_unused]],
                                      UDF_ARGS *args [[maybe_unused]],
                                      unsigned char *null_value
                                      [[maybe_unused]],
                                      unsigned char *error [[maybe_unused]]) {
  if (args && args->arg_count == 2 && args->arg_type[0] == STRING_RESULT &&
      args->arg_type[1] == STRING_RESULT) {
    const char *meter = args->args[0];
    const char *metric = args->args[1];
    return get_metric_value(meter, metric);
  }
  return -1;
}

/**
  Implements test_report_metrics UDF. This function implements
  telemetry metric measurements export (to log file).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long report_metrics(UDF_INIT *init [[maybe_unused]],
                                UDF_ARGS *args [[maybe_unused]],
                                unsigned char *null_value [[maybe_unused]],
                                unsigned char *error [[maybe_unused]]) {
  return enumerate_meters_with_metrics(g_log);
}

/**
  Implements test_register_10k_metrics UDF. This function registers
  10k of telemetry metrics (used for manual performance tests).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long register_10k_metrics(UDF_INIT *init [[maybe_unused]],
                                      UDF_ARGS *args [[maybe_unused]],
                                      unsigned char *null_value
                                      [[maybe_unused]],
                                      unsigned char *error [[maybe_unused]]) {
  g_log.write("register_10k_metric > called\n");

  // only do this if not already registered
  if (perftest_metrics == nullptr) {
    perftest_metrics = new PSI_metric_info_v1[MAX_PERFTEST_METRICS];
    perftest_meter.m_metrics = perftest_metrics;
    perftest_meter.m_metrics_size = MAX_PERFTEST_METRICS;

    PSI_metric_info_v1 *item = perftest_metrics;
    for (unsigned int i = 0; i < MAX_PERFTEST_METRICS; i++, item++) {
      char unique_name[100];
      (void)sprintf(unique_name, "test.perftest_metric_%05u", i + 1);

      item->m_metric = strdup(unique_name);
      item->m_unit = "By";
      item->m_description = "Performance test dummy metric";
      item->m_metric_type = MetricOTELType::ASYNC_UPDOWN_COUNTER;
      item->m_num_type = MetricNumType::METRIC_INTEGER;
      item->m_key = 0;
      item->m_flags = 0;
      item->m_measurement_callback = get_metric_dummy_metric;
      item->m_measurement_context = nullptr;
    }

    mysql_meter_register(&perftest_meter, 1);
  }

  return 0;
}

static void unregister_10k_metrics_imp() {
  if (perftest_metrics != nullptr) {
    mysql_meter_unregister(&perftest_meter, 1);
    PSI_metric_info_v1 *item = perftest_metrics;
    for (unsigned int i = 0; i < MAX_PERFTEST_METRICS; i++, item++) {
      void *metric = const_cast<char *>(item->m_metric);
      free(metric);
    }
    delete[] perftest_metrics;
    perftest_metrics = nullptr;
  }
}

/**
  Implements test_unregister_10k_metrics UDF. This function unregisters
  10k of telemetry metrics if previously registered (used for manual performance
  tests).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long unregister_10k_metrics(UDF_INIT *init [[maybe_unused]],
                                        UDF_ARGS *args [[maybe_unused]],
                                        unsigned char *null_value
                                        [[maybe_unused]],
                                        unsigned char *error [[maybe_unused]]) {
  g_log.write("unregister_10k_metric > called\n");
  unregister_10k_metrics_imp();
  return 0;
}

/**
  Implements test_register_metric_a UDF. This function registers
  single test metric, if not already registered (used for manual performance
  tests).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long register_metric_a(UDF_INIT *init [[maybe_unused]],
                                   UDF_ARGS *args [[maybe_unused]],
                                   unsigned char *null_value [[maybe_unused]],
                                   unsigned char *error [[maybe_unused]]) {
  g_log.write("register_metric_a > called\n");
  mysql_meter_register(&test_meter_a, 1);
  return 0;
}

static void unregister_metric_a_imp() {
  mysql_meter_unregister(&test_meter_a, 1);
}

/**
  Implements test_unregister_metric_a UDF. This function unregisters
  single test metric, if registered (used for manual performance tests).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long unregister_metric_a(UDF_INIT *init [[maybe_unused]],
                                     UDF_ARGS *args [[maybe_unused]],
                                     unsigned char *null_value [[maybe_unused]],
                                     unsigned char *error [[maybe_unused]]) {
  g_log.write("unregister_metric_a > called\n");
  unregister_metric_a_imp();
  return 0;
}

static void unregister_metric_b_imp() {
  mysql_meter_unregister(&test_meter_b, 1);
}

/**
  Implements test_register_metric_b UDF. This function registers
  single test metric, if not already registered (used for manual performance
    tests).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long register_metric_b(UDF_INIT *init [[maybe_unused]],
                                   UDF_ARGS *args [[maybe_unused]],
                                   unsigned char *null_value [[maybe_unused]],
                                   unsigned char *error [[maybe_unused]]) {
  g_log.write("register_metric_b > called\n");
  mysql_meter_register(&test_meter_b, 1);
  return 0;
}

/**
  Implements test_unregister_metric_b UDF. This function unregisters
  single test metric, if registered (used for manual performance tests).

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long unregister_metric_b(UDF_INIT *init [[maybe_unused]],
                                     UDF_ARGS *args [[maybe_unused]],
                                     unsigned char *null_value [[maybe_unused]],
                                     unsigned char *error [[maybe_unused]]) {
  g_log.write("unregister_metric_b > called\n");
  unregister_metric_b_imp();
  return 0;
}

/**
  Implements test_duplicate_metrics UDF. This function tests
  handling of duplicate meters/metrics on registation.

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long test_duplicate_metrics(UDF_INIT *init [[maybe_unused]],
                                        UDF_ARGS *args [[maybe_unused]],
                                        unsigned char *null_value
                                        [[maybe_unused]],
                                        unsigned char *error [[maybe_unused]]) {
  g_log.write("test_duplicate_metrics > called\n");
  mysql_meter_register(duplicate_meters, std::size(duplicate_meters));
  mysql_meter_unregister(duplicate_meters, std::size(duplicate_meters));
  return 0;
}

/**
  Implements test_invalid_metrics UDF. This function tests
  handling of badly defined meters/metrics on registation.

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long test_invalid_metrics(UDF_INIT *init [[maybe_unused]],
                                      UDF_ARGS *args [[maybe_unused]],
                                      unsigned char *null_value
                                      [[maybe_unused]],
                                      unsigned char *error [[maybe_unused]]) {
  g_log.write("test_invalid_metrics > called\n");
  mysql_meter_register(invalid_meters, std::size(invalid_meters));
  mysql_meter_unregister(invalid_meters, std::size(invalid_meters));
  return 0;
}

/**
  Implements test_set_updown_metric UDF. This function sets the integer
  value of a test "async updown counter" metric. It's helper used to test
  E2E handling (mysqld->OTEL collector->Prometheus) of this metric type.

  @param init       Unused.
  @param args       Input array with metric value to set.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long test_set_updown_metric(UDF_INIT *init [[maybe_unused]],
                                        UDF_ARGS *args [[maybe_unused]],
                                        unsigned char *null_value
                                        [[maybe_unused]],
                                        unsigned char *error [[maybe_unused]]) {
  g_log.write("test_set_updown_metric > called\n");
  if (!updown_metric_registered) {
    mysql_meter_register(updown_meter, std::size(updown_meter));
    updown_metric_registered = true;
  }
  if (args && args->arg_count == 1 && args->arg_type[0] == INT_RESULT) {
    updown_metric_value = *((long long *)args->args[0]);
  }
  return 0;
}

static void unregister_updown_metric() {
  if (updown_metric_registered) {
    mysql_meter_unregister(updown_meter, std::size(updown_meter));
    updown_metric_registered = false;
  }
}

/**
  Implements component_metric_log UDF. This function passes input
  string parameter to component log (helps make test logs more readable).

  @param init       Unused.
  @param args       input array with log entry string.
  @param null_value Unused.
  @param error      Unused.

  @retval This function returns 0 on success, -1 on error.
*/
static long long component_metric_log(UDF_INIT *init [[maybe_unused]],
                                      UDF_ARGS *args,
                                      unsigned char *null_value
                                      [[maybe_unused]],
                                      unsigned char *error [[maybe_unused]]) {
  if (args && args->arg_count == 1 && args->arg_type[0] == STRING_RESULT) {
    const char *message = args->args[0];
    g_log.write("%s\n", message);
    return 0;
  }
  return -1;
}

static bool unregister_udf() {
  int was_present = 0;

  udf_registration_srv->udf_unregister("test_report_single_metric",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_report_metrics", &was_present);
  udf_registration_srv->udf_unregister("test_register_10k_metrics",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_unregister_10k_metrics",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_register_metric_a", &was_present);
  udf_registration_srv->udf_unregister("test_unregister_metric_a",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_register_metric_b", &was_present);
  udf_registration_srv->udf_unregister("test_unregister_metric_b",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_component_metric_log",
                                       &was_present);
  udf_registration_srv->udf_unregister("test_duplicate_metrics", &was_present);
  udf_registration_srv->udf_unregister("test_invalid_metrics", &was_present);
  udf_registration_srv->udf_unregister("test_set_updown_metric", &was_present);

  return false;
}

static bool register_udf() {
  if (udf_registration_srv->udf_register(
          "test_report_single_metric", INT_RESULT,
          (Udf_func_any)report_single_metric, nullptr, nullptr))
    return true;

  if (udf_registration_srv->udf_register("test_report_metrics", INT_RESULT,
                                         (Udf_func_any)report_metrics, nullptr,
                                         nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register(
          "test_register_10k_metrics", INT_RESULT,
          (Udf_func_any)register_10k_metrics, nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register(
          "test_unregister_10k_metrics", INT_RESULT,
          (Udf_func_any)unregister_10k_metrics, nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_register_metric_a", INT_RESULT,
                                         (Udf_func_any)register_metric_a,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_unregister_metric_a", INT_RESULT,
                                         (Udf_func_any)unregister_metric_a,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_register_metric_b", INT_RESULT,
                                         (Udf_func_any)register_metric_b,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_unregister_metric_b", INT_RESULT,
                                         (Udf_func_any)unregister_metric_b,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register(
          "test_component_metric_log", INT_RESULT,
          (Udf_func_any)component_metric_log, nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_duplicate_metrics", INT_RESULT,
                                         (Udf_func_any)test_duplicate_metrics,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_invalid_metrics", INT_RESULT,
                                         (Udf_func_any)test_invalid_metrics,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  if (udf_registration_srv->udf_register("test_set_updown_metric", INT_RESULT,
                                         (Udf_func_any)test_set_updown_metric,
                                         nullptr, nullptr)) {
    unregister_udf();
    return true;
  }

  return false;
}

static void meter_change_notify_callback(const char *meter,
                                         MeterNotifyType change) {
  // just log the notification, real metric component would need to update its
  // OTEL object collection
  const char *change_type = (change == MeterNotifyType::METER_ADDED) ? "added"
                            : (change == MeterNotifyType::METER_REMOVED)
                                ? "removed"
                                : "updated";
  g_log.write("*** Meter change notification: %s %s\n", meter, change_type);

  // to test for possible deadlock, simulate production component
  // re-enumerating metrics (without reading value) within the
  //  added/updated meter notification callback
  // (no logging for this enumeration, it just tests for deadlock)
  if (change != MeterNotifyType::METER_REMOVED) {
    g_log.write("*** Meter change - silently enumerate metrics for %s\n",
                meter);
    g_log.suppress(true);
    enumerate_metrics(meter, g_log, false);
    g_log.suppress(false);
  }
}

static void register_change_notification() {
  mysql_meter_notify_register(meter_change_notify_callback);
}

static void unregister_change_notification() {
  mysql_meter_notify_unregister(meter_change_notify_callback);
}

/**
 *  Initialize the test_server_telemetry_metrics component at server start or
 *  component installation:
 *
 *    - Register UDFs
 *    - Register meter change notification callback
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_metrics_component_init() {
  mysql_service_status_t result = 0;

  g_log.write("test_server_telemetry_metrics_component_init init:\n");

  if (register_udf()) {
    g_log.write("Error returned from register_udf()\n");
    result = 1;
    goto error;
  }
  g_log.write(" - UDFs registered.\n");

  register_change_notification();
  g_log.write(" - Meter change notification callback registered.\n");

error:
  g_log.write("End of init\n");
  return result;
}

/**
 *  Terminate the test_server_telemetry_metrics_component at server shutdown or
 *  component deinstallation:
 *
 *   - Unregister meter change notification callback
 *   - Unregister UDFs
 *   - Unregister test metrics
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_metrics_component_deinit() {
  mysql_service_status_t result = 0;

  g_log.write("test_server_telemetry_metrics_component_deinit:\n");

  unregister_change_notification();
  g_log.write(" - Meter change notification callback unregistered.\n");

  if (unregister_udf()) {
    g_log.write("Error returned from unregister_udf()\n");
    result = 1;
    goto error;
  }
  g_log.write(" - UDFs unregistered.\n");

  unregister_10k_metrics_imp();
  unregister_metric_a_imp();
  unregister_metric_b_imp();
  unregister_updown_metric();
  g_log.write(" - Test metrics unregistered.\n");

error:
  g_log.write("End of deinit\n");
  return result;
}

/* test_server_telemetry_metrics component doesn't provide any service */
BEGIN_COMPONENT_PROVIDES(test_server_telemetry_metrics)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_server_telemetry_metrics)
REQUIRES_SERVICE_AS(mysql_server_telemetry_metrics_v1, metrics_v1_srv),
    REQUIRES_SERVICE_AS(udf_registration, udf_registration_srv),
    REQUIRES_SERVICE_AS(mysql_string_factory, string_factory_srv),
    REQUIRES_SERVICE_AS(mysql_string_converter, string_converter_srv),
    REQUIRES_SERVICE(psi_metric_v1), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_server_telemetry_metrics)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_server_telemetry_metrics,
                  "mysql:test_server_telemetry_metrics")
test_server_telemetry_metrics_component_init,
    test_server_telemetry_metrics_component_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_server_telemetry_metrics)
    END_DECLARE_LIBRARY_COMPONENTS
