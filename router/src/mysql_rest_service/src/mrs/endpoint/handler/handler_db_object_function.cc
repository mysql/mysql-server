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

#include "mrs/endpoint/handler/handler_db_object_function.h"

#include <string>

#include "helper/json/jvalue.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "helper/sqlstring_utils.h"
#include "mrs/database/helper/sp_function_query.h"
#include "mrs/database/query_rest_function.h"
#include "mrs/database/query_rest_task.h"
#include "mrs/database/query_rest_task_status.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/routine_utilities.h"
#include "mrs/http/error.h"
#include "mrs/monitored/gtid_functions.h"
#include "mrs/rest/request_context.h"
#include "mrs/router_observation_entities.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/container/generic.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = mrs::rest::Handler::HttpResult;
using CachedObject = collector::MysqlCacheManager::CachedObject;
using Url = helper::http::Url;
using Authorization = mrs::rest::Handler::Authorization;

// static std::string to_string(
//    const std::string &value,
//    mrs::database::entry::Parameter::ParameterDataType dt) {
//  switch (dt) {
//    case mrs::database::entry::Parameter::parameterString: {
//      rapidjson::Document d;
//      d.SetString(value.c_str(), value.length());
//      return helper::json::to_string(&d);
//    }
//    case mrs::database::entry::Parameter::parameterInt:
//      return value;
//    case mrs::database::entry::Parameter::parameterDouble:
//      return value;
//    case mrs::database::entry::Parameter::parameterBoolean:
//      return value;
//    case mrs::database::entry::Parameter::parameterLong:
//      return value;
//    case mrs::database::entry::Parameter::parameterTimestamp: {
//      rapidjson::Document d;
//      d.SetString(value.c_str(), value.length());
//      return helper::json::to_string(&d);
//    }
//    default:
//      return "";
//  }
//
//  return "";
//}

HandlerDbObjectFunction::HandlerDbObjectFunction(
    std::weak_ptr<DbObjectEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager,
    mrs::GtidManager *gtid_manager, collector::MysqlCacheManager *cache,
    mrs::ResponseCache *response_cache,
    mrs::database::SlowQueryMonitor *slow_monitor,
    mrs::database::MysqlTaskMonitor *task_monitor)
    : HandlerDbObjectTable{endpoint, auth_manager,   gtid_manager,
                           cache,    response_cache, slow_monitor},
      task_monitor_(task_monitor) {}

HttpResult HandlerDbObjectFunction::handle_put(rest::RequestContext *ctxt) {
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto request_body = input_buffer.pop_front(size);

  return handle_post(ctxt, request_body);
}

HttpResult HandlerDbObjectFunction::handle_post(
    rest::RequestContext *ctxt, const std::vector<uint8_t> &document) {
  using namespace std::string_literals;

  rapidjson::Document doc;
  doc.Parse(reinterpret_cast<const char *>(document.data()), document.size());

  if (!doc.IsObject())
    throw std::invalid_argument(
        "Parameters must be encoded as fields in Json object.");

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

HttpResult HandlerDbObjectFunction::call(
    rest::RequestContext *ctxt, const helper::http::Url::Parameters &query_kv) {
  using namespace std::string_literals;

  Url::Keys keys;
  Url::Values values;

  auto obj = entry_->object_description;
  auto user_id = get_user_id(ctxt, false);

  auto sql_values = database::create_function_argument_list(
      obj.get(), query_kv, ownership_, user_id);
  auto session = get_session(ctxt, collector::kMySQLConnectionUserdataRW);
  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reset.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  assert(entry_->format != mrs::database::entry::DbObject::formatFeed &&
         "Functions may generate only single value results, thus feed is not "
         "acceptable.");
  database::QueryRestFunction db;
  try {
    if (entry_->format != mrs::database::entry::DbObject::formatMedia) {
      log_debug(
          "HandlerDbObjectFunction::handle_get - generating 'Item' response");

      slow_monitor_->execute(
          [&]() { db.query_entries(session.get(), obj, sql_values); },
          session.get(), get_options().query.timeout);

      Counter<kEntityCounterRestReturnedItems>::increment(db.items);
      Counter<kEntityCounterRestAffectedItems>::increment(
          session->affected_rows());

      database::QueryRestFunction::CustomMetadata custom_metadata;
      if (get_options().metadata.gtid && gtid_manager_) {
        auto gtid =
            mrs::monitored::get_session_tracked_gtids_for_metadata_response(
                session.get(), gtid_manager_);
        if (!gtid.empty()) {
          custom_metadata["gtid"] = gtid;
        }
      }
      db.serialize_response(custom_metadata);

      return {std::move(db.response)};
    }
    slow_monitor_->execute(
        [&]() { db.query_raw(session.get(), obj, sql_values); }, session.get(),
        get_options().query.timeout);

    log_debug("media has size:%i", (int)db.response.length());

    Counter<kEntityCounterRestReturnedItems>::increment(db.items);
    Counter<kEntityCounterRestAffectedItems>::increment(
        session->affected_rows());
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, db.get_sql_state());
  }

  if (entry_->autodetect_media_type) {
    log_debug("HandlerDbObjectFunction::handle_get - autodetection response");
    helper::MediaDetector md;
    auto detected_type = md.detect(db.response);

    return {std::move(db.response), detected_type};
  }

  if (entry_->media_type.has_value()) {
    return {std::move(db.response), entry_->media_type.value()};
  }

  return {std::move(db.response), helper::MediaType::typeUnknownBinary};
}

HttpResult HandlerDbObjectFunction::call(rest::RequestContext *ctxt,
                                         rapidjson::Document doc) {
  auto session = get_session(ctxt, collector::kMySQLConnectionUserdataRW);

  auto obj = entry_->object_description;
  auto user_id = get_user_id(ctxt, false);

  auto values = database::create_function_argument_list(obj.get(), doc,
                                                        ownership_, user_id);

  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reset.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  log_debug("HandlerDbObjectFunction::handle_put start format=%i",
            (int)entry_->format);

  database::QueryRestFunction db;
  try {
    if (entry_->format != mrs::database::entry::DbObject::formatMedia) {
      slow_monitor_->execute(
          [&]() { db.query_entries(session.get(), obj, values); },
          session.get(), get_options().query.timeout);

      database::QueryRestFunction::CustomMetadata custom_metadata;
      if (get_options().metadata.gtid && gtid_manager_) {
        auto gtid =
            mrs::monitored::get_session_tracked_gtids_for_metadata_response(
                session.get(), gtid_manager_);
        if (!gtid.empty()) {
          custom_metadata["gtid"] = gtid;
        }
      }
      db.serialize_response(custom_metadata);

      return {std::move(db.response)};
    }

    slow_monitor_->execute([&]() { db.query_raw(session.get(), obj, values); },
                           session.get(), get_options().query.timeout);
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, db.get_sql_state());
  }

  if (entry_->autodetect_media_type) {
    log_debug("HandlerDbObjectFunction::handle_get - autodetection response");
    helper::MediaDetector md;
    auto detected_type = md.detect(db.response);

    return {std::move(db.response), detected_type};
  }

  if (entry_->media_type.has_value()) {
    return {std::move(db.response), entry_->media_type.value()};
  }

  return {std::move(db.response), helper::MediaType::typeUnknownBinary};
}

HttpResult HandlerDbObjectFunction::call_async(rest::RequestContext *ctxt,
                                               rapidjson::Document doc) {
  using namespace helper::json::sql;

  PoolManagerRef pool_ref;
  // only authenticated users can start async tasks
  auto user_id = get_user_id(ctxt, true);
  auto session =
      get_session(ctxt, collector::kMySQLConnectionUserdataRW, &pool_ref);

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
            db.execute_function_at_server(
                session.get(), user_id, get_user_name(ctxt),
                user_ownership_column, schema_entry_->name, entry_->name,
                get_endpoint_url(endpoint_), get_options().mysql_task, doc,
                entry_->fields);
          else
            db.execute_function_at_router(
                std::move(session), std::move(pool_ref), user_id,
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

HttpResult HandlerDbObjectFunction::handle_get(rest::RequestContext *ctxt) {
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

    log_debug("HandlerDbObjectFunction::handle_get check task_id=%s",
              task_id.c_str());
    database::QueryRestTaskStatus db;
    db.query_status(session.get(), ctxt->request->get_uri().get_path(), user_id,
                    get_options().mysql_task, task_id);

    return {db.status, std::move(db.response), HttpResult::Type::typeJson};
  } else {
    auto &requests_uri = ctxt->request->get_uri();
    const auto &query_kv = requests_uri.get_query_elements();

    if (response_cache_) {
      auto entry =
          response_cache_->lookup_routine(ctxt->request->get_uri(), {});
      if (entry) {
        if (entry->media_type.has_value())
          return {std::string{entry->data}, entry->media_type.value()};
        else if (entry->media_type_str.has_value())
          return {std::string{entry->data}, entry->media_type_str.value()};
        else
          return {std::string{entry->data}};
      }
    }

    auto res = call(ctxt, query_kv);

    if (response_cache_ && res.status == ::http::base::status_code::Ok) {
      auto entry = response_cache_->create_routine_entry(
          ctxt->request->get_uri(), {}, res.response, res.type);
    }

    return res;
  }
}

HttpResult HandlerDbObjectFunction::handle_delete(rest::RequestContext *ctxt) {
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

uint32_t HandlerDbObjectFunction::get_access_rights() const {
  return HandlerDbObjectTable::get_access_rights() &
         (mrs::database::entry::Operation::valueRead |
          mrs::database::entry::Operation::valueCreate |
          mrs::database::entry::Operation::valueUpdate |
          mrs::database::entry::Operation::valueDelete);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
