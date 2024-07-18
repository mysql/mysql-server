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

#include <string>
#include <vector>

#include <scope_guard.h> /* create_scope_guard */

#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/mysql_current_thread_reader.h"
#include "mysql/components/services/mysql_thd_store_service.h"
#include "mysql/components/services/udf_registration.h"
#include "scope_guard.h"

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader, thread_service);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_store, mysql_thd_store_service);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, mysql_udf_registration);

namespace test_mysql_thd_store_service {

class Test_mysql_thd_data final {
 public:
  Test_mysql_thd_data() {
    vector_.push_back("Quick ");
    vector_.push_back("Brown ");
    vector_.push_back("Fox ");
    vector_.push_back("Jumped ");
    vector_.push_back("Over ");
    vector_.push_back("The ");
    vector_.push_back("Lazy ");
    vector_.push_back("Dog.");
  }

  bool sanity(const std::string &expected) {
    std::string actual{};

    for (auto one : vector_) {
      actual.append(one);
    }

    return (actual == expected);
  }

  ~Test_mysql_thd_data() { vector_.clear(); }

 private:
  std::vector<std::string> vector_;
};

mysql_thd_store_slot g_slot{nullptr};

int free_resource_callback(void *resource [[maybe_unused]]) {
  if (resource) {
    auto *test_mysql_thd_data =
        reinterpret_cast<Test_mysql_thd_data *>(resource);
    delete test_mysql_thd_data;
  }
  return 0;
}

bool test_thd_store_service_function_init(UDF_INIT *initid [[maybe_unused]],
                                          UDF_ARGS *args, char *message) {
  initid->ptr = nullptr;
  if (args->arg_count != 0) {
    sprintf(message, "Function does not expect any arguments.");
    return true;
  }
  return false;
}

long long test_thd_store_service_function(UDF_INIT *, UDF_ARGS *,
                                          unsigned char *,
                                          unsigned char *error) {
  auto cleanup = create_scope_guard([&] { *error = 1; });
  MYSQL_THD o_thd{nullptr};
  if (thread_service->get(&o_thd)) return 0;

  Test_mysql_thd_data *test_mysql_thd_data =
      reinterpret_cast<Test_mysql_thd_data *>(
          mysql_thd_store_service->get(o_thd, g_slot));

  if (test_mysql_thd_data) delete test_mysql_thd_data;

  test_mysql_thd_data = new Test_mysql_thd_data();

  if (!test_mysql_thd_data) return 0;

  if (mysql_thd_store_service->set(o_thd, g_slot, test_mysql_thd_data)) {
    delete test_mysql_thd_data;
    return 0;
  }

  cleanup.release();

  return 1;
}

/**
  Note: Test assumes that INSTALL and UNINSTALL operations are done
        from the same client connection. If not, the test will fail.
*/

static mysql_service_status_t init() {
  std::string expected{"Quick Brown Fox Jumped Over The Lazy Dog."};
  MYSQL_THD o_thd{nullptr};
  if (thread_service->get(&o_thd)) return true;

  if (mysql_udf_registration->udf_register(
          "test_thd_store_service_function", INT_RESULT,
          (Udf_func_any)test_thd_store_service_function,
          test_thd_store_service_function_init, nullptr)) {
    return true;
  }

  if (mysql_thd_store_service->register_slot(
          "component_test_mysql_thd_store_service", free_resource_callback,
          &g_slot))
    return true;

  Test_mysql_thd_data *test_mysql_thd_data =
      new (std::nothrow) Test_mysql_thd_data();

  auto cleanup_guard = create_scope_guard([&] {
    if (g_slot) (void)mysql_thd_store_service->unregister_slot(g_slot);
    if (test_mysql_thd_data) delete test_mysql_thd_data;
  });

  if (!test_mysql_thd_data ||
      mysql_thd_store_service->set(
          o_thd, g_slot, reinterpret_cast<void *>(test_mysql_thd_data)))
    return true;

  Test_mysql_thd_data *retrieved_test_mysql_thd_data =
      reinterpret_cast<Test_mysql_thd_data *>(
          mysql_thd_store_service->get(nullptr, g_slot));

  if (!retrieved_test_mysql_thd_data ||
      !retrieved_test_mysql_thd_data->sanity(expected))
    return true;

  unsigned int first_slot = *(reinterpret_cast<unsigned int *>(g_slot));

  (void)mysql_thd_store_service->set(o_thd, g_slot, nullptr);

  if (mysql_thd_store_service->unregister_slot(g_slot)) return true;

  g_slot = nullptr;

  if (mysql_thd_store_service->register_slot(
          "component_test_mysql_thd_store_service", free_resource_callback,
          &g_slot))
    return true;

  unsigned int second_slot = *(reinterpret_cast<unsigned int *>(g_slot));

  if (first_slot == second_slot) return true;

  if (mysql_thd_store_service->set(o_thd, g_slot, test_mysql_thd_data))
    return true;

  cleanup_guard.release();

  return false;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  (void)mysql_udf_registration->udf_unregister(
      "test_thd_store_service_function", &was_present);

  std::string expected{"Quick Brown Fox Jumped Over The Lazy Dog."};
  MYSQL_THD o_thd{nullptr};
  if (thread_service->get(&o_thd)) return true;

  Test_mysql_thd_data *test_mysql_thd_data =
      reinterpret_cast<Test_mysql_thd_data *>(
          mysql_thd_store_service->get(o_thd, g_slot));

  if (!test_mysql_thd_data || !test_mysql_thd_data->sanity(expected))
    return true;

  if (mysql_thd_store_service->set(nullptr, g_slot, nullptr)) {
    delete test_mysql_thd_data;
    return true;
  }

  delete test_mysql_thd_data;

  test_mysql_thd_data = reinterpret_cast<Test_mysql_thd_data *>(
      mysql_thd_store_service->get(o_thd, g_slot));

  if (test_mysql_thd_data) return true;

  if (mysql_thd_store_service->unregister_slot(g_slot)) return true;

  return false;
}

}  // namespace test_mysql_thd_store_service

/** ================ Component declaration related stuff ================ */

/**
  Component provides

  Intentionally empty as no services are provided by the component
*/
BEGIN_COMPONENT_PROVIDES(component_test_mysql_thd_store_service)
END_COMPONENT_PROVIDES();

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(component_test_mysql_thd_store_service)
REQUIRES_SERVICE_AS(mysql_current_thread_reader, thread_service),
    REQUIRES_SERVICE_AS(mysql_thd_store, mysql_thd_store_service),
    REQUIRES_SERVICE_AS(udf_registration, mysql_udf_registration),
    END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(component_test_mysql_thd_store_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("component_test_mysql_thd_store_service", "1"),
    END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(component_test_mysql_thd_store_service,
                  "mysql::component_test_mysql_thd_store_service")
test_mysql_thd_store_service::init,
    test_mysql_thd_store_service::deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(
    component_test_mysql_thd_store_service) END_DECLARE_LIBRARY_COMPONENTS
