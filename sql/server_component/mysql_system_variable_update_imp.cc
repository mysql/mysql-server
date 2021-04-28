/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/service_implementation.h>
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
   - doesn't catch or print the error onto the error log but just
  passes it up
   - stores and restores the current_thd correctly
*/
class Storing_auto_THD {
  THD *m_previous_thd, *thd;

 public:
  Storing_auto_THD() {
    m_previous_thd = current_thd;
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
    if (m_previous_thd) m_previous_thd->store_globals();
  }
  THD *get_THD() { return thd; }
};

/**
   Implementation for the @ref
  mysql_service_mysql_system_variable_update_string_t service.

   Sets the value of a system variable to a new value specified in
   variable_value (or default if my_h_string is a NULL).
   Works only for system variables taking string values.
   May generate an SQL error that it stores into the current THD (if available)

   @param hthd thread session handle. if NULL a temp one will be created and
  then deleted.
   @param set_variable_type: one of [GLOBAL, PERSIST, PERSIST_ONLY].
     If NULL, then assumes GLOBAL.
   @param variable_name_base: a mysql string of the variable name prefix, NULL
  of none
   @param variable_name: a mysql string of the variable name
   @param variable_value: a mysql string value to pass to the variable.
   @retval FALSE: success
   @retval TRUE: failure, error set
  @sa @ref mysql_service_mysql_system_variable_update_string_t
*/
DEFINE_BOOL_METHOD(mysql_system_variable_update_string_imp::set,
                   (MYSQL_THD hthd, const char *set_variable_type,
                    my_h_string variable_name_base, my_h_string variable_name,
                    my_h_string variable_value)) {
  try {
    THD *thd;
    std::unique_ptr<Storing_auto_THD> athd = nullptr;
    enum_var_type vt = OPT_GLOBAL;

    if (set_variable_type) {
      if (!strcmp(set_variable_type, "GLOBAL"))
        vt = OPT_GLOBAL;
      else if (!strcmp(set_variable_type, "PERSIST"))
        vt = OPT_PERSIST;
      else if (!strcmp(set_variable_type, "PERSIST_ONLY"))
        vt = OPT_PERSIST_ONLY;
      else
        return true;
    }

    if (hthd)
      thd = static_cast<THD *>(hthd);
    else {
      athd.reset(new Storing_auto_THD());
      thd = athd->get_THD();
    }

    String *name_base = reinterpret_cast<String *>(variable_name_base);
    String *name = reinterpret_cast<String *>(variable_name);
    String *value = reinterpret_cast<String *>(variable_value);
    Item *val_val = new (thd->mem_root)
        Item_string(value->c_ptr_safe(), value->length(), value->charset());

    List<set_var_base> tmp_var_list;
    LEX *sav_lex = thd->lex, lex_tmp;
    thd->lex = &lex_tmp;
    lex_start(thd);

    sys_var *sysvar = nullptr;
    LEX_CSTRING base_name = {name_base ? name_base->c_ptr_safe() : nullptr,
                             name_base ? name_base->length() : 0};
    mysql_rwlock_wrlock(&LOCK_system_variables_hash);
    sysvar = intern_find_sys_var(name->c_ptr_safe(), name->length());
    if (!sysvar) {
      mysql_rwlock_unlock(&LOCK_system_variables_hash);
      my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), name->c_ptr_safe());
      thd->lex = sav_lex;
      return true;
    }

    tmp_var_list.push_back(new (thd->mem_root)
                               set_var(vt, sysvar, base_name, val_val));

    if (sql_set_variables(thd, &tmp_var_list, false)) {
      mysql_rwlock_unlock(&LOCK_system_variables_hash);
      thd->lex = sav_lex;
      return true;
    }
    thd->lex = sav_lex;
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return false;
}
