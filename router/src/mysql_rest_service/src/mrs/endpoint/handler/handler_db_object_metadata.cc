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

#include "mrs/endpoint/handler/handler_db_object_metadata.h"

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

auto get_path_object_metadata(std::weak_ptr<DbObjectEndpoint> endpoint) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result;

  auto endpoint_obj = lock(endpoint);
  if (!endpoint_obj) return result;

  auto endpoint_sch = endpoint_obj->get_parent_ptr();
  if (!endpoint_sch) return result;

  result.push_back(path_object_metadata(endpoint_sch->get_url_path(),
                                        endpoint_obj->get()->request_path));

  return result;
}

}  // namespace

HandlerDbObjectMetadata::HandlerDbObjectMetadata(
    std::weak_ptr<DbObjectEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager)
    : Handler(handler::get_protocol(endpoint), get_endpoint_host(endpoint),
              /* path: /service/schema/object/_metadata */
              get_path_object_metadata(endpoint),
              get_endpoint_options(lock(endpoint)), auth_manager),
      endpoint_{endpoint} {
  auto ep = lock(endpoint_);
  auto ep_parent = lock_parent(ep);
  assert(ep_parent);
  auto ep_service = lock_parent(ep_parent);
  assert(ep_service);
  entry_ = ep->get();
  schema_entry_ = ep_parent->get();
  service_entry_ = ep_service->get();
}

HttpResult HandlerDbObjectMetadata::handle_get(rest::RequestContext *) {
  auto endpoint = lock_or_throw_unavail(endpoint_);

  return entry_->metadata ? std::string(*entry_->metadata) : "{}";
}

HttpResult HandlerDbObjectMetadata::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbObjectMetadata::handle_delete(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbObjectMetadata::handle_put(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

uint32_t HandlerDbObjectMetadata::get_access_rights() const {
  using Operation = mrs::database::entry::Operation;

  return Operation::valueRead;
}

Authorization HandlerDbObjectMetadata::requires_authentication() const {
  bool requires_auth =
      entry_->requires_authentication || schema_entry_->requires_auth;
  return requires_auth ? Authorization::kCheck : Authorization::kNotNeeded;
}

UniversalId HandlerDbObjectMetadata::get_service_id() const {
  return schema_entry_->service_id;
}

UniversalId HandlerDbObjectMetadata::get_schema_id() const {
  return schema_entry_->id;
}

UniversalId HandlerDbObjectMetadata::get_db_object_id() const {
  return entry_->id;
}

const std::string &HandlerDbObjectMetadata::get_service_path() const {
  return service_entry_->url_context_root;
}

const std::string &HandlerDbObjectMetadata::get_schema_path() const {
  return schema_entry_->request_path;
}

const std::string &HandlerDbObjectMetadata::get_db_object_path() const {
  return entry_->request_path;
}

void HandlerDbObjectMetadata::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
