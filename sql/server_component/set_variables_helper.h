/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef SET_VARIABLES_HELPER_H
#define SET_VARIABLES_HELPER_H

#include <sql/set_var.h>   // set_var_base, enum_var_type
#include <sql/sql_lex.h>   // LEX
#include <sql/sql_list.h>  // List

class THD;
class Storing_auto_THD;

/**
  A helper class to fascilitate executing SET on system variables.

  Runs in the current @ref THD, but in a temp @ref LEX to avoid
  polluting the THD::lex with the changes.

  Useful for service implementations looking to set values
  to system variables.

  Should be used in a single THD.

  A proper usage is something of the sort:

  <code>
  THD *curr_thd = ...;
  Item *value1 = Item_int(20), *val2= Item_int(30);

  Set_variables_helper hlp(curr_thd);

  if (hlp.add_variable(nullptr, 0, "foo", 3, enum_var_type::PERSIST_ONLY,
  value1)) return true;

  if (hlp.add_variable("bar", 3, "baz", 3, enum_var_type::PERSIST, val2))
    return true;

  ...

  if (hlp->execute())
    return true;
  </code>

  The above executes the following command
  <code>
  SET PERSIST_ONLY foo = 20, PERSIST bar.baz = 30;
  </code>

  @sa @ref sql_set_variables
*/
class Set_variables_helper {
  List<set_var_base> m_sysvar_list;
  THD *m_thd;
  LEX *m_lex_save, m_lex_tmp;
  Storing_auto_THD *m_thd_auto;

 public:
  /**
    Initializes the helper and switches to the canned temp @ref LEX
    @param existing_thd the THD to execute on.
  */
  Set_variables_helper(THD *existing_thd);

  ~Set_variables_helper();

  /**
    Adds setting one variable instruction.

    Adds a:
    @code
      <var_type> [prefix.]suffix = variable_value
    @endcode
    to the SET command being constructed.

    @param prefix the prefix for the variable name or nullptr if none
    @param prefix_length length of prefix, 0 if empty
    @param suffix the name of the variable to set. Must be nonnull
    @param suffix_length the length of suffix
    @param variable_value the value to set
    @param var_type how to set: PERSIST, PERSIST_ONLY etc
    @retval true failure
    @retval false success
  */
  bool add_variable(const char *prefix, size_t prefix_length,
                    const char *suffix, size_t suffix_length,
                    Item *variable_value, enum_var_type var_type);

  /**
    Executes the SET command for all variables added.

    @retval true failure
    @retval false success

  */
  bool execute();

  /* gets the current thread to execute the SET in */
  THD *get_thd() { return m_thd; }

  /* returns true if it's running on a local session */
  bool is_auto_thd() { return m_thd_auto != nullptr; }

  /**
    @brief Checks the update type for the system variable and throws error if
    any found
    @param prefix the prefix for the variable name or nullptr if none
    @param prefix_length length of prefix, 0 if empty
    @param suffix the name of the variable to set. Must be nonnull
    @param suffix_length the length of suffix
    @param variable_value the value to set
    @retval true failure
    @retval false success
  */
  bool check_variable_update_type(const char *prefix, size_t prefix_length,
                                  const char *suffix, size_t suffix_length,
                                  Item *variable_value);
};

#endif /* SET_VARIABLES_HELPER_H */
