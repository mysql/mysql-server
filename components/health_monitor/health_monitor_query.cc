/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_query.h"

#include <mysql.h>
#include <cstring>

#include "component_command.h"  // command services
#include "component_service.h"

#include "mysql/mysql_lex_string.h"  // MYSQL_LEX_CSTRING
#include "string_with_len.h"         // STRING_WITH_LEN

namespace hm {

/**
  Default way to execute queries. Will execute the query as SQL.
  @param[out] result_set        Result set returned to caller
  @return 0 success, 1 failure
*/
int Query::run(Result_Set &result_set) const {
  return Query_Manager::run_query(*this, result_set);
}

/**
  Establish a connection to the server then execute all report queries.
  @param[in]  query_info        One query to run
  @param[out] result_set        Result set returned to caller
  @return 0 success, 1 failure
*/
int Query_Manager::run_query(const Query &query_info, Result_Set &result_set) {
  MYSQL_H mysql_h{nullptr};
  MYSQL_RES_H mysql_res{nullptr};
  const std::string query{query_info.query};

  const Row title_row{query_info.title};
  result_set.clear();
  result_set.push_back(title_row);

  // Title only
  if (query.empty()) return 0;

  // Free query resources and update the results with the last error message
  class Cleanup {
    MYSQL_H *mysql_h{nullptr};
    MYSQL_RES_H *mysql_res{nullptr};
    Result_Set &results;

   public:
    Cleanup(MYSQL_H *m_h, MYSQL_RES_H *m_r, Result_Set &result_set)
        : mysql_h(m_h), mysql_res(m_r), results(result_set) {}
    ~Cleanup() {
      char errmsg[MYSQL_ERRMSG_SIZE]{'\0'};
      char *perr = errmsg;
      // Append error message, if any, to the result set
      if ((*mysql_h) != nullptr &&
          cmd_error_info_srv->sql_error(*mysql_h, &perr) == 0) {
        if (strlen(perr) > 0) {
          const Row err_row{errmsg};
          results.push_back(err_row);
        }
      }
      if (*mysql_res != nullptr) cmd_query_result_srv->free_result(*mysql_res);
      if (*mysql_h != nullptr) cmd_factory_srv->close(*mysql_h);
      cmd_thread_srv->end();
    }
    Cleanup(const Cleanup &) = delete;
    Cleanup &operator=(const Cleanup &) = delete;
    Cleanup(Cleanup &&) = delete;
    Cleanup &operator=(Cleanup &&) = delete;
  } cleanup(&mysql_h, &mysql_res, result_set);

  const MYSQL_LEX_CSTRING sql_mode{
      STRING_WITH_LEN("SET @@SESSION.sql_mode="
                      "'NO_ENGINE_SUBSTITUTION,"
                      "ONLY_FULL_GROUP_BY,"
                      "STRICT_TRANS_TABLES,"
                      "NO_ZERO_IN_DATE,"
                      "NO_ZERO_DATE,"
                      "ERROR_FOR_DIVISION_BY_ZERO'")};

  const MYSQL_LEX_CSTRING is_stats_expiry{
      STRING_WITH_LEN("SET @@SESSION.information_schema_stats_expiry=0")};

  const MYSQL_LEX_CSTRING statement_truncate_len{
      STRING_WITH_LEN("SET @sys.statement_truncate_len=256")};

  // Establish connection, turn on autocommit, set sql mode, execute query
  if (cmd_factory_srv->init(&mysql_h) != 0 || mysql_h == nullptr ||
      cmd_thread_srv->init() != 0)
    return 1;
  // Set flag to not take a lock on the registry.
  // This is to avoid race condition while uninstalling component health
  // monitor.
  if (cmd_options_srv->set(mysql_h, MYSQL_NO_LOCK_REGISTRY,
                           reinterpret_cast<void *>(1)) != 0)
    return 1;
  if (cmd_factory_srv->connect(mysql_h) != 0 ||
      cmd_factory_srv->reset(mysql_h) != 0 ||
      cmd_factory_srv->autocommit(mysql_h, true) != 0 ||
      cmd_query_srv->query(mysql_h, sql_mode.str, sql_mode.length) != 0 ||
      cmd_query_srv->query(mysql_h, is_stats_expiry.str,
                           is_stats_expiry.length) != 0 ||
      cmd_query_srv->query(mysql_h, statement_truncate_len.str,
                           statement_truncate_len.length) != 0 ||
      cmd_query_srv->query(mysql_h, query.data(), query.length()) != 0) {
    return 1;
  }

  mysql_res = reinterpret_cast<MYSQL_RES_H>(1);
  if (cmd_query_result_srv->store_result(mysql_h, &mysql_res) != 0) {
    // If mysql_res was set to nullptr, we know the result set was empty
    if (mysql_res == nullptr) return 0;
    // Otherwise, we must reset mysql_res to avoid problems in the cleanup
    // handler
    mysql_res = nullptr;
    return 1;
  }

  uint64_t row_count{0};
  unsigned int col_count{0};

  if (cmd_query_srv->affected_rows(mysql_h, &row_count) != 0 ||
      cmd_field_info_srv->num_fields(mysql_res, &col_count) != 0)
    return 1;

  // Column headers
  if (cmd_query_result_srv->result_metadata(mysql_res) == 0) {
    MYSQL_FIELD_H mysql_field{nullptr};
    Row row;
    for (unsigned int j = 0; j < col_count; j++) {
      const char *field_name{nullptr};
      if (cmd_field_info_srv->fetch_field(mysql_res, &mysql_field) != 0 ||
          mysql_field == nullptr ||
          cmd_field_meta_srv->get(mysql_field,
                                  MYSQL_COMMAND_FIELD_METADATA_NAME,
                                  &field_name) != 0 ||
          field_name == nullptr)
        return 1;
      row.emplace_back(field_name);
    }
    result_set.push_back(row);
  }
  // Result set, field by field, row by row
  // TODO: Add memory check to avoid using excessive memory if the server is
  // already approaching OOM
  for (uint64_t i = 0; i < row_count; i++) {
    MYSQL_ROW_H mysql_row{nullptr};
    if (cmd_query_result_srv->fetch_row(mysql_res, &mysql_row) != 0 ||
        mysql_row == nullptr)
      return 1;
    Row row;
    for (unsigned int j = 0; j < col_count; j++) {
      row.emplace_back(mysql_row[j] != nullptr ? mysql_row[j] : "NULL");
    }
    result_set.push_back(row);
  }

  return 0;
}

/**
  Execute a query, return result set.
  @param[in] query_info   One query to run
  @param[out] result_set  Result set
  @return 0 success, 1 failure
*/
int Query_Manager::run_query(const Query &&query_info, Result_Set &result_set) {
  return run_query(query_info, result_set);
}

/**
  Execute a query, ignore result set.
  @param[in] query_info  One query to run
  @return 0 success, 1 failure
*/
int Query_Manager::run_query(const Query &query_info) {
  Result_Set results;
  return run_query(query_info, results);
}

int Query_Manager::run_query(const Query &&query_info) {
  Result_Set results;
  return run_query(query_info, results);
}

}  // namespace hm
