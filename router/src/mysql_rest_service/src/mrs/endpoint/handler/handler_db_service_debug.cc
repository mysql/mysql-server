/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler/handler_db_service_debug.h"

#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = mrs::rest::Handler::HttpResult;
using Authorization = mrs::rest::Handler::Authorization;

namespace {

auto get_path_service_debug(std::weak_ptr<DbServiceEndpoint> endpoint) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result;

  auto endpoint_srvc = lock(endpoint);
  if (!endpoint_srvc) return result;

  result.push_back(path_service_debug(endpoint_srvc->get_url_path()));

  return result;
}

}  // namespace

HandlerDbServiceDebug::HandlerDbServiceDebug(
    std::weak_ptr<DbServiceEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager)
    : Handler(handler::get_protocol(endpoint), get_endpoint_host(endpoint),
              /* path: /service/_debug */
              get_path_service_debug(endpoint),
              get_endpoint_options(lock(endpoint)), auth_manager),
      endpoint_{endpoint} {
  auto ep = lock(endpoint_);
  entry_ = ep->get();
}

HttpResult HandlerDbServiceDebug::handle_get(rest::RequestContext *) {
  auto endpoint = lock_or_throw_unavail(endpoint_);

  return HttpResult(endpoint->is_debug_enabled() ? "true" : "false");
}

HttpResult HandlerDbServiceDebug::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbServiceDebug::handle_delete(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  assert(false && "Should be never called. Stubbing parent class.");
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbServiceDebug::handle_put(rest::RequestContext *ctxt) {
  // Get the request body for cache lookup or normal processing
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto request_body = input_buffer.pop_front(size);
  std::string_view body{reinterpret_cast<const char *>(request_body.data()),
                        request_body.size()};

  rapidjson::Document doc;
  doc.Parse(body.data(), body.size());

  if (doc.IsObject() && doc["enabled"].IsBool()) {
    auto endpoint = lock_or_throw_unavail(endpoint_);
    endpoint->set_debug_enabled(doc["enabled"].GetBool());
    return HttpResult();
  }

  throw http::Error(HttpStatusCode::BadRequest);
}

uint32_t HandlerDbServiceDebug::get_access_rights() const {
  using Operation = mrs::database::entry::Operation;

  return Operation::valueRead | Operation::valueUpdate;
}

Authorization HandlerDbServiceDebug::requires_authentication() const {
  return Authorization::kNotNeeded;
}

UniversalId HandlerDbServiceDebug::get_service_id() const { return entry_->id; }

UniversalId HandlerDbServiceDebug::get_db_object_id() const {
  return UniversalId{};
}

UniversalId HandlerDbServiceDebug::get_schema_id() const {
  return UniversalId{};
}

const std::string &HandlerDbServiceDebug::get_service_path() const {
  return entry_->url_context_root;
}

const std::string &HandlerDbServiceDebug::get_db_object_path() const {
  return empty_path();
}

const std::string &HandlerDbServiceDebug::get_schema_path() const {
  return empty_path();
}
}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
