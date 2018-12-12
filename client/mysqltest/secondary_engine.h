#ifndef SECONDARY_ENGINE_INCLUDED
#define SECONDARY_ENGINE_INCLUDED

// Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <string>
#include <vector>

#include "mysql.h"

enum Statement_type {
  UNKNOWN_STMT = 0,
  CREATE_STMT,
  DDL_STMT,
  DML_STMT,
  TRUNCATE_STMT
};

typedef std::vector<unsigned int> Errors;

class Secondary_engine {
 public:
  Secondary_engine() {}
  Secondary_engine(const char *load_pool, const char *engine_name);

  ~Secondary_engine() {}

  /// Get the secondary engine execution count value.
  ///
  /// @param mysql mysql handle
  /// @param mode  Mode value (either "after" or "before")
  ///
  /// @retval True if the query fails, false otherwise.
  bool offload_count(MYSQL *mysql, const char *mode);

  /// Check if the statement is a CREATE TABLE statement or a DDL
  /// statement. If yes, run the ALTER TABLE statements needed to
  /// change the secondary engine and to load the data from primary
  /// engine to secondary engine.
  ///
  /// @param mysql           mysql handle
  /// @param expected_errors List of expected errors
  ///
  /// @retval True if load operation fails, false otherwise.
  bool run_load_statements(MYSQL *mysql, Errors expected_errors);

  /// Check if the statement is a DDL statement. If yes, run the ALTER
  /// TABLE statements needed to change the secondary engine to NULL
  /// and to unload the data from secondary engine.
  ///
  /// @param mysql           mysql handle
  /// @param expected_errors List of expected errors
  ///
  /// @retval True if unload operation fails, false otherwise.
  bool run_unload_statements(MYSQL *mysql, Errors expected_errors);

  /// Check if a statement is a CREATE TABLE statement or a DDL
  /// statement or a DML statement or a TRUNCATE TABLE statement. If
  /// match found, set the statement type flag accordingly.
  ///
  /// @param statement Original statement
  /// @param errors    Count of expected errors
  void match_statement(char *statement, std::size_t errors);

  /// Report secondary engine execution count value.
  ///
  /// @param filename File to store the count value
  void report_offload_count(const char *filename);

  /// Return statement type
  ///
  /// @retval Type of statement
  Statement_type statement_type() { return m_stmt_type; }

 private:
  /// Run ALTER TABLE table SECONDARY_ENGINE engine_name statement to
  /// change the secondary engine of a table. If the statement fails
  /// with an expected error, it will be ignored and the test run will
  /// continue.
  ///
  /// @param mysql           mysql handle
  /// @param engine_name     Secondary engine name
  /// @param expected_errors List of expected errors
  ///
  /// @retval True if execution of statement changing secondary engine
  ///         fails, false otherwise.
  bool run_secondary_engine_statement(MYSQL *mysql, std::string engine_name,
                                      Errors expected_errors);

  /// Run ALTER TABLE table_name SECONDARY_LOAD statement to load the
  /// table contents from primary engine to secondary engine. If the
  /// statement fails with an expected error, it will be ignored and the
  /// test run will continue.
  ///
  /// @param mysql           mysql handle
  /// @param expected_errors List of expected errors
  ///
  /// @retval True if execution of load statement fails, false otherwise.
  bool run_secondary_load_statement(MYSQL *mysql, Errors expected_errors);

  /// Run ALTER TABLE table_name SECONDARY_UNLOAD statement to unload
  /// the table contents from secondary engine. If the statement fails
  /// with an expected error, it will be ignored and the test run will
  /// continue.
  ///
  /// @param mysql           mysql handle
  /// @param expected_errors List of expected errors
  ///
  /// @retval True if execution of unload statement fails, false otherwise.
  bool run_secondary_unload_statement(MYSQL *mysql, Errors expected_errors);

  const char *m_engine_name;
  const char *m_load_pool;
  std::string m_table_name;
  Statement_type m_stmt_type;

  std::vector<int> m_secondary_engine_errors;
  std::vector<int> m_secondary_load_errors;
  std::vector<int> m_secondary_unload_errors;
};

#endif  // SECONDARY_ENGINE_INCLUDED
