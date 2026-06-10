/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler/authentication/handler_authorize_logout.h"

#include <cassert>

#include "helper/json/serializer_to_text.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = HandlerAuthorizeLogout::HttpResult;

static std::string impl_get_json_response(
    const HttpStatusCode::key_type k_status, const char *message) {
  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    obj->member_add_value("message", message);
    obj->member_add_value("status", k_status);
  }

  return stt.get_result();
}

static HttpResult get_json_response_ok() {
  const auto k_status_ok = HttpStatusCode::Ok;
  return {k_status_ok,
          impl_get_json_response(k_status_ok, "Logged out successfully"),
          HttpResult::Type::typeJson};
}

HandlerAuthorizeLogout::HandlerAuthorizeLogout(
    const Protocol protocol, const std::string &url_host,
    const UniversalId service_id, const std::string &service_path,
    const UriPathMatcher &rest_path_matcher, const std::string &options,
    interface::AuthorizeManager *auth_manager)
    : HandlerAuthorizeBase(protocol, url_host, {rest_path_matcher}, options,
                           auth_manager),
      service_id_{service_id},
      service_path_{service_path},
      auth_manager_{auth_manager} {}

mrs::rest::Handler::Authorization
HandlerAuthorizeLogout::requires_authentication() const {
  return Authorization::kCheck;
}

UniversalId HandlerAuthorizeLogout::get_service_id() const {
  return service_id_;
}

UniversalId HandlerAuthorizeLogout::get_schema_id() const { return {}; }

UniversalId HandlerAuthorizeLogout::get_db_object_id() const { return {}; }

const std::string &HandlerAuthorizeLogout::get_service_path() const {
  return service_path_;
}

const std::string &HandlerAuthorizeLogout::get_schema_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

const std::string &HandlerAuthorizeLogout::get_db_object_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

uint32_t HandlerAuthorizeLogout::get_access_rights() const {
  using Op = mrs::database::entry::Operation::Values;
  return Op::valueCreate;
}

HttpResult HandlerAuthorizeLogout::handle_get(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeLogout::handle_post(RequestContext *ctxt,
                                               const std::vector<uint8_t> &) {
  auth_manager_->unauthorize(ctxt->session, &ctxt->cookies);
  return get_json_response_ok();
}

HttpResult HandlerAuthorizeLogout::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeLogout::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerAuthorizeLogout::may_check_access() const { return false; }

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
