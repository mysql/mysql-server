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

#include "mrs/database/query_rest_task.h"

#include <map>
#include <vector>

#include "helper/json/rapid_json_iterator.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/sqlstring_utils.h"
#include "mrs/database/helper/sp_function_query.h"
#include "mrs/http/error.h"

#include "mysql/harness/string_utils.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

namespace {
inline std::string join_script(const std::vector<std::string> &script) {
  std::string r;
  for (const auto &s : script) {
    r.append(s);
    if (s.back() != ';')
      r.append(";\n");
    else
      r.append("\n");
  }
  return r;
}

std::string prepare_monitor_script(const std::vector<std::string> &script,
                                   const std::string &quoted_user_id,
                                   const std::string &connection_id,
                                   const std::string &thread_id,
                                   const std::string &start_time) {
  std::string sql;
  // @task_id is by the monitoring event in mysql_tasks
  sql.append("SET @task_app_user_id=" + quoted_user_id + "; ");
  sql.append("SET @task_connection_id=" + connection_id + "; ");
  sql.append("SET @task_thread_id=" + thread_id + "; ");
  // monitor thread starts before query, so it's ok for start time to be earlier
  sql.append("SET @task_start_time='" + start_time + "';\n");
  sql.append(join_script(script));

  return sql;
}

mysqlrouter::sqlstring cast_as_json(const mysqlrouter::sqlstring &sql,
                                    mrs::database::entry::ColumnType type) {
  switch (type) {
    case mrs::database::entry::ColumnType::JSON: {
      mysqlrouter::sqlstring res("CAST((?) AS JSON)");
      res << sql;
      return res;
    }
    case mrs::database::entry::ColumnType::VECTOR: {
      mysqlrouter::sqlstring res(
          "CAST(CONVERT(VECTOR_TO_STRING((?)) USING utf8mb4) AS JSON)");
      res << sql;
      return res;
    }
    default:
      return sql;
  }
}

inline void sql_append_item(mysqlrouter::sqlstring *q, bool *first,
                            const mysqlrouter::sqlstring &item) {
  if (!*first) q->append_preformatted(",");
  *first = false;
  q->append_preformatted(item);
}

}  // namespace

QueryRestMysqlTask::QueryRestMysqlTask(
    mrs::database::MysqlTaskMonitor *task_monitor)
    : task_monitor_(task_monitor) {}

const char *QueryRestMysqlTask::get_sql_state() {
  if (!sqlstate_.has_value()) return nullptr;
  return sqlstate_.value().c_str();
}

mysqlrouter::sqlstring QueryRestMysqlTask::build_procedure_call(
    const std::string &schema, const std::string &object,
    const mysqlrouter::sqlstring &user_id,
    std::optional<std::string> user_ownership_column, const ResultSets &rs,
    const rapidjson::Document &doc, std::list<std::string> *out_preamble,
    std::list<std::string> *out_postamble) {
  using namespace std::string_literals;
  using namespace helper::json::sql;

  auto &param_fields = rs.parameters.fields;

  mysqlrouter::sqlstring query{"CALL !.!("};
  query << schema << object;

  auto result = mysqlrouter::sqlstring("");
  bool first = true;
  for (auto &el : param_fields) {
    if (user_ownership_column.has_value() &&
        (*user_ownership_column == el.bind_name)) {
      sql_append_item(&query, &first, user_id);
    } else if (el.mode == mrs::database::entry::Field::Mode::modeIn) {
      auto it = doc.FindMember(el.name.c_str());
      if (it != doc.MemberEnd()) {
        if (el.is_user_variable) {
          mysqlrouter::sqlstring sql("SET @!=?",
                                     mysqlrouter::QuoteOnlyIfNeeded);
          sql << el.bind_name;
          sql << helper::get_sql_formatted(it->value, el.data_type);
          out_preamble->emplace_back(std::move(sql));
        } else {
          mysqlrouter::sqlstring sql = get_sql_format(el.data_type, it->value);
          sql << it->value;
          sql_append_item(&query, &first, sql);
        }
      } else {
        if (!el.is_user_variable) sql_append_item(&query, &first, "NULL");
      }
    } else {
      mysqlrouter::sqlstring var{"@!", mysqlrouter::QuoteOnlyIfNeeded};
      if (el.is_user_variable) {
        var << el.bind_name;
      } else {
        var << "__" + el.bind_name;

        sql_append_item(&query, &first, var);
      }
      mysqlrouter::sqlstring item("?, ?");
      item << el.name;
      item << cast_as_json(var, el.data_type);
      result.append_preformatted_sep(", ", item);

      if (el.mode == mrs::database::entry::Field::Mode::modeInOut) {
        mysqlrouter::sqlstring set_var{"SET ? = ?"};
        set_var << var;
        auto it = doc.FindMember(el.name.c_str());
        if (it != doc.MemberEnd()) {
          mysqlrouter::sqlstring sql = get_sql_format(el.data_type, it->value);
          sql << it->value;
          set_var << sql;
        } else {
          set_var << nullptr;
        }
        out_preamble->emplace_back(set_var.str());
      }
    }
  }
  query.append_preformatted(")");

  out_postamble->emplace_back(
      "SET @task_result = JSON_MERGE_PATCH(JSON_OBJECT(" + result.str() +
      "), IF(JSON_VALID(@task_result),"
      " JSON_SET('{}', '$.taskResult', CAST(@task_result AS JSON)),"
      " JSON_SET('{}', '$.taskResult', @task_result)))");

  return query;
}

mysqlrouter::sqlstring QueryRestMysqlTask::build_function_call(
    const std::string &schema, const std::string &object,
    const mysqlrouter::sqlstring &user_id,
    std::optional<std::string> user_ownership_column, const ResultSets &rs,
    const rapidjson::Document &doc, std::list<std::string> *out_preamble,
    [[maybe_unused]] std::list<std::string> *out_postamble) {
  using namespace std::string_literals;
  using namespace helper::json::sql;

  auto &param_fields = rs.parameters.fields;

  mysqlrouter::sqlstring query{"SELECT !.!("};
  query << schema << object;

  auto result = mysqlrouter::sqlstring("");
  bool first = true;
  for (auto &el : param_fields) {
    if (user_ownership_column.has_value() &&
        (*user_ownership_column == el.bind_name)) {
      sql_append_item(&query, &first, user_id);
    } else if (el.mode == mrs::database::entry::Field::Mode::modeIn ||
               el.mode == mrs::database::entry::Field::Mode::modeInOut) {
      auto it = doc.FindMember(el.name.c_str());
      if (it != doc.MemberEnd()) {
        if (el.is_user_variable) {
          mysqlrouter::sqlstring sql("SET @!=?",
                                     mysqlrouter::QuoteOnlyIfNeeded);
          sql << el.bind_name;
          sql << helper::get_sql_formatted(it->value, el.data_type);
          out_preamble->emplace_back(std::move(sql));
        } else {
          mysqlrouter::sqlstring sql = get_sql_format(el.data_type, it->value);
          sql << it->value;
          sql_append_item(&query, &first, sql);
        }
      } else {
        if (!el.is_user_variable) sql_append_item(&query, &first, "NULL");
      }
    }
  }
  query.append_preformatted(")");

  if (rs.results.size() == 1 && rs.results[0].fields.size() == 1) {
    query = cast_as_json(query, rs.results[0].fields[0].data_type);
  }

  mysqlrouter::sqlstring wrapper{
      "SET @task_result = JSON_MERGE_PATCH(JSON_OBJECT('result', (?)"};
  wrapper << query;

  for (auto &el : param_fields) {
    if ((el.mode == mrs::database::entry::Field::modeOut ||
         el.mode == mrs::database::entry::Field::modeInOut) &&
        el.is_user_variable) {
      mysqlrouter::sqlstring uvar{"@!", mysqlrouter::QuoteOnlyIfNeeded};
      uvar << el.bind_name;

      mysqlrouter::sqlstring item("?, ?");

      item << el.name;
      item << cast_as_json(uvar, el.data_type);

      wrapper.append_preformatted_sep(", ", item);
    }
  }

  wrapper.append_preformatted(
      "), IF(JSON_VALID(@task_result),"
      " JSON_SET('{}', '$.taskResult', CAST(@task_result AS JSON)),"
      " JSON_SET('{}', '$.taskResult', @task_result)))");

  return wrapper;
}

mysqlrouter::sqlstring QueryRestMysqlTask::wrap_async_server_call(
    const std::string &schema, const mysqlrouter::sqlstring &user_id,
    const std::string &user_name, const MysqlTaskOptions &task_options,
    mysqlrouter::sqlstring query, std::list<std::string> preamble,
    std::list<std::string> postamble) {
  std::string task_sql;
  {
    for (const auto &s : preamble) {
      task_sql.append(s).append(";");
    }

    task_sql.append(query.str()).append(";");

    for (const auto &s : postamble) {
      task_sql.append(s).append(";");
    }
  }

  mysqlrouter::sqlstring sql(
      "CALL mysql_tasks.execute_prepared_stmt_from_app_async(?, ?, ?, ?, ?, ?, "
      "?, ?, ?, NULL, @task_id)",
      0);

  sql << task_sql << user_id
      << (task_options.event_schema.empty()
              ? schema
              : mysql_harness::replace(task_options.event_schema, "$username",
                                       user_name))
      << nullptr  // task_type
      << (task_options.name.empty() ? "REST:" + url_ : task_options.name)
      << nullptr   // task_data
      << nullptr;  // data_json_schema
  if (task_options.status_data_json_schema.empty())
    sql << nullptr;
  else
    sql << task_options.status_data_json_schema;
  sql << join_script(task_options.monitoring_sql);

  return sql;
}

void QueryRestMysqlTask::execute_procedure_at_server(
    collector::CountedMySQLSession *session,
    const mysqlrouter::sqlstring &user_id, const std::string &user_name,
    std::optional<std::string> user_ownership_column, const std::string &schema,
    const std::string &object, const std::string &url,
    const MysqlTaskOptions &task_options, const rapidjson::Document &doc,
    const ResultSets &rs) {
  url_ = url;
  execute_at_server(session, user_id, user_name, user_ownership_column, true,
                    schema, object, url, task_options, doc, rs);
}

void QueryRestMysqlTask::execute_procedure_at_router(
    CachedSession session, PoolManagerRef pool_ref,
    const mysqlrouter::sqlstring &user_id,
    std::optional<std::string> user_ownership_column, const std::string &schema,
    const std::string &object, const std::string &url,
    const MysqlTaskOptions &task_options, const rapidjson::Document &doc,
    const ResultSets &rs) {
  url_ = url;
  execute_at_router(std::move(session), std::move(pool_ref), user_id,
                    user_ownership_column, true, schema, object, task_options,
                    doc, rs);
}

void QueryRestMysqlTask::execute_function_at_server(
    collector::CountedMySQLSession *session,
    const mysqlrouter::sqlstring &user_id, const std::string &user_name,
    std::optional<std::string> user_ownership_column, const std::string &schema,
    const std::string &object, const std::string &url,
    const MysqlTaskOptions &task_options, const rapidjson::Document &doc,
    const ResultSets &rs) {
  url_ = url;

  execute_at_server(session, user_id, user_name, user_ownership_column, false,
                    schema, object, url, task_options, doc, rs);
}

void QueryRestMysqlTask::execute_function_at_router(
    CachedSession session, PoolManagerRef pool_ref,
    const mysqlrouter::sqlstring &user_id,
    std::optional<std::string> user_ownership_column, const std::string &schema,
    const std::string &object, const std::string &url,
    const MysqlTaskOptions &task_options, const rapidjson::Document &doc,
    const ResultSets &rs) {
  url_ = url;
  execute_at_router(std::move(session), std::move(pool_ref), user_id,
                    user_ownership_column, false, schema, object, task_options,
                    doc, rs);
}

void QueryRestMysqlTask::execute_at_server(
    collector::CountedMySQLSession *session,
    const mysqlrouter::sqlstring &user_id, const std::string &user_name,
    std::optional<std::string> user_ownership_column, bool is_procedure,
    const std::string &schema, const std::string &object,
    const std::string &url, const MysqlTaskOptions &task_options,
    const rapidjson::Document &doc, const ResultSets &rs) {
  url_ = url;

  std::list<std::string> preamble;
  std::list<std::string> postamble;
  mysqlrouter::sqlstring call_sql;

  if (is_procedure)
    call_sql =
        build_procedure_call(schema, object, user_id, user_ownership_column, rs,
                             doc, &preamble, &postamble);
  else
    call_sql =
        build_function_call(schema, object, user_id, user_ownership_column, rs,
                            doc, &preamble, &postamble);

  query_ = wrap_async_server_call(schema, user_id, user_name, task_options,
                                  std::move(call_sql), std::move(preamble),
                                  std::move(postamble));
  execute(session);

  std::string task_id;
  auto row = session->query_one("select @task_id as taskId");
  if (row && (*row)[0]) {
    task_id = (*row)[0];
  } else {
    log_warning("Could not start async task for %s", url.c_str());
    throw std::runtime_error("Error starting asynchronous task");
  }

  response = helper::json::to_string(
      {{"message", "Request accepted. Starting to process task in background."},
       {"taskId", task_id},
       {"statusUrl", url + "/" + task_id}});
}

void QueryRestMysqlTask::execute_at_router(
    CachedSession session, PoolManagerRef pool_ref,
    const mysqlrouter::sqlstring &user_id,
    std::optional<std::string> user_ownership_column, bool is_procedure,
    const std::string &schema, const std::string &object,
    const MysqlTaskOptions &task_options, const rapidjson::Document &doc,
    const ResultSets &rs) {
  using namespace std::string_literals;
  using namespace helper::json::sql;

  std::string connection_id;
  std::string thread_id;
  std::string task_id;
  std::string user_name;
  std::string progress_event_name;
  std::string start_time;
  auto row = session->query_one(
      "select uuid(), replace(uuid(), '-', ''), connection_id(),"
      "  ps_current_thread_id(), mysql_tasks.extract_username(current_user()),"
      "  now(6)");
  if (row && (*row)[0]) {
    task_id = (*row)[0];
    connection_id = (*row)[2] ? (*row)[2] : "NULL";
    thread_id = (*row)[3] ? (*row)[3] : "NULL";
    user_name = (*row)[4] ? (*row)[4] : "";
    start_time = (*row)[5] ? (*row)[5] : "";
    mysqlrouter::sqlstring tmp("!.!");
    tmp << (task_options.event_schema.empty()
                ? schema
                : mysql_harness::replace(task_options.event_schema, "$username",
                                         user_name));
    tmp << (*row)[1];
    progress_event_name = tmp.str();
  } else {
    throw std::runtime_error("Error in UUID() call");
  }

  mysqlrouter::sqlstring internal_data(
      "JSON_OBJECT('mysqlMetadata', JSON_OBJECT('events', ?, 'autoGc', true))");

  if (task_options.monitoring_sql.empty()) {
    auto event = mysqlrouter::sqlstring("JSON_ARRAY(NULL)");
    internal_data << event;
  } else {
    auto event = mysqlrouter::sqlstring("JSON_ARRAY(NULL, ?)");
    event << progress_event_name;
    internal_data << event;
  }

  query_ = {
      "CALL `mysql_tasks`.`create_app_task_with_id`(?, ?, ?,"
      " 'Router_Async_SQL', json_merge_patch(?, ?), ?, ?)"};
  query_ << user_id << task_id
         << (task_options.name.empty() ? "REST:" + url_ : task_options.name)
         << internal_data  // internal_data
         << "{}"           // data
         << "{}";          // data_json_schema
  if (task_options.status_data_json_schema.empty())
    query_ << nullptr;
  else
    query_ << task_options.status_data_json_schema;
  execute(session.get());

  query_ = "SET @mysql_tasks_initiated = 'MRS'";
  execute(session.get());

  query_ = {"CALL `mysql_tasks`.`start_task_monitor`(?, ?, ?, NULL)"};
  query_ << progress_event_name << task_id;
  if (task_options.monitoring_sql.empty())
    query_ << nullptr;
  else
    query_ << prepare_monitor_script(task_options.monitoring_sql, user_id,
                                     connection_id, thread_id, start_time);
  execute(session.get());

  query_ = {
      "CALL `mysql_tasks`.`add_task_log`(?, 'Executing...', NULL, 0,"
      " 'RUNNING')"};
  query_ << task_id;
  execute(session.get());

  // the rest is executed asynchronously by the task thread

  std::list<std::string> preamble;
  std::string script;
  std::list<std::string> postamble;

  preamble.emplace_back("SET @task_result = NULL");

  if (is_procedure)
    query_ =
        build_procedure_call(schema, object, user_id, user_ownership_column, rs,
                             doc, &preamble, &postamble);
  else
    query_ = build_function_call(schema, object, user_id, user_ownership_column,
                                 rs, doc, &preamble, &postamble);

  script = query_.str();

  mysqlrouter::sqlstring query{"CALL `mysql_tasks`.`stop_task_monitor`(?, ?)"};
  query << progress_event_name << task_id;
  postamble.emplace_front(query.str());

  postamble.emplace_back("SET @mysql_tasks_initiated = NULL");

  query = {
      "CALL `mysql_tasks`.`add_task_log`(?, 'Execution finished.',"
      " CAST(@task_result AS JSON), 100, 'COMPLETED')"};
  query << task_id;
  postamble.emplace_back(query.str());

  task_monitor_->call_async(
      std::move(session), std::move(pool_ref), std::move(preamble),
      std::move(script), std::move(postamble),
      [task_id, progress_event_name](const std::exception &e) {
        return on_task_error(e, task_id, progress_event_name);
      },
      task_id);

  response = helper::json::to_string(
      {{"message", "Request accepted. Starting to process task."},
       {"taskId", task_id},
       {"statusUrl", url_ + "/" + task_id}});
}

std::list<std::string> QueryRestMysqlTask::on_task_error(
    const std::exception &e, const std::string &task_id,
    const std::string &progress_event_name) {
  std::list<std::string> sql;

  mysqlrouter::sqlstring query{"CALL `mysql_tasks`.`stop_task_monitor`(?, ?)"};
  query << progress_event_name << task_id;
  sql.emplace_back(query.str());

  query = {"CALL `mysql_tasks`.`add_task_log`(?, ?, NULL, 100, 'ERROR')"};
  query << task_id;
  query << e.what();
  sql.emplace_back(query.str());

  return sql;
}

void QueryRestMysqlTask::kill_task(collector::CountedMySQLSession *session,
                                   const mysqlrouter::sqlstring &user_id,
                                   const std::string &task_id) {
  mysqlrouter::sqlstring query{"CALL mysql_tasks.kill_app_task(?, ?)"};
  query << user_id << task_id;
  try {
    session->execute(query.str());
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (e.code() == 1644 && e.message() == "Task inactive.") {
      // task already finished
      return;
    } else if (e.code() == 1095) {
      // task belongs to a different mysql user (sql security definer?)
      throw http::Error(HttpStatusCode::Forbidden);
    }
    throw;
  }
}

}  // namespace database
}  // namespace mrs
