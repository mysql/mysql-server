/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <stdbool.h>
#include <list>
#include <string>

#include "my_compiler.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration_aggregate);

/**
  Helper class to maintain a list of registered UDFs per component.

  Instantiate one per component. Initialize and add the UDFs at init().
  At deinit() if unregister() passes deinitialize and allow component unload.
  Otherwise fail the component unload and keep the set for subsequent unloads.
*/
class udf_list {
  typedef std::list<std::string> udf_list_t;

 public:
  ~udf_list() { unregister(); }
  bool add_scalar(const char *func_name, enum Item_result return_type,
                  Udf_func_any func, Udf_func_init init_func = nullptr,
                  Udf_func_deinit deinit_func = nullptr) {
    if (!mysql_service_udf_registration->udf_register(
            func_name, return_type, func, init_func, deinit_func)) {
      set.push_back(func_name);
      return false;
    }
    return true;
  }

  bool add_aggregate(const char *func_name, enum Item_result return_type,
                     Udf_func_any func, Udf_func_add add_func = nullptr,
                     Udf_func_clear clear_func = nullptr,
                     Udf_func_init init_func = nullptr,
                     Udf_func_deinit deinit_func = nullptr) {
    if (!mysql_service_udf_registration_aggregate->udf_register(
            func_name, return_type, func, init_func, deinit_func, add_func,
            clear_func)) {
      set.push_back(func_name);
      return false;
    }
    return true;
  }

  bool unregister() {
    udf_list_t delete_set;
    /* try to unregister all of the udfs */
    for (auto udf : set) {
      int was_present = 0;
      if (!mysql_service_udf_registration->udf_unregister(udf.c_str(),
                                                          &was_present) ||
          !was_present)
        delete_set.push_back(udf);
    }

    /* remove the unregistered ones from the list */
    for (auto udf : delete_set) set.remove(udf);

    /* success: empty set */
    if (set.empty()) return false;

    /* failure: entries still in the set */
    return true;
  }

 private:
  udf_list_t set;
} * list;

/* actual test material */
namespace udf_impl {
const char *test_init = "test_init", *test_udf = "test_udf",
           *test_udf_clear = "test_clear", *test_udf_add = "test_udf_add";

static bool dynamic_udf_init(UDF_INIT *initid, UDF_ARGS *, char *) {
  initid->ptr = const_cast<char *>(test_init);
  return false;
}

static void dynamic_udf_deinit(UDF_INIT *initid [[maybe_unused]]) {
  assert(initid->ptr == test_init || initid->ptr == test_udf);
}

static long long dynamic_udf(UDF_INIT *initid, UDF_ARGS *,
                             unsigned char *is_null, unsigned char *error) {
  if (initid->ptr == test_init) initid->ptr = const_cast<char *>(test_udf);
  if (initid->ptr != test_udf) {
    *error = 1;
    *is_null = 1;
    return 0;
  }
  return 42;
}

static void dynamic_agg_deinit(UDF_INIT *initid [[maybe_unused]]) {
  assert(initid->ptr == test_init || initid->ptr == test_udf ||
         initid->ptr == test_udf_clear || initid->ptr == test_udf_add);
}

static long long dynamic_agg(UDF_INIT *initid, UDF_ARGS *,
                             unsigned char *is_null, unsigned char *error) {
  if (initid->ptr == test_init || initid->ptr == test_udf_add)
    initid->ptr = const_cast<char *>(test_udf_clear);
  if (initid->ptr == test_udf_clear) initid->ptr = const_cast<char *>(test_udf);
  if (initid->ptr != test_udf) {
    *error = 1;
    *is_null = 1;
    return 0;
  }
  return 42;
}

static void dynamic_agg_clear(UDF_INIT *initid, unsigned char *,
                              unsigned char *) {
  initid->ptr = const_cast<char *>(test_udf_clear);
}

static void dynamic_agg_add(UDF_INIT *initid, UDF_ARGS *, unsigned char *,
                            unsigned char *) {
  initid->ptr = const_cast<char *>(test_udf_add);
}
} /* namespace udf_impl */

static mysql_service_status_t init() {
  /*
   Use the global list pointer without a lock
   assuming serialization by the component infrastructure
  */
  list = new udf_list();

  if (list->add_scalar("dynamic_udf", Item_result::INT_RESULT,
                       (Udf_func_any)udf_impl::dynamic_udf,
                       udf_impl::dynamic_udf_init,
                       udf_impl::dynamic_udf_deinit) ||
      list->add_aggregate(
          "dynamic_agg", Item_result::INT_RESULT,
          (Udf_func_any)udf_impl::dynamic_agg, udf_impl::dynamic_agg_add,
          udf_impl::dynamic_agg_clear, udf_impl::dynamic_udf_init,
          udf_impl::dynamic_agg_deinit)) {
    delete list;
    return 1; /* failure: one of the UDF registrations failed */
  }

  /* success */
  return 0;
}

static mysql_service_status_t deinit() {
  if (list->unregister()) return 1; /* failure: some UDFs still in use */

  delete list;
  return 0; /* success */
}

BEGIN_COMPONENT_PROVIDES(test_udf_registration)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_udf_registration)
REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(udf_registration_aggregate), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_udf_registration)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_udf_registration, "mysql:test_udf_registration")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_udf_registration)
    END_DECLARE_LIBRARY_COMPONENTS
