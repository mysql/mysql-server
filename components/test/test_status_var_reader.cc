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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_status_variable_reader.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>
#include <cstring>
#include <list>
#include <string>

#include "my_compiler.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_status_variable_string);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);

/**
  Helper class to maintain a list of registered UDFs per component.

  Instantiate one udf_list per component. Initialize the list and add the UDFs
  to it during component init(). During component deinit() if the unregister()
  method passes deinitialize the component and allow it to unload. Otherwise
  fail the component unload and keep the set for subsequent unloads.
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

const size_t size = 1024;
const char *charset = "utf8mb4";

static bool test_get_status_var_init(UDF_INIT *initid, UDF_ARGS *args, char *) {
  initid->ptr = new char[size + 1];
  initid->maybe_null = true;
  return mysql_service_mysql_udf_metadata->result_set(
             initid, "charset", const_cast<char *>(charset)) ||
         mysql_service_mysql_udf_metadata->argument_set(
             args, "charset", 0, const_cast<char *>("latin1"));
}

static void test_get_status_var_deinit(UDF_INIT *initid) {
  delete[] initid->ptr;
}

static char *test_get_status_var(UDF_INIT *initid, UDF_ARGS *args,
                                 char * /* result */, unsigned long *length,
                                 unsigned char *is_null, unsigned char *error) {
  my_h_string str = nullptr;
  bool get_global = *(reinterpret_cast<long long *>(args->args[1])) == 0;
  MYSQL_THD thd = nullptr;

  if (!get_global && mysql_service_mysql_current_thread_reader->get(&thd)) {
    *is_null = 1;
    *error = 1;
    return nullptr;
  }
  if (!mysql_service_mysql_status_variable_string->get(thd, args->args[0],
                                                       get_global, &str) &&
      str &&
      !mysql_service_mysql_string_converter->convert_to_buffer(str, initid->ptr,
                                                               size, charset)) {
    mysql_service_mysql_string_factory->destroy(str);
    *is_null = 0;
    *length = strlen(initid->ptr);
    return initid->ptr;
  }

  /* wasn't able to get the value */
  if (str) mysql_service_mysql_string_factory->destroy(str);
  *is_null = 1;
  *error = 1;
  return nullptr;
}

} /* namespace udf_impl */

static mysql_service_status_t init() {
  /*
   Use the global list pointer without a lock
   assuming serialization by the component infrastructure
  */
  list = new udf_list();

  if (list->add_scalar("test_get_status_var", Item_result::STRING_RESULT,
                       (Udf_func_any)udf_impl::test_get_status_var,
                       udf_impl::test_get_status_var_init,
                       udf_impl::test_get_status_var_deinit)) {
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

BEGIN_COMPONENT_PROVIDES(test_status_var_reader)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_status_var_reader)
REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(mysql_status_variable_string),
    REQUIRES_SERVICE(mysql_string_factory),
    REQUIRES_SERVICE(mysql_string_converter),
    REQUIRES_SERVICE(mysql_udf_metadata),
    REQUIRES_SERVICE(mysql_current_thread_reader), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_status_var_reader)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_status_var_reader, "mysql:test_status_var_reader")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_status_var_reader)
    END_DECLARE_LIBRARY_COMPONENTS
