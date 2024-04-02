/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mrs/rest/handler_function.h"

#include <string>

#include "helper/container/generic.h"
#include "helper/json/jvalue.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "mrs/database/helper/sp_function_query.h"
#include "mrs/database/query_rest_function.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"
#include "mrs/router_observation_entities.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using HttpResult = mrs::rest::Handler::HttpResult;
using CachedObject = collector::MysqlCacheManager::CachedObject;
using Url = helper::http::Url;

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
static CachedObject get_session(::mysqlrouter::MySQLSession *,
                                collector::MysqlCacheManager *cache_manager) {
  //  if (session) return CachedObject(nullptr, session);

  return cache_manager->get_instance(collector::kMySQLConnectionUserdataRW,
                                     false);
}

HandlerFunction::HandlerFunction(Route *r,
                                 mrs::interface::AuthorizeManager *auth_manager)
    : Handler{r->get_rest_url(), r->get_rest_path(), r->get_options(),
              auth_manager},
      route_{r},
      auth_manager_{auth_manager} {}

HttpResult HandlerFunction::handle_delete([
    [maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

static HttpResult handler_mysqlerror(const mysqlrouter::MySQLSession::Error &e,
                                     database::QueryRestFunction *db) {
  static const std::string k_state_with_user_defined_error = "45000";
  if (!db->get_sql_state()) throw e;

  auto sql_state = db->get_sql_state();
  log_debug("While handling SP, received a mysql-error with state: %s",
            sql_state);
  if (k_state_with_user_defined_error != sql_state) {
    throw e;
  }
  // 5000 is the offset for HTTPStatus errors,
  // Still first HTTP status begins with 100 code,
  // because of that we are validating the value
  // not against 5000, but 5100.
  if (e.code() < 5100 || e.code() >= 5600) {
    throw e;
  }
  helper::json::MapObject map{{"message", e.message()}};
  HttpResult::HttpStatus status = e.code() - 5000;
  try {
    HttpStatusCode::get_default_status_text(status);
  } catch (...) {
    throw e;
  }
  auto json = helper::json::to_string(map);
  log_debug("SP - generated custom HTTPstats + message:%s", json.c_str());
  return HttpResult(status, std::move(json), HttpResult::Type::typeJson);
}

HttpResult HandlerFunction::handle_put([
    [maybe_unused]] rest::RequestContext *ctxt) {
  using namespace std::string_literals;

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto &input_buffer = ctxt->request->get_input_buffer();
  // TODO(lkotula): New api doesn't have inputbuffer, it has string (Shouldn't
  // be in review)
  auto data = input_buffer.pop_front(input_buffer.length());

  auto obj = route_->get_cached_object();
  auto user_row_ownership = route_->get_user_row_ownership();
  auto values = database::create_function_argument_list(
      obj.get(), data, user_row_ownership,
      ctxt->user.has_user_id ? &ctxt->user.user_id : nullptr);

  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reseted.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  const auto format = route_->get_format();
  log_debug("HandlerFunction::handle_put start format=%i", (int)format);

  database::QueryRestFunction db;
  try {
    if (format != Route::kMedia) {
      db.query_entries(session.get(), obj, values);

      Counter<kEntityCounterRestReturnedItems>::increment(db.items);
      Counter<kEntityCounterRestAffectedItems>::increment(
          session->affected_rows());

      return {std::move(db.response)};
    }

    db.query_raw(session.get(), obj, values);

    Counter<kEntityCounterRestReturnedItems>::increment(db.items);
    Counter<kEntityCounterRestAffectedItems>::increment(
        session->affected_rows());
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, &db);
  }

  auto media_type = route_->get_media_type();

  if (media_type.auto_detect) {
    log_debug("HandlerFunction::handle_get - autodetection response");
    helper::MediaDetector md;
    auto detected_type = md.detect(db.response);

    return {std::move(db.response), detected_type};
  }

  if (media_type.force_type) {
    return {std::move(db.response), media_type.force_type.value()};
  }

  return {std::move(db.response), helper::MediaType::typeUnknownBinary};
}

HttpResult HandlerFunction::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

void HandlerFunction::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerFunction::handle_get([
    [maybe_unused]] rest::RequestContext *ctxt) {
  using namespace std::string_literals;

  Url::Keys keys;
  Url::Values values;

  auto &requests_uri = ctxt->request->get_uri();
  auto obj = route_->get_cached_object();
  auto user_row_ownership = route_->get_user_row_ownership();

  auto sql_values = database::create_function_argument_list(
      obj.get(), requests_uri.get_query_elements(), user_row_ownership,
      ctxt->user.has_user_id ? &ctxt->user.user_id : nullptr);
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reseted.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  const auto format = route_->get_format();

  assert(format != Route::kFeed &&
         "Functions may generate only single value results, thus feed is not "
         "acceptable.");
  database::QueryRestFunction db;
  try {
    if (format != Route::kMedia) {
      log_debug("HandlerFunction::handle_get - generating 'Item' response");
      db.query_entries(session.get(), obj, sql_values);

      Counter<kEntityCounterRestReturnedItems>::increment(db.items);
      Counter<kEntityCounterRestAffectedItems>::increment(
          session->affected_rows());

      return {std::move(db.response)};
    }

    db.query_raw(session.get(), obj, sql_values);

    log_debug("media has size:%i", (int)db.response.length());

    Counter<kEntityCounterRestReturnedItems>::increment(db.items);
    Counter<kEntityCounterRestAffectedItems>::increment(
        session->affected_rows());
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, &db);
  }

  auto media_type = route_->get_media_type();

  if (media_type.auto_detect) {
    log_debug("HandlerFunction::handle_get - autodetection response");
    helper::MediaDetector md;
    auto detected_type = md.detect(db.response);

    return {std::move(db.response), detected_type};
  }

  if (media_type.force_type) {
    return {std::move(db.response), media_type.force_type.value()};
  }

  return {std::move(db.response), helper::MediaType::typeUnknownBinary};
}

Handler::Authorization HandlerFunction::requires_authentication() const {
  return route_->requires_authentication() ? Authorization::kCheck
                                           : Authorization::kNotNeeded;
}

UniversalId HandlerFunction::get_service_id() const {
  return route_->get_service_id();
}

UniversalId HandlerFunction::get_db_object_id() const {
  return route_->get_id();
}

UniversalId HandlerFunction::get_schema_id() const {
  return route_->get_schema()->get_id();
}

uint32_t HandlerFunction::get_access_rights() const {
  return Route::kRead | Route::kUpdate;
}

}  // namespace rest
}  // namespace mrs
