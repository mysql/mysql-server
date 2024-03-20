/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <cstdarg>
#include <string>
#include <tuple>
#include <vector>

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_global_variable_attributes_service.h>
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_update_string);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_update_integer);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_update_default);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
REQUIRES_SERVICE_PLACEHOLDER(mysql_global_variable_attributes);
REQUIRES_SERVICE_PLACEHOLDER(mysql_global_variable_attributes_iterator);

BEGIN_COMPONENT_PROVIDES(test_mysql_system_variable_set)
END_COMPONENT_PROVIDES();

// clang-format off
BEGIN_COMPONENT_REQUIRES(test_mysql_system_variable_set)
  REQUIRES_SERVICE(mysql_current_thread_reader),
  REQUIRES_SERVICE(mysql_system_variable_update_string),
  REQUIRES_SERVICE(mysql_system_variable_update_integer),
  REQUIRES_SERVICE(mysql_system_variable_update_default),
  REQUIRES_SERVICE(udf_registration),
  REQUIRES_SERVICE(mysql_string_factory),
  REQUIRES_SERVICE(mysql_string_converter),
  REQUIRES_SERVICE(mysql_udf_metadata),
  REQUIRES_SERVICE(mysql_global_variable_attributes),
  REQUIRES_SERVICE(mysql_global_variable_attributes_iterator),  
END_COMPONENT_REQUIRES();
// clang-format on

void write_log(const char *logfile, const char *format, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)));
#else
    ;
#endif

void write_log(const char *logfile, const char *format, ...) {
  FILE *outfile = fopen(logfile, "a+");
  if (!outfile) return;

  char msg[2048];
  va_list args;
  va_start(args, format);
  const int len = vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  const int bytes = std::min(len, (int)(sizeof(msg) - 1));
  auto written [[maybe_unused]] = fwrite(msg, sizeof(char), bytes, outfile);
  (void)fclose(outfile);
}

bool test_set_system_variable_string_init(UDF_INIT *, UDF_ARGS *args,
                                          char *message) {
  if (args->arg_count != 5) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = false;
  args->maybe_null[1] = true;
  args->maybe_null[2] = false;
  args->maybe_null[3] = false;
  args->maybe_null[4] = false;

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 1,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the second argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 2,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the third argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 3,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the forth argument");
    return true;
  }

  return false;
}

long long test_set_system_variable_string(UDF_INIT * /*initd*/, UDF_ARGS *args,
                                          unsigned char * /*is_null*/,
                                          unsigned char *error) {
  bool make_new_thread = *((long long *)args->args[0]) > 0;

  MYSQL_THD thd = nullptr;

  *error = 0;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";

  my_h_string base = nullptr, name = nullptr, value = nullptr;
  if ((args->args[1] != nullptr &&
       mysql_service_mysql_string_converter->convert_from_buffer(
           &base, args->args[1], args->lengths[1], cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, args->args[2], args->lengths[2], cs) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &value, args->args[3], args->lengths[3], cs)) {
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    if (value) mysql_service_mysql_string_factory->destroy(value);
    *error = 1;
    return 0;
  }

  const char *type = args->args[4];
  if (mysql_service_mysql_system_variable_update_string->set(thd, type, base,
                                                             name, value))
    *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  if (value) mysql_service_mysql_string_factory->destroy(value);
  return *error ? 1 : 0;
}

bool test_set_system_variable_signed_integer_init(UDF_INIT *, UDF_ARGS *args,
                                                  char *message) {
  if (args->arg_count != 5) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = false;
  args->maybe_null[1] = true;
  args->maybe_null[2] = false;
  args->maybe_null[3] = false;
  args->maybe_null[4] = false;

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 1,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the second argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 2,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the third argument");
    return true;
  }

  return false;
}

long long test_set_system_variable_signed_integer(UDF_INIT * /*initd*/,
                                                  UDF_ARGS *args,
                                                  unsigned char * /*is_null*/,
                                                  unsigned char *error) {
  bool make_new_thread = *((long long *)args->args[0]) > 0;

  MYSQL_THD thd = nullptr;

  *error = 0;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";

  my_h_string name = nullptr, base = nullptr;
  if ((args->args[1] != nullptr &&
       mysql_service_mysql_string_converter->convert_from_buffer(
           &base, args->args[1], args->lengths[1], cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, args->args[2], args->lengths[2], cs)) {
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    *error = 1;
    return 0;
  }
  long long value = *((long long *)args->args[3]);
  const char *type = args->args[4];

  if (mysql_service_mysql_system_variable_update_integer->set_signed(
          thd, type, base, name, value))
    *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  return *error ? 1 : 0;
}

bool test_set_system_variable_unsigned_integer_init(UDF_INIT *, UDF_ARGS *args,
                                                    char *message) {
  if (args->arg_count != 5) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = false;
  args->maybe_null[1] = true;
  args->maybe_null[2] = false;
  args->maybe_null[3] = false;
  args->maybe_null[4] = false;

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 1,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the second argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 2,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the third argument");
    return true;
  }

  return false;
}

long long test_set_system_variable_unsigned_integer(UDF_INIT * /*initd*/,
                                                    UDF_ARGS *args,
                                                    unsigned char * /*is_null*/,
                                                    unsigned char *error) {
  bool make_new_thread = *((long long *)args->args[0]) > 0;

  MYSQL_THD thd = nullptr;

  *error = 0;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";

  my_h_string name = nullptr, base = nullptr;
  if ((args->args[1] != nullptr &&
       mysql_service_mysql_string_converter->convert_from_buffer(
           &base, args->args[1], args->lengths[1], cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, args->args[2], args->lengths[2], cs)) {
    *error = 1;
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    return 0;
  }
  unsigned long long value = *((long long *)args->args[3]);
  const char *type = args->args[4];

  if (mysql_service_mysql_system_variable_update_integer->set_unsigned(
          thd, type, base, name, value))
    *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  return *error ? 1 : 0;
}

bool test_set_system_variable_default_init(UDF_INIT *, UDF_ARGS *args,
                                           char *message) {
  if (args->arg_count != 4) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = false;
  args->maybe_null[1] = true;
  args->maybe_null[2] = false;
  args->maybe_null[3] = false;

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 1,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the second argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 2,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the third argument");
    return true;
  }

  return false;
}

long long test_set_system_variable_default(UDF_INIT * /*initd*/, UDF_ARGS *args,
                                           unsigned char * /*is_null*/,
                                           unsigned char *error) {
  bool make_new_thread = *((long long *)args->args[0]) > 0;

  MYSQL_THD thd = nullptr;

  *error = 0;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";

  my_h_string name = nullptr, base = nullptr;
  if ((args->args[1] != nullptr &&
       mysql_service_mysql_string_converter->convert_from_buffer(
           &base, args->args[1], args->lengths[1], cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, args->args[2], args->lengths[2], cs)) {
    *error = 1;
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    return 0;
  }

  const char *type = args->args[3];
  if (mysql_service_mysql_system_variable_update_default->set(thd, type, base,
                                                              name))
    *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  return *error ? 1 : 0;
}

bool test_set_global_variable_attrs_init(UDF_INIT *, UDF_ARGS *args,
                                         char *message) {
  if (args->arg_count < 4) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = true;   // variable base
  args->maybe_null[1] = false;  // variable name
  args->maybe_null[2] = false;  // 1st attribute name
  args->maybe_null[3] = false;  // 1st attribute value
  // more optional attribute key/value pairs may follow

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 1,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the second argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 2,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the third argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 3,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the forth argument");
    return true;
  }

  return false;
}

long long test_set_global_variable_attrs(UDF_INIT * /*initd*/, UDF_ARGS *args,
                                         unsigned char * /*is_null*/,
                                         unsigned char *error) {
  *error = 0;

  const char *variable_base = args->args[0];
  const char *variable_name = args->args[1];
  size_t number_of_attributes = (args->arg_count - 2) / 2;

  for (size_t i = 0; i < number_of_attributes; i++) {
    const int idx1 = 2 + i * 2;
    if (*error == 0 && mysql_service_mysql_global_variable_attributes->set(
                           variable_base, variable_name, args->args[idx1],
                           args->args[idx1 + 1])) {
      *error = 1;
      break;
    }
  }

  return *error ? 1 : 0;
}

bool test_get_global_variable_attrs_init(UDF_INIT *, UDF_ARGS *args,
                                         char *message) {
  if (args->arg_count < 3) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = true;   // variable base
  args->maybe_null[1] = false;  // variable name
  args->maybe_null[2] = true;   // attribute name

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 0,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the first argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 1,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the second argument");
    return true;
  }
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 2,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the third argument");
    return true;
  }

  return false;
}

// custom log file for attribute testing
#define LOGFILE "test_mysql_global_variable_attributes.log"

long long test_get_global_variable_attrs(UDF_INIT * /*initd*/, UDF_ARGS *args,
                                         unsigned char * /*is_null*/,
                                         unsigned char *error) {
  *error = 0;

  const char *variable_base = args->args[0];
  const char *variable_name = args->args[1];
  const char *attribute_name = args->args[2];

  write_log(LOGFILE,
            "*** test_get_global_variable_attributes: Iterate attributes of "
            "system variable [%s] (attribute=%s)\n",
            variable_name, attribute_name);

  const char *cs = "latin1";
  global_variable_attributes_iterator attr_iterator = nullptr;
  if (*error == 0 &&
      mysql_service_mysql_global_variable_attributes_iterator->create(
          variable_base, variable_name, attribute_name, &attr_iterator)) {
    write_log(LOGFILE,
              "*** test_get_global_variable_attributes: Failed to create "
              "iterator (%s)\n",
              variable_name);
    *error = 1;
  }

  my_h_string name_handle = nullptr;
  my_h_string value_handle = nullptr;

  if (*error == 0) {
    for (;;) {
      if (mysql_service_mysql_global_variable_attributes_iterator->get_name(
              attr_iterator, &name_handle)) {
        write_log(
            LOGFILE,
            "*** test_get_global_variable_attributes: Failed to get name\n");
        *error = 1;
        break;
      }
      if (mysql_service_mysql_global_variable_attributes_iterator->get_value(
              attr_iterator, &value_handle)) {
        write_log(
            LOGFILE,
            "*** test_get_global_variable_attributes: Failed to get value\n");
        *error = 1;
        break;
      }

      char attr_name[32 + 1];
      if (mysql_service_mysql_string_converter->convert_to_buffer(
              name_handle, attr_name, sizeof(attr_name), cs)) {
        write_log(LOGFILE,
                  "*** test_get_global_variable_attributes: Failed to convert "
                  "name\n");
        *error = 1;
        break;
      }
      attr_name[32] = '\0';

      char attr_value[1024 + 1];
      if (mysql_service_mysql_string_converter->convert_to_buffer(
              value_handle, attr_value, sizeof(attr_value), cs)) {
        write_log(
            LOGFILE,
            "*** test_get_global_variable_attributes: Failed to get value\n");
        *error = 1;
        break;
      }
      attr_value[1024] = '\0';

      write_log(LOGFILE, " >> attribute_name [%s], attribute_value [%s]\n",
                attr_name, attr_value);

      if (name_handle) {
        mysql_service_mysql_string_factory->destroy(name_handle);
        name_handle = nullptr;
      }
      if (value_handle) {
        mysql_service_mysql_string_factory->destroy(value_handle);
        value_handle = nullptr;
      }

      if (mysql_service_mysql_global_variable_attributes_iterator->advance(
              attr_iterator))
        break;
    }
  }

  if (*error == 0 &&
      mysql_service_mysql_global_variable_attributes_iterator->destroy(
          attr_iterator)) {
    write_log(LOGFILE,
              "*** test_get_global_variable_attributes: Failed to destroy "
              "iterator\n");
    *error = 1;
  }

  if (name_handle) mysql_service_mysql_string_factory->destroy(name_handle);
  if (value_handle) mysql_service_mysql_string_factory->destroy(value_handle);
  return *error ? 1 : 0;
}

static std::vector<std::tuple<const char *, Udf_func_longlong, Udf_func_init>>
    function_list{
        {"test_set_system_variable_string", test_set_system_variable_string,
         test_set_system_variable_string_init},
        {"test_set_system_variable_signed_integer",
         test_set_system_variable_signed_integer,
         test_set_system_variable_signed_integer_init},
        {"test_set_system_variable_unsigned_integer",
         test_set_system_variable_unsigned_integer,
         test_set_system_variable_unsigned_integer_init},
        {"test_set_system_variable_default", test_set_system_variable_default,
         test_set_system_variable_default_init},
        {"test_set_global_variable_attributes", test_set_global_variable_attrs,
         test_set_global_variable_attrs_init},
        {"test_get_global_variable_attributes", test_get_global_variable_attrs,
         test_get_global_variable_attrs_init},
    };

static mysql_service_status_t init() {
  size_t pos = 0;
  for (const auto &[name, fn_udf, fn_init] : function_list) {
    if (mysql_service_udf_registration->udf_register(
            name, INT_RESULT, reinterpret_cast<Udf_func_any>(fn_udf), fn_init,
            nullptr)) {
      fprintf(stderr, "Can't register the %s UDF\n", name);
      // cleanup, unregister already registered UDFs
      for (size_t j = 0; j < pos; ++j) {
        int was_present = 0;
        if (mysql_service_udf_registration->udf_unregister(
                std::get<0>(function_list[j]), &was_present))
          fprintf(stderr, "Can't unregister the %s UDF\n",
                  std::get<0>(function_list[j]));
      }
      return 1;
    }
    ++pos;
  }

  return 0;
}

static mysql_service_status_t deinit() {
  for (const auto &item : function_list) {
    int was_present = 0;
    if (mysql_service_udf_registration->udf_unregister(std::get<0>(item),
                                                       &was_present))
      fprintf(stderr, "Can't unregister the %s UDF\n", std::get<0>(item));
  }

  return 0; /* success */
}

// clang-format off
BEGIN_COMPONENT_METADATA(test_mysql_system_variable_set)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_mysql_system_variable_set,
                  "mysql:test_mysql_system_variable_set")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_mysql_system_variable_set)
    END_DECLARE_LIBRARY_COMPONENTS
    // clang-format on
