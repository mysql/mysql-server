/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "test_udf_extension.h"
#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/components/services/udf_metadata.h>
#include <string.h>
#include <sstream>
#include <string>

extern REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);

namespace udf_ext {
namespace consts {
const std::string charset("charset");
const std::string collation("collation");
}  // namespace consts

/**
  Initializer method for UDF(s) that tests the character set conversion of
  return value. It acquires necessory services, validates the input arguments,
  fetch the charset of second argument, sets that as charset of return value.

  @param [in]   initid  A pointer to the UDF_INIT structure
  @param [in]   args    A pointer to the UDF_ARGS structure
  @param [in]   expected_arg_count The number of arguments UDf accepts
  @param [in]   type    UDF type if it is collation or charset.

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset::prepare_return_udf(UDF_INIT *initid, UDF_ARGS *args,
                                          const size_t expected_arg_count,
                                          Type type) {
  set_ext_type(type);
  // Consider the second UDF argument to determine the charset of return value
  const uint index = 1;
  std::string csname;
  if (validate_inputs(args, expected_arg_count) ||
      fetch_charset_or_collation_from_arg(args, index, csname) ||
      set_return_value_charset_or_collation(initid, csname) ||
      set_udf_init(initid, args))
    return true;

  return false;
}

/**
  Initializer method for UDF(s) that tests the character set conversion of the
  UDF arguments. It validates the input arguments, fetches the charset or
  collation from the second argument, sets the same as charset of first
  argument.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] expected_arg_count The number of arguments UDf accepts
  @param [in] type  UDF type if it is collation or charset.

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset::prepare_args_udf(UDF_INIT *initid, UDF_ARGS *args,
                                        const size_t expected_arg_count,
                                        Type type) {
  set_ext_type(type);
  const uint index = 1;
  std::string csname;
  if (validate_inputs(args, expected_arg_count) ||
      fetch_charset_or_collation_from_arg(args, index, csname) ||
      set_args_init(args, csname) || set_udf_init(initid, args))
    return true;

  return false;
}

/**
  The udf implementation that tests the change in the character set of
  return value. It picks the first UDF argument as return value then
  it converts the return value into the charset as specified during init()
  time. It uses charset converter service to do the actual conversion.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [out] result The return value that UDF will return
  @param [out] result_len  Length of the return value

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset::run_return_udf(UDF_INIT *initid, UDF_ARGS *args,
                                      char **result,
                                      unsigned long &result_len) {
  return Test_udf_charset_base::run_return_udf(initid, args, result,
                                               result_len);
}

/**
  The udf implementation that tests the change in the character set of the
  UDF arguments. It expects the arguments values to in the character set which
  was specified during the initialization time. It returns the first
  UDF argument as it is.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [out] result The return value the UDF will return
  @param [out] result_len Length of the return value

  @retval false Success
  @retval true  Otherwise.
*/
bool Test_udf_charset::run_args_udf(UDF_INIT *initid, UDF_ARGS *args,
                                    char **result, unsigned long &result_len) {
  return Test_udf_charset_base::run_args_udf(initid, args, result, result_len);
}

/**
  Free the  acquire resources during UDF initialization time.

  @param [in] initd  A pointer to the UDF_INIT structure
*/
void Test_udf_charset::deinit(UDF_INIT *initd) {
  Test_udf_charset_base::deinit(initd);
}

/**
  Depending upon the UDF type, fetch the charset/collation name of a
  UDF argument

  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] index Index of the UDF argument
  @param [in] name  Charset/collation name

  @retval false Success
  @retval true  Otherwise.
*/
bool Test_udf_charset::fetch_charset_or_collation_from_arg(UDF_ARGS *args,
                                                           const int index,
                                                           std::string &name) {
  void *p = nullptr;

  if (mysql_service_mysql_udf_metadata->argument_get(args, s_ext_type.c_str(),
                                                     index, &p)) {
    s_message << "Unable to fetch extension " << s_ext_type << " of argument "
              << index + 1;
    return true;
  }
  name = ((const char *)p);
  return false;
}

/**
  Initializer method for UDF(s) that tests the change in the character set of
  return value. It validates the input arguments, sets the expected character
  set of the return value and initialize the other UDF prerequisites.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] expected_arg_count The number of arguments UDf accepts
  @param [in] type  UDF type if it is collation or charset.

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_const_value::prepare_return_udf(
    UDF_INIT *initid, UDF_ARGS *args, const size_t expected_arg_count,
    Type type) {
  set_ext_type(type);
  // Consider the second UDF argument to determine the charset of return value
  const uint index = 1;
  std::string csname;
  if (validate_inputs(args, expected_arg_count) ||
      fetch_charset_or_collation_from_arg(args, index, csname) ||
      set_return_value_charset_or_collation(initid, csname) ||
      set_udf_init(initid, args))
    return true;

  return false;
}

/**
  Initializer method for UDF(s) that tests the change in the character set
  of UDF arguments. It validates the input arguments, sets the expected
  character set of the UDF argument.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] expected_arg_count The number of arguments UDf accepts
  @param [in] type  UDF type if it is collation or charset.

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_const_value::prepare_args_udf(
    UDF_INIT *initid, UDF_ARGS *args, const size_t expected_arg_count,
    Type type) {
  set_ext_type(type);
  std::string csname;
  const uint index = 1;
  if (validate_inputs(args, expected_arg_count) ||
      fetch_charset_or_collation_from_arg(args, index, csname) ||
      set_args_init(args, csname) || set_udf_init(initid, args))
    return true;

  return false;
}

/**
  The udf implementation that tests the change in the character set of
  return value. It picks the first UDF argument as return value, converts
  the same into the charset specified during the init method and
  returns the converted value

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [out] result The return value the UDF will return
  @param [out] result_len  Length of the return value

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_const_value::run_return_udf(UDF_INIT *initid,
                                                  UDF_ARGS *args, char **result,
                                                  unsigned long &result_len) {
  return Test_udf_charset_base::run_return_udf(initid, args, result,
                                               result_len);
}

/**
  The udf implementation that tests the change in the character set of the
  UDF arguments. It expects the arguments values to in the character set which
  was specified during the initialization time. It returns the first
  UDF argument as it is.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [out] result The return value the UDF will return
  @param [out] result_len Length of the return value

  @retval false UDF executed successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_const_value::run_args_udf(UDF_INIT *initid,
                                                UDF_ARGS *args, char **result,
                                                unsigned long &result_len) {
  return Test_udf_charset_base::run_args_udf(initid, args, result, result_len);
}

/**
  Free the  acquire resources during UDF initialization time.

  @param [in] initd  A pointer to the UDF_INIT structure
*/
void Test_udf_charset_const_value::deinit(UDF_INIT *initd) {
  Test_udf_charset_base::deinit(initd);
}
/**
  Fetch the charset/collation name from the UDF argument.

  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] index Index of the argument
  @param [in] name  Value of the argument
*/
bool Test_udf_charset_const_value::fetch_charset_or_collation_from_arg(
    UDF_ARGS *args, const int index, std::string &name) {
  name = args->args[index];
  if (name.empty()) {
    s_message << s_ext_type << " name cannot be empty. Specify " << s_ext_type
              << " name that is supported by server.";
  }
  return false;
}

std::stringstream Test_udf_charset_base::s_message;
std::string Test_udf_charset_base::s_ext_type;

/*
  Return the last error message if encountered any.
  Clear the error stream
*/
std::string Test_udf_charset_base::get_last_error() {
  std::string err = s_message.str();
  std::stringstream().swap(s_message);  // Reset the the Stringstream
  return err;
}

/**
  A helper method to validate  the UDF arguments type.

  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] expected_arg_count The number of arguments UDf accepts

  @retval false Arguments are validated successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_base::validate_inputs(UDF_ARGS *args,
                                            const size_t expected_arg_count) {
  if (!args) {
    s_message << "UDF_ARGS cannot be NULL.";
    return true;
  }
  if (args->arg_count != expected_arg_count) {
    s_message << "Arguments count mismatch. Expected " << expected_arg_count
              << " while specified arguments " << args->arg_count << ".";
    return true;
  }

  for (unsigned int i = 0; i < args->arg_count; i++) {
    if (args->arg_type[i] != STRING_RESULT) {
      s_message << "This UDF accepts only string arguments. Specify argument "
                << i + 1 << " as string.";
      return true;
    }
  }
  return false;
}

/**
  Fetch the expected character set of the return value. It would have been set
  during init() time. Fetch the character set of the first UDF argument.
  Copy the value for first UDF argument in the buffer and call character
  set conversion service to convert buffer 'from' -> 'to' charset.
  Return the retrieved buffer.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [out] result The return value the UDF will return
  @param [out] result_len Length of the return value

  @retval false UDF executed successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_base::run_return_udf(UDF_INIT *initid, UDF_ARGS *args,
                                           char **result,
                                           unsigned long &result_len) {
  for (uint i = 0; i < args->arg_count; i++) {
    if (!args->args[i]) {
      s_message << "Recieved argument " << i + 1
                << " as null. Specify valid argument";
      return true;
    }
  }
  void *return_charset_name_ptr = nullptr;  // Retrieve the charset of args
  if (mysql_service_mysql_udf_metadata->result_get(
          initid, consts::charset.c_str(), &return_charset_name_ptr) &&
      !return_charset_name_ptr) {
    s_message << "Could not retrieve requested " << consts::charset
              << " extension argument.";
    return true;
  }

  *result = initid->ptr;  // Set the output buffer

  // Retrieve the charset of first arg
  void *first_arg_charset_ptr = nullptr;
  const int index = 0;
  if (mysql_service_mysql_udf_metadata->argument_get(
          args, consts::charset.c_str(), index, &first_arg_charset_ptr)) {
    s_message << "Could not retrieve requested " << consts::charset
              << " extension argument.";
    return true;
  }

  // Here, we are guarateed that charset name pointers are not nullptr
  const char *out_charset_name =
      static_cast<const char *>(return_charset_name_ptr);
  const char *in_charset_name =
      static_cast<const char *>(first_arg_charset_ptr);
  const std::string in_buffer(args->args[index], args->lengths[index]);
  if (convert(out_charset_name, in_charset_name, in_buffer, initid->max_length,
              *result)) {
    return true;
  }

  result_len = strlen(*result);
  return false;
}
/**
  A helper method to initialize the UDF return structure.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure

  @retval false UDF_INIT structure is set for return value
  @retval true  Otherwise.
*/
bool Test_udf_charset_base::set_udf_init(UDF_INIT *initid, UDF_ARGS *args) {
  /*
    We use the first argument for testing purpose.
    Max size of the converted string could be in charset utf16.
    Therefore, allocate the ample memory accordingly.
  */
  size_t length = args->lengths[0] * 4 + 1;
  try {
    initid->ptr = new char[length];
  } catch (...) {
    s_message << "UDF could not allocate the memory. Try after some time once "
                 "the load on server is reduced.";
    return true;
  }
  initid->max_length = length;
  initid->maybe_null = true;
  return false;
}
/**
  Set the charset or collaion for the first UDF argument.

  @param [in] args  A pointer to the UDF_ARGS structure
  @param [in] name  Value to be set.

  @retval false Success
  @retval true  Otherwise
*/
bool Test_udf_charset_base::set_args_init(UDF_ARGS *args,
                                          const std::string &name) {
  char *value = const_cast<char *>(name.c_str());
  if (mysql_service_mysql_udf_metadata->argument_set(
          args, s_ext_type.c_str(), 0, static_cast<void *>(value))) {
    s_message << "Could not set the " << s_ext_type << " = " << name;
    return true;
  }
  return false;
}

void Test_udf_charset_base::set_ext_type(Type ext_type) {
  s_ext_type = consts::charset;
  if (ext_type == Type::collation) s_ext_type = consts::collation;
}

/**
  The udf implementation that tests the change in the character set of the
  UDF arguments. It expects the arguments values to in the character set which
  was specified during the initialization time. It returns the first
  UDF argument as it is.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] args  A pointer to the UDF_ARGS structure
  @param [out] result The return value the UDF will return
  @param [out] result_len Length of the return value

  @retval false UDF is initialized successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_base::run_args_udf(UDF_INIT *initid, UDF_ARGS *args,
                                         char **result,
                                         unsigned long &result_len) {
  for (uint i = 0; i < args->arg_count; i++) {
    if (!args->args[i]) {
      s_message << "Recieved argument " << i + 1
                << " as null. Specify valid argument";
      return true;
    }
  }
  const int index = 0;  // Return the first argument
  strncpy(initid->ptr, args->args[index], args->lengths[index]);
  result_len = args->lengths[index];
  *result = initid->ptr;
  return false;
}

/**
  Converts the buffer from one character set to another.
  - It first acquires the opaque handle from the input buffer and input charset
  - It then acquires the output buffer in the expected character set.

  This method uses string component services to do the conversion.

  @param [in] out_charset_name  Character set name in which out buffer is
                                expected
  @param [in] in_charset_name  Character set name of the input buffer
  @param [in] in_buffer  Input buffer to be converted
  @param [in] out_buffer_length Max size that output buffer can return
  @param [out] out_buffer Output buffer which is converted in the charset
                          specified
  @retval false Buffer is converted into the charset
  @retval true  Otherwise
*/
bool Test_udf_charset_base::convert(const std::string &out_charset_name,
                                    const std::string &in_charset_name,
                                    const std::string &in_buffer,
                                    size_t out_buffer_length,
                                    char *out_buffer) {
  my_h_string out_string = nullptr;
  if (mysql_service_mysql_string_factory->create(&out_string)) {
    s_message << "Create string failed.";
    return true;
  } else {
    mysql_service_mysql_string_factory->destroy(out_string);
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string, in_buffer.c_str(), in_buffer.length(),
            in_charset_name.c_str())) {
      mysql_service_mysql_string_factory->destroy(out_string);
      s_message << "Failed to retrieve the buffer in charset " +
                       in_charset_name;
      return true;
    }
    if (mysql_service_mysql_string_converter->convert_to_buffer(
            out_string, out_buffer, out_buffer_length,
            out_charset_name.c_str())) {
      mysql_service_mysql_string_factory->destroy(out_string);
      s_message << "Failed to convert the buffer in charset " +
                       out_charset_name;
      return true;
    }
  }
  mysql_service_mysql_string_factory->destroy(out_string);
  return false;
}

/* Cleanup the acquired resources during UDF init() */
void Test_udf_charset_base::deinit(UDF_INIT *initd) { delete[] initd->ptr; }

/**
  A helper method that sets the charset names in the extension argument of
  return value.

  @param [in] initid  A pointer to the UDF_INIT structure
  @param [in] name    Value to be set in the extension argument.

  @retval false Value is set successfully.
  @retval true  Otherwise.
*/
bool Test_udf_charset_base::set_return_value_charset_or_collation(
    UDF_INIT *initid, const std::string &name) {
  char *ret_name = const_cast<char *>(name.c_str());
  if (mysql_service_mysql_udf_metadata->result_set(initid, s_ext_type.c_str(),
                                                   (void *)(ret_name))) {
    s_message << "Unable to set " << s_ext_type << " : " << name
              << " of result argument. Specify " << s_ext_type
              << " name which is supported by Server.";
    return true;
  }
  return false;
}
}  // namespace udf_ext
