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

#include "mrs/rest/handler_object.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "helper/container/generic.h"
#include "helper/http/url.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/database/query_rest_table.h"
#include "mrs/database/query_rest_table_delete.h"
#include "mrs/database/query_rest_table_insert.h"
#include "mrs/database/query_rest_table_single_row.h"
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

class SqlValueIterator
    : public sqlstring::CustomContainerIterator<SqlStrings::iterator,
                                                SqlStrings::iterator> {
  using Parent = sqlstring::CustomContainerIterator<SqlStrings::iterator,
                                                    SqlStrings::iterator>;
  using Parent::CustomContainerIterator;
};
class JsonKeyIterator
    : public sqlstring::CustomContainerIterator<MemberIterator,
                                                JsonKeyIterator> {
 public:
  JsonKeyIterator(const MemberIterator &it) : CustomContainerIterator{it} {}
  JsonKeyIterator(JsonKeyIterator &&jk)
      : CustomContainerIterator{std::move(jk.it_)} {}
  JsonKeyIterator(const JsonKeyIterator &js) : CustomContainerIterator{js} {}

  JsonKeyIterator &operator=(const JsonKeyIterator &other) {
    CustomContainerIterator::operator=(other);
    return *this;
  }

  std::string operator*() {
    return std::string{it_->name.GetString(), it_->name.GetStringLength()};
  }
};

mysqlrouter::sqlstring rest_param_to_sql_value(const std::string &value) {
  if (value.empty()) return {};

  auto it = value.begin();
  bool is_negative = ('-' == *it);
  bool is_number = isdigit(*it) || is_negative;

  mysqlrouter::sqlstring result{"?"};
  ++it;
  while (is_number && it != value.end()) {
    is_number = isdigit(*(it++));
  }

  if (is_number) {
    if (is_negative) {
      auto i = strtoll(value.c_str(), nullptr, 10);
      result << i;
    } else {
      auto i = strtoull(value.c_str(), nullptr, 10);
      result << i;
    }
  } else {
    result << value;
  }

  return result;
}

template <typename Function>
std::vector<sqlstring> create_value_container(const JObject &json_obj,
                                              const Function &f) {
  using namespace helper::json::sql;

  std::vector<sqlstring> values;
  values.reserve(json_obj.MemberCount());
  mysqlrouter::sqlstring out_value;
  log_debug("Filling array value.");
  for (auto &member : json_obj) {
    if (f(member.name.GetString(), out_value)) {
      values.push_back(out_value);
    } else {
      values.emplace_back("?") << member.value;
    }

    log_debug("Filling member: %s value: %s", member.name.GetString(),
              values.back().str().c_str());
  }
  return values;
}

class FillOwnership {
 public:
  FillOwnership(const RowUserOwnership &ruo, const UserId &uid)
      : ruo_{ruo}, uid_{uid} {}

  bool operator()(const std::string &name,
                  mysqlrouter::sqlstring &out_value) const {
    log_debug("FillOwnership::operator()");
    if (ruo_.user_ownership_enforced && ruo_.user_ownership_column == name) {
      out_value = to_sqlstring(uid_);
      log_debug("FillOwnership");
      return true;
    }
    return false;
  }

  const RowUserOwnership &ruo_;
  const UserId &uid_;
};

class FillSpecificColumn {
 public:
  FillSpecificColumn(const std::string &column_name,
                     const mysqlrouter::sqlstring &value)
      : cn_{column_name}, v_{value} {}

  bool operator()(const std::string &name,
                  mysqlrouter::sqlstring &out_value) const {
    log_debug("FillSpecificColumn::operator()");
    if (cn_ == name) {
      out_value = v_;
      log_debug("FillSpecificColumn");
      return true;
    }
    return false;
  }

  const std::string &cn_;
  const mysqlrouter::sqlstring &v_;
};

template <typename... Types>
class FillMultiple {
 public:
  FillMultiple(const Types &... types) : tuple_{types...} {}

  bool operator()(const std::string &name,
                  mysqlrouter::sqlstring &out_value) const {
    bool result;
    std::apply(
        [&result, &name, &out_value](Types const &... ta) {
          result = (... || ta(name, out_value));
        },
        tuple_);

    return result;
  }

 private:
  std::tuple<Types...> tuple_;
};

template <typename... Types>
auto fill_multiple(const Types &... t) {
  return FillMultiple<Types...>(t...);
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
HandlerObject::HandlerObject(Route *route,
                             mrs::interface::AuthorizeManager *auth_manager)
    : Handler(route->get_rest_url(), route->get_rest_path(),
              route->get_options(), auth_manager),
      route_{route} {}

void HandlerObject::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerObject::handle_get(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  mysqlrouter::sqlstring pk_value =
      rest_param_to_sql_value(get_path_after_object_name(requests_uri));
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto object = route_->get_cached_object();
  database::ObjectFieldFilter field_filter;
  std::optional<std::string> target_field;

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

  if (pk_value.str().empty()) {
    uint32_t offset = 0;
    uint32_t limit = route_->get_on_page();
    Url::parse_offset_limit(uri_param.parameters_, &offset, &limit);

    if (raw_value.empty()) {
      database::QueryRestTable rest;
      static const std::string empty;
      auto row_ownershop_user_id =
          route_->get_user_row_ownership().user_ownership_enforced
              ? &ctxt->user.user_id
              : nullptr;

      rest.query_entries(
          session.get(), object, field_filter, offset, limit,
          route_->get_rest_url(), route_->get_cached_primary().name,
          route_->get_on_page() == limit, route_->get_user_row_ownership(),
          row_ownershop_user_id, route_->get_group_row_ownership(),
          ctxt->user.groups, uri_param.get_query_parameter("q"));

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
  }

  if (!route_->get_cached_primary().name.empty()) {
    if (raw_value.empty()) {
      database::QueryRestTableSingleRow rest;
      rest.query_entries(session.get(), object, field_filter,
                         route_->get_cached_primary().name, pk_value,
                         route_->get_rest_url());

      if (rest.response.empty()) throw http::Error(HttpStatusCode::NotFound);
      Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

      return std::move(rest.response);
    }

    database::QueryRestSPMedia rest;

    rest.query_entries(session.get(), *target_field, route_->get_schema_name(),
                       route_->get_object_name(),
                       route_->get_cached_primary().name, pk_value);

    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);

    return {std::move(rest.response), detected_type};
  }

  // TODO(lkotula): Return proper error. (Shouldn't be in review)
  throw http::Error(HttpStatusCode::InternalError);
}

/// Post is insert
HttpResult HandlerObject::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    const std::vector<uint8_t> &document) {
  using namespace helper::json::sql;
  rapidjson::Document json_doc;
  auto object = route_->get_cached_object();

  // std::optional<mysqlrouter::sqlstring> expected_pk_value;
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

  if (route_->get_user_row_ownership().user_ownership_enforced &&
      !ctxt->user.has_user_id)
    throw http::Error(HttpStatusCode::Unauthorized);

  database::QueryRestObjectInsert insert;

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  auto pk = insert.execute_insert(
      session.get(), object, json_doc,
      route_->get_user_row_ownership().user_ownership_enforced
          ? route_->get_user_row_ownership().user_ownership_column
          : "",
      rapidjson::Value(ctxt->user.user_id.to_string().c_str(),
                       json_doc.GetAllocator()));

  Counter<kEntityCounterRestAffectedItems>::increment();

  if (!pk.empty()) {
    database::QueryRestTableSingleRow fetch_one;

    assert(pk.size() == 1);

    fetch_one.query_entries(session.get(), object,
                            database::ObjectFieldFilter::from_object(*object),
                            pk.begin()->first, pk.begin()->second,
                            route_->get_rest_url());
    Counter<kEntityCounterRestReturnedItems>::increment(fetch_one.items);

    return std::move(fetch_one.response);
  }

  // TODO(lkotula): return proper error ! (Shouldn't be in review)
  return {};
}

std::string HandlerObject::get_path_after_object_name(HttpUri &requests_uri) {
  auto path = requests_uri.get_path();
  auto last_path = Url::extra_path_element(route_->get_rest_path_raw(), path);
  return last_path;
}

std::string HandlerObject::get_rest_query_parameter(HttpUri &requests_uri) {
  Url uri_param(requests_uri);
  auto query = uri_param.get_query_parameter("q");
  return query;
}

HttpResult HandlerObject::handle_delete(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  auto query = get_rest_query_parameter(requests_uri);
  auto last_path = get_path_after_object_name(requests_uri);
  if (!last_path.empty())
    throw http::Error(
        HttpStatusCode::BadRequest,
        "To delete entries in the object, use only 'filter' selector.");
  auto object = route_->get_cached_object();

  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());
  mrs::database::QueryRestObjectDelete d;
  d.execute_delete(session.get(), object, query);

  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    obj->member_add_value("itemsDeleted", session->affected_rows());
  }

  return {stt.get_result(), helper::MediaType::typeJson};
}

// Update, with insert possibility
HttpResult HandlerObject::handle_put([
    [maybe_unused]] rest::RequestContext *ctxt) {
  auto pk_value = rest_param_to_sql_value(
      get_path_after_object_name(ctxt->request->get_uri()));

  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto document = input_buffer.pop_front(size);

  rapidjson::Document json_doc;
  database::QueryRestObjectInsert insert;
  auto object = route_->get_cached_object();

  json_doc.Parse((const char *)document.data(), document.size());

  // TODO(lkotula): return error msg ? (Shouldn't be in review)
  if (json_doc.HasParseError()) {
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

  auto pk = insert.execute_upsert(
      session.get(), object, json_doc, pk_value,
      route_->get_user_row_ownership().user_ownership_enforced
          ? route_->get_user_row_ownership().user_ownership_column
          : "",
      rapidjson::Value(ctxt->user.user_id.to_string().c_str(),
                       json_doc.GetAllocator()));

  Counter<kEntityCounterRestAffectedItems>::increment(insert.affected);

  if (!route_->get_cached_primary().name.empty()) {
    database::QueryRestTableSingleRow fetch_one;

    fetch_one.query_entries(session.get(), object,
                            database::ObjectFieldFilter::from_object(*object),
                            pk.begin()->first, pk.begin()->second,
                            route_->get_rest_url());

    Counter<kEntityCounterRestAffectedItems>::increment(fetch_one.items);
    return std::move(fetch_one.response);
  }

  // TODO(lkotula): return proper error ! (Shouldn't be in review)
  return {};
}

Handler::Authorization HandlerObject::requires_authentication() const {
  return route_->requires_authentication() ? Authorization::kCheck
                                           : Authorization::kNotNeeded;
}

UniversalId HandlerObject::get_service_id() const {
  return route_->get_service_id();
}

UniversalId HandlerObject::get_db_object_id() const { return route_->get_id(); }

UniversalId HandlerObject::get_schema_id() const {
  return route_->get_schema()->get_id();
}

uint32_t HandlerObject::get_access_rights() const {
  return route_->get_access();
}

}  // namespace rest
}  // namespace mrs
