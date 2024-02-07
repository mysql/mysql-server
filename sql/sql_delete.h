/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_DELETE_INCLUDED
#define SQL_DELETE_INCLUDED

#include "my_sqlcommand.h"
#include "my_table_map.h"
#include "sql/sql_cmd_dml.h"  // Sql_cmd_dml

class JOIN;
class Select_lex_visitor;
class THD;
class Table_ref;
template <typename T>
class SQL_I_List;

class Sql_cmd_delete final : public Sql_cmd_dml {
 public:
  Sql_cmd_delete(bool multitable_arg, SQL_I_List<Table_ref> *delete_tables_arg)
      : multitable(multitable_arg), delete_tables(delete_tables_arg) {}

  enum_sql_command sql_command_code() const override {
    return multitable ? SQLCOM_DELETE_MULTI : SQLCOM_DELETE;
  }

  bool is_single_table_plan() const override { return !multitable; }

  bool accept(THD *thd, Select_lex_visitor *visitor) override;

 protected:
  bool precheck(THD *thd) override;
  bool check_privileges(THD *thd) override;

  bool prepare_inner(THD *thd) override;

  bool execute_inner(THD *thd) override;

 private:
  bool delete_from_single_table(THD *thd);

  bool multitable;
  /**
    References to tables that are deleted from in a multitable delete statement.
    Only used to track such tables from the parser. In preparation and
    optimization, use the Table_ref::updating property instead.
  */
  SQL_I_List<Table_ref> *delete_tables;
};

/// Find out which of the delete target tables can be deleted from immediately
/// while scanning. This is used by the old optimizer *after* the plan has been
/// created. The hypergraph optimizer does not use this function, as it makes
/// the decision about immediate delete *during* planning, not after planning.
table_map GetImmediateDeleteTables(const JOIN *join, table_map delete_tables);

/// Checks if the sql_safe_updates option is enabled, and raises an error and
/// returns true if the statement is likely to delete or update a large number
/// of rows. Specifically, it raises an error if there is a full table scan or
/// full index scan of one of the tables deleted from, and there is no LIMIT
/// clause.
bool CheckSqlSafeUpdate(THD *thd, const JOIN *join);

#endif /* SQL_DELETE_INCLUDED */
