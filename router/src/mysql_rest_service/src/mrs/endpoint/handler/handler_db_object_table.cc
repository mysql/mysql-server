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

#include "mrs/endpoint/handler/handler_db_object_table.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysqld_error.h"

#include "helper/container/to_string.h"
#include "helper/http/url.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "helper/mysql_numeric_value.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/helper/object_row_ownership.h"
#include "mrs/database/helper/query_gtid_executed.h"
#include "mrs/database/helper/query_retry_on_ro.h"
#include "mrs/database/json_mapper/select.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/database/query_rest_table.h"
#include "mrs/database/query_rest_table_single_row.h"
#include "mrs/database/query_rest_table_updater.h"
#include "mrs/endpoint/db_schema_endpoint.h"
#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/endpoint/url_host_endpoint.h"
#include "mrs/http/error.h"
#include "mrs/monitored/gtid_functions.h"
#include "mrs/monitored/query_retry_on_ro.h"
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
using MediaType = helper::MediaType;
using HeaderAccept = mrs::http::HeaderAccept;
using rapidjson::StringRef;
using Authorization = mrs::rest::Handler::Authorization;

MediaType validate_content_type_encoding(HeaderAccept *accepts) {
  static const std::vector<MediaType> allowedMimeTypes{
      MediaType::typeJson, MediaType::typeXieee754ClientJson};

  auto allowed_type = accepts->is_acceptable(allowedMimeTypes);
  if (!allowed_type.has_value()) {
    throw mrs::http::Error(HttpStatusCode::NotAcceptable,
                           "The request must accept one of: ",
                           helper::container::to_string(allowedMimeTypes));
  }

  return allowed_type.value();
}

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
    case mrs::database::entry::ColumnType::VECTOR: {
      mysqlrouter::sqlstring result{"STRING_TO_VECTOR(?)"};
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

std::string get_path_after_object_name(const http::base::Uri &base_uri,
                                       const http::base::Uri &requests_uri) {
  const auto &elements_path = requests_uri.get_path_elements();
  const auto &elements_base = base_uri.get_path_elements();

  if (elements_path.size() > elements_base.size())
    return elements_path[elements_base.size()];

  return {};
}

std::string get_rest_query_parameter(const http::base::Uri &requests_uri) {
  auto query = Url::get_query_parameter(requests_uri, "q");
  return query;
}

mrs::database::PrimaryKeyColumnValues get_rest_pk_parameter(
    std::shared_ptr<mrs::database::entry::Object> object,
    const ::http::base::Uri &base_uri, const ::http::base::Uri &requests_uri) {
  auto id = get_path_after_object_name(base_uri, requests_uri);
  auto pk_columns = object->primary_key();

  if (id.empty()) return {};

  mrs::database::PrimaryKeyColumnValues pk;
  if (1 == pk_columns.size()) {
    pk[pk_columns[0]->column_name] =
        rest_param_to_sql_value(*pk_columns[0], id);
    return pk;
  }

  auto pk_values = mysql_harness::split_string(id, ',', true);

  if (pk_columns.empty()) {
    throw std::logic_error("Table has no primary key");
  }

  if (pk_values.size() != pk_columns.size()) {
    throw mrs::http::Error(HttpStatusCode::NotFound, "Invalid ID requested");
  }

  for (size_t i = 0; i < pk_columns.size(); i++) {
    pk[pk_columns[i]->column_name] =
        rest_param_to_sql_value(*pk_columns[i], pk_values[i]);
  }

  return pk;
}

mrs::database::entry::RowUserOwnership get_user_ownership(
    const std::string &obj_name,
    const std::shared_ptr<mrs::database::entry::Object> &obj) {
  mrs::database::entry::RowUserOwnership result;

  result.user_ownership_enforced = false;
  if (obj && obj->user_ownership_field.has_value()) {
    auto dfield = std::dynamic_pointer_cast<mrs::database::entry::Column>(
        obj->user_ownership_field->field);

    if (!dfield) {
      log_warning("ownership disabled for db_object:%s", obj_name.c_str());
      return result;
    }

    result.user_ownership_enforced = true;
    result.user_ownership_column = dfield->column_name;
  }

  return result;
}

auto get_path_for_db_object(
    std::weak_ptr<mrs::endpoint::DbObjectEndpoint> endpoint) {
  auto ep = mrs::endpoint::handler::lock(endpoint);
  auto parent_ep = ep->get_parent_ptr();

  const std::string &object_path = ep->get_url_path();
  const std::string &service_schema_path = parent_ep->get_url_path();
  const bool is_index = ep->is_index();

  return mrs::endpoint::handler::path_db_object_with_index(
      object_path, service_schema_path, is_index);
}

template <typename F>
void execute_and_handle_timeout(F fn) {
  try {
    fn();
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (e.code() == ER_QUERY_TIMEOUT) {
      mrs::Counter<kEntityCounterSqlQueryTimeouts>::increment();

      throw mrs::http::Error(HttpStatusCode::GatewayTimeout,
                             "Database request timed out");
    } else {
      throw;
    }
  }
}

}  // namespace

namespace mrs {
namespace endpoint {
namespace handler {

using CachedObject = collector::MysqlCacheManager::CachedObject;
using MysqlCacheManager = collector::MysqlCacheManager;
using MySQLConnection = collector::MySQLConnection;
using MediaType = helper::MediaType;

using HttpResult = mrs::rest::Handler::HttpResult;

HandlerDbObjectTable::HandlerDbObjectTable(
    std::weak_ptr<DbObjectEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager,
    mrs::GtidManager *gtid_manager, collector::MysqlCacheManager *cache,
    mrs::ResponseCache *response_cache,
    mrs::database::SlowQueryMonitor *slow_monitor)
    : Handler(handler::get_protocol(endpoint), get_endpoint_host(endpoint),
              /* path: /service/schema/object[/...] */
              get_path_for_db_object(endpoint),
              get_endpoint_options(lock(endpoint)), auth_manager),
      gtid_manager_{gtid_manager},
      cache_{cache},
      endpoint_{endpoint},
      slow_monitor_(slow_monitor) {
  auto ep = lock(endpoint_);
  auto ep_parent = lock_parent(ep);
  assert(ep_parent);
  entry_ = ep->get();
  schema_entry_ = ep_parent->get();
  auto ep_service = lock_parent(ep_parent);
  assert(ep_service);
  service_entry_ = ep_service->get();
  ownership_ = get_user_ownership(entry_->name, entry_->object_description);

  if (get_options().result.cache_ttl_ms > 0 && response_cache) {
    response_cache_ = std::make_shared<ItemEndpointResponseCache>(
        response_cache, get_options().result.cache_ttl_ms);
  }
}

void HandlerDbObjectTable::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

uint64_t HandlerDbObjectTable::slow_query_timeout() const {
  return get_options().query.timeout > 0
             ? get_options().query.timeout
             : (slow_monitor_ ? slow_monitor_->default_timeout()
                              : mrs::database::k_default_sql_query_timeout_ms);
}

HttpResult HandlerDbObjectTable::handle_get(rest::RequestContext *ctxt) {
  auto session = get_session(ctxt);
  auto object = entry_->object_description;
  database::dv::ObjectFieldFilter field_filter;
  std::optional<std::string> target_field;
  auto endpoint = lock_or_throw_unavail(endpoint_);
  auto endpoint_url = endpoint->get_url();
  auto pk =
      get_rest_pk_parameter(object, endpoint_url, ctxt->request->get_uri());
  const auto accepted_content_type =
      validate_content_type_encoding(&ctxt->accepts);
  const bool opt_sp_include_links = get_options().result.include_links;
  const bool opt_encode_bigints_as_string =
      accepted_content_type == MediaType::typeXieee754ClientJson;
  const auto row_ownership = row_ownership_info(ctxt, object);

  if (response_cache_) {
    auto entry = response_cache_->lookup_table(
        ctxt->request->get_uri(),
        row_ownership.enabled() ? row_ownership.owner_user_id() : "");
    if (entry) {
      Counter<kEntityCounterRestReturnedItems>::increment(entry->items);
      return {std::string{entry->data}};
    }
  }

  Url uri_param(ctxt->request->get_uri());

  auto it_f = uri_param.is_query_parameter("f");
  auto it_raw = uri_param.is_query_parameter("raw");

  if (it_f) {
    auto filter = mysql_harness::split_string(
        uri_param.get_query_parameter("f"), ',', false);

    try {
      field_filter =
          database::dv::ObjectFieldFilter::from_url_filter(*object, filter);
    } catch (const std::exception &e) {
      throw http::Error(HttpStatusCode::BadRequest, e.what());
    }

    if (filter.size() == 1) target_field = filter.front();
  } else {
    field_filter = database::dv::ObjectFieldFilter::from_object(*object);
  }

  std::string raw_value = it_raw ? uri_param.get_query_parameter("raw") : "";

  if (!raw_value.empty() && !target_field.has_value()) {
    throw http::Error(HttpStatusCode::BadRequest);
  }

  database::FilterObjectGenerator fog(object, true, get_options().query.wait,
                                      get_options().query.embed_wait);
  fog.parse(uri_param.get_query_parameter("q"));

  if (pk.empty()) {
    uint64_t offset = 0;
    uint64_t limit = get_items_on_page();
    uri_param.parse_offset_limit(&offset, &limit);

    if (raw_value.empty()) {
      static const std::string empty;
      database::QueryRestTable rest{opt_encode_bigints_as_string,
                                    opt_sp_include_links, slow_query_timeout()};

      monitored::QueryRetryOnRO query_retry{cache_,
                                            session,
                                            gtid_manager_,
                                            fog,
                                            get_options().query.wait,
                                            get_options().query.embed_wait};

      do {
        query_retry.before_query();
        const bool is_default_limit = get_items_on_page() == limit;

        execute_and_handle_timeout([&]() {
          rest.query_entries(
              query_retry.get_session(), object, field_filter, offset, limit,
              endpoint_url.join(), is_default_limit, row_ownership,
              query_retry.get_fog(), !field_filter.is_filter_configured());
        });
      } while (query_retry.should_retry(rest.items));

      Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

      if (response_cache_) {
        response_cache_->create_table_entry(
            ctxt->request->get_uri(),
            row_ownership.enabled() ? row_ownership.owner_user_id() : "",
            rest.response, rest.items);
      }

      return std::move(rest.response);
    }

    if (limit != 1) throw http::Error(HttpStatusCode::BadRequest);

    database::QueryRestSPMedia rest;
    execute_and_handle_timeout([&]() {
      rest.query_entries(session.get(), *target_field, schema_entry_->name,
                         entry_->name, limit, offset);
    });
    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);
    Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

    return {std::move(rest.response), detected_type};
  } else {
    if (fog.has_where() || fog.has_order()) {
      throw http::Error(HttpStatusCode::BadRequest,
                        "Invalid filter object for GET request by id");
    }

    if (raw_value.empty()) {
      database::QueryRestTableSingleRow rest(
          nullptr, opt_encode_bigints_as_string, opt_sp_include_links,
          mrs::database::RowLockType::NONE, slow_query_timeout());
      log_debug("Rest select single row %s",
                database::dv::format_key(*object, pk).str().c_str());

      monitored::QueryRetryOnRO query_retry{cache_,
                                            session,
                                            gtid_manager_,
                                            fog,
                                            get_options().query.wait,
                                            get_options().query.embed_wait};

      do {
        query_retry.before_query();
        execute_and_handle_timeout([&]() {
          rest.query_entry(session.get(), object, pk, field_filter,
                           endpoint_url.join(), row_ownership,
                           query_retry.get_fog(), true);
        });
      } while (query_retry.should_retry(rest.items));

      if (rest.response.empty()) throw http::Error(HttpStatusCode::NotFound);
      Counter<kEntityCounterRestReturnedItems>::increment(rest.items);

      if (response_cache_) {
        response_cache_->create_table_entry(
            ctxt->request->get_uri(),
            row_ownership.enabled() ? row_ownership.owner_user_id() : "",
            rest.response, rest.items);
      }

      return std::move(rest.response);
    }

    database::QueryRestSPMedia rest;

    execute_and_handle_timeout([&]() {
      rest.query_entries(session.get(), *target_field, schema_entry_->name,
                         entry_->name, pk);
    });

    helper::MediaDetector md;
    auto detected_type = md.detect(rest.response);

    return {std::move(rest.response), detected_type};
  }

  // TODO(lkotula): Return proper error. (Shouldn't be in review)
  throw http::Error(HttpStatusCode::InternalError);
}

/// Post is insert
HttpResult HandlerDbObjectTable::handle_post(
    rest::RequestContext *ctxt, const std::vector<uint8_t> &document) {
  using namespace helper::json::sql;
  rapidjson::Document json_doc;
  auto endpoint = lock_or_throw_unavail(endpoint_);
  auto object = entry_->object_description;
  auto session = get_session(ctxt, MySQLConnection::kMySQLConnectionUserdataRW);
  auto endpoint_url = endpoint->get_url();

  auto last_path =
      get_path_after_object_name(endpoint_url, ctxt->request->get_uri());

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

  database::dv::JsonMappingUpdater updater(object,
                                           row_ownership_info(ctxt, object));

  mysqlrouter::MySQLSession::Transaction transaction{session.get(), false};
  mrs::database::PrimaryKeyColumnValues pk;

  slow_monitor_->execute(
      [&updater, &session, &pk, &json_doc]() {
        pk = updater.insert(session.get(), json_doc);
      },
      session.get(), get_options().query.timeout);

  Counter<kEntityCounterRestAffectedItems>::increment();

  if (!pk.empty()) {
    database::QueryRestTableSingleRow fetch_one(
        nullptr, false, true, mrs::database::RowLockType::NONE,
        get_options().query.timeout > 0 ? get_options().query.timeout
                                        : slow_monitor_->default_timeout());

    execute_and_handle_timeout([&]() {
      fetch_one.query_entry(
          session.get(), object, pk,
          database::dv::ObjectFieldFilter::from_object(*object),
          endpoint->get_url().join(), row_ownership_info(ctxt, object), {},
          true, [&]() {
            // commit needs to happen after reading back the posted row
            transaction.commit();

            if (get_options().metadata.gtid)
              return mrs::monitored::
                  get_session_tracked_gtids_for_metadata_response(
                      session.get(), gtid_manager_);
            else
              return std::string{};
          });
    });

    Counter<kEntityCounterRestReturnedItems>::increment(fetch_one.items);

    return std::move(fetch_one.response);
  } else {
    transaction.commit();
    return {};
  }
}

HttpResult HandlerDbObjectTable::handle_delete(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  auto endpoint = lock_or_throw_unavail(endpoint_);
  auto endpoint_url = endpoint->get_url();
  auto last_path = get_path_after_object_name(endpoint_url, requests_uri);
  auto object = entry_->object_description;
  auto session = get_session(ctxt, MySQLConnection::kMySQLConnectionUserdataRW);
  auto addr = session->get_connection_parameters().conn_opts.destination;

  uint64_t count = 0;
  const auto accepted_content_type =
      validate_content_type_encoding(&ctxt->accepts);

  database::dv::JsonMappingUpdater rest(object,
                                        row_ownership_info(ctxt, object));

  if (!last_path.empty()) {
    auto pk = get_rest_pk_parameter(object, endpoint_url, requests_uri);

    slow_monitor_->execute([&rest, &count, &session,
                            pk]() { count = rest.delete_(session.get(), pk); },
                           session.get(), get_options().query.timeout);
  } else {
    auto query = get_rest_query_parameter(requests_uri);

    database::FilterObjectGenerator fog(object, true, get_options().query.wait,
                                        get_options().query.embed_wait);

    fog.parse(query);

    if (fog.has_asof()) {
      // This is a write operation, thus the `session` is RW.
      mrs::monitored::count_using_wait_at_rw_connection();

      for (int retry = 0; retry < 2; ++retry) {
        auto result =
            gtid_manager_->is_executed_on_server(addr, {fog.get_asof()});
        if (result == mrs::GtidAction::k_needs_update) {
          auto gtidsets = database::get_gtid_executed(session.get());
          gtid_manager_->reinitialize(addr, gtidsets);
          continue;
        }
        if (result == mrs::GtidAction::k_is_on_server) {
          fog.reset(database::FilterObjectGenerator::Clear::kAsof);
        }
      }
    }

    if (!get_options().query.embed_wait && fog.has_asof()) {
      auto gtid = fog.get_asof();
      if (!database::wait_gtid_executed(session.get(), gtid,
                                        get_options().query.wait)) {
        monitored::throw_rest_error_asof_timeout();
      }
    }

    auto result = fog.get_result();
    if (result.is_empty())
      throw std::runtime_error("Filter must contain valid JSON object.");
    if (fog.has_order())
      throw std::runtime_error(
          "Filter must not contain ordering informations.");

    log_debug("rest.handle_delete");
    slow_monitor_->execute(
        [&count, &rest, &session, fog]() {
          count = rest.delete_(session.get(), fog);
        },
        session.get(), get_options().query.timeout);

    if (get_options().query.embed_wait && fog.has_asof() && 0 == count) {
      mrs::monitored::throw_rest_error_asof_timeout_if_not_gtid_executed(
          session.get(), fog.get_asof());
    }
  }

  auto gtid = mrs::monitored::get_session_tracked_gtids_for_metadata_response(
      session.get(), gtid_manager_);

  helper::json::SerializerToText stt{accepted_content_type ==
                                     MediaType::typeXieee754ClientJson};
  {
    auto obj = stt.add_object();
    obj->member_add_value("itemsDeleted", count);

    if (get_options().metadata.gtid) {
      if (count && !gtid.empty()) {
        auto metadata = obj->member_add_object("_metadata");
        metadata->member_add_value("gtid", gtid);
      }
    }
  }
  return {stt.get_result(), accepted_content_type};
}

// Update, with insert possibility
HttpResult HandlerDbObjectTable::handle_put(rest::RequestContext *ctxt) {
  using namespace helper::json::sql;  // NOLINT(build/namespaces)
  auto endpoint = lock_or_throw_unavail(endpoint_);
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto document = input_buffer.pop_front(size);
  auto object = entry_->object_description;
  auto endpoint_url = endpoint->get_url();

  auto pk =
      get_rest_pk_parameter(object, endpoint_url, ctxt->request->get_uri());

  rapidjson::Document json_doc;

  database::dv::JsonMappingUpdater updater(object,
                                           row_ownership_info(ctxt, object));

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

  if (ownership_.user_ownership_enforced) {
    if (!ctxt->user.has_user_id)
      throw http::Error(HttpStatusCode::Unauthorized);
  }

  auto json_obj = json_doc.GetObject();
  auto session = get_session(ctxt, MySQLConnection::kMySQLConnectionUserdataRW);

  mysqlrouter::MySQLSession::Transaction transaction{session.get(), false};

  slow_monitor_->execute(
      [&]() { pk = updater.update(session.get(), pk, json_doc, true); },
      session.get(), get_options().query.timeout);

  Counter<kEntityCounterRestAffectedItems>::increment(updater.affected());

  database::QueryRestTableSingleRow fetch_one(nullptr, false, true,
                                              mrs::database::RowLockType::NONE,
                                              slow_query_timeout());

  execute_and_handle_timeout([&]() {
    fetch_one.query_entry(
        session.get(), object, pk,
        database::dv::ObjectFieldFilter::from_object(*object),
        endpoint_url.join(), row_ownership_info(ctxt, object), {}, true, [&]() {
          // commit needs to happen after reading back the posted row
          transaction.commit();

          if (get_options().metadata.gtid)
            return mrs::monitored::
                get_session_tracked_gtids_for_metadata_response(session.get(),
                                                                gtid_manager_);
          else
            return std::string{};
        });
  });

  transaction.commit();

  Counter<kEntityCounterRestReturnedItems>::increment(fetch_one.items);
  return std::move(fetch_one.response);
}

Authorization HandlerDbObjectTable::requires_authentication() const {
  bool requires_auth =
      entry_->requires_authentication || schema_entry_->requires_auth;
  return requires_auth ? Authorization::kCheck : Authorization::kNotNeeded;
}

UniversalId HandlerDbObjectTable::get_service_id() const {
  return schema_entry_->service_id;
}

UniversalId HandlerDbObjectTable::get_schema_id() const {
  return schema_entry_->id;
}

UniversalId HandlerDbObjectTable::get_db_object_id() const {
  return entry_->id;
}

const std::string &HandlerDbObjectTable::get_service_path() const {
  return service_entry_->url_context_root;
}

const std::string &HandlerDbObjectTable::get_db_object_path() const {
  return entry_->request_path;
}

const std::string &HandlerDbObjectTable::get_schema_path() const {
  return schema_entry_->request_path;
}

uint32_t HandlerDbObjectTable::get_access_rights() const {
  return entry_->crud_operation;
}

uint64_t HandlerDbObjectTable::get_items_on_page() const {
  if (entry_->items_per_page.has_value()) return entry_->items_per_page.value();

  if (schema_entry_->items_per_page.has_value())
    return schema_entry_->items_per_page.value();

  return k_default_items_on_page;
}

mrs::database::ObjectRowOwnership HandlerDbObjectTable::row_ownership_info(
    rest::RequestContext *ctxt,
    std::shared_ptr<database::entry::Object> object) const {
  if (ownership_.user_ownership_enforced && !ctxt->user.has_user_id)
    throw http::Error(HttpStatusCode::Unauthorized);

  return mrs::database::ObjectRowOwnership{
      object, &ownership_,
      ctxt->user.has_user_id ? ctxt->user.user_id : std::optional<UserId>(),
      entry_->row_group_security, ctxt->user.groups};

  return {};
}

HandlerDbObjectTable::CachedSession HandlerDbObjectTable::get_session(
    rest::RequestContext *ctxt, collector::MySQLConnection type,
    PoolManagerRef *out_pool) {
  if (get_options().query.passthrough_db_user &&
      (type == collector::kMySQLConnectionUserdataRW ||
       type == collector::kMySQLConnectionUserdataRO)) {
    if (!ctxt->session || !ctxt->session->db_session_pool) {
      log_debug(
          "Request to service with passthroughDbUser from a user authenticated "
          " through auth app '%s', which does not support it",
          ctxt->user.app_id.to_string().c_str());
      throw http::Error(HttpStatusCode::BadRequest,
                        "Service requires authentication with "
                        "MySQL Internal, but user is authenticated with "
                        "other authApp (or authentication was not configured)");
    }

    try {
      if (out_pool) *out_pool = ctxt->session->db_session_pool;

      return ctxt->session->db_session_pool->get_instance();
    } catch (const collector::db_pool_exhausted &) {
      throw http::Error(HttpStatusCode::TooManyRequests);
    } catch (const std::exception &e) {
      log_error("Could not get DB session for user '%s' for service: %s",
                ctxt->user.name.c_str(), e.what());
      throw;
    }
  }

  return cache_->get_instance(type, false);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
