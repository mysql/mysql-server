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
#include <sql/auto_thd.h>
#include <sql/current_thd.h>
#include <sql/mysqld.h>
#include <sql/set_var.h>
#include <sql/sql_class.h>
#include <sql/sql_lex.h>
#include <sql/sql_list.h>
#include <sql/sql_thd_internal_api.h>
#include <sql/sys_vars_shared.h>
#include <sql_string.h>
#include "sql/item_func.h"  // Item_func_set_user_var

/**
  A version of Auto_THD that:
   - doesn't catch or print the error onto the error log but just passes it up
   - stores and restores the current_thd correctly
*/
class Storing_auto_THD {
  THD *m_previous_thd, *thd;

 public:
  Storing_auto_THD() {
    m_previous_thd = current_thd;
    /* Allocate thread local memory if necessary. */
    if (!m_previous_thd) {
      my_thread_init();
    }
    thd = create_internal_thd();
  }

  ~Storing_auto_THD() {
    if (m_previous_thd) {
      Diagnostics_area *prev_da = m_previous_thd->get_stmt_da();
      Diagnostics_area *curr_da = thd->get_stmt_da();
      /* We need to put any errors in the DA as well as the condition list. */
      if (curr_da->is_error())
        prev_da->set_error_status(curr_da->mysql_errno(),
                                  curr_da->message_text(),
                                  curr_da->returned_sqlstate());

      prev_da->copy_sql_conditions_from_da(m_previous_thd, curr_da);
    }
    destroy_internal_thd(thd);
    if (!m_previous_thd) {
      my_thread_end();
    }
    if (m_previous_thd) {
      m_previous_thd->store_globals();
    }
  }
  THD *get_THD() { return thd; }
};

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
    if (!strcmp(type_name, "GLOBAL")) return OPT_GLOBAL;
    if (!strcmp(type_name, "SESSION"))
      return OPT_SESSION;
    else if (!strcmp(type_name, "PERSIST"))
      return OPT_PERSIST;
    else if (!strcmp(type_name, "PERSIST_ONLY"))
      return OPT_PERSIST_ONLY;
  }
  return OPT_DEFAULT;
}

/**
  Implementation for the
  @ref mysql_service_mysql_system_variable_update_string_t service.

  Sets the value of a system variable to a new value specified in variable_value
  (or default if my_h_string is a NULL).
  Works only for system variables taking unsigned string values.
  May generate an SQL error that it stores into the current THD (if available).

  @param hthd: THD session handle. If NULL, then a temporary THD will be created
               and then deleted.
  @param variable_type:  One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         If NULL, then assumes GLOBAL. SESSION is not supported
                         for a temporary THD.
  @param variable_base:  MySQL string of the variable prefix, NULL if none.
  @param variable_name:  MySQL string of the variable name.
  @param variable_value: MySQL string value to pass to the variable.
                         If NULL, then the system variable default is used.
  @retval FALSE: Success
  @retval TRUE:  Failure, error set
  @sa @ref mysql_service_mysql_system_variable_update_string_t
*/
DEFINE_BOOL_METHOD(mysql_system_variable_update_string_imp::set,
                   (MYSQL_THD hthd, const char *variable_type,
                    my_h_string variable_base, my_h_string variable_name,
                    my_h_string variable_value)) {
  bool result{false};
  constexpr ulong timeout{5};  // temporary lock wait timeout, in seconds

  try {
    THD *thd;
    std::unique_ptr<Storing_auto_THD> athd = nullptr;
    enum_var_type var_type = sysvar_type(variable_type);
    if (var_type == OPT_DEFAULT) var_type = OPT_GLOBAL;

    /* Use either the THD provided or create a temporary one */
    if (hthd)
      thd = static_cast<THD *>(hthd);
    else {
      /* A session variable update for a temporary THD has no effect
         and is not supported. */
      if (var_type == OPT_SESSION) {
        String *name = reinterpret_cast<String *>(variable_name);
        LogErr(ERROR_LEVEL, ER_TMP_SESSION_FOR_VAR, name->c_ptr_safe());
        return true;
      }
      athd.reset(new Storing_auto_THD());
      thd = athd->get_THD();
    }

    LEX *lex_save = thd->lex, lex_tmp;
    thd->lex = &lex_tmp;
    lex_start(thd);

    String *base = reinterpret_cast<String *>(variable_base);
    String *name = reinterpret_cast<String *>(variable_name);
    String *value = reinterpret_cast<String *>(variable_value);
    Item *value_str = new (thd->mem_root)
        Item_string(value->c_ptr_safe(), value->length(), value->charset());

    std::string_view prefix{base ? base->ptr() : nullptr,
                            base ? base->length() : 0};
    std::string_view suffix{name->ptr(), name->length()};

    /* Set a temporary lock wait timeout before updating the system variable.
       Some system variables, such as super-read-only, can be blocked by other
       locks during the update. Should that happen, we don't want to be holding
       LOCK_system_variables_hash.
    */
    Lock_wait_timeout lock_wait_timeout(thd, timeout);

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
                              set_var(var_type, var_tracker, value_str));
    /* Update the system variable */
    if (sql_set_variables(thd, &sysvar_list, false)) {
      result = true;
    }

    lex_end(thd->lex);
    thd->lex = lex_save;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return result;
}
