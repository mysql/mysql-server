/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_CHECK_CONSTRAINT_INCLUDED
#define SQL_CHECK_CONSTRAINT_INCLUDED

#include <map>

#include "lex_string.h"          // LEX_STRING
#include "mem_root_array.h"      // Mem_root_array
#include "sql/dd/string_type.h"  // dd::String_type

class Item;
class String;
struct TABLE;
class THD;
class Value_generator;

/**
  Class to represent the check constraint specifications obtained from the SQL
  statement parse.
*/
class Sql_check_constraint_spec {
 public:
  /*
    Validate check constraint name, perform per item-type to check if the
    expression is allowed for the check constraint. Check expression is
    pre-validated at this stage. Validation of specific functions in expression
    is done later in the method open_table_from_share.

    @retval  false  Success.
    @retval  true   Failure.
  */
  bool pre_validate();

  /*
    Write check constraint expression into a String with proper syntax.

    @param[in]   THD   Thread handle.
    @param[out]  out   Check constraint expression.
  */
  void print_expr(THD *thd, String &out);

  /*
    Method to check if column "column_name" referred in the check constraint
    expression.

    @param[in]  column_name   Column name.

    @retval     true       If column name is referenced in the check expression.
    @retval     false      Otherwise.
  */
  bool expr_refers_column(const char *column_name);

  /*
    Method to check if constraint expresssion refers to only "column_name"
    column of the table.

    @param[in]  column_name   Column name.

    @retval     true       If expression refers to only "column_name".
    @retval     false      If expression refers to more than one column
                           or if expression does not refers to "column_name".
  */
  bool expr_refers_to_only_column(const char *column_name);

 public:
  /// Name of the check constraint.
  LEX_STRING name{nullptr, 0};

  /// Check constraint expression.
  Item *check_expr{nullptr};

  /// Name of the column if check clause is defined at the column level.
  LEX_STRING column_name{nullptr, 0};

  /// Check constraint state (enforced/not enforced)
  bool is_enforced{true};
};

/**
  Class to represent check constraint in the TABLE_SHARE.

  The instance of Sql_check_constraint_share contains information as name,
  state and expression in string form. These informations are filled from
  the data-dictionary. The check expression is not in itemized (materialized)
  form here.
*/
class Sql_check_constraint_share {
 public:
  // Default check constraint.
  Sql_check_constraint_share() = default;

  Sql_check_constraint_share(const LEX_CSTRING &name,
                             const LEX_CSTRING &expr_str, bool is_enforced)
      : m_name(name), m_expr_str(expr_str), m_is_enforced(is_enforced) {}

  ~Sql_check_constraint_share() {
    if (m_name.str != nullptr) delete m_name.str;
    if (m_expr_str.str != nullptr) delete m_expr_str.str;
  }

  // Constraint name.
  LEX_CSTRING &name() { return m_name; }
  // Check expression in string form.
  LEX_CSTRING &expr_str() { return m_expr_str; }
  // Check constraint state (enforced / not enforced)
  bool is_enforced() { return m_is_enforced; }

 private:
  // Check constraint name.
  LEX_CSTRING m_name{nullptr, 0};

  // Check constraint expression.
  LEX_CSTRING m_expr_str{nullptr, 0};

  // Check constraint state.
  bool m_is_enforced{true};

 private:
  // Delete default copy and assignment operator to avoid accidental destruction
  // of shallow copied Sql_table_check_constraint_share objects.
  Sql_check_constraint_share(const Sql_check_constraint_share &) = delete;
  Sql_check_constraint_share &operator=(const Sql_check_constraint_share &) =
      delete;
};

/**
  Class to represent check constraint in the TABLE instance.

  The Sql_table_check_constraint is a Sql_check_constraint_share with reference
  to the parent TABLE instance and itemized (materialized) form of check
  constraint expression.
  Sql_table_check_constraint is prepared from the Sql_check_constraint_share of
  TABLE_SHARE instance.
*/
class Sql_table_check_constraint : public Sql_check_constraint_share {
 public:
  Sql_table_check_constraint() = default;

  Sql_table_check_constraint(const LEX_CSTRING &name,
                             const LEX_CSTRING &expr_str, bool is_enforced,
                             Value_generator *val_gen, TABLE *table)
      : Sql_check_constraint_share(name, expr_str, is_enforced),
        m_val_gen(val_gen),
        m_table(table) {}

  ~Sql_table_check_constraint();

  // Value generator.
  Value_generator *value_generator() { return m_val_gen; }
  void set_value_generator(Value_generator *val_gen) { m_val_gen = val_gen; }

  // Reference to owner table.
  TABLE *table() const { return m_table; }

 private:
  // Value generator for the check constraint expression.
  Value_generator *m_val_gen{nullptr};

  // Parent table reference.
  TABLE *m_table{nullptr};

 private:
  // Delete default copy and assignment operator to avoid accidental destruction
  // of shallow copied Sql_table_check_constraint objects.
  Sql_table_check_constraint(const Sql_table_check_constraint &) = delete;
  Sql_table_check_constraint &operator=(const Sql_table_check_constraint &) =
      delete;
};

// Type for the list of Sql_check_constraint_spec elements.
using Sql_check_constraint_spec_list =
    Mem_root_array<Sql_check_constraint_spec *>;

// Type for the list of Sql_check_constraint_share elements.
using Sql_check_constraint_share_list =
    Mem_root_array<Sql_check_constraint_share *>;

// Type for the list of Sql_table_check_constraint elements.
using Sql_table_check_constraint_list =
    Mem_root_array<Sql_table_check_constraint *>;

/**
  Class to represent the adjusted check constraint names to actual check
  constraint names during ALTER TABLE operation.

  During ALTER TABLE operation the check constraint names of a table is
  adjusted to avoid check constraint name conflicts and restored after older
  table version is either dropped or when new version is renamed to table
  name. Instance of this class holds the mapping between adjusted name to
  actual check constraint names. Actual names are required to restore and also
  to report errors with correct check constraint name. Class is a wrapper over
  a std::map<dd::String_type, const char*> class.
*/
class Check_constraints_adjusted_names_map final {
 public:
  Check_constraints_adjusted_names_map() = default;
  /**
    Method to insert adjusted name and actual name to map.

    @param   adjusted_name     Adjusted name of check constraint.
    @param   actual_name       Actual name of check constraint.
  */
  void insert(const char *adjusted_name, const char *actual_name) {
    m_names_map.insert({adjusted_name, actual_name});
  }
  /**
    Method to get actual check constraint name from the adjusted name.

    @param   adjusted_name     Adjusted name of check constraint.

    @retval  Actual name of check constraint.
  */
  const char *actual_name(const char *adjusted_name) {
    auto name = m_names_map.find(adjusted_name);
    DBUG_ASSERT(name != m_names_map.end());
    return name->second;
  }
  /**
    Method to check if map is empty.

    @retval   true  If map is empty.
    @retval   false Otherwise.
  */
  bool empty() { return m_names_map.empty(); }
  /**
    Method to clear map.
  */
  void clear() { m_names_map.clear(); }

 private:
  std::map<dd::String_type, const char *> m_names_map;
};
#endif  // SQL_CHECK_CONSTRAINT_INCLUDED
