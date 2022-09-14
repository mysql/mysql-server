/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
#include "storing_auto_thd.h"

/**
  Set a custom lock wait timeout for this session.
*/
class Lock_wait_timeout {
 public:
  Lock_wait_timeout(THD *thd, ulong new_timeout)
      : m_thd(thd), m_save_timeout(thd->variables.lock_wait_timeout) {
    m_thd->variables.lock_wait_timeout = new_timeout;
  }
  ~Lock_wait_timeout() { m_thd->variables.lock_wait_timeout = m_save_timeout; }

 private:
  THD *const m_thd;
  ulong m_save_timeout;
};

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

  @param thd_handle: THD session handle. If NULL, then a temporary THD will be
  created and then deleted at the end of the operation.
  @param variable_type:  One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         For any other value (including NULL), GLOBAL is
  assumed. SESSION is not supported for a temporary THD.
  @param variable_name:  MySQL string of the variable name.
  @param thd: Reference to output THD session handle.
  @param thd_auto: Reference to output unique_ptr that ensures the disposal of
  the temporary THD, if we create one.
  @param var_type: Reference to output variable type enumeration (avoids parsing
  type multiple times).
  @param override_thd_lock_wait_timeout: Reference to output variable, override
  the value of thd lock_wait_timeout
  @retval FALSE: Success
  @retval TRUE:  Failure, error set
*/
static bool prepare_thread_and_validate(
    MYSQL_THD thd_handle, const char *variable_type, my_h_string variable_name,
    THD *&thd, std::unique_ptr<Storing_auto_THD> &thd_auto,
    enum_var_type &var_type, bool &override_thd_lock_wait_timeout) {
  var_type = sysvar_type(variable_type);
  if (var_type == OPT_DEFAULT) var_type = OPT_GLOBAL;

  /* Use either the THD provided or create a temporary one */
  if (thd_handle) {
    /*
      If a THD is being reused, we will use its lock_wait_timeout value.
    */
    override_thd_lock_wait_timeout = false;
    thd = static_cast<THD *>(thd_handle);
  } else {
    /* A session variable update for a temporary THD has no effect
       and is not supported. */
    if (var_type == OPT_SESSION) {
      String *name = reinterpret_cast<String *>(variable_name);
      LogErr(ERROR_LEVEL, ER_TMP_SESSION_FOR_VAR, name->c_ptr_safe());
      return true;
    }
    /*
      lock_wait_timeout value will be override on a temporary THD.
    */
    override_thd_lock_wait_timeout = true;
    thd_auto.reset(new Storing_auto_THD());
    thd = thd_auto->get_THD();
  }
  // success
  return false;
}

/**
  Common system variable update code (used by different variable value types).

  @param thd: THD session handle.
  @param var_type:  Enum type matching one of GLOBAL, SESSION, PERSIST,
  PERSIST_ONLY.
  @param variable_base:  MySQL string of the variable prefix, NULL if none.
  @param variable_name:  MySQL string of the variable name.
  @param variable_value: Pointer to Item object storing the value of the correct
  type (matching the type stored in system variable). If NULL, then the system
  variable default will be set.
  @param override_thd_lock_wait_timeout: override the value of thd
  lock_wait_timeout
  @retval FALSE: Success
  @retval TRUE:  Failure, error set
*/
static bool common_system_variable_update_set(
    THD *thd, enum_var_type var_type, my_h_string variable_base,
    my_h_string variable_name, Item *variable_value,
    bool override_thd_lock_wait_timeout) {
  bool result{false};
  constexpr ulong timeout{5};  // temporary lock wait timeout, in seconds
  std::unique_ptr<Lock_wait_timeout> lock_wait_timeout;

  String *base = reinterpret_cast<String *>(variable_base);
  String *name = reinterpret_cast<String *>(variable_name);

  std::string_view prefix{base ? base->ptr() : nullptr,
                          base ? base->length() : 0};
  std::string_view suffix{name->ptr(), name->length()};

  LEX *lex_save = thd->lex, lex_tmp;
  thd->lex = &lex_tmp;
  lex_start(thd);

  /* Set a temporary lock wait timeout before updating the system variable.
     Some system variables, such as super-read-only, can be blocked by other
     locks during the update. Should that happen, we don't want to be holding
     LOCK_system_variables_hash.
  */
  if (override_thd_lock_wait_timeout) {
    lock_wait_timeout.reset(new Lock_wait_timeout(thd, timeout));
  }

  System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(prefix, suffix);
  if (var_tracker.access_system_variable(thd)) {
    lex_end(thd->lex);
    thd->lex = lex_save;
    return true;
  }
  /* Create a list of system variables to be updated (a list of one) */
  List<set_var_base> sysvar_list;
  sysvar_list.push_back(new (thd->mem_root)
                            set_var(var_type, var_tracker, variable_value));
  /* Update the system variable */
  if (sql_set_variables(thd, &sysvar_list, false)) {
    result = true;
  }

  lex_end(thd->lex);
  thd->lex = lex_save;

  return result;
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
    THD *thd;
    std::unique_ptr<Storing_auto_THD> thd_auto;
    enum_var_type var_type;

    if (variable_value == nullptr) return true;

    bool override_thd_lock_wait_timeout = true;
    if (prepare_thread_and_validate(hthd, variable_type, variable_name, thd,
                                    thd_auto, var_type,
                                    override_thd_lock_wait_timeout))
      return true;
    String *value = reinterpret_cast<String *>(variable_value);
    Item *value_str = new (thd->mem_root)
        Item_string(value->c_ptr_safe(), value->length(), value->charset());

    return common_system_variable_update_set(thd, var_type, variable_base,
                                             variable_name, value_str,
                                             override_thd_lock_wait_timeout);
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
    THD *thd;
    std::unique_ptr<Storing_auto_THD> thd_auto;
    enum_var_type var_type;

    bool override_thd_lock_wait_timeout = true;
    if (prepare_thread_and_validate(hthd, variable_type, variable_name, thd,
                                    thd_auto, var_type,
                                    override_thd_lock_wait_timeout))
      return true;

    Item *value_num = new (thd->mem_root) Item_int(variable_value);

    return common_system_variable_update_set(thd, var_type, variable_base,
                                             variable_name, value_num,
                                             override_thd_lock_wait_timeout);
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
    THD *thd;
    std::unique_ptr<Storing_auto_THD> thd_auto;
    enum_var_type var_type;

    bool override_thd_lock_wait_timeout = true;
    if (prepare_thread_and_validate(hthd, variable_type, variable_name, thd,
                                    thd_auto, var_type,
                                    override_thd_lock_wait_timeout))
      return true;

    Item *value_num = new (thd->mem_root) Item_uint(variable_value);

    return common_system_variable_update_set(thd, var_type, variable_base,
                                             variable_name, value_num,
                                             override_thd_lock_wait_timeout);
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
    THD *thd;
    std::unique_ptr<Storing_auto_THD> thd_auto;
    enum_var_type var_type;

    bool override_thd_lock_wait_timeout = true;
    if (prepare_thread_and_validate(hthd, variable_type, variable_name, thd,
                                    thd_auto, var_type,
                                    override_thd_lock_wait_timeout))
      return true;

    return common_system_variable_update_set(thd, var_type, variable_base,
                                             variable_name, nullptr,
                                             override_thd_lock_wait_timeout);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
