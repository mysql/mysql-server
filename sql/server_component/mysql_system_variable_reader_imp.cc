/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "sql/server_component/mysql_system_variable_reader_imp.h"
#include "sql/server_component/mysql_system_variable_bits.h"

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <sql/set_var.h>

/**
   Gets the value of a system variable.

   Works only for system variables taking integer or compatible values.
   Passing non-NULL THD means that the operation will be executed within the
   scope of existing transaction, thus any operation side effects impacting
   transaction itself (for example it may generate an SQL error that it stores
   into the current THD). If using existing THD, security context of the thread
   is checked to make sure that required privileges exist. Passing NULL makes a
   call to current_thd and gets the GLOBAL value of the variable.
   Passing NULL with SESSION type will gives error and returns.

   @param hthd thread session handle. if NULL, the GLOBAL value of the variable
   is returned.
   @param variable_type one of [GLOBAL, SESSION].
   @param component_name Name of the component or "mysql_server" for the legacy
   ones.
   @param variable_name Name of the variable
   @param[in,out] val On input: a buffer to hold the value. On output a pointer
   to the value.
   @param[in,out] out_length_of_val On input: the buffer size. On output the
   length of the data copied.

   @retval false   success
   @retval TRUE failure, error set. For error info, see THD if supplied.
*/
DEFINE_BOOL_METHOD(mysql_system_variable_reader_imp::get,
                   (MYSQL_THD hthd, const char *variable_type,
                    const char *component_name, const char *variable_name,
                    void **val, size_t *out_length_of_val)) {
  try {
    enum_var_type var_type;

    var_type = sysvar_type(variable_type);
    if ((var_type == OPT_SESSION) && (hthd == nullptr)) {
      LogErr(ERROR_LEVEL, ER_TMP_SESSION_FOR_VAR, variable_name);
      return true;
    }
    if (var_type == OPT_DEFAULT) var_type = OPT_GLOBAL;
    if (hthd == nullptr) {
      hthd = current_thd;
    }
    // all of the non-prefixed variables are treated as part of the server
    // component
    const char *prefix =
        strcmp(component_name, "mysql_server") == 0 ? "" : component_name;
    auto f = [val, out_length_of_val, var_type, hthd](
                 const System_variable_tracker &, sys_var *var) -> bool {
      return get_variable_value(hthd, var, (char *)*val, var_type,
                                out_length_of_val) == nullptr;
    };
    return System_variable_tracker::make_tracker(prefix, variable_name)
        .access_system_variable<bool>(hthd, f, Suppress_not_found_error::YES)
        .value_or(true);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
