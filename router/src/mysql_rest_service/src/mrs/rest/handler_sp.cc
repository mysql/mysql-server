/*
 Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
#include "mysqlrouter/http_request.h"

#include "helper/container/generic.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "mrs/database/query_rest_sp.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/http/url.h"
#include "mrs/rest/request_context.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using Result = mrs::rest::Handler::Result;
using CachedObject = collector::MysqlCacheManager::CachedObject;

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

  return cache_manager->get_instance(collector::kMySQLConnectionUserdata);
}

Result HandlerSP::handle_delete([[maybe_unused]] rest::RequestContext *ctxt) {
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
Result HandlerSP::handle_put([[maybe_unused]] rest::RequestContext *ctxt) {
  using namespace std::string_literals;

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto data = input_buffer.pop_front(size);
  rapidjson::Document doc;
  doc.Parse(reinterpret_cast<const char *>(&data[0]), data.size());

  if (!doc.IsObject()) throw http::Error(HttpStatusCode::BadRequest);

  auto &p = route_->get_parameters();
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
      result += (mysqlrouter::sqlstring("?") << to_string(&it->value)).str();
    } else {
      result += "?";
      variables.push_back(to_mysql_type(el.data_type));
    }
  }

  database::QueryRestSP db;

  db.query_entries(session.get(), route_->get_schema_name(),
                   route_->get_object_name(), route_->get_rest_url(),
                   route_->get_user_row_ownership().user_ownership_column,
                   result.c_str(), variables);

  return {std::move(db.response)};
}

Result HandlerSP::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

void HandlerSP::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

Result HandlerSP::handle_get([[maybe_unused]] rest::RequestContext *ctxt) {
  using namespace std::string_literals;

  http::Url::Keys keys;
  http::Url::Values values;

  auto &requests_uri = ctxt->request->get_uri();
  http::Url::parse_query(requests_uri.get_query().c_str(), &keys, &values);

  auto &p = route_->get_parameters();
  for (auto key : keys) {
    const database::entry::Field *param;
    if (!helper::container::get_ptr_if(
            p, [key](auto &v) { return v.name == key; }, &param)) {
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not allowed parameter:"s + key);
    }
  }

  for (auto &el : p) {
    if (el.mode != mrs::database::entry::Field::modeIn)
      throw http::Error(
          HttpStatusCode::BadRequest,
          "Only 'in' parameters allowed, '"s + el.name + "' is output.");
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
      auto idx = helper::container::index_of(keys, el.name);
      if (idx == -1)
        throw http::Error(HttpStatusCode::BadRequest,
                          "Parameter not set:"s + el.name);
      result += (mysqlrouter::sqlstring("?") << values[idx]).str();
    } else {
      result += "?";
      variables.push_back(to_mysql_type(el.data_type));
    }
  }

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());

  const auto format = route_->get_format();
  log_debug("HandlerSP::handle_get start format=%i", (int)format);

  if (format == Route::kFeed) {
    log_debug("HandlerSP::handle_get - generating feed response");
    database::QueryRestSP db;

    db.query_entries(session.get(), route_->get_schema_name(),
                     route_->get_object_name(), route_->get_rest_url(),
                     route_->get_user_row_ownership().user_ownership_column,
                     result.c_str(), variables);

    return {std::move(db.response)};
  }

  database::QueryRestSPMedia db;

  db.query_entries(session.get(), route_->get_schema_name(),
                   route_->get_object_name(), result.c_str());

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
