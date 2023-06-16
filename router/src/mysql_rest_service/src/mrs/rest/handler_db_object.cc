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

#include "mrs/rest/handler_db_object.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "helper/container/generic.h"
#include "helper/http/url.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "helper/mysql_numeric_value.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/database/query_rest_table.h"
#include "mrs/database/query_rest_table_single_row.h"
#include "mrs/database/query_rest_table_updater.h"
#include "mrs/http/error.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"
#include "mrs/router_observation_entities.h"

IMPORT_LOG_FUNCTIONS()

namespace {

using JObject = rapidjson::Document::Object;
using MemberIterator = rapidjson::Document::MemberIterator;
using UserId = mrs::database::entry::AuthUser::UserId;
using RowUserOwnership = mrs::database::entry::RowUserOwnership;
using sqlstring = mysqlrouter::sqlstring;
using SqlStrings = std::vector<sqlstring>;
using Url = helper::http::Url;
using rapidjson::StringRef;

mysqlrouter::sqlstring rest_param_to_sql_value(
    const mrs::database::entry::Column &col, const std::string &value) {
  using helper::get_type_inside_text;
  using helper::JsonType;
  if (value.empty()) return {};

  switch (col.type) {
    case mrs::database::entry::ColumnType::INTEGER:
    case mrs::database::entry::ColumnType::DOUBLE: {
      auto type = get_type_inside_text(value);
      if (type == helper::kDataInteger || type == helper::kDataFloat) {
        mysqlrouter::sqlstring result{value.c_str()};
        return result;
      }
      break;
    }
    case mrs::database::entry::ColumnType::BOOLEAN: {
      auto type = get_type_inside_text(value);
      if (helper::kDataInteger == type) {
        if (atoi(value.c_str()) > 0) return mysqlrouter::sqlstring{"true"};
        return mysqlrouter::sqlstring{"false"};
      }
      auto v = mysql_harness::make_lower(value);
      if (v == "true") return mysqlrouter::sqlstring{"true"};

      return mysqlrouter::sqlstring{"false"};
    }
    case mrs::database::entry::ColumnType::BINARY: {
      mysqlrouter::sqlstring result{"FROM_BASE64(?)"};
      result << value;
      return result;
    }
    case mrs::database::entry::ColumnType::GEOMETRY: {
      mysqlrouter::sqlstring result{"ST_GeomFromGeoJSON(?)"};
      result << value;
      return result;
    }
    case mrs::database::entry::ColumnType::STRING: {
      mysqlrouter::sqlstring result{"?"};
      result << value;
      return result;
    }
    case mrs::database::entry::ColumnType::JSON: {
      mysqlrouter::sqlstring result{"CAST(? AS JSON)"};
      result << value;
      return result;
    }
    case mrs::database::entry::ColumnType::UNKNOWN:
      return {};
  }

  mysqlrouter::sqlstring result{"?"};
  result << value;
  return result;
}

}  // namespace

namespace mrs {
namespace rest {

using CachedObject = collector::MysqlCacheManager::CachedObject;

static CachedObject get_session(::mysqlrouter::MySQLSession *,
                                collector::MysqlCacheManager *cache_manager) {
  //  if (session) {
  //    log_debug("Reusing SQL session");
  //    return CachedObject(nullptr, session);
  //  }

  return cache_manager->get_instance(collector::kMySQLConnectionUserdata);
}

using HttpResult = Handler::HttpResult;

// TODO(lkotula): We should remove AuthManager from here, and Route should
// return supported Authentication methods for given service (Shouldn't be in
// review)
HandlerDbObject::HandlerDbObject(Route *route,
                                 mrs::interface::AuthorizeManager *auth_manager)
    : Handler(route->get_rest_url(), route->get_rest_path(),
              route->get_options(), auth_manager),
      route_{route} {}

void HandlerDbObject::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

database::PrimaryKeyColumnValues HandlerDbObject::get_rest_pk_parameter(
    std::shared_ptr<database::entry::Object> object,
    const HttpUri &requests_uri) {
  auto id = get_path_after_object_name(requests_uri);

  if (id.empty()) return {};

  auto pk_values = mysql_harness::split_string(id, ',', true);
  auto pk_columns = object->get_base_table()->primary_key();

  if (pk_columns.empty()) {
    throw std::logic_error("Table has no primary key");
  }

  if (pk_values.size() != pk_columns.size()) {
    throw http::Error(HttpStatusCode::NotFound, "Invalid ID requested");
  }

  mrs::database::PrimaryKeyColumnValues pk;
  for (size_t i = 0; i < pk_columns.size(); i++) {
    pk[pk_columns[i]->name] =
        rest_param_to_sql_value(*pk_columns[i], pk_values[i]);
  }

  return pk;
}

HttpResult HandlerDbObject::handle_get(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto object = route_->get_cached_object();
  database::ObjectFieldFilter field_filter;
  std::optional<std::string> target_field;
  auto pk = get_rest_pk_parameter(object, requests_uri);

  Url uri_param(requests_uri);

  auto it_f = uri_param.is_query_parameter("f");
  auto it_raw = uri_param.is_query_parameter("raw");

  if (it_f) {
    auto filter = mysql_harness::split_string(
        uri_param.get_query_parameter("f"), ',', false);

    try {
      field_filter =
          database::ObjectFieldFilter::from_url_filter(*object, filter);
    } catch (const std::exception &e) {
      throw http::Error(HttpStatusCode::BadRequest, e.what());
    }

    if (filter.size() == 1) target_field = filter.front();
  } else {
    field_filter = database::ObjectFieldFilter::from_object(*object);
  }

  std::string raw_value = it_raw ? uri_param.get_query_parameter("raw") : "";

  if (!raw_value.empty() && !target_field.has_value()) {
    throw http::Error(HttpStatusCode::BadRequest);
  }

  if (pk.empty()) {
    uint32_t offset = 0;
    uint32_t limit = route_->get_on_page();
    Url::parse_offset_limit(uri_param.parameters_, &offset, &limit);

    if (raw_value.empty()) {
      database::QueryRestTable rest;
      static const std::string empty;
      rest.query_entries(session.get(), object, field_filter, offset, limit,
                         route_->get_rest_url(), route_->get_on_page() == limit,
                         row_ownership_info(ctxt, object),
                         uri_param.get_query_parameter("q"), true);

      Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

      return std::move(rest.response);
    }

    if (limit != 1) throw http::Error(HttpStatusCode::BadRequest);

    database::QueryRestSPMedia rest;

    rest.query_entries(session.get(), *target_field, route_->get_schema_name(),
                       route_->get_object_name(), limit, offset);

    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);
    Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

    return {std::move(rest.response), detected_type};
  } else {
    if (raw_value.empty()) {
      database::QueryRestTableSingleRow rest;
      log_debug(
          "Rest select single row %s",
          database::format_key(object->get_base_table(), pk).str().c_str());
      rest.query_entries(session.get(), object, field_filter, pk,
                         route_->get_rest_url(), true);

      if (rest.response.empty()) throw http::Error(HttpStatusCode::NotFound);
      Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

      return std::move(rest.response);
    }

    database::QueryRestSPMedia rest;

    rest.query_entries(session.get(), *target_field, route_->get_schema_name(),
                       route_->get_object_name(), pk);

    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);

    return {std::move(rest.response), detected_type};
  }

  // TODO(lkotula): Return proper error. (Shouldn't be in review)
  throw http::Error(HttpStatusCode::InternalError);
}

/// Post is insert
HttpResult HandlerDbObject::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    const std::vector<uint8_t> &document) {
  using namespace helper::json::sql;
  rapidjson::Document json_doc;
  auto object = route_->get_cached_object();

  auto last_path = get_path_after_object_name(ctxt->request->get_uri());

  if (!last_path.empty())
    throw http::Error(HttpStatusCode::BadRequest,
                      "Full object must be specified in the request body. "
                      "Setting ID, from the URL is not supported.");

  json_doc.Parse((const char *)document.data(), document.size());

  // TODO(lkotula): return error msg ? (Shouldn't be in review)
  if (json_doc.HasParseError() || !json_doc.IsObject())
    throw http::Error(HttpStatusCode::BadRequest,
                      "Invalid JSON document inside the HTTP request.");

  if (json_doc.GetType() != rapidjson::kObjectType)
    throw http::Error(HttpStatusCode::BadRequest,
                      "Invalid JSON document inside the HTTP request, must be "
                      "an JSON object.");

  database::TableUpdater updater(object, row_ownership_info(ctxt, object));

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto pk = updater.handle_post(session.get(), json_doc);

  Counter<kEntityCounterRestAffectedItems>::increment();

  if (!pk.empty()) {
    database::QueryRestTableSingleRow fetch_one;

    fetch_one.query_entries(session.get(), object,
                            database::ObjectFieldFilter::from_object(*object),
                            pk, route_->get_rest_url(), true);
    Counter<kEntityCounterRestReturnedItems>::increment(fetch_one.items);

    return std::move(fetch_one.response);
  }

  // TODO(lkotula): return proper error ! (Shouldn't be in review)
  return {};
}

std::string HandlerDbObject::get_path_after_object_name(
    const HttpUri &requests_uri) {
  auto path = requests_uri.get_path();
  auto last_path = Url::extra_path_element(route_->get_rest_path_raw(), path);
  return last_path;
}

std::string HandlerDbObject::get_rest_query_parameter(HttpUri &requests_uri) {
  Url uri_param(requests_uri);
  auto query = uri_param.get_query_parameter("q");
  return query;
}

HttpResult HandlerDbObject::handle_delete(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  auto last_path = get_path_after_object_name(requests_uri);
  auto object = route_->get_cached_object();
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());

  uint64_t count = 0;

  mrs::database::TableUpdater rest(object, row_ownership_info(ctxt, object));

  if (!last_path.empty()) {
    auto pk = get_rest_pk_parameter(object, requests_uri);

    count = rest.handle_delete(session.get(), pk);
  } else {
    auto query = get_rest_query_parameter(requests_uri);

    database::FilterObjectGenerator fog(object, false);
    fog.parse(helper::json::text_to_document(query));

    auto result = fog.get_result();
    if (result.is_empty())
      throw std::runtime_error("Filter must contain valid JSON object.");
    if (fog.has_order())
      throw std::runtime_error(
          "Filter must not contain ordering informations.");

    count = rest.handle_delete(session.get(), fog);
  }

  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    obj->member_add_value("itemsDeleted", count);
  }
  return {stt.get_result(), helper::MediaType::typeJson};
}

// Update, with insert possibility
HttpResult HandlerDbObject::handle_put(rest::RequestContext *ctxt) {
  using namespace helper::json::sql;  // NOLINT(build/namespaces)
  auto &requests_uri = ctxt->request->get_uri();
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto document = input_buffer.pop_front(size);
  auto object = route_->get_cached_object();

  auto pk = get_rest_pk_parameter(object, requests_uri);

  rapidjson::Document json_doc;

  database::TableUpdater updater(object, row_ownership_info(ctxt, object));

  json_doc.Parse((const char *)document.data(), document.size());

  // TODO(lkotula): return error msg ? (Shouldn't be in review)
  if (json_doc.HasParseError() || !json_doc.IsObject()) {
    throw http::Error(HttpStatusCode::BadRequest,
                      "Invalid JSON document inside the HTTP request.");
  }

  if (json_doc.GetType() != rapidjson::kObjectType)
    throw http::Error(HttpStatusCode::BadRequest,
                      "Invalid JSON document inside the HTTP request, must be "
                      "an JSON object.");

  if (route_->get_user_row_ownership().user_ownership_enforced) {
    if (!ctxt->user.has_user_id)
      throw http::Error(HttpStatusCode::Unauthorized);
  }

  auto json_obj = json_doc.GetObject();
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());

  updater.handle_put(session.get(), json_doc, pk);

  Counter<kEntityCounterRestAffectedItems>::increment(updater.affected());

  database::QueryRestTableSingleRow fetch_one;

  fetch_one.query_entries(session.get(), object,
                          database::ObjectFieldFilter::from_object(*object), pk,
                          route_->get_rest_url(), true);

  Counter<kEntityCounterRestAffectedItems>::increment(fetch_one.items);
  return std::move(fetch_one.response);
}

Handler::Authorization HandlerDbObject::requires_authentication() const {
  return route_->requires_authentication() ? Authorization::kCheck
                                           : Authorization::kNotNeeded;
}

UniversalId HandlerDbObject::get_service_id() const {
  return route_->get_service_id();
}

UniversalId HandlerDbObject::get_db_object_id() const {
  return route_->get_id();
}

UniversalId HandlerDbObject::get_schema_id() const {
  return route_->get_schema()->get_id();
}

uint32_t HandlerDbObject::get_access_rights() const {
  return route_->get_access();
}

mrs::database::ObjectRowOwnership HandlerDbObject::row_ownership_info(
    rest::RequestContext *ctxt,
    std::shared_ptr<database::entry::Object> object) const {
  if (route_->get_user_row_ownership().user_ownership_enforced &&
      !ctxt->user.has_user_id)
    throw http::Error(HttpStatusCode::Unauthorized);

  return mrs::database::ObjectRowOwnership{
      object->get_base_table(), &route_->get_user_row_ownership(),
      ctxt->user.has_user_id ? ctxt->user.user_id : std::optional<UserId>(),
      route_->get_group_row_ownership(), ctxt->user.groups};
}

}  // namespace rest
}  // namespace mrs
