/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler/handler_db_schema_metadata.h"

#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/http/error.h"

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = mrs::rest::Handler::HttpResult;
using Authorization = mrs::rest::Handler::Authorization;

namespace {

auto get_path_schema_metadata(std::weak_ptr<DbSchemaEndpoint> endpoint) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result;

  auto endpoint_sch = lock(endpoint);
  if (!endpoint_sch) return result;

  result.push_back(path_schema_metadata(endpoint_sch->get_url_path()));

  return result;
}

}  // namespace

HandlerDbSchemaMetadata::HandlerDbSchemaMetadata(
    std::weak_ptr<DbSchemaEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager)
    : Handler(handler::get_protocol(endpoint), get_endpoint_host(endpoint),
              /* path: /service/schema/_metadata */
              get_path_schema_metadata(endpoint),
              get_endpoint_options(lock(endpoint)), auth_manager),
      endpoint_{endpoint} {
  auto ep = lock(endpoint_);
  auto ep_service = lock_parent(ep);
  assert(ep_service);
  entry_ = ep->get();
  service_entry_ = ep_service->get();
}

HttpResult HandlerDbSchemaMetadata::handle_get(rest::RequestContext *) {
  auto endpoint = lock_or_throw_unavail(endpoint_);

  return entry_->metadata ? std::string(*entry_->metadata) : "{}";
}

HttpResult HandlerDbSchemaMetadata::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbSchemaMetadata::handle_delete(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbSchemaMetadata::handle_put(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

uint32_t HandlerDbSchemaMetadata::get_access_rights() const {
  using Operation = mrs::database::entry::Operation;

  return Operation::valueRead;
}

Authorization HandlerDbSchemaMetadata::requires_authentication() const {
  bool requires_auth = entry_->requires_auth;
  return requires_auth ? Authorization::kCheck : Authorization::kNotNeeded;
}

UniversalId HandlerDbSchemaMetadata::get_service_id() const {
  return entry_->service_id;
}

UniversalId HandlerDbSchemaMetadata::get_db_object_id() const { return {}; }

UniversalId HandlerDbSchemaMetadata::get_schema_id() const {
  return entry_->id;
}

const std::string &HandlerDbSchemaMetadata::get_service_path() const {
  return service_entry_->url_context_root;
}

const std::string &HandlerDbSchemaMetadata::get_schema_path() const {
  return entry_->request_path;
}

const std::string &HandlerDbSchemaMetadata::get_db_object_path() const {
  return empty_path();
}

void HandlerDbSchemaMetadata::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
