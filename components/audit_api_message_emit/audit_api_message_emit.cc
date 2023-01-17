/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

/**
  @brief

  audit_api_message_emit component exposes audit_log_message UDF that
  generates MYSQL_AUDIT_MESSAGE_USER event of the MYSQL_AUDIT_MESSAGE_CLASS
  class.

  All installed audit plugins that subscribe MYSQL_AUDIT_MESSAGE_USER event
  will receive this event.
*/

#include <assert.h>
#include <ctype.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/audit_api_message_service.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/service_plugin_registry.h>
#include <mysql_com.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <cstdarg>
#include <map>
#include <memory>
#include <string>
#include "my_compiler.h"
#include "template_utils.h"

REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
REQUIRES_SERVICE_PLACEHOLDER(mysql_audit_api_message);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);

/**
  @class IError_handler

  Error handling interface.
*/
class IError_handler {
 public:
  /**
    Virtual destructor.
  */
  virtual ~IError_handler() = default;
  /**
    Error reporting method.

    @param message Error message.
  */
  virtual void error(const char *message, ...) = 0;
};

/**
  Argument validation function prototype.

  @param handler Error reporting handler.
  @param arg     Argument value pointer.
  @param length  Argument value length.
  @param arg_pos Argument pos.
*/
typedef bool (*validate_function)(IError_handler &handler, const char *arg,
                                  unsigned long length, size_t arg_pos);

/**
  Check, whether the argument is not null pointer.
*/
static bool not_null(IError_handler &handler, const char *arg,
                     unsigned long length [[maybe_unused]], size_t arg_pos) {
  if (arg == nullptr) {
    handler.error("Argument cannot be NULL [%d].", arg_pos);
    return false;
  }

  return true;
}

/**
  Structure used for argument type validation.
*/
struct Arg_type {
  /**
    Expected argument type.
  */
  Item_result type;
  /**
    Custom argument validation function.
  */
  validate_function validator;
};

/**
  Obligatory UDF parameters.
*/
Arg_type audit_log_primary_args[] = {{STRING_RESULT, not_null},
                                     {STRING_RESULT, not_null},
                                     {STRING_RESULT, not_null}};
/**
  Key value string parameters.
*/
Arg_type audit_log_key_value_string_args[] = {{STRING_RESULT, not_null},
                                              {STRING_RESULT, nullptr}};
/**
  Key value numeric parameters.
*/
Arg_type audit_log_key_value_int_args[] = {{STRING_RESULT, not_null},
                                           {INT_RESULT, nullptr}};
/**
  Argument definition structure.
*/
struct Arg_def {
  /**
    Argument array pointer.
  */
  Arg_type *args;
  /**
    Number of arguments in the array.
  */
  size_t count;
};

/**
  Obligatory arguments definition (component, producer, message).
*/
Arg_def audit_log_primary_args_def[] = {
    {audit_log_primary_args, array_elements(audit_log_primary_args)}};

/**
  Optional arguments definition (key, value).
*/
Arg_def audit_log_extra_args_def[] = {
    {audit_log_key_value_string_args,
     array_elements(audit_log_key_value_string_args)},
    {audit_log_key_value_int_args,
     array_elements(audit_log_key_value_int_args)}};

/**
  Max argument count of all definitions.

  @param arg_def      Argument definitions.
  @param arg_def_size Argument definitions size.

  @return Max argument count.
*/
size_t max_arg_count(Arg_def *arg_def, size_t arg_def_size) {
  size_t max = 0;

  while (arg_def_size-- > 0) {
    max = std::max(arg_def++->count, max);
  }

  return max;
}

namespace {
// We chose this collation since audit_log plugin sets the same explicitly.
const char *collation = ("utf8mb4_general_ci");
char *collation_name = const_cast<char *>(collation);
}  // namespace

/**
  Set the character set and collation of each argument

  @param [in, out]  args      UDF arguments structure
  @param [out]      handler   Error handler

  @retval false Set the charset of all arguments successfully
  @retval true  Otherwise
*/
static bool set_args_charset_info(UDF_ARGS *args, IError_handler &handler) {
  for (size_t index = 0; index < args->arg_count; ++index) {
    if (args->arg_type[index] == STRING_RESULT &&
        mysql_service_mysql_udf_metadata->argument_set(
            args, "collation", index, pointer_cast<void *>(collation_name))) {
      handler.error("Could not set the %s collation of argument '%d'.",
                    collation_name, index);
      return true;
    }
  }
  return false;
}

/**
  Sets the charset info of the return value to utf8mb4.

  @param [in, out]  initid    A pointer to the UDF_INIT structure
  @param [out]      handler   Error handler that keeps the error message

  @retval false Charset info of return value set successfully.
  @retval true  Otherwise
*/
bool set_return_value_charset_info(UDF_INIT *initid, IError_handler &handler) {
  if (mysql_service_mysql_udf_metadata->result_set(
          initid, "collation", pointer_cast<void *>(collation_name))) {
    handler.error("Could not set the %s collation of return value.", collation);
    return true;
  }
  return false;
}

/**
  Check, whether specified arguments match the definitions.

  @param handler          Error reporting handler.
  @param arg_count        Number of arguments.
  @param arg_type         Argument type array.
  @param arg_def          Argument definitions.
  @param arg_def_size     Argument definitions size.
  @param args             UDF arguments.
  @param arg_lengths      UDF argument lengths.
  @param strict_arg_count Strictly check provided argument count. If this is
                          set to false, if the provided argument count is
                          greater, this does not return error.

  @retval -1  None of the argument definition was matched.
  @retval >=0 n-th argument definition was matched.
*/
static int arg_check(IError_handler &handler, unsigned int arg_count,
                     Item_result *arg_type, Arg_def *arg_def,
                     size_t arg_def_size, char **args,
                     unsigned long *arg_lengths, bool strict_arg_count = true) {
  /*
    This array should not be smaller than number of argument definitions.
  */
  bool res[2];
  bool result = false;

  assert(array_elements(res) >= arg_def_size);

  /*
    Check, whether provided argument count matches expected argument count.
  */
  for (size_t i = 0; i < arg_def_size; ++i)
    if ((res[i] = ((strict_arg_count && arg_def[i].count == arg_count) ||
                   (!strict_arg_count && arg_def[i].count <= arg_count))))
      result = true;

  /*
    At least one argument count was matched against definition.
  */
  if (result == false) {
    handler.error("Invalid argument count.");
    return -1;
  }

  /*
    Do not use actual argument count. Use maximal argument count provided by
    the definitions.
  */
  arg_count = static_cast<unsigned int>(max_arg_count(arg_def, arg_def_size));

  for (size_t i = 0; i < arg_count; ++i) {
    size_t j;

    /*
      Check argument type.
    */
    for (j = 0, result = false; j < arg_def_size; ++j) {
      if ((res[j] = res[j] && arg_def[j].args[i].type == arg_type[i]))
        result = true;
    }

    if (result == false) {
      handler.error("Invalid argument type [%d].", i);
      return -1;
    }

    /*
      Apply custom arg validation.
    */
    for (j = 0, result = false; j < arg_def_size; ++j)
      if ((res[j] = res[j] && (arg_def[j].args[i].validator == nullptr ||
                               arg_def[j].args[i].validator(
                                   handler, args[i], arg_lengths[i], i))))
        result = true;

    if (result == false) {
      /*
        Error has been already set by the validator.
      */
      return -1;
    }
  }

  /*
    Find which argument definition was matched.
  */
  for (size_t i = 0; i < arg_def_size; ++i) {
    if (res[i]) return static_cast<int>(i);
  }

  return -1;
}

/**
  Argument check for UDF provided arguments. This checks both, obligatory and
  optional arguments.

  @param handler Error handler used for error handling.
  @param args    UDF_ARGS structure.

  @retval false Succeeded. Arguments are ok.
  @retval true  Failed. Error is reported via specified handler.
*/
static bool arg_check(IError_handler &handler, UDF_ARGS *args) {
  /*
    Check obligatory args.
  */
  int arg_res;

  if ((arg_res = arg_check(handler, args->arg_count, args->arg_type,
                           audit_log_primary_args_def,
                           array_elements(audit_log_primary_args_def),
                           args->args, args->lengths, false)) < 0) {
    return true;
  }

  unsigned int arg_count =
      args->arg_count -
      static_cast<unsigned int>(audit_log_primary_args_def[arg_res].count);
  Item_result *arg_type =
      args->arg_type + audit_log_primary_args_def[arg_res].count;
  char **arguments = args->args + audit_log_primary_args_def[arg_res].count;
  unsigned long *arg_lengths =
      args->lengths + audit_log_primary_args_def[arg_res].count;

  while (arg_count > 0) {
    /*
      Check key-value pairs if present.
    */
    if ((arg_res =
             arg_check(handler, arg_count, arg_type, audit_log_extra_args_def,
                       array_elements(audit_log_extra_args_def), arguments,
                       arg_lengths, false)) < 0) {
      return true;
    }

    arg_count -=
        static_cast<unsigned int>(audit_log_extra_args_def[arg_res].count);
    arg_type += audit_log_extra_args_def[arg_res].count;
    arguments += audit_log_extra_args_def[arg_res].count;
    arg_lengths += audit_log_extra_args_def[arg_res].count;
  };

  if (set_args_charset_info(args, handler)) return true;

  return false;
}

/**
  @class String_error_handler

  Error handler that copies error message into specified buffer.
*/
class String_error_handler : public IError_handler {
 public:
  /**
    Object construction.

    @param buffer         Buffer, where the error is to be copied.
    @param size           Buffer size.
    @param out_size [out] Written bytes into the buffer.
  */
  String_error_handler(char *buffer, size_t size,
                       unsigned long *out_size = nullptr)
      : m_buffer(buffer), m_size(size), m_out_size(out_size) {}

  /**
    Copy message into the buffer.

    @param message Message to be copied.
  */
  void error(const char *message, ...) override
      MY_ATTRIBUTE((format(printf, 2, 3))) {
    va_list va;
    va_start(va, message);
    int copied = vsnprintf(m_buffer, m_size - 1, message, va);
    va_end(va);
    m_buffer[copied] = '\0';

    if (m_out_size != nullptr) *m_out_size = static_cast<unsigned long>(copied);
  }

 private:
  /**
    Buffer pointer.
  */
  char *m_buffer;
  /**
    Buffer size.
  */
  size_t m_size;
  /**
    Written buffer size.
  */
  unsigned long *m_out_size;
};

/**
  UDF function itself.
*/
static char *emit(UDF_INIT *initid [[maybe_unused]], UDF_ARGS *args,
                  char *result, unsigned long *length,
                  unsigned char *null_value [[maybe_unused]],
                  unsigned char *error [[maybe_unused]]) {
  /*
    Store the error as the result of the UDF.
  */
  String_error_handler handler(result, static_cast<size_t>(*length), length);

  int arg_res;

  /*
    Check obligatory args.
  */
  if ((arg_res = arg_check(handler, args->arg_count, args->arg_type,
                           audit_log_primary_args_def,
                           array_elements(audit_log_primary_args_def),
                           args->args, args->lengths, false)) < 0) {
    return result;
  }

  Item_result *arg_type =
      args->arg_type +
      static_cast<unsigned int>(audit_log_primary_args_def[arg_res].count);
  char **arguments = args->args + audit_log_primary_args_def[arg_res].count;
  unsigned long *arg_lengths =
      args->lengths + audit_log_primary_args_def[arg_res].count;

  /*
    Obligatory parameters has been consumed. Process key value arguments.
  */
  std::map<std::string, mysql_event_message_key_value_t> key_values;

  /*
    Check whether the key value has not been duplicated and create
    audit_api_message_key_value structures that go directly into audit api
    call.

    This step could be improved, but we need to implement duplicates check.
  */
  for (unsigned int arg_count =
           args->arg_count -
           static_cast<unsigned int>(audit_log_primary_args_def[arg_res].count);
       arg_count > 0; arg_count -= static_cast<unsigned int>(
                          audit_log_extra_args_def[arg_res].count)) {
    if ((arg_res =
             arg_check(handler, arg_count, arg_type, audit_log_extra_args_def,
                       array_elements(audit_log_extra_args_def), arguments,
                       arg_lengths, false)) < 0)
      return result;

    std::string key(*arguments, *arg_lengths);

    std::map<std::string, mysql_event_message_key_value_t>::const_iterator
        iter = key_values.find(key);
    if (iter != key_values.end()) {
      handler.error("Duplicated key [%d].", args->arg_count - arg_count);
      return result;
    }

    /*
      Build the structure depending on the type of the specified UDF argument.
    */
    mysql_event_message_key_value_t val;

    /*
      Key is always text.
    */
    val.key.str = arguments[0];
    val.key.length = arg_lengths[0];

    /*
      Value depends on the type of the argument.
    */
    if (arg_res == 0) {
      val.value_type = MYSQL_AUDIT_MESSAGE_VALUE_TYPE_STR;
      val.value.str.str = arguments[1];
      val.value.str.length = arg_lengths[1];
    } else if (arg_res == 1) {
      val.value_type = MYSQL_AUDIT_MESSAGE_VALUE_TYPE_NUM;
      val.value.num = *reinterpret_cast<long long *>(arguments[1]);
    }

    key_values[key] = val;

    arg_type += audit_log_extra_args_def[arg_res].count;
    arguments += audit_log_extra_args_def[arg_res].count;
    arg_lengths += audit_log_extra_args_def[arg_res].count;
  }

  /**
    Allocate array that is used by the audit api service.
  */
  std::unique_ptr<mysql_event_message_key_value_t[]> key_value_map(
      key_values.size() > 0
          ? new mysql_event_message_key_value_t[key_values.size()]
          : nullptr);

  mysql_event_message_key_value_t *kv = key_value_map.get();

  /*
    Convert key value map into an array passed to the message function.
  */
  for (std::map<std::string, mysql_event_message_key_value_t>::const_iterator
           i = key_values.begin();
       i != key_values.end(); ++i, ++kv) {
    *kv = i->second;
  }

  /*
    Everything was ok till this point.
  */
  *length = static_cast<unsigned long>(sprintf(result, "%s", "OK"));

  /*
    Audit message service does not return any meaningful value.
  */
  mysql_service_mysql_audit_api_message->emit(
      MYSQL_AUDIT_MESSAGE_USER, args->args[0], args->lengths[0], args->args[1],
      args->lengths[1], args->args[2], args->lengths[2], key_value_map.get(),
      key_values.size());

  return result;
}

/**
  UDF initialization. Check argument correctness.

  @param initd   UDF initializer structure
  @param args    UDF arguments.
  @param message Buffer, where the error is to be written.
*/
static bool emit_init(UDF_INIT *initd, UDF_ARGS *args, char *message) {
  String_error_handler handler(message, MYSQL_ERRMSG_SIZE);

  return (arg_check(handler, args) ||
          set_return_value_charset_info(initd, handler));
}

/**
  Component initialization.
*/
static mysql_service_status_t init() {
  return mysql_service_udf_registration->udf_register(
      "audit_api_message_emit_udf", STRING_RESULT, (Udf_func_any)emit,
      (Udf_func_init)emit_init, nullptr);
}

/**
  Component deinitialization.
*/
static mysql_service_status_t deinit() {
  int was_present = 0;
  return mysql_service_udf_registration->udf_unregister(
             "audit_api_message_emit_udf", &was_present) != 0;
}

BEGIN_COMPONENT_PROVIDES(audit_api_message_emit)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(audit_api_message_emit)
REQUIRES_SERVICE(mysql_audit_api_message), REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(mysql_udf_metadata), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(audit_api_message_emit)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(audit_api_message_emit, "audit_api_message_emit")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(audit_api_message_emit)
    END_DECLARE_LIBRARY_COMPONENTS
