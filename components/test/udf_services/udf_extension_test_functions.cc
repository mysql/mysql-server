/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "udf_extension_test_functions.h"
#include <string.h>
#include "test_udf_extension.h"

using namespace udf_ext;

namespace {
/**
  Helper method that calls the UDF initializer function set in the function
  pointer. It helps us to keep the UDF init function definition tidy.
*/
bool init(UDF_INIT *initid, UDF_ARGS *args, char *message,
          const int expected_arguments, Type type,
          bool (*function)(UDF_INIT *initid, UDF_ARGS *args,
                           const size_t expected_arg_count, Type type)) {
  if (function(initid, args, expected_arguments, type)) {
    strcpy(message, Test_udf_charset::get_last_error().c_str());
    return true;
  }
  return false;
}
/**
  Helper method that calls the UDF execute function set in the function
  pointer. It helps us to keep the UDF init function definition tidy.
*/
char *execute(UDF_INIT *initid, UDF_ARGS *args, char *result,
              unsigned long *length, unsigned char *is_null,
              unsigned char *error,
              bool (*function)(UDF_INIT *, UDF_ARGS *, char **,
                               unsigned long &)) {
  if ((*function)(initid, args, &result, *length)) {
    *is_null = 1;
    *error = 1;
    result = nullptr;
  } else {
    *is_null = 0;
    *error = 0;
  }
  return result;
}
}  // namespace

bool test_result_charset_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  return init(initid, args, message, 2, Type::charset,
              Test_udf_charset::prepare_return_udf);
}

char *test_result_charset(UDF_INIT *initid, UDF_ARGS *args, char *result,
                          unsigned long *length, unsigned char *is_null,
                          unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset::run_return_udf);
}

bool test_args_charset_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  return init(initid, args, message, 2, Type::charset,
              Test_udf_charset::prepare_args_udf);
}

char *test_args_charset(UDF_INIT *initid, UDF_ARGS *args, char *result,
                        unsigned long *length, unsigned char *is_null,
                        unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset::run_args_udf);
}

bool test_result_collation_init(UDF_INIT *initid, UDF_ARGS *args,
                                char *message) {
  return init(initid, args, message, 2, Type::collation,
              Test_udf_charset::prepare_return_udf);
}

char *test_result_collation(UDF_INIT *initid, UDF_ARGS *args, char *result,
                            unsigned long *length, unsigned char *is_null,
                            unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset::run_return_udf);
}

bool test_args_collation_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  return init(initid, args, message, 2, Type::collation,
              Test_udf_charset::prepare_args_udf);
}

char *test_args_collation(UDF_INIT *initid, UDF_ARGS *args, char *result,
                          unsigned long *length, unsigned char *is_null,
                          unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset::run_args_udf);
}

bool test_result_charset_with_value_init(UDF_INIT *initid, UDF_ARGS *args,
                                         char *message) {
  return init(initid, args, message, 2, Type::charset,
              Test_udf_charset_const_value::prepare_return_udf);
}

char *test_result_charset_with_value(UDF_INIT *initid, UDF_ARGS *args,
                                     char *result, unsigned long *length,
                                     unsigned char *is_null,
                                     unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset::run_return_udf);
}

bool test_args_charset_with_value_init(UDF_INIT *initid, UDF_ARGS *args,
                                       char *message) {
  return init(initid, args, message, 2, Type::charset,
              Test_udf_charset_const_value::prepare_args_udf);
}

char *test_args_charset_with_value(UDF_INIT *initid, UDF_ARGS *args,
                                   char *result, unsigned long *length,
                                   unsigned char *is_null,
                                   unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset_const_value::run_args_udf);
}

bool test_result_collation_with_value_init(UDF_INIT *initid, UDF_ARGS *args,
                                           char *message) {
  return init(initid, args, message, 2, Type::collation,
              Test_udf_charset_const_value::prepare_return_udf);
}

char *test_result_collation_with_value(UDF_INIT *initid, UDF_ARGS *args,
                                       char *result, unsigned long *length,
                                       unsigned char *is_null,
                                       unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset_const_value::run_return_udf);
}

bool test_args_collation_with_value_init(UDF_INIT *initid, UDF_ARGS *args,
                                         char *message) {
  return init(initid, args, message, 2, Type::collation,
              Test_udf_charset_const_value::prepare_args_udf);
}

char *test_args_collation_with_value(UDF_INIT *initid, UDF_ARGS *args,
                                     char *result, unsigned long *length,
                                     unsigned char *is_null,
                                     unsigned char *error) {
  return execute(initid, args, result, length, is_null, error,
                 Test_udf_charset_const_value::run_args_udf);
}

char *test_args_without_init_deinit_methods(UDF_INIT *, UDF_ARGS *args,
                                            char *result, unsigned long *length,
                                            unsigned char *,
                                            unsigned char *error) {
  if (args->arg_count != 1 || args->args[0] == nullptr) {
    *error = 1;
    return nullptr;
  }
  strncpy(result, args->args[0], args->lengths[0]);
  *length = args->lengths[0];
  return result;
}

/* ---------------------- UDF(s) denit methods ------------------------------ */

void test_result_charset_deinit(UDF_INIT *initid) {
  Test_udf_charset::deinit(initid);
}

void test_args_charset_deinit(UDF_INIT *initid) {
  Test_udf_charset::deinit(initid);
}

void test_result_collation_deinit(UDF_INIT *initid) {
  Test_udf_charset::deinit(initid);
}

void test_args_collation_deinit(UDF_INIT *initid) {
  Test_udf_charset::deinit(initid);
}

void test_result_charset_with_value_deinit(UDF_INIT *initid) {
  Test_udf_charset_const_value::deinit(initid);
}

void test_args_charset_with_value_deinit(UDF_INIT *initid) {
  Test_udf_charset_const_value::deinit(initid);
}

void test_result_collation_with_value_deinit(UDF_INIT *initid) {
  Test_udf_charset_const_value::deinit(initid);
}

void test_args_collation_with_value_deinit(UDF_INIT *initid) {
  Test_udf_charset_const_value::deinit(initid);
}
