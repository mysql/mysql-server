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

#include "mrs/rest/handler_sp.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/http_request.h"

#include "helper/media_detector.h"
#include "mrs/database/query_rest_sp.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/http/url.h"
#include "mrs/rest/handler_request_context.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using Result = mrs::rest::Handler::Result;
using CachedObject = collector::MysqlCacheManager::CachedObject;

static CachedObject get_session(::mysqlrouter::MySQLSession *session,
                                collector::MysqlCacheManager *cache_manager) {
  if (session) return CachedObject(nullptr, session);

  return cache_manager->get_instance(collector::kMySQLConnectionUserdata);
}

Result HandlerSP::handle_delete([[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

Result HandlerSP::handle_put([[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

Result HandlerSP::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

Result HandlerSP::handle_get([[maybe_unused]] rest::RequestContext *ctxt) {
  auto session =
      get_session(ctxt->sql_session_cache.get(), route_->get_cache());

  const auto format = route_->get_format();
  log_debug("HandlerSP::handle_get start format=%i", (int)format);

  if (format == Route::kFeed) {
    log_debug("HandlerSP::handle_get - generating feed response");
    database::QueryRestSP db;

    db.query_entries(session.get(), route_->get_schema_name(),
                     route_->get_object_name(), route_->get_rest_url(),
                     route_->get_user_row_ownership().user_ownership_column);

    return {std::move(db.response)};
  }

  database::QueryRestSPMedia db;
  http::Url::Keys keys;
  http::Url::Values values;

  auto &requests_uri = ctxt->request->get_uri();
  http::Url::parse_query(requests_uri.get_query().c_str(), &keys, &values);

  db.query_entries(session.get(), route_->get_schema_name(),
                   route_->get_object_name(), values);

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
  return route_->requires_authentication() ? Authorization::kRequires
                                           : Authorization::kNotNeeded;
}

std::pair<IdType, uint64_t> HandlerSP::get_id() const {
  return {IdType::k_id_type_service_id, route_->get_service_id()};
}

uint64_t HandlerSP::get_db_object_id() const { return route_->get_id(); }

uint64_t HandlerSP::get_schema_id() const {
  return route_->get_schema()->get_id();
}

uint32_t HandlerSP::get_access_rights() const { return Route::kRead; }

}  // namespace rest
}  // namespace mrs
