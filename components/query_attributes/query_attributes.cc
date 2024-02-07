/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
#include <string.h>

#include "mysql/components/services/mysql_query_attributes.h"
#include "mysql/components/services/mysql_string.h"
#include "mysql/components/services/psi_memory.h"
#include "mysql/components/services/udf_metadata.h"
#include "mysql/components/services/udf_registration.h"

#include "mysql/components/library_mysys/my_memory.h"

#include <my_compiler.h>
#include <mysql/psi/mysql_memory.h>

static REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
static REQUIRES_SERVICE_PLACEHOLDER(mysql_query_attributes_iterator);
static REQUIRES_SERVICE_PLACEHOLDER(mysql_query_attribute_string);
static REQUIRES_SERVICE_PLACEHOLDER(mysql_query_attribute_isnull);
static REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
static REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
static REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;

static const size_t query_attribute_value_max_length = 1024;
static const char *query_attribute_return_charset = "utf8mb4";

static PSI_memory_key KEY_memory;

static char *mysql_query_attribute_string(UDF_INIT *initid, UDF_ARGS *args,
                                          char *result [[maybe_unused]],
                                          unsigned long *length,
                                          unsigned char *is_null,
                                          unsigned char *error
                                          [[maybe_unused]]) {
  const char *name = args->args[0];
  mysqlh_query_attributes_iterator iter = nullptr;
  char *ret = nullptr;
  my_h_string h_str = nullptr;

  if (mysql_service_mysql_query_attributes_iterator->create(nullptr, name,
                                                            &iter))
    return nullptr;

  bool is_null_val = true;
  if (mysql_service_mysql_query_attribute_isnull->get(iter, &is_null_val))
    goto end;
  if (is_null_val) goto end;

  if (mysql_service_mysql_query_attribute_string->get(iter, &h_str)) goto end;
  if (mysql_service_mysql_string_converter->convert_to_buffer(
          h_str, initid->ptr, initid->max_length,
          query_attribute_return_charset))
    goto end;
  *length = strlen(initid->ptr);
  ret = initid->ptr;

end:
  if (iter) mysql_service_mysql_query_attributes_iterator->release(iter);
  if (h_str) mysql_service_mysql_string_factory->destroy(h_str);
  if (!ret) *is_null = true;
  return ret;
}

static bool mysql_query_attribute_string_init(UDF_INIT *initid, UDF_ARGS *args,
                                              char *message) {
  if (args->arg_count != 1) {
    strcpy(message, "mysql_query_attribute_string() expects 1 argument");
    return true;
  }

  if (args->arg_type[0] != STRING_RESULT) {
    strcpy(message, "mysql_query_attribute_string() expects a string argument");
    return false;
  }
  initid->maybe_null = true;
  initid->max_length = query_attribute_value_max_length;

  if (mysql_service_mysql_udf_metadata->result_set(
          initid, "charset",
          const_cast<char *>(query_attribute_return_charset))) {
    strcpy(message,
           "mysql_query_attribute_string() failed to set result charset");
    return false;
  }

  if (mysql_service_mysql_udf_metadata->argument_set(
          args, "charset", 0,
          const_cast<char *>(query_attribute_return_charset))) {
    strcpy(message,
           "mysql_query_attribute_string() failed to set result charset");
    return false;
  }

  initid->ptr = reinterpret_cast<char *>(
      my_malloc(KEY_memory, query_attribute_value_max_length + 1, MYF(0)));
  if (!initid->ptr) {
    strcpy(message, "mysql_query_attribute_string() failed to allocate memory");
    return true;
  }
  return false;
}

static void mysql_query_attribute_string_deinit(UDF_INIT *initid
                                                [[maybe_unused]]) {
  if (initid->ptr) my_free(initid->ptr);
}

static mysql_service_status_t init() {
  static PSI_memory_info all_memory[] = {
      {&KEY_memory, "general", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
       "Memory allocated by the query_attributes component"}};
  mysql_memory_register("query_attributes", all_memory,
                        sizeof(all_memory) / sizeof(all_memory[0]));

  if (mysql_service_udf_registration->udf_register(
          "mysql_query_attribute_string", Item_result::STRING_RESULT,
          (Udf_func_any)mysql_query_attribute_string,
          mysql_query_attribute_string_init,
          mysql_query_attribute_string_deinit))
    return 1;
  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration->udf_unregister(
          "mysql_query_attribute_string", &was_present))
    return 1;
  return 0;
}

BEGIN_COMPONENT_PROVIDES(query_attributes)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(query_attributes)
REQUIRES_PSI_MEMORY_SERVICE, REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(mysql_query_attributes_iterator),
    REQUIRES_SERVICE(mysql_query_attribute_string),
    REQUIRES_SERVICE(mysql_query_attribute_isnull),
    REQUIRES_SERVICE(mysql_string_converter),
    REQUIRES_SERVICE(mysql_string_factory),
    REQUIRES_SERVICE(mysql_udf_metadata), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(query_attributes)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("query_attributes", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(query_attributes, "mysql:query_attributes")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(query_attributes)
    END_DECLARE_LIBRARY_COMPONENTS
