/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_is_authorized.h"

#include <cassert>

#include "helper/json/serializer_to_text.h"
#include "mrs/http/cookie.h"
#include "mrs/interface/route.h"
#include "mrs/rest/handler_request_context.h"

namespace mrs {
namespace rest {

using Result = HandlerIsAuthorized::Result;
using Route = mrs::interface::Route;

HandlerIsAuthorized::HandlerIsAuthorized(const uint64_t id,
                                         const std::string &url,
                                         const std::string &rest_path_matcher,
                                         interface::AuthManager *auth_manager)
    : Handler(url, rest_path_matcher, auth_manager), id_{id} {}

Handler::Authorization HandlerIsAuthorized::requires_authentication() const {
  return Authorization::kCheck;
}

std::pair<IdType, uint64_t> HandlerIsAuthorized::get_id() const {
  return {IdType::k_id_type_auth_id, id_};
}

uint64_t HandlerIsAuthorized::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return 0;
}

uint64_t HandlerIsAuthorized::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return 0;
}

uint32_t HandlerIsAuthorized::get_access_rights() const { return Route::kRead; }

Result HandlerIsAuthorized::handle_get(RequestContext *ctxt) {
  helper::json::SerializerToText serializer;
  {
    auto obj = serializer.add_object();
    obj->member_add_value(
        "status", ctxt->user.has_user_id ? "authorized" : "unauthorized");

    if (ctxt->user.has_user_id) {
      auto user = obj->member_add_object("user");
      user->member_add_value("name", ctxt->user.name);
      user->member_add_value("id", ctxt->user.user_id);
    }
  }

  return Result(serializer.get_result(), helper::typeJson);
}

Result HandlerIsAuthorized::handle_post(RequestContext *,
                                        const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerIsAuthorized::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerIsAuthorized::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerIsAuthorized::request_begin(RequestContext *) { return true; }

void HandlerIsAuthorized::request_end(RequestContext *) {}

bool HandlerIsAuthorized::request_error(RequestContext *, const http::Error &) {
  return false;
}

}  // namespace rest
}  // namespace mrs
