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

#include "client/mysqltest/secondary_engine.h"

#include <cstring>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

#include "client/mysqltest/error_names.h"
#include "my_dbug.h"

/// Run ALTER TABLE statement to change the secondary engine or, to
/// load or to unload the table contents.
///
/// If ALTER TABLE statement fails with any of the errors listed as
/// expected, then the error is ignored and test run will continue. If
/// the statement fails with any other error, an error is reported and
/// the test run is aborted.
///
/// @param alter_stmt ALTER TABLE statement
/// @param mysql      mysqld handle
/// @param errors     List of errors to be ignored
///
/// @retval False if statement execution is successful, true otherwise
static bool run_alter_statement(std::string alter_stmt, MYSQL *mysql,
                                std::vector<unsigned int> errors) {
  // Run the ALTER TABLE statement
  if (mysql_query(mysql, alter_stmt.c_str())) {
    // Statement failed, check if the error matches with any
    // of the expected errors.
    for (const unsigned int &error : errors)
      if (mysql_errno(mysql) == error) return false;
  }

  // Statement didn't match any of the expected error
  if (mysql_errno(mysql) != 0) {
    alter_stmt.pop_back();
    std::cerr << "mysqltest: Query '" << alter_stmt << "' failed, ERROR "
              << mysql_errno(mysql) << " (" << mysql_sqlstate(mysql)
              << "): " << mysql_error(mysql) << std::endl;
    return true;
  }

  return false;
}

/// Run ALTER TABLE table SECONDARY_ENGINE engine_name statement to
/// change the secondary engine of a table. If the statement fails
/// with an expected error, it will be ignored and the test run will
/// continue.
///
/// @param mysql           mysql handle
/// @param table_name      Table name string
/// @param engine_name     Secondary engine name
/// @param expected_errors List of expected errors
///
/// @retval True if execution of statement changing secondary engine
///         fails, false otherwise.
static bool run_secondary_engine_statement(
    MYSQL *mysql, std::string table_name, std::string engine_name,
    std::vector<unsigned int> expected_errors) {
  std::vector<unsigned int> ignore_errors;
  ignore_errors.insert(ignore_errors.end(), expected_errors.begin(),
                       expected_errors.end());

  ignore_errors.push_back(get_errcode_from_name("ER_BAD_LOG_STATEMENT"));
  ignore_errors.push_back(get_errcode_from_name("ER_CHECK_NOT_IMPLEMENTED"));
  ignore_errors.push_back(get_errcode_from_name("ER_DBACCESS_DENIED_ERROR"));
  ignore_errors.push_back(get_errcode_from_name("ER_SECONDARY_ENGINE"));
  ignore_errors.push_back(get_errcode_from_name("ER_TABLEACCESS_DENIED_ERROR"));

  // ALTER TABLE statement
  std::string alter_stmt =
      "ALTER TABLE " + table_name + " SECONDARY_ENGINE " + engine_name + ";";

  return run_alter_statement(alter_stmt, mysql, ignore_errors);
}

/// Run ALTER TABLE table_name SECONDARY_LOAD statement to load the
/// table contents from primary engine to secondary engine. If the
/// statement fails with an expected error, it will be ignored and the
/// test run will continue.
///
/// @param mysql      mysql handle
/// @param table_name Table name string
///
/// @retval True if execution of load statement fails, false otherwise.
static bool run_secondary_load_statement(MYSQL *mysql, std::string table_name) {
  std::vector<unsigned int> ignore_errors;
  ignore_errors.push_back(get_errcode_from_name("ER_CHECK_NOT_IMPLEMENTED"));
  ignore_errors.push_back(get_errcode_from_name("ER_RAPID_PLUGIN"));

  std::stringstream table_names(table_name);
  std::string table;

  // DML Statements like DELETE or UPDATE may contain multiple table
  // names separated by comma.
  while (std::getline(table_names, table, ',')) {
    // ALTER TABLE statement to load the data to secondary engine.
    std::string alter_stmt = "ALTER TABLE " + table + " SECONDARY_LOAD;";
    if (run_alter_statement(alter_stmt, mysql, ignore_errors)) return true;
  }

  return false;
}

/// Run ALTER TABLE table_name SECONDARY_UNLOAD statement to unload
/// the table contents from secondary engine. If the statement fails
/// with an expected error, it will be ignored and the test run will
/// continue.
///
/// @param mysql           mysql handle
/// @param table_name      Table name string
/// @param expected_errors List of expected errors
///
/// @retval True if execution of unload statement fails, false otherwise.
static bool run_secondary_unload_statement(
    MYSQL *mysql, std::string table_name,
    std::vector<unsigned int> expected_errors) {
  std::vector<unsigned int> ignore_errors;
  ignore_errors.insert(ignore_errors.end(), expected_errors.begin(),
                       expected_errors.end());

  ignore_errors.push_back(get_errcode_from_name("ER_BAD_LOG_STATEMENT"));
  ignore_errors.push_back(get_errcode_from_name("ER_DBACCESS_DENIED_ERROR"));
  ignore_errors.push_back(get_errcode_from_name("ER_SECONDARY_ENGINE"));

  std::stringstream table_names(table_name);
  std::string table;

  // DML Statements like DELETE or UPDATE may contain multiple table
  // names separated by comma.
  while (std::getline(table_names, table, ',')) {
    // ALTER TABLE statement to unload the data from secondary engine.
    std::string alter_stmt = "ALTER TABLE " + table + " SECONDARY_UNLOAD;";
    if (run_alter_statement(alter_stmt, mysql, ignore_errors)) return true;
  }

  return false;
}

/// Match a statement string against the pattern. If match found,
/// extract the table name from the it.
///
/// @param statement Statement string
/// @param pattern   Pattern string
/// @param index     Index value in sub-match array containing table name
///
/// @retval Table name if found, empty string otherwise.
static std::string regex_match_statement(std::string statement,
                                         std::string pattern,
                                         std::uint16_t index) {
  std::smatch match;
  std::string table_name("");

  // Construct regex pattern object.
  std::regex regex_pattern(pattern, std::regex_constants::icase);

  // Pattern string doesn't work if the statement contains newline
  // character, replace it with a space character.
  std::replace(statement.begin(), statement.end(), '\n', ' ');

  // Check if the statement matches the pattern string
  if (std::regex_match(statement, match, regex_pattern)) {
    // Match found, extract the table name
    table_name = match[index].str();
    DBUG_ASSERT(table_name.length());
    return table_name;
  }

  // Query didn't match the pattern, return an empty string.
  DBUG_ASSERT(table_name.length() == 0);
  return table_name;
}

/// Check if a statement is a CREATE TABLE statement. If yes, parse
/// the statement to extract the table name and store it in a string
/// object.
///
/// @param statement  Original statement
/// @param table_name String object to store the table name
///
/// @retval True if the match or table name found, false otherwise.
static bool match_create_statement(std::string statement,
                                   std::string *table_name) {
  std::string pattern_str =
      "^\\s*create\\s+table\\s+((\\/\\*\\+.*\\*\\/\\s+)?)"
      "((if\\s+not\\s+exists\\s+)?)((`.*?`)|(\".*?\")|(.*?))(\\s*\\(|\\s+).*";

  *table_name = regex_match_statement(statement, pattern_str, 5);
  if (table_name->length()) return true;
  return false;
}

/// Check if a statement is a DDL statement. If yes, parse the
/// statement to extract the table name and store it in a string
/// object.
///
/// @param statement  Original statement
/// @param table_name String object to store the table name
///
/// @retval True if match or table name found, false otherwise.
static bool match_ddl_statement(std::string statement,
                                std::string *table_name) {
  std::map<std::string, std::uint16_t> ddl_pattern_strings = {
      {"^\\s*alter\\s+((\\/\\*\\+.*\\*\\/\\s+)?)table\\s+"
       "((\\/\\*\\+.*\\*\\/\\s+)?)(.*?)\\s+.*",
       5},
      {"^\\s*create\\s+((unique\\s+|fulltext\\s+|spatial\\s+)?)index.*on\\s+"
       "(.*?)(\\s*\\(|\\s+).*",
       3},
      {"^\\s*drop\\s+index\\s+.*\\s+on\\s+(.*?)((\\s+.*)?)", 1}};

  for (std::pair<std::string, std::uint16_t> pattern : ddl_pattern_strings) {
    *table_name =
        regex_match_statement(statement, pattern.first, pattern.second);
    if (table_name->length()) return true;
  }

  return false;
}

/// Check if a statement is a DML statement. If yes, parse the statement
/// to extract the table name and store it in a string object.
///
/// @param statement  Original statement
/// @param table_name String object to store the table name
///
/// @retval True if match or table name found, false otherwise.
static bool match_dml_statement(std::string statement,
                                std::string *table_name) {
  std::map<std::string, std::uint16_t> dml_pattern_strings = {
      {"^\\s*delete\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((LOW_PRIORITY\\s+)?)((QUICK\\s+)?)((IGNORE\\s+)?)"
       "FROM\\s+(.*?)((\\s+.*)?)",
       9},
      {"^\\s*insert\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((LOW_PRIORITY\\s+|DELAYED\\s+|HIGH_PRIORITY\\s+)?)"
       "((IGNORE\\s+)?)((INTO\\s+)?)(.*?)(\\s*\\(|\\s+).*",
       9},
      {"^\\s*replace\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((LOW_PRIORITY\\s+|DELAYED\\s+)?)((INTO\\s+)?)(.*?)"
       "(\\s*\\(|\\s+).*",
       7},
      {"^\\s*update\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((LOW_PRIORITY\\s+)?)((IGNORE\\s+)?)(.*?)\\s+set.*",
       7}};

  for (std::pair<std::string, std::uint16_t> pattern : dml_pattern_strings) {
    *table_name =
        regex_match_statement(statement, pattern.first, pattern.second);
    if (table_name->length()) return true;
  }

  return false;
}

/// Check if a statement is a TRUNCATE statement. If yes, parse the
/// statement to extract the table name and store it in a string object.
///
/// @param statement  Original statement
/// @param table_name String object to store the table name
///
/// @retval True if the match or table name found, false otherwise.
static bool match_truncate_statement(std::string statement,
                                     std::string *table_name) {
  std::string pattern_str = "^\\s*truncate\\s+(table\\s+)?(.*?)(\\s+.*|\\s*)";
  *table_name = regex_match_statement(statement, pattern_str, 2);
  if (table_name->length()) return true;
  return false;
}

/// Check if the statement is a CREATE TABLE statement or a DDL
/// statement. If yes, run the ALTER TABLE statements needed to change
/// the secondary engine and to load the data from primary engine to
/// secondary engine.
///
/// @param secondary_engine       Secondary engine name
/// @param statement              Original statement
/// @param mysql                  mysql handle
/// @param expected_errors        List of expected errors
/// @param opt_change_propagation Boolean flag indicating whether change
///                               propagation is enabled or not.
///
/// @retval True if load operation fails, false otherwise.
bool run_secondary_engine_load_statements(
    const char *secondary_engine, char *statement, MYSQL *mysql,
    std::vector<unsigned int> expected_errors, bool opt_change_propagation) {
  std::string table_name("");

  // Check if the statement is a CREATE TABLE statement or a DDL statement
  if ((expected_errors.size() == 0 &&
       match_create_statement(statement, &table_name)) ||
      match_ddl_statement(statement, &table_name)) {
    DBUG_ASSERT(table_name.length());

    // Change secondary engine.
    if (run_secondary_engine_statement(mysql, table_name, secondary_engine,
                                       expected_errors))
      return true;

    // Skip running ALTER TABLE statement to load the data from primary
    // engine to secondary engine if the previous ALTER TABLE statement
    // failed with an error.
    if (mysql_errno(mysql) == 0 ||
        std::strstr(mysql_error(mysql),
                    "Table already has a secondary engine defined")) {
      // Load the data from primary engine to secondary engine.
      if (run_secondary_load_statement(mysql, table_name)) return true;
    }
  } else if (expected_errors.size() == 0) {
    if (match_truncate_statement(statement, &table_name) ||
        (!opt_change_propagation &&
         match_dml_statement(statement, &table_name))) {
      DBUG_ASSERT(table_name.length());

      // Unload the data from secondary engine.
      if (run_secondary_unload_statement(mysql, table_name, expected_errors))
        return true;

      // Skip running ALTER TABLE statement to load the data from primary
      // engine to secondary engine if the previous ALTER TABLE statement
      // failed with an error.
      if (mysql_errno(mysql) == 0)
        // Load the data from primary engine to secondary engine.
        if (run_secondary_load_statement(mysql, table_name)) return true;
    }
  }

  return false;
}

/// Check if the statement is a DDL statement. If yes, run the ALTER
/// TABLE statements needed to change the secondary engine to NULL and
/// to unload the data from secondary engine.
///
/// @param statement       Original statement
/// @param mysql           mysql handle
/// @param expected_errors List of expected errors
///
/// @retval True if unload operation fails, false otherwise.
bool run_secondary_engine_unload_statements(
    char *statement, MYSQL *mysql, std::vector<unsigned int> expected_errors) {
  std::string table_name("");

  // Check if the statement is a DDL statement.
  if (match_ddl_statement(statement, &table_name)) {
    DBUG_ASSERT(table_name.length());

    // Unload the data from secondary engine.
    if (run_secondary_unload_statement(mysql, table_name, expected_errors))
      return true;

    // Skip running ALTER TABLE statement to change the secondary engine
    // to NULL if the previous ALTER TABLE statement to unload the data
    // from secondary engine failed with an error.
    if (mysql_errno(mysql) == 0) {
      // Change the secondary engine to 'NULL' after unload.
      if (run_secondary_engine_statement(mysql, table_name, "NULL",
                                         expected_errors))
        return true;
    }
  }

  return false;
}
