/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <mysqld_error.h>
#include <cstdio>
#include <cstdlib>

namespace mysql_runtime_error {  // To avoid ODR asan error
#include <mysql/components/services/mysql_runtime_error_service.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration_aggregate);
BEGIN_COMPONENT_PROVIDES(test_udf_aggregate)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_udf_aggregate)
REQUIRES_SERVICE(mysql_runtime_error), REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(udf_registration_aggregate), END_COMPONENT_REQUIRES();

static void test_udf_aggregate_error_clear(UDF_INIT * /* initid */,
                                           unsigned char *is_null,
                                           unsigned char *error) {
  *is_null = 0;
  *error = 0;
}

static void test_udf_aggregate_error_add(UDF_INIT * /* initid */,
                                         UDF_ARGS * /* args */,
                                         unsigned char *is_null,
                                         unsigned char *error) {
  my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "Test Aggregate", "by MySQL");
  *is_null = 1;
  *error = 1;
}

static long long test_udf_aggregate_error(UDF_INIT * /* initid */,
                                          UDF_ARGS * /* args */,
                                          unsigned char *is_null,
                                          unsigned char *error) {
  my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "Test Aggregate", "by MySQL");
  *is_null = 1;
  *error = 1;
  return 0;
}

static void test_udf_aggregate_crash_clear(UDF_INIT * /* initid */,
                                           unsigned char *is_null,
                                           unsigned char *error) {
  *is_null = 0;
  *error = 0;
}

static void test_udf_aggregate_crash_add(UDF_INIT * /* initid */,
                                         UDF_ARGS * /* args */,
                                         unsigned char * /* is_null */,
                                         unsigned char * /* error */) {
  /* Crash on invocation. This verifies that after the fix for BUG#37398919,
   * test_udf_aggregate_crash_add is not called, after
   * test_udf_aggregate_error_add returns error */
  std::abort();
}

static long long test_udf_aggregate_crash(UDF_INIT * /* initid */,
                                          UDF_ARGS * /* args */,
                                          unsigned char * /* is_null */,
                                          unsigned char * /* error */) {
  std::abort();
}

static mysql_service_status_t init() {
  if (mysql_service_udf_registration_aggregate->udf_register(
          "test_udf_aggregate_error", INT_RESULT,
          reinterpret_cast<Udf_func_any>(test_udf_aggregate_error), nullptr,
          nullptr, test_udf_aggregate_error_add,
          test_udf_aggregate_error_clear)) {
    std::fprintf(stderr, "Can't register the test_udf_aggregate_error UDF\n");
    return 1;
  }
  if (mysql_service_udf_registration_aggregate->udf_register(
          "test_udf_aggregate_crash", INT_RESULT,
          reinterpret_cast<Udf_func_any>(test_udf_aggregate_crash), nullptr,
          nullptr, test_udf_aggregate_crash_add,
          test_udf_aggregate_crash_clear)) {
    std::fprintf(stderr, "Can't register the test_udf_aggregate_crash UDF\n");
    return 1;
  }
  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration_aggregate->udf_unregister(
          "test_udf_aggregate_error", &was_present)) {
    std::fprintf(stderr, "Can't unregister the test_udf_aggregate_error UDF\n");
    return 1;
  }
  was_present = 0;
  if (mysql_service_udf_registration_aggregate->udf_unregister(
          "test_udf_aggregate_crash", &was_present)) {
    std::fprintf(stderr, "Can't unregister the test_udf_aggregate_crash UDF\n");
    return 1;
  }
  return 0; /* success */
}

BEGIN_COMPONENT_METADATA(test_udf_aggregate)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_udf_aggregate, "mysql:test_udf_aggregate")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_udf_aggregate)
    END_DECLARE_LIBRARY_COMPONENTS

}  // namespace mysql_runtime_error
