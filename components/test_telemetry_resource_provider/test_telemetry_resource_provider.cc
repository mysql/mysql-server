/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/telemetry_resource_provider_service.h>

namespace test_telemetry {

struct ResourceEntry {
  const char *m_otel_key;
  const char *m_value;
};

ResourceEntry g_resource_array[] = {
    {"cloud.provider", "test_cloud"},
    {"cloud.resource_id", "RESOURCE-1234"},
    {"cloud.region", "CLOUD-DEV"},
    {"test.tenant", "TENANT-5678"},
};
size_t g_resource_array_size = 4;

struct Iterator {
  size_t m_index{0};
};

telemetry_resource_t *test_resource_create() {
  auto *res = reinterpret_cast<telemetry_resource_t *>(g_resource_array);
  return res;
}

void test_resource_destroy(telemetry_resource_t * /* resource */) {}

telemetry_resource_iterator_t *test_resource_iterator_create(
    telemetry_resource_t * /* resource */) {
  auto *iter = new Iterator();
  auto *res_iterator = reinterpret_cast<telemetry_resource_iterator_t *>(iter);
  return res_iterator;
}

void test_resource_iterator_destroy(
    telemetry_resource_iterator_t *resource_iterator) {
  auto *iter = reinterpret_cast<Iterator *>(resource_iterator);
  delete iter;
}

bool test_resource_iterator_next(
    telemetry_resource_iterator_t *resource_iterator) {
  auto *iter = reinterpret_cast<Iterator *>(resource_iterator);

  iter->m_index++;

  return static_cast<bool>(iter->m_index < g_resource_array_size);
}

bool test_resource_iterator_get_key_name(
    telemetry_resource_iterator_t *resource_iterator, const char **name) {
  auto *iter = reinterpret_cast<Iterator *>(resource_iterator);
  if (iter->m_index < g_resource_array_size) {
    *name = g_resource_array[iter->m_index].m_otel_key;
    return false;
  }

  return true;
}

bool test_resource_iterator_get_key_value(
    telemetry_resource_iterator_t *resource_iterator, const char **value) {
  auto *iter = reinterpret_cast<Iterator *>(resource_iterator);
  if (iter->m_index < g_resource_array_size) {
    *value = g_resource_array[iter->m_index].m_value;
    return false;
  }

  return true;
}

mysql_service_status_t test_telemetry_resource_provider_init() { return 0; }

mysql_service_status_t test_telemetry_resource_provider_deinit() { return 0; }

SERVICE_TYPE(telemetry_resource_provider)
SERVICE_IMPLEMENTATION(component_test_telemetry_resource_provider,
                       telemetry_resource_provider) = {
    test_resource_create,
    test_resource_destroy,
    test_resource_iterator_create,
    test_resource_iterator_destroy,
    test_resource_iterator_next,
    test_resource_iterator_get_key_name,
    test_resource_iterator_get_key_value};

BEGIN_COMPONENT_PROVIDES(component_test_telemetry_resource_provider)
PROVIDES_SERVICE(component_test_telemetry_resource_provider,
                 telemetry_resource_provider),
    END_COMPONENT_PROVIDES();

// clang-format off
BEGIN_COMPONENT_REQUIRES(component_test_telemetry_resource_provider)
END_COMPONENT_REQUIRES();
// clang-format on

// clang-format off
BEGIN_COMPONENT_METADATA(component_test_telemetry_resource_provider)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("mysql.license", "GPL"),
  METADATA("component_test_telemetry_resource_provider", "1"),
END_COMPONENT_METADATA();
// clang-format on

// clang-format off
DECLARE_COMPONENT(component_test_telemetry_resource_provider,
                  "mysql::component_test_telemetry_resource_provider")
  test_telemetry_resource_provider_init,
  test_telemetry_resource_provider_deinit
END_DECLARE_COMPONENT();
// clang-format on

// clang-format off
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(component_test_telemetry_resource_provider)
END_DECLARE_LIBRARY_COMPONENTS
// clang-format on

}  // namespace test_telemetry
