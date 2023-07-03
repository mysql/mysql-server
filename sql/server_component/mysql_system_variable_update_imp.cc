/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "sql/server_component/mysql_system_variable_update_imp.h"

#include <string.h>
#include <string_view>

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/psi/mysql_rwlock.h>
#include <sql/mysqld.h>
#include <sql/set_var.h>
#include <sql/sql_class.h>
#include <sql/sql_lex.h>
#include <sql/sql_list.h>
#include <sql/sys_vars_shared.h>
#include <sql_string.h>
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/item_func.h"  // Item_func_set_user_var
#include "sql/server_component/set_variables_helper.h"
#include "storing_auto_thd.h"

/**
  Return the system variable type given a type name.
*/
static enum_var_type sysvar_type(const char *type_name) {
  if (type_name) {
    if (!strcmp(type_name, "GLOBAL"))
      return OPT_GLOBAL;
    else if (!strcmp(type_name, "SESSION"))
      return OPT_SESSION;
    else if (!strcmp(type_name, "PERSIST"))
      return OPT_PERSIST;
    else if (!strcmp(type_name, "PERSIST_ONLY"))
      return OPT_PERSIST_ONLY;
  }
  return OPT_DEFAULT;
}

/**
  This is an internal method that handles preparation tasks common for
  all system variable update service APIs:
  - creates temporary THD for an operation, if needed
  - validates that SESSION type is not used with temporary THD

  @param hlp: Execution context handle.
  @param variable_type:  One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         For any other value (including NULL), GLOBAL is
  assumed. SESSION is not supported for a temporary THD.
  @param variable_name:  MySQL string of the variable name.
  @param var_type: Reference to output variable type enumeration (avoids parsing
  type multiple times).
  @retval FALSE: Success
  @retval TRUE:  Failure, error set
*/
static bool prepare_thread_and_validate(Set_variables_helper &hlp,
                                        const char *variable_type,
                                        my_h_string variable_name,
                                        enum_var_type &var_type) {
  var_type = sysvar_type(variable_type);
  if (var_type == OPT_DEFAULT) var_type = OPT_GLOBAL;

  /* Use either the THD provided or create a temporary one */
  if (hlp.is_auto_thd()) {
    /* A session variable update for a temporary THD has no effect
       and is not supported. */
    if (var_type == OPT_SESSION) {
      String *name = reinterpret_cast<String *>(variable_name);
      LogErr(ERROR_LEVEL, ER_TMP_SESSION_FOR_VAR, name->c_ptr_safe());
      return true;
    }
    /*
      Set a temporary lock wait timeout before updating the system variable.
      Some system variables, such as super-read-only, can be blocked by other
      locks during the update. Should that happen, we don't want to be holding
      LOCK_system_variables_hash.
    */
    hlp.get_thd()->variables.lock_wait_timeout = 5;
  }
  // success
  return false;
}

/**
  Common system variable update code (used by different variable value types).

  @param hlp: An execution context
  @param var_type:  Enum type matching one of GLOBAL, SESSION, PERSIST,
  PERSIST_ONLY.
  @param variable_base:  MySQL string of the variable prefix, NULL if none.
  @param variable_name:  MySQL string of the variable name.
  @param variable_value: Pointer to Item object storing the value of the correct
  @retval false: Success
  @retval true:  Failure, error set
*/
static bool common_system_variable_update_set(Set_variables_helper &hlp,
                                              enum_var_type var_type,
                                              my_h_string variable_base,
                                              my_h_string variable_name,
                                              Item *variable_value) {
  String *base = reinterpret_cast<String *>(variable_base);
  String *name = reinterpret_cast<String *>(variable_name);

  if (hlp.add_variable(base ? base->ptr() : nullptr, base ? base->length() : 0,
                       name->ptr(), name->length(), variable_value, var_type))
    return true;

  return hlp.execute();
}

/**
  Implementation for the
  @ref mysql_service_mysql_system_variable_update_string_t service.

  Sets the value of a system variable to a new value specified in
  variable_value. Works only for system variables taking unsigned string values.
  May generate an SQL error that it stores into the current THD (if available).

  @param hthd: THD session handle. If NULL, then a temporary THD will be created
               and then deleted.
  @param variable_type:  One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         For any other value (including NULL), GLOBAL is
  assumed. SESSION is not supported for a temporary THD.
  @param variable_base:  MySQL string of the variable prefix, NULL if none.
  @param variable_name:  MySQL string of the variable name.
  @param variable_value: MySQL string value to pass to the variable.
  @retval FALSE: Success
  @retval TRUE:  Failure, error set
  @sa @ref mysql_service_mysql_system_variable_update_string_t
*/
DEFINE_BOOL_METHOD(mysql_system_variable_update_imp::set_string,
                   (MYSQL_THD hthd, const char *variable_type,
                    my_h_string variable_base, my_h_string variable_name,
                    my_h_string variable_value)) {
  try {
    enum_var_type var_type;
    Set_variables_helper hlp(static_cast<THD *>(hthd));

    if (variable_value == nullptr) return true;

    if (prepare_thread_and_validate(hlp, variable_type, variable_name,
                                    var_type))
      return true;

    String *value = reinterpret_cast<String *>(variable_value);
    Item *value_str = new (hlp.get_thd()->mem_root)
        Item_string(value->c_ptr_safe(), value->length(), value->charset());

    return common_system_variable_update_set(hlp, var_type, variable_base,
                                             variable_name, value_str);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
   Sets the value of a system variable to a new signed integer value.

   Works only for system variables taking integer or compatible values.
   Passing non-NULL THD means that the operation will be executed within the
   scope of existing transaction, thus any operation side effects impacting
   transaction itself (for example it may generate an SQL error that it stores
   into the current THD). If using existing THD, security context of the thread
   is checked to make sure that required privileges exist. Passing NULL makes a
   temporary THD created as a execution context (and destroyed afterwards), i.e.
   no impacts on existing transactions. It doesn't make sense to change SESSION
   variable on a temporary THD, so this operation will generate error.

   @param hthd thread session handle. if NULL a temporary one will be created
   and then deleted.
   @param variable_type: One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         For any other value (including NULL), GLOBAL is
   assumed. SESSION is not supported for a temporary THD.
   @param variable_base: a mysql string of the variable name prefix. NULL if
   none
   @param variable_name: MySQL string of the variable name
   @param variable_value: long long to set as value
   @retval FALSE: success
   @retval TRUE: failure, error set. For error info, see THD if supplied.
*/
DEFINE_BOOL_METHOD(mysql_system_variable_update_imp::set_signed,
                   (MYSQL_THD hthd, const char *variable_type,
                    my_h_string variable_base, my_h_string variable_name,
                    long long variable_value)) {
  try {
    enum_var_type var_type;
    Set_variables_helper hlp(static_cast<THD *>(hthd));

    if (prepare_thread_and_validate(hlp, variable_type, variable_name,
                                    var_type))
      return true;

    Item *value_num = new (hlp.get_thd()->mem_root) Item_int(variable_value);

    return common_system_variable_update_set(hlp, var_type, variable_base,
                                             variable_name, value_num);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
   Sets the value of a system variable to a new unsigned integer value.

   Same analysis as for mysql_system_variable_update_integer::set_signed
   applies here as well.

   @param hthd thread session handle. if NULL a temporary one will be created
   and then deleted.
   @param variable_type: One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         For any other value (including NULL), GLOBAL is
   assumed. SESSION is not supported for a temporary THD.
   @param variable_base: a mysql string of the variable name prefix. NULL if
   none
   @param variable_name: MySQL string of the variable name
   @param variable_value: unsigned long long to set as value
   @retval FALSE: success
   @retval TRUE: failure, error set. For error info, see THD if supplied.
*/
DEFINE_BOOL_METHOD(mysql_system_variable_update_imp::set_unsigned,
                   (MYSQL_THD hthd, const char *variable_type,
                    my_h_string variable_base, my_h_string variable_name,
                    unsigned long long variable_value)) {
  try {
    enum_var_type var_type;
    Set_variables_helper hlp(static_cast<THD *>(hthd));

    if (prepare_thread_and_validate(hlp, variable_type, variable_name,
                                    var_type))
      return true;

    Item *value_num = new (hlp.get_thd()->mem_root) Item_uint(variable_value);

    return common_system_variable_update_set(hlp, var_type, variable_base,
                                             variable_name, value_num);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
   Sets the value of a system variable to its default value.

   Same analysis as for mysql_system_variable_update_integer::set_signed
   applies here as well.

   @param hthd thread session handle. if NULL a temporary one will be created
   and then removed.
   @param variable_type: One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         For any other value (including NULL), GLOBAL is
   assumed. SESSION is not supported for a temporary THD.
   @param variable_base: a mysql string of the variable name prefix. NULL if
   none
   @param variable_name: MySQL string of the variable name
   @retval FALSE: success
   @retval TRUE: failure, error set. For error info, see THD if supplied.
*/
DEFINE_BOOL_METHOD(mysql_system_variable_update_imp::set_default,
                   (MYSQL_THD hthd, const char *variable_type,
                    my_h_string variable_base, my_h_string variable_name)) {
  try {
    enum_var_type var_type;
    Set_variables_helper hlp(static_cast<THD *>(hthd));

    if (prepare_thread_and_validate(hlp, variable_type, variable_name,
                                    var_type))
      return true;

    return common_system_variable_update_set(hlp, var_type, variable_base,
                                             variable_name, nullptr);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
