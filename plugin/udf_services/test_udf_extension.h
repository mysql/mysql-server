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

#ifndef TEST_UDF_EXTENSION_INCLUDED
#define TEST_UDF_EXTENSION_INCLUDED

#include <my_inttypes.h>
#include <sstream>
#include <string>

// Forward declarations
struct UDF_INIT;
struct UDF_ARGS;

namespace udf_ext {
enum class Type { charset = 0, collation = 1 };

/*
  A stateless base class that implements the methods needed to test the
  character set/collation of UDF's arguments and return value. It implements
  the methods which are shared by the derived classes.
*/
class Test_udf_charset_base {
 public:
  static std::string get_last_error();

  static void udf_charset_base_init();
  static void udf_charset_base_deinit();

 protected:
  static bool validate_inputs(UDF_ARGS *args, const size_t expected_arg_count);
  static bool set_udf_init(UDF_INIT *initid, UDF_ARGS *args);
  static bool set_args_init(UDF_ARGS *args, const std::string &name);
  static bool set_return_value_charset_or_collation(UDF_INIT *initid,
                                                    const std::string &name);
  static void set_ext_type(Type ext_type);
  static bool run_return_udf(UDF_INIT *initd, UDF_ARGS *args, char **result,
                             unsigned long &result_len);
  static bool run_args_udf(UDF_INIT *initd, UDF_ARGS *args, char **result,
                           unsigned long &result_len);
  static void deinit(UDF_INIT *initd);
  static std::stringstream *s_message;
  static const char *s_ext_type;
};

/**
  A stateless class that provides the implementation of the UDFs that tests the
  character set extension argument. It determines the charset or collation
  names of the second UDF argument and use that to convert either return value
  or first UDF argument.
*/
class Test_udf_charset : public Test_udf_charset_base {
 public:
  static bool prepare_return_udf(UDF_INIT *initid, UDF_ARGS *args,
                                 const size_t expected_arg_count, Type type);
  static bool prepare_args_udf(UDF_INIT *initid, UDF_ARGS *args,
                               const size_t expected_arg_count, Type type);

  static bool run_return_udf(UDF_INIT *initd, UDF_ARGS *args, char **result,
                             unsigned long &result_len);

  static bool run_args_udf(UDF_INIT *initd, UDF_ARGS *args, char **result,
                           unsigned long &result_len);
  static void deinit(UDF_INIT *initd);

 private:
  static bool fetch_charset_or_collation_from_arg(UDF_ARGS *args,
                                                  const int index,
                                                  std::string &name);
};

/**
  A stateless class that provides the implementation of the UDFs that tests the
  character set extension argument. It determines the charset or collation names
  from the second UDF argument, and use that to convert either return value or
  first UDF argument.
*/
class Test_udf_charset_const_value : public Test_udf_charset_base {
 public:
  static bool prepare_return_udf(UDF_INIT *initid, UDF_ARGS *args,
                                 const size_t expected_arg_count, Type type);

  static bool prepare_args_udf(UDF_INIT *initid, UDF_ARGS *args,
                               const size_t expected_arg_count, Type type);

  static bool run_return_udf(UDF_INIT *initd, UDF_ARGS *args, char **result,
                             unsigned long &result_len);

  static bool run_args_udf(UDF_INIT *initd, UDF_ARGS *args, char **result,
                           unsigned long &result_len);
  static void deinit(UDF_INIT *initd);

 private:
  static bool fetch_charset_or_collation_from_arg(UDF_ARGS *args,
                                                  const int index,
                                                  std::string &name);
};
}  // namespace udf_ext
#endif  // !TEST_UDF_EXTENSION_INCLUDED
