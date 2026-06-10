/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mrs/endpoint/handler/handler_db_object_sp.h"

#include <string>

#include "helper/http/url.h"
#include "helper/json/jvalue.h"
#include "helper/json/rapid_json_iterator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "helper/mysql_numeric_value.h"
#include "helper/sqlstring_utils.h"
#include "mrs/database/helper/bind.h"
#include "mrs/database/helper/sp_function_query.h"
#include "mrs/database/query_rest_sp.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/database/query_rest_task.h"
#include "mrs/database/query_rest_task_status.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/routine_utilities.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"
#include "mrs/router_observation_entities.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysql/harness/utility/string.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = mrs::rest::Handler::HttpResult;
using CachedObject = collector::MysqlCacheManager::CachedObject;
using Url = helper::http::Url;
using Authorization = mrs::rest::Handler::Authorization;

HandlerDbObjectSP::HandlerDbObjectSP(
    std::weak_ptr<DbObjectEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager,
    mrs::GtidManager *gtid_manager, collector::MysqlCacheManager *cache,
    mrs::ResponseCache *response_cache,
    mrs::database::SlowQueryMonitor *slow_monitor,
    mrs::database::MysqlTaskMonitor *task_monitor)
    : HandlerDbObjectTable{endpoint, auth_manager,   gtid_manager,
                           cache,    response_cache, slow_monitor},
      task_monitor_(task_monitor) {}

HttpResult HandlerDbObjectSP::handle_put(rest::RequestContext *ctxt) {
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto request_body = input_buffer.pop_front(size);

  return handle_post(ctxt, request_body);
}

HttpResult HandlerDbObjectSP::call(rest::RequestContext *ctxt,
                                   rapidjson::Document doc) {
  using namespace helper::json::sql;

  auto session = get_session(ctxt, collector::kMySQLConnectionUserdataRW);

  auto url = get_endpoint_url(endpoint_);
  auto &rs = entry_->fields;
  mrs::database::MysqlBind binds;
  std::string result;
  auto user_id = get_user_id(ctxt, false);

  fill_procedure_argument_list_with_binds(rs, doc, ownership_, user_id, &binds,
                                          &result);

  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reset.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  database::QueryRestSP db;
  try {
    auto *gtid_manager = get_options().metadata.gtid ? gtid_manager_ : nullptr;

    slow_monitor_->execute(
        [&]() {
          db.query_entries(
              session.get(), schema_entry_->name, entry_->name, url,
              ownership_.user_ownership_column, result.c_str(),
              binds.parameters, rs,
              database::JsonTemplateType::kObjectNestedOutParameters,
              gtid_manager);
        },
        session.get(), get_options().query.timeout);
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, db.get_sql_state());
  }

  return {HttpStatusCode::Ok, std::move(db.response),
          HttpResult::Type::typeJson};
}

HttpResult HandlerDbObjectSP::call(
    rest::RequestContext *ctxt, const helper::http::Url::Parameters &query_kv) {
  using namespace std::string_literals;

  Url::Keys keys;
  Url::Values values;

  auto url = get_endpoint_url(endpoint_);
  auto &rs = entry_->fields;
  std::string result;
  mrs::database::MysqlBind binds;
  auto user_id = get_user_id(ctxt, false);

  fill_procedure_argument_list_with_binds(rs, query_kv, ownership_, user_id,
                                          &binds, &result);

  auto session = get_session(ctxt, collector::kMySQLConnectionUserdataRW);
  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reset.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  log_debug("HandlerDbObjectSP::handle_get start format=%i",
            (int)entry_->format);

  if (entry_->format == mrs::database::entry::DbObject::formatFeed) {
    log_debug(
        "HandlerDbObjectSP::handle_get - generating feed "
        "response");
    database::QueryRestSP db;
    try {
      auto *gtid_manager =
          get_options().metadata.gtid ? gtid_manager_ : nullptr;
      slow_monitor_->execute(
          [&]() {
            db.query_entries(
                session.get(), schema_entry_->name, entry_->name, url,
                ownership_.user_ownership_column, result.c_str(),
                binds.parameters, rs,
                database::JsonTemplateType::kObjectNestedOutParameters,
                gtid_manager);
          },
          session.get(), get_options().query.timeout);
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      return handler_mysqlerror(e, db.get_sql_state());
    }

    return {std::move(db.response)};
  }

  database::QueryRestSPMedia db;
  slow_monitor_->execute(
      [&]() {
        db.query_entries(session.get(), schema_entry_->name, entry_->name,
                         result.c_str());
      },
      session.get(), get_options().query.timeout);

  if (entry_->autodetect_media_type) {
    log_debug(
        "HandlerDbObjectSP::handle_get - autodetection "
        "response");
    helper::MediaDetector md;
    auto detected_type = md.detect(db.response);

    return {std::move(db.response), detected_type};
  }

  if (entry_->media_type.has_value()) {
    return {std::move(db.response), entry_->media_type.value()};
  }

  return {std::move(db.response), helper::MediaType::typeUnknownBinary};
}

HttpResult HandlerDbObjectSP::call_async(rest::RequestContext *ctxt,
                                         rapidjson::Document doc) {
  using namespace helper::json::sql;

  mrs::database::MysqlTaskMonitor::PoolManagerRef pool;

  // only authenticated users can start async tasks
  auto user_id = get_user_id(ctxt, true);
  auto session =
      get_session(ctxt, collector::kMySQLConnectionUserdataRW, &pool);

  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reset.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  database::QueryRestMysqlTask db(task_monitor_);
  try {
    slow_monitor_->execute(
        [&]() {
          std::optional<std::string> user_ownership_column{
              ownership_.user_ownership_enforced
                  ? std::make_optional(ownership_.user_ownership_column)
                  : std::nullopt};

          if (get_options().mysql_task.driver ==
              interface::Options::MysqlTask::DriverType::kDatabase)
            db.execute_procedure_at_server(
                session.get(), user_id, get_user_name(ctxt),
                user_ownership_column, schema_entry_->name, entry_->name,
                get_endpoint_url(endpoint_), get_options().mysql_task, doc,
                entry_->fields);
          else
            db.execute_procedure_at_router(
                std::move(session), std::move(pool), user_id,
                user_ownership_column, schema_entry_->name, entry_->name,
                get_endpoint_url(endpoint_), get_options().mysql_task, doc,
                entry_->fields);
        },
        session.get(), get_options().query.timeout);
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, db.get_sql_state());
  }

  return {HttpStatusCode::Accepted, std::move(db.response),
          HttpResult::Type::typeJson};
}

HttpResult HandlerDbObjectSP::handle_post(
    rest::RequestContext *ctxt, const std::vector<uint8_t> &document) {
  using namespace std::string_literals;

  rapidjson::Document doc;
  doc.Parse(reinterpret_cast<const char *>(document.data()), document.size());

  if (!doc.IsObject()) throw http::Error(HttpStatusCode::BadRequest);

  auto &rs = entry_->fields;
  check_input_parameters(rs.parameters.fields, doc);

  // Execute the SP. If it's an async task, then start the task and return 202
  if (get_options().mysql_task.driver !=
      interface::Options::MysqlTask::DriverType::kNone) {
    return call_async(ctxt, std::move(doc));
  } else {
    return call(ctxt, std::move(doc));
  }
}

HttpResult HandlerDbObjectSP::handle_get(rest::RequestContext *ctxt) {
  // Get async task status on /svc/db/sp/taskId
  if (get_options().mysql_task.driver !=
      interface::Options::MysqlTask::DriverType::kNone) {
    // only authenticated users can start async tasks
    auto user_id = get_user_id(ctxt, true);
    auto session = get_session(ctxt, collector::kMySQLConnectionUserdataRW);
    std::string task_id =
        get_path_after_object_name(endpoint_, ctxt->request->get_uri());
    if (task_id.empty()) throw http::Error(HttpStatusCode::NotFound);

    std::string result;

    log_debug("HandlerDbObjectSP::handle_get check task_id=%s",
              task_id.c_str());
    database::QueryRestTaskStatus db;
    db.query_status(session.get(), ctxt->request->get_uri().get_path(), user_id,
                    get_options().mysql_task, task_id);

    return {db.status, std::move(db.response), HttpResult::Type::typeJson};
  } else {
    auto &requests_uri = ctxt->request->get_uri();
    const auto &query_kv = requests_uri.get_query_elements();

    // cache likely dynamically rendered content
    if (response_cache_) {
      auto entry =
          response_cache_->lookup_routine(ctxt->request->get_uri(), {});
      if (entry) {
        return {std::string{entry->data}};
      }
    }

    auto res = call(ctxt, query_kv);

    if (response_cache_ && res.status == HttpStatusCode::Ok) {
      auto entry = response_cache_->create_routine_entry(
          ctxt->request->get_uri(), {}, res.response);
    }

    return res;
  }
}

HttpResult HandlerDbObjectSP::handle_delete(rest::RequestContext *ctxt) {
  // no task, no status
  if (get_options().mysql_task.driver ==
      interface::Options::MysqlTask::DriverType::kNone)
    throw http::Error(HttpStatusCode::Forbidden);

  // only authenticated users can start async tasks
  auto user_id = get_user_id(ctxt, true);
  auto session = get_session(ctxt, collector::kMySQLConnectionUserdataRW);
  std::string task_id =
      get_path_after_object_name(endpoint_, ctxt->request->get_uri());
  if (task_id.empty()) throw http::Error(HttpStatusCode::NotFound);

  database::QueryRestMysqlTask::kill_task(session.get(), user_id, task_id);

  return {"{}"};
}

uint32_t HandlerDbObjectSP::get_access_rights() const {
  return HandlerDbObjectTable::get_access_rights() &
         (mrs::database::entry::Operation::valueRead |
          mrs::database::entry::Operation::valueCreate |
          mrs::database::entry::Operation::valueUpdate |
          mrs::database::entry::Operation::valueDelete);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
