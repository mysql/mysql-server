/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_unauthorize.h"

#include <cassert>

#include "helper/json/serializer_to_text.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace rest {

using HttpResult = HandlerUnauthorize::HttpResult;
using Route = mrs::interface::Object;

std::string impl_get_json_response_ok() {
  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    obj->member_add_value("message", "OK");
    obj->member_add_value("status", 200);
  }

  return stt.get_result();
}

std::string get_json_response_ok() {
  static std::string result = impl_get_json_response_ok();

  return result;
}

HandlerUnauthorize::HandlerUnauthorize(
    const UniversalId service_id, const std::string &url,
    const std::string &rest_path_matcher, const std::string &options,
    interface::AuthorizeManager *auth_manager)
    : Handler(url, {rest_path_matcher}, options, auth_manager),
      service_id_{service_id},
      auth_manager_{auth_manager} {}

Handler::Authorization HandlerUnauthorize::requires_authentication() const {
  return Authorization::kCheck;
}

UniversalId HandlerUnauthorize::get_service_id() const { return service_id_; }

UniversalId HandlerUnauthorize::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerUnauthorize::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

uint32_t HandlerUnauthorize::get_access_rights() const { return Route::kRead; }

HttpResult HandlerUnauthorize::handle_get(RequestContext *ctxt) {
  auth_manager_->unauthorize(service_id_, &ctxt->cookies);
  return {HttpStatusCode::Unauthorized, get_json_response_ok(),
          HttpResult::Type::typeJson};
}

HttpResult HandlerUnauthorize::handle_post(RequestContext *,
                                           const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerUnauthorize::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerUnauthorize::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerUnauthorize::request_begin(RequestContext *) { return true; }

void HandlerUnauthorize::request_end(RequestContext *) {}

bool HandlerUnauthorize::request_error(RequestContext *, const http::Error &) {
  return false;
}

bool HandlerUnauthorize::may_check_access() const { return false; }

}  // namespace rest
}  // namespace mrs
