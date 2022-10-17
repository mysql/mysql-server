/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_object.h"

#include <string_view>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "helper/json/rapid_json_to_text.h"
#include "helper/media_detector.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/database/query_rest_table.h"
#include "mrs/database/query_rest_table_insert.h"
#include "mrs/database/query_rest_table_single_row.h"
#include "mrs/http/url.h"
#include "mrs/rest/request_context.h"

IMPORT_LOG_FUNCTIONS()

namespace {

using MemberIterator = rapidjson::Document::MemberIterator;
using sqlstring = mysqlrouter::sqlstring;

class JsonKeyIterator
    : public sqlstring::CustomContainerIterator<MemberIterator,
                                                JsonKeyIterator> {
 public:
  using CustomContainerIterator::CustomContainerIterator;

  std::string operator*() {
    return std::string{it_->name.GetString(), it_->name.GetStringLength()};
  }
};

class JsonValueIterator
    : public sqlstring::CustomContainerIterator<MemberIterator,
                                                JsonValueIterator> {
 public:
  using CustomContainerIterator::CustomContainerIterator;

  std::string operator*() {
    std::string result;
    helper::json::rapid_json_to_text(&it_->value, result);
    return result;
  }
};

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

using Result = Handler::Result;

// TODO(lkotula): We should remove AuthManager from here, and Route should
// return supported Authentication methods for given service (Shouldn't be in
// review)
HandlerObject::HandlerObject(Route *route,
                             mrs::interface::AuthorizeManager *auth_manager)
    : Handler(route->get_rest_url(), route->get_rest_path(),
              route->get_options(), auth_manager),
      route_{route} {}

void HandlerObject::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

Result HandlerObject::handle_get(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  auto path = requests_uri.get_path();
  log_debug("Object::handle_get '%s'", path.c_str());
  auto last_path =
      http::Url::extra_path_element(route_->get_rest_path_raw(), path);
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto columns = route_->get_cached_columnes();

  http::Url uri_param(requests_uri);

  auto it_f = uri_param.is_query_parameter("f");
  auto it_raw = uri_param.is_query_parameter("raw");

  if (it_f) {
    auto filter_columns = mysql_harness::split_string(
        uri_param.get_query_parameter("f"), ',', false);
    columns.erase(std::remove_if(columns.begin(), columns.end(),
                                 [&filter_columns](auto &item) {
                                   return std::find(filter_columns.begin(),
                                                    filter_columns.end(),
                                                    item.name) ==
                                          filter_columns.end();
                                 }),
                  columns.end());
  }

  std::string raw_value = it_raw ? uri_param.get_query_parameter("raw") : "";

  if (columns.empty()) throw http::Error(HttpStatusCode::BadRequest);
  if (!raw_value.empty() && columns.size() != 1) {
    throw http::Error(HttpStatusCode::BadRequest);
  }

  if (last_path.empty()) {
    uint32_t offset = 0;
    uint32_t limit = route_->get_on_page();
    http::Url::parse_offset_limit(uri_param.parameters_, &offset, &limit);

    if (raw_value.empty()) {
      database::QueryRestTable rest;
      static const std::string empty;
      auto row_ownershop_user_id =
          route_->get_user_row_ownership().user_ownership_enforced
              ? &ctxt->user.user_id
              : nullptr;

      rest.query_entries(
          session.get(), columns, route_->get_schema_name(),
          route_->get_object_name(), offset, limit, route_->get_rest_url(),
          route_->get_cached_primary(), route_->get_on_page() == limit,
          route_->get_user_row_ownership(), row_ownershop_user_id,
          route_->get_group_row_ownership(), ctxt->user.groups,
          uri_param.get_query_parameter("q"));

      return std::move(rest.response);
    }

    if (limit != 1) throw http::Error(HttpStatusCode::BadRequest);

    database::QueryRestSPMedia rest;

    rest.query_entries(session.get(), columns[0].name,
                       route_->get_schema_name(), route_->get_object_name(),
                       limit, offset);

    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);

    return {std::move(rest.response), detected_type};
  }

  if (!route_->get_cached_primary().empty()) {
    if (raw_value.empty()) {
      database::QueryRestTableSingleRow rest;
      rest.query_entries(session.get(), columns, route_->get_schema_name(),
                         route_->get_object_name(),
                         route_->get_cached_primary(), last_path,
                         route_->get_rest_url());

      return std::move(rest.response);
    }

    database::QueryRestSPMedia rest;

    rest.query_entries(session.get(), columns[0].name,
                       route_->get_schema_name(), route_->get_object_name(),
                       route_->get_cached_primary(), last_path);

    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);

    return {std::move(rest.response), detected_type};
  }

  // TODO(lkotula): Return proper error. (Shouldn't be in review)
  throw http::Error(HttpStatusCode::InternalError);
}  // namespace rest

Result HandlerObject::handle_post([[maybe_unused]] rest::RequestContext *ctxt,
                                  const std::vector<uint8_t> &document) {
  auto &requests_uri = ctxt->request->get_uri();
  log_debug("Object::handle_post '%s'", requests_uri.get_path().c_str());

  rapidjson::Document json_doc;
  json_doc.Parse((const char *)document.data(), document.size());

  // TODO(lkotula): return error msg ? (Shouldn't be in review)
  if (json_doc.HasParseError()) {
    throw http::Error(HttpStatusCode::BadRequest);
  }

  if (json_doc.GetType() != rapidjson::kObjectType)
    throw http::Error(HttpStatusCode::BadRequest);

  database::QueryRestObjectInsert insert;
  const auto &columns = route_->get_cached_columnes();
  std::string pk_value = "LAST_INSERT_ID()";

  auto it = json_doc.FindMember(route_->get_cached_primary().c_str());
  if (it != json_doc.MemberEnd()) {
    pk_value.assign((*it).name.GetString(), (*it).name.GetStringLength());
  }

  log_debug("level1 %s", (ctxt->user.has_user_id ? "yes" : "no"));
  if (route_->get_user_row_ownership().user_ownership_enforced) {
    if (!ctxt->user.has_user_id)
      throw http::Error(HttpStatusCode::Unauthorized);

    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();
    auto &key = route_->get_user_row_ownership().user_ownership_column;
    json_doc.RemoveMember(key.c_str());
    json_doc.AddMember(rapidjson::StringRef(key.c_str(), key.length()),
                       rapidjson::Value(ctxt->user.user_id), allocator);
  }

  // TODO(lkotula): Step1. Remember column types and look at json-type.
  // Step2. Choose best conversions for both types or return an error.(Shouldn't
  // be in review)
  auto keys_iterators = JsonKeyIterator::from_iterators(json_doc.MemberBegin(),
                                                        json_doc.MemberEnd());
  auto values_iterators = JsonValueIterator::from_iterators(
      json_doc.MemberBegin(), json_doc.MemberEnd());

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  insert.execute(session.get(), route_->get_schema_name(),
                 route_->get_object_name(), keys_iterators, values_iterators);

  if (!route_->get_cached_primary().empty()) {
    database::QueryRestTableSingleRow fetch_one;

    fetch_one.query_entries(session.get(), columns, route_->get_schema_name(),
                            route_->get_object_name(),
                            route_->get_cached_primary(), pk_value,
                            route_->get_rest_url());
    return std::move(fetch_one.response);
  }

  // TODO(lkotula): return proper error ! (Shouldn't be in review)
  return {};
}

Result HandlerObject::handle_delete([
    [maybe_unused]] rest::RequestContext *ctxt) {
  // TODO(lkotula): Implement me (Shouldn't be in review)
  throw http::Error(HttpStatusCode::NotImplemented);
}

Result HandlerObject::handle_put([[maybe_unused]] rest::RequestContext *ctxt) {
  // TODO(lkotula): Implement me (Shouldn't be in review)
  throw http::Error(HttpStatusCode::NotImplemented);
}

Handler::Authorization HandlerObject::requires_authentication() const {
  return route_->requires_authentication() ? Authorization::kCheck
                                           : Authorization::kNotNeeded;
}

uint64_t HandlerObject::get_service_id() const {
  return route_->get_service_id();
}

uint64_t HandlerObject::get_db_object_id() const { return route_->get_id(); }

uint64_t HandlerObject::get_schema_id() const {
  return route_->get_schema()->get_id();
}

uint32_t HandlerObject::get_access_rights() const {
  return route_->get_access();
}

}  // namespace rest
}  // namespace mrs
