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

#include "mrs/rest/handler_sp.h"

#include <string>

#include "mysql/harness/logging/logging.h"

#include <helper/container/generic.h>
#include "helper/http/url.h"
#include "helper/json/jvalue.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "helper/mysql_numeric_value.h"
#include "mrs/database/query_rest_sp.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"
#include "mrs/router_observation_entities.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

namespace {

// CLANG doesn't allow capture, already captured variable.
// Instead using lambda let use class (llvm-issue #48582).
class CompareFieldName {
 public:
  CompareFieldName(const std::string &k) : key_{k} {}

  bool operator()(const mrs::database::entry::Field &f) const {
    return f.name == key_;
  }

 private:
  const std::string &key_;
};

}  // namespace

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

HttpResult HandlerSP::handle_delete([
    [maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

enum_field_types to_mysql_type(mrs::database::entry::Field::DataType pdt) {
  using Pdt = mrs::database::entry::Field::DataType;
  switch (pdt) {
    case Pdt::typeString:
      return MYSQL_TYPE_STRING;
    case Pdt::typeInt:
      return MYSQL_TYPE_LONG;
    case Pdt::typeDouble:
      return MYSQL_TYPE_DOUBLE;
    case Pdt::typeBoolean:
      return MYSQL_TYPE_BOOL;
    case Pdt::typeLong:
      return MYSQL_TYPE_LONGLONG;
    case Pdt::typeTimestamp:
      return MYSQL_TYPE_TIMESTAMP;

    default:
      return MYSQL_TYPE_NULL;
  }
}

std::string to_string(rapidjson::Value *v) {
  if (v->IsString()) {
    return std::string{v->GetString(), v->GetStringLength()};
  }

  return helper::json::to_string(v);
}

using DataType = mrs::database::entry::Field::DataType;

mysqlrouter::sqlstring to_sqlstring(const std::string &value, DataType type) {
  using namespace helper;
  auto v = get_type_inside_text(value);
  switch (type) {
    case DataType::typeBoolean:
      if (kDataInteger == v) return mysqlrouter::sqlstring{value.c_str()};
      return mysqlrouter::sqlstring("?") << value;

    case DataType::typeDouble:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case DataType::typeInt:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case DataType::typeLong:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case DataType::typeString:
      return mysqlrouter::sqlstring("?") << value;

    case DataType::typeTimestamp:
      return mysqlrouter::sqlstring("?") << value;
      break;
  }

  assert(nullptr && "Shouldn't happen");
  return {};
}

static HttpResult handler_mysqlerror(const mysqlrouter::MySQLSession::Error &e,
                                     database::QueryRestSP *db) {
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

HandlerSP::HandlerSP(Route *r, mrs::interface::AuthorizeManager *auth_manager)
    : Handler{r->get_rest_url(), r->get_rest_path(), r->get_options(),
              auth_manager},
      route_{r},
      auth_manager_{auth_manager} {}

HttpResult HandlerSP::handle_put([[maybe_unused]] rest::RequestContext *ctxt) {
  using namespace std::string_literals;
  using namespace helper::json::sql;

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto &input_buffer = ctxt->request->get_input_buffer();
  // TODO(lkotula): The API doesn't have input buffer. (Shouldn't be in review)
  auto size = input_buffer.length();
  auto request_body = input_buffer.pop_front(size);
  rapidjson::Document doc;
  doc.Parse(reinterpret_cast<const char *>(request_body.data()),
            request_body.size());

  if (!doc.IsObject()) throw http::Error(HttpStatusCode::BadRequest);

  auto &rs = route_->get_parameters();
  auto &p = rs.input_parameters.fields;
  for (auto el : helper::json::member_iterator(doc)) {
    auto key = el.first;
    const database::entry::Field *param;
    if (!helper::container::get_ptr_if(
            p, [key](auto &v) { return v.name == key; }, &param)) {
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not allowed parameter:"s + key);
    }
  }

  std::string result;
  std::vector<enum_field_types> variables;
  auto &ownership = route_->get_user_row_ownership();
  for (auto &el : p) {
    if (!result.empty()) result += ",";

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == el.bind_name)) {
      result += to_sqlstring(ctxt->user.user_id).str();
    } else if (el.mode == mrs::database::entry::Field::Mode::modeIn) {
      auto it = doc.FindMember(el.name.c_str());
      if (it == doc.MemberEnd())
        throw http::Error(HttpStatusCode::BadRequest,
                          "Parameter not set:"s + el.name);
      mysqlrouter::sqlstring sql("?");
      sql << it->value;
      result += sql.str();
    } else {
      result += "?";
      variables.push_back(to_mysql_type(el.data_type));
    }
  }

  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reseted.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  database::QueryRestSP db;
  try {
    db.query_entries(session.get(), route_->get_schema_name(),
                     route_->get_object_name(), route_->get_rest_url(),
                     route_->get_user_row_ownership().user_ownership_column,
                     result.c_str(), variables, rs);

    Counter<kEntityCounterRestReturnedItems>::increment(db.items);
    Counter<kEntityCounterRestAffectedItems>::increment(
        session->affected_rows());

  } catch (const mysqlrouter::MySQLSession::Error &e) {
    return handler_mysqlerror(e, &db);
  }
  return {std::move(db.response)};
}

HttpResult HandlerSP::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

void HandlerSP::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerSP::handle_get([[maybe_unused]] rest::RequestContext *ctxt) {
  using namespace std::string_literals;

  Url::Keys keys;
  Url::Values values;

  auto &requests_uri = ctxt->request->get_uri();
  const auto &query_kv = requests_uri.get_query_elements();

  auto &p = route_->get_parameters();
  auto &pf = p.input_parameters.fields;
  for (const auto &[key, _] : query_kv) {
    const database::entry::Field *param;
    CompareFieldName search_for(key);
    if (!helper::container::get_ptr_if(pf, search_for, &param)) {
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not allowed parameter:"s + key);
    }
  }

  //  for (auto &el : pf) {
  //    if (el.mode != mrs::database::entry::Field::modeIn)
  //      throw http::Error(
  //          HttpStatusCode::BadRequest,
  //          "Only 'in' parameters allowed, '"s + el.name + "' is output.");
  //  }

  std::string result;
  std::vector<enum_field_types> variables;
  auto &ownership = route_->get_user_row_ownership();
  for (auto &el : pf) {
    if (!result.empty()) result += ",";

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == el.bind_name)) {
      result += to_sqlstring(ctxt->user.user_id).str();
    } else if (el.mode == mrs::database::entry::Field::Mode::modeIn) {
      auto it = query_kv.find(el.name);
      if (query_kv.end() == it)
        throw http::Error(HttpStatusCode::BadRequest,
                          "Parameter not set:"s + el.name);
      result += to_sqlstring(it->second, el.data_type).str();
    } else {
      result += "?";
      variables.push_back(to_mysql_type(el.data_type));
    }
  }

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  // Stored procedures may change the state of the SQL session,
  // we need ensure that its going to be reseted.
  // Set as dirty, directly before executing queries.
  session.set_dirty();

  const auto format = route_->get_format();
  log_debug("HandlerSP::handle_get start format=%i", (int)format);

  if (format == Route::kFeed) {
    log_debug("HandlerSP::handle_get - generating feed response");
    database::QueryRestSP db;
    try {
      db.query_entries(session.get(), route_->get_schema_name(),
                       route_->get_object_name(), route_->get_rest_url(),
                       route_->get_user_row_ownership().user_ownership_column,
                       result.c_str(), variables, p,
                       get_options().result.stored_procedure_nest_resultsets);

      Counter<kEntityCounterRestReturnedItems>::increment(db.items);
      Counter<kEntityCounterRestAffectedItems>::increment(
          session->affected_rows());
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      return handler_mysqlerror(e, &db);
    }

    return {std::move(db.response)};
  }

  database::QueryRestSPMedia db;

  db.query_entries(session.get(), route_->get_schema_name(),
                   route_->get_object_name(), result.c_str());

  Counter<kEntityCounterRestReturnedItems>::increment(db.items);
  Counter<kEntityCounterRestAffectedItems>::increment(session->affected_rows());

  auto media_type = route_->get_media_type();

  if (media_type.auto_detect) {
    log_debug("HandlerSP::handle_get - autodetection response");
    helper::MediaDetector md;
    auto detected_type = md.detect(db.response);

    return {std::move(db.response), detected_type};
  }

  if (media_type.force_type) {
    return {std::move(db.response), media_type.force_type.value()};
  }

  return {std::move(db.response), helper::MediaType::typeUnknownBinary};
}

Handler::Authorization HandlerSP::requires_authentication() const {
  return route_->requires_authentication() ? Authorization::kCheck
                                           : Authorization::kNotNeeded;
}

UniversalId HandlerSP::get_service_id() const {
  return route_->get_service_id();
}

UniversalId HandlerSP::get_db_object_id() const { return route_->get_id(); }

UniversalId HandlerSP::get_schema_id() const {
  return route_->get_schema()->get_id();
}

uint32_t HandlerSP::get_access_rights() const {
  return Route::kRead | Route::kUpdate;
}

}  // namespace rest
}  // namespace mrs
