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

#include <mysql/components/services/telemetry_secret_provider_service.h>

#include <cstring>

namespace test_telemetry {

struct SecretEntry {
  const char *m_name;
  /** Value, encoded in base 64 */
  const char *m_value;
};

SecretEntry g_secret_array[] = {
    /* "key1=12, key2=34" */
    {"secret-1234", "a2V5MT0xMiwga2V5Mj0zNAo="},
    /* "key1=56, key2=78" */
    {"secret-5678", "a2V5MT01Niwga2V5Mj03OAo="},
    /* "key1=90, key2=ab" */
    {"secret-90ab", "a2V5MT05MCwga2V5Mj1hYgo="},
    /* "A=1, broken, ok_empty=, ok_weird=x=y, broken_again" */
    {"secret-broken",
     "QT0xLCBicm9rZW4sIG9rX2VtcHR5PSwgb2tfd2VpcmQ9eD15LCBicm9rZW5fYWdhaW4K"},
    /* "Authorization=dataKey YOUR-APM-KEY-HERE" */
    {"secret-dummy-apm",
     "QXV0aG9yaXphdGlvbj1kYXRhS2V5IFlPVVItQVBNLUtFWS1IRVJFCg=="},
    /* End marker */
    {nullptr, nullptr},
};

mysql_service_status_t test_telemetry_secret_provider_init() { return 0; }

mysql_service_status_t test_telemetry_secret_provider_deinit() { return 0; }

telemetry_secret_client_t *test_secret_init() {
  return reinterpret_cast<telemetry_secret_client_t *>(&g_secret_array[0]);
}

void test_secret_cleanup(telemetry_secret_client_t *) {}

telemetry_secret_t *test_secret_open(telemetry_secret_client_t *,
                                     const char *secret_name) {
  SecretEntry *entry;
  telemetry_secret_t *res = nullptr;

  for (entry = &g_secret_array[0]; entry->m_name != nullptr; entry++) {
    if (strcmp(entry->m_name, secret_name) == 0) {
      res = reinterpret_cast<telemetry_secret_t *>(entry);
      break;
    }
  }

  return res;
}

bool test_secret_read(telemetry_secret_t *secret, const char **secret_value) {
  auto *entry = reinterpret_cast<SecretEntry *>(secret);

  if (entry != nullptr) {
    *secret_value = entry->m_value;
    return false;
  }

  return true;
}

void test_secret_close(telemetry_secret_t * /* secret */) {}

SERVICE_TYPE(telemetry_secret_provider)
SERVICE_IMPLEMENTATION(component_test_telemetry_secret_provider,
                       telemetry_secret_provider) = {
    test_secret_init, test_secret_cleanup, test_secret_open, test_secret_read,
    test_secret_close};

BEGIN_COMPONENT_PROVIDES(component_test_telemetry_secret_provider)
PROVIDES_SERVICE(component_test_telemetry_secret_provider,
                 telemetry_secret_provider),
    END_COMPONENT_PROVIDES();

// clang-format off
BEGIN_COMPONENT_REQUIRES(component_test_telemetry_secret_provider)
END_COMPONENT_REQUIRES();
// clang-format on

// clang-format off
BEGIN_COMPONENT_METADATA(component_test_telemetry_secret_provider)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("mysql.license", "GPL"),
  METADATA("component_test_telemetry_secret_provider", "1"),
END_COMPONENT_METADATA();
// clang-format on

// clang-format off
DECLARE_COMPONENT(component_test_telemetry_secret_provider,
                  "mysql::component_test_telemetry_secret_provider")
  test_telemetry_secret_provider_init,
  test_telemetry_secret_provider_deinit
END_DECLARE_COMPONENT();
// clang-format on

// clang-format off
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(component_test_telemetry_secret_provider)
END_DECLARE_LIBRARY_COMPONENTS
// clang-format on

}  // namespace test_telemetry
