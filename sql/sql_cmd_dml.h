/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_CMD_DML_INCLUDED
#define SQL_CMD_DML_INCLUDED

#include <assert.h>

#include "sql/sql_cmd.h"
#include "sql/sql_prepare.h"

struct LEX;
class Query_result;

class Sql_cmd_dml : public Sql_cmd {
 public:
  /// @return true if data change statement, false if not (SELECT statement)
  virtual bool is_data_change_stmt() const { return true; }

  /**
    Command-specific resolving (doesn't include LEX::prepare())

    @param thd  Current THD.

    @returns false on success, true on error
  */
  bool prepare(THD *thd) override;

  /**
    Execute a DML statement.

    @param thd       thread handler

    @returns false if success, true if error

    @details
      Processing a statement goes through 6 phases (parsing is already done)
       - Prelocking
       - Preparation
       - Locking of tables
       - Optimization
       - Execution or explain
       - Cleanup

      If the statement is already prepared, this step is skipped.

      The queries handled by this function are:

      SELECT
      INSERT ... SELECT
      INSERT ... VALUES
      REPLACE ... SELECT
      REPLACE ... VALUES
      UPDATE (single-table and multi-table)
      DELETE (single-table and multi-table)
      DO

    @todo make this function also handle SET.
   */
  bool execute(THD *thd) override;

  bool is_dml() const override { return true; }

  virtual bool may_use_cursor() const { return false; }

  bool is_single_table_plan() const override { return false; }

  /// @return the query result associated with a prepared query
  Query_result *query_result() const;

  /// Set query result object for this query statement
  void set_query_result(Query_result *result);

  /// Signal that root result object needs preparing in next execution
  void set_lazy_result() { m_lazy_result = true; }

 protected:
  Sql_cmd_dml()
      : Sql_cmd(),
        lex(nullptr),
        result(nullptr),
        m_empty_query(false),
        m_lazy_result(false) {}

  /// @return true if query is guaranteed to return no data
  /**
    @todo Also check this for the following cases:
          - Empty source for multi-table UPDATE and DELETE.
          - Check empty query expression for INSERT
  */
  bool is_empty_query() const {
    assert(is_prepared());
    return m_empty_query;
  }

  /// Set statement as returning no data
  void set_empty_query() { m_empty_query = true; }

  /**
    Perform a precheck of table privileges for the specific operation.

    @details
    Check that user has some relevant privileges for all tables involved in
    the statement, e.g. SELECT privileges for tables selected from, INSERT
    privileges for tables inserted into, etc. This function will also populate
    Table_ref::grant with all privileges the user has for each table,
    which is later used during checking of column privileges. Note that at
    preparation time, views are not expanded yet. Privilege checking is thus
    rudimentary and must be complemented with later calls to
    Query_block::check_view_privileges().
    The reason to call this function at such an early stage is to be able to
    quickly reject statements for which the user obviously has insufficient
    privileges.
    This function is called before preparing the statement.
    The function must also be complemented with proper privilege checks for all
    involved columns (e.g. check_column_grant_*).
    @see also the function comment of Query_block::prepare().
    During execution of a prepared statement, call check_privileges() instead.

    @param thd thread handler

    @returns false if success, true if false
  */
  virtual bool precheck(THD *thd) = 0;

  /**
    Check privileges on a prepared statement, called at start of execution
    of the statement.

    @details
    Check that user has all relevant privileges to the statement,
    ie. INSERT privilege for columns inserted into, UPDATE privilege
    for columns that are updated, DELETE privilege for tables that are
    deleted from, SELECT privilege for columns that are referenced, etc.

    @param thd thread handler

    @returns false if success, true if false
  */
  virtual bool check_privileges(THD *thd) = 0;

  /**
    Read and check privileges for all tables in a DML statement.

    @param thd thread handler

    @returns false if success, true if false

  */
  bool check_all_table_privileges(THD *thd);

  /**
    Perform the command-specific parts of DML command preparation,
    to be called from prepare()

    @param thd the current thread

    @returns false if success, true if error
  */
  virtual bool prepare_inner(THD *thd) = 0;

  /**
    The inner parts of query optimization and execution.
    Single-table DML operations needs to reimplement this.

    @param thd Thread handler

    @returns false on success, true on error
  */
  virtual bool execute_inner(THD *thd);

  /**
    Restore command properties before execution
    - Bind metadata for tables and fields
    - Restore clauses (e.g ORDER BY, GROUP BY) that were destroyed in
      last optimization.
  */
  virtual bool restore_cmd_properties(THD *thd);

  /// Save command properties, such as prepared query details and table props
  virtual bool save_cmd_properties(THD *thd);

  /**
    Helper function that checks if the command is eligible for secondary engine
    and if that's true returns the name of that eligible secondary storage
    engine.

    @return nullptr if not eligible or the name of the engine otherwise
  */
  const MYSQL_LEX_CSTRING *get_eligible_secondary_engine() const;

 protected:
  LEX *lex;              ///< Pointer to LEX for this statement
  Query_result *result;  ///< Pointer to object for handling of the result
  bool m_empty_query;    ///< True if query will produce no rows
  bool m_lazy_result;    ///< True: prepare query result on next execution
};

#endif /* SQL_CMD_DML_INCLUDED */
