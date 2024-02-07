/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include <sql/server_component/set_variables_helper.h>
#include <sql/server_component/storing_auto_thd.h>
#include <sql/sql_class.h>

Set_variables_helper::Set_variables_helper(THD *thd) {
  if (thd) {
    m_thd_auto = nullptr;
    m_thd = thd;
  } else {
    m_thd_auto = new Storing_auto_THD();
    m_thd = m_thd_auto->get_THD();
  }
  m_lex_save = m_thd->lex;
  m_thd->lex = &m_lex_tmp;
  lex_start(m_thd);
}

Set_variables_helper::~Set_variables_helper() {
  lex_end(m_thd->lex);
  m_thd->lex = m_lex_save;
  if (m_thd_auto) {
    delete m_thd_auto;
    m_thd = nullptr;
  }
}

bool Set_variables_helper::add_variable(
    const char *prefix, size_t prefix_length, const char *suffix,
    size_t suffix_length, Item *variable_value, enum_var_type var_type) {
  const std::string_view prefix_v{prefix, prefix_length};
  const std::string_view suffix_v{suffix, suffix_length};

  const System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(prefix_v, suffix_v);
  if (var_tracker.access_system_variable(m_thd)) return true;

  return m_sysvar_list.push_back(
      new (m_thd->mem_root) set_var(var_type, var_tracker, variable_value));
}

bool Set_variables_helper::execute() {
  return m_sysvar_list.elements > 0 &&
         sql_set_variables(m_thd, &m_sysvar_list, false);
}

bool Set_variables_helper::check_variable_update_type(const char *prefix,
                                                      size_t prefix_length,
                                                      const char *suffix,
                                                      size_t suffix_length,
                                                      Item *variable_value) {
  const std::string_view prefix_v{prefix, prefix_length};
  const std::string_view suffix_v{suffix, suffix_length};

  const System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(prefix_v, suffix_v);

  auto f = [variable_value](const System_variable_tracker &,
                            sys_var *var) -> int {
    if (var->check_update_type(variable_value->result_type())) {
      my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name.str);
      return -1;
    }
    return 0;
  };

  int ret =
      var_tracker
          .access_system_variable<int>(m_thd, f, Suppress_not_found_error::NO)
          .value_or(-1);

  return ret == -1;
}
