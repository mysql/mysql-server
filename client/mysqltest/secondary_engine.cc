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
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "client/mysqltest/error_names.h"
#include "client/mysqltest/utils.h"
#include "my_dbug.h"

static int offload_count_after = 0;
static int offload_count_before = 0;

/// Constructor function for 'Secondary_engine' class
///
/// @param load_pool   Load pool value for secondary engine.
/// @param engine_name Secondary engine name
Secondary_engine::Secondary_engine(const char *load_pool,
                                   const char *engine_name)
    : m_engine_name(engine_name),
      m_load_pool(load_pool),
      m_table_name(""),
      m_stmt_type(UNKNOWN_STMT),
      m_secondary_engine_errors{
          get_errcode_from_name("ER_BAD_LOG_STATEMENT"),
          get_errcode_from_name("ER_CHECK_NOT_IMPLEMENTED"),
          get_errcode_from_name("ER_DBACCESS_DENIED_ERROR"),
          get_errcode_from_name("ER_SECONDARY_ENGINE"),
          get_errcode_from_name("ER_TABLE_NOT_LOCKED_FOR_WRITE"),
          get_errcode_from_name("ER_WRONG_OBJECT"),
          get_errcode_from_name("ER_TABLEACCESS_DENIED_ERROR")},
      m_secondary_load_errors{
          get_errcode_from_name("ER_CHECK_NOT_IMPLEMENTED"),
          get_errcode_from_name("ER_SECONDARY_ENGINE_PLUGIN"),
          get_errcode_from_name("ER_TABLEACCESS_DENIED_ERROR"),
          get_errcode_from_name("ER_TABLESPACE_DISCARDED"),
          get_errcode_from_name("ER_WRONG_OBJECT")},
      m_secondary_unload_errors{
          get_errcode_from_name("ER_BAD_LOG_STATEMENT"),
          get_errcode_from_name("ER_DBACCESS_DENIED_ERROR"),
          get_errcode_from_name("ER_SECONDARY_ENGINE"),
          get_errcode_from_name("ER_TABLE_NOT_LOCKED_FOR_WRITE"),
          get_errcode_from_name("ER_TABLEACCESS_DENIED_ERROR"),
          get_errcode_from_name("ER_WRONG_OBJECT")} {}

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
                                         std::uint8_t index) {
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
  std::unordered_map<std::string, std::uint8_t> ddl_pattern_strings = {
      {"^\\s*alter\\s+((\\/\\*\\+.*\\*\\/\\s+)?)table\\s+"
       "((\\/\\*\\+.*\\*\\/\\s+)?)(.*?)\\s+.*",
       5},
      {"^\\s*create\\s+((unique\\s+|fulltext\\s+|spatial\\s+)?)index.*on\\s+"
       "(.*?)(\\s*\\(|\\s+).*",
       3},
      {"^\\s*drop\\s+index\\s+.*\\s+on\\s+(.*?)((\\s+.*)?)", 1}};

  for (std::pair<std::string, std::uint8_t> pattern : ddl_pattern_strings) {
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
  std::unordered_map<std::string, std::uint8_t> dml_pattern_strings = {
      {"^\\s*delete\\s+((\\/\\*\\+.*\\*\\/\\s+)?)"
       "((low_priority\\s+)?)((quick\\s+)?)((ignore\\s+)?)"
       "from\\s+(.*)using\\s+(.*?)((\\s+.*)?)",
       10},
      {"^\\s*delete\\s+((\\/\\*\\+.*\\*\\/\\s+)?)((.*)?)"
       "((low_priority\\s+)?)((quick\\s+)?)((ignore\\s+)?)"
       "from\\s+(.*?)((\\s+.*)?)",
       11},
      {"^\\s*insert\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((low_priority\\s+|delayed\\s+|high_priority\\s+)?)"
       "((ignore\\s+)?)((into\\s+)?)(.*?)(\\s*\\(|\\s+).*",
       9},
      {"^\\s*replace\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((low_priority\\s+|delayed\\s+)?)((into\\s+)?)(.*?)"
       "(\\s*\\(|\\s+).*",
       7},
      {"^\\s*update\\s+((\\/\\*\\+.*\\*\\/"
       "\\s+)?)((low_priority\\s+)?)((ignore\\s+)?)(.*?)\\s+set.*",
       7}};

  for (std::pair<std::string, std::uint8_t> pattern : dml_pattern_strings) {
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
                                Errors errors) {
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

/// Table name extracted from the statement might contain leading
/// white space characters and/or alias names. Remove the unnecessary
/// characters from it and return only the table name string.
///
/// @param table Table name
///
/// @retval table Table name string
static std::string get_table_name(std::string table) {
  table.erase(0, table.find_first_not_of(' '));
  table = table.substr(0, table.find(' '));
  return table;
}

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
bool Secondary_engine::run_secondary_engine_statement(MYSQL *mysql,
                                                      std::string engine_name,
                                                      Errors expected_errors) {
  // List of errors to be ignored
  Errors ignore_errors;
  ignore_errors.insert(ignore_errors.end(), expected_errors.begin(),
                       expected_errors.end());
  ignore_errors.insert(ignore_errors.end(), m_secondary_engine_errors.begin(),
                       m_secondary_engine_errors.end());

  // ALTER TABLE statement
  std::string alter_stmt =
      "ALTER TABLE " + m_table_name + " SECONDARY_ENGINE " + engine_name + ";";

  return run_alter_statement(alter_stmt, mysql, ignore_errors);
}

/// Run ALTER TABLE table_name SECONDARY_LOAD statement to load the
/// table contents from primary engine to secondary engine. If the
/// statement fails with an expected error, it will be ignored and the
/// test run will continue.
///
/// @param mysql           mysql handle
/// @param expected_errors List of expected errors
///
/// @retval True if execution of load statement fails, false otherwise.
bool Secondary_engine::run_secondary_load_statement(MYSQL *mysql,
                                                    Errors expected_errors) {
  // List of errors to be ignored
  Errors ignore_errors;
  ignore_errors.insert(ignore_errors.end(), expected_errors.begin(),
                       expected_errors.end());
  ignore_errors.insert(ignore_errors.end(), m_secondary_load_errors.begin(),
                       m_secondary_load_errors.end());

  std::string table;
  std::stringstream table_names(m_table_name);

  // DML Statements like DELETE or UPDATE may contain multiple table
  // names separated by comma.
  while (std::getline(table_names, table, ',')) {
    table = get_table_name(table);
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
/// @param expected_errors List of expected errors
///
/// @retval True if execution of unload statement fails, false otherwise.
bool Secondary_engine::run_secondary_unload_statement(MYSQL *mysql,
                                                      Errors expected_errors) {
  // List of errors to be ignored
  Errors ignore_errors;
  ignore_errors.insert(ignore_errors.end(), expected_errors.begin(),
                       expected_errors.end());
  ignore_errors.insert(ignore_errors.end(), m_secondary_unload_errors.begin(),
                       m_secondary_unload_errors.end());

  std::string table;
  std::stringstream table_names(m_table_name);

  // DML Statements like DELETE or UPDATE may contain multiple table
  // names separated by comma.
  while (std::getline(table_names, table, ',')) {
    table = get_table_name(table);
    // ALTER TABLE statement to unload the data from secondary engine.
    std::string alter_stmt = "ALTER TABLE " + table + " SECONDARY_UNLOAD;";
    if (run_alter_statement(alter_stmt, mysql, ignore_errors)) return true;
  }

  return false;
}

/// Check if a statement is a CREATE TABLE statement or a DDL
/// statement or a DML statement or a TRUNCATE TABLE statement. If
/// match found, set the statement type flag accordingly.
///
/// @param statement Original statement
/// @param errors    Count of expected errors
void Secondary_engine::match_statement(char *statement, std::size_t errors) {
  if (m_stmt_type != UNKNOWN_STMT) m_stmt_type = UNKNOWN_STMT;

  // Check the statement type
  if (match_ddl_statement(statement, &m_table_name)) {
    // DDL statement
    m_stmt_type = DDL_STMT;
  } else if (errors == 0) {
    if (match_create_statement(statement, &m_table_name)) {
      // CREATE TABLE statement
      m_stmt_type = CREATE_STMT;
    } else if (!std::strcmp(m_load_pool, "SNAPSHOT") &&
               match_dml_statement(statement, &m_table_name)) {
      // DML statement
      m_stmt_type = DML_STMT;
    } else if (match_truncate_statement(statement, &m_table_name)) {
      // TRUNCATE TABLE statement
      m_stmt_type = TRUNCATE_STMT;
    }
  }
}

/// Check if the statement is a CREATE TABLE statement or a DDL
/// statement. If yes, run the ALTER TABLE statements needed to change
/// the secondary engine and to load the data from primary engine to
/// secondary engine.
///
/// @param mysql           mysql handle
/// @param expected_errors List of expected errors
///
/// @retval True if load operation fails, false otherwise.
bool Secondary_engine::run_load_statements(MYSQL *mysql,
                                           Errors expected_errors) {
  // Check if the statement is a CREATE TABLE statement or a DDL statement
  if (m_stmt_type == CREATE_STMT || m_stmt_type == DDL_STMT) {
    DBUG_ASSERT(m_table_name.length());
    // Change secondary engine.
    if (run_secondary_engine_statement(mysql, m_engine_name, expected_errors))
      return true;

    // Skip running ALTER TABLE statement to load the data from primary
    // engine to secondary engine if the previous ALTER TABLE statement
    // failed with an error.
    if (mysql_errno(mysql) == 0 ||
        std::strstr(mysql_error(mysql),
                    "Table already has a secondary engine defined")) {
      // Load the data from primary engine to secondary engine.
      if (run_secondary_load_statement(mysql, expected_errors)) return true;
    }
  } else if (m_stmt_type == DML_STMT || m_stmt_type == TRUNCATE_STMT) {
    DBUG_ASSERT(m_table_name.length());
    // Unload the data from secondary engine.
    if (run_secondary_unload_statement(mysql, expected_errors)) return true;

    // Skip running ALTER TABLE statement to load the data from primary
    // engine to secondary engine if the previous ALTER TABLE statement
    // failed with an error.
    if (mysql_errno(mysql) == 0)
      // Load the data from primary engine to secondary engine.
      if (run_secondary_load_statement(mysql, expected_errors)) return true;
  }

  return false;
}

/// Check if the statement is a DDL statement. If yes, run the ALTER
/// TABLE statements needed to change the secondary engine to NULL and
/// to unload the data from secondary engine.
///
/// @param mysql           mysql handle
/// @param expected_errors List of expected errors
///
/// @retval True if unload operation fails, false otherwise.
bool Secondary_engine::run_unload_statements(MYSQL *mysql,
                                             Errors expected_errors) {
  // Check if the statement is a DDL statement.
  if (m_stmt_type == DDL_STMT) {
    DBUG_ASSERT(m_table_name.length());
    // Unload the data from secondary engine.
    if (run_secondary_unload_statement(mysql, expected_errors)) return true;

    // Skip running ALTER TABLE statement to change the secondary engine
    // to NULL if the previous ALTER TABLE statement to unload the data
    // from secondary engine failed with an error.
    if (mysql_errno(mysql) == 0) {
      // Change the secondary engine to 'NULL' after unload.
      if (run_secondary_engine_statement(mysql, "NULL", expected_errors))
        return true;
    }
  }

  return false;
}

/// Get secondary engine execution count value.
//
/// @param mysql mysql handle
/// @param mode  Mode value (either "after" or "before")
///
/// @retval True if the query fails, false otherwise.
bool Secondary_engine::offload_count(MYSQL *mysql, const char *mode) {
  std::string offload_count;

  const char *query =
      "SHOW GLOBAL STATUS LIKE 'Secondary_engine_execution_count'";

  if (query_get_string(mysql, query, 1, &offload_count)) {
    int error = mysql_errno(mysql);
    if (error == 0 || error == 1104 || error == 2006) return false;
    std::cerr << "mysqltest: Query '" << query << "' failed, ERROR " << error
              << " (" << mysql_sqlstate(mysql) << "): " << mysql_error(mysql)
              << std::endl;
    return true;
  }

  if (!std::strcmp(mode, "before")) {
    offload_count_before = get_int_val(offload_count.c_str());
  } else if (!std::strcmp(mode, "after")) {
    if (!offload_count_after) {
      offload_count_after = get_int_val(offload_count.c_str());
    } else {
      offload_count_after =
          offload_count_after + get_int_val(offload_count.c_str());
    }
  }

  return false;
}

/// Report secondary engine execution count value.
///
/// @param filename File to store the count value
void Secondary_engine::report_offload_count(const char *filename) {
  if (!offload_count_after && offload_count_after < offload_count_before)
    offload_count_after = offload_count_before;

  int count_val = offload_count_after - offload_count_before;
  DBUG_ASSERT(count_val >= 0);

  std::ofstream report_file(filename, std::ios::out);

  if (report_file.is_open()) {
    std::string count = std::to_string(count_val);
    report_file << count << std::endl;
  }

  report_file.close();
}
