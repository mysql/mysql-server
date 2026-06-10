/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MYSQL_TASK_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MYSQL_TASK_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "collector/counted_mysql_session.h"
#include "helper/mysql_column.h"
#include "http/base/status_code.h"
#include "mrs/database/entry/field.h"
#include "mrs/database/helper/query.h"
#include "mrs/database/json_template.h"
#include "mrs/database/mysql_task_monitor.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/rest_handler.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class QueryRestMysqlTask : private Query {
  using Row = Query::Row;
  using ResultSets = entry::ResultSets;
  using MysqlTaskOptions = interface::Options::MysqlTask;
  using CachedSession = collector::MysqlFixedPoolManager::CachedObject;
  using PoolManager = collector::MysqlFixedPoolManager;
  using PoolManagerRef = std::shared_ptr<PoolManager>;

 public:
  explicit QueryRestMysqlTask(mrs::database::MysqlTaskMonitor *task_monitor);

  void execute_procedure_at_server(
      collector::CountedMySQLSession *session,
      const mysqlrouter::sqlstring &user_id, const std::string &user_name,
      std::optional<std::string> user_ownership_column,
      const std::string &schema, const std::string &object,
      const std::string &url, const MysqlTaskOptions &task_options,
      const rapidjson::Document &doc, const ResultSets &rs);

  void execute_procedure_at_router(
      CachedSession session, PoolManagerRef pool_ref,
      const mysqlrouter::sqlstring &user_id,
      std::optional<std::string> user_ownership_column,
      const std::string &schema, const std::string &object,
      const std::string &url, const MysqlTaskOptions &task_options,
      const rapidjson::Document &doc, const ResultSets &rs);

  void execute_function_at_server(
      collector::CountedMySQLSession *session,
      const mysqlrouter::sqlstring &user_id, const std::string &user_name,
      std::optional<std::string> user_ownership_column,
      const std::string &schema, const std::string &object,
      const std::string &url, const MysqlTaskOptions &task_options,
      const rapidjson::Document &doc, const ResultSets &rs);

  void execute_function_at_router(
      CachedSession session, PoolManagerRef pool_ref,
      const mysqlrouter::sqlstring &user_id,
      std::optional<std::string> user_ownership_column,
      const std::string &schema, const std::string &object,
      const std::string &url, const MysqlTaskOptions &task_options,
      const rapidjson::Document &doc, const ResultSets &rs);

  static void kill_task(collector::CountedMySQLSession *session,
                        const mysqlrouter::sqlstring &user_id,
                        const std::string &task_id);

  const char *get_sql_state();
  uint64_t items;
  // To be fed to HTTP Result
  std::string response;

 protected:
  std::string url_;
  mrs::database::MysqlTaskMonitor *task_monitor_;

  void execute_at_router(CachedSession session, PoolManagerRef pool_ref,
                         const mysqlrouter::sqlstring &user_id,
                         std::optional<std::string> user_ownership_column,
                         bool is_procedure, const std::string &schema,
                         const std::string &object,
                         const MysqlTaskOptions &task_options,
                         const rapidjson::Document &doc, const ResultSets &rs);

  void execute_at_server(collector::CountedMySQLSession *session,
                         const mysqlrouter::sqlstring &user_id,
                         const std::string &user_name,
                         std::optional<std::string> user_ownership_column,
                         bool is_procedure, const std::string &schema,
                         const std::string &object, const std::string &url,
                         const MysqlTaskOptions &task_options,
                         const rapidjson::Document &doc, const ResultSets &rs);

  mysqlrouter::sqlstring build_procedure_call(
      const std::string &schema, const std::string &object,
      const mysqlrouter::sqlstring &user_id,
      std::optional<std::string> user_ownership_column, const ResultSets &rs,
      const rapidjson::Document &doc, std::list<std::string> *out_preamble,
      std::list<std::string> *out_postamble);

  mysqlrouter::sqlstring build_function_call(
      const std::string &schema, const std::string &object,
      const mysqlrouter::sqlstring &user_id,
      std::optional<std::string> user_ownership_column, const ResultSets &rs,
      const rapidjson::Document &doc, std::list<std::string> *out_preamble,
      std::list<std::string> *out_postamble);

  mysqlrouter::sqlstring wrap_async_server_call(
      const std::string &schema, const mysqlrouter::sqlstring &user_id,
      const std::string &user_name, const MysqlTaskOptions &task_options,
      mysqlrouter::sqlstring query, std::list<std::string> preamble,
      std::list<std::string> postamble);

  static std::list<std::string> on_task_error(
      const std::exception &e, const std::string &task_id,
      const std::string &progress_event_name);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MYSQL_TASK_H_
