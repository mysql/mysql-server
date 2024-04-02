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

#include "mrs/rest/handler_user.h"

#include <cassert>

#include "helper/container/generic.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/serializer_to_text.h"
#include "helper/json/text_to.h"
#include "mrs/database/query_entries_auth_role.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using HttpResult = HandlerUser::HttpResult;
using Route = mrs::interface::Object;

uint32_t HandlerUser::get_access_rights() const {
  return Route::kRead | Route::kUpdate;
}

HttpResult HandlerUser::handle_put(RequestContext *ctxt) {
  using namespace std::string_literals;
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto vinput = input_buffer.pop_front(size);
  std::string input{vinput.begin(), vinput.end()};

  rapidjson::Document doc;

  if (!helper::json::text_to(&doc, input) || !doc.IsObject())
    throw http::Error(HttpStatusCode::BadRequest,
                      "PUT value isn't a JSON object.");
  const std::vector<std::string> allowed_members{"name", "email"};
  for (auto kv : helper::json::member_iterator(doc)) {
    if (!helper::container::has(allowed_members, kv.first))
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not supported member: "s + kv.first);
  }

  auto it = doc.FindMember("email");
  if (it != doc.MemberEnd() && it->value.IsString()) {
    ctxt->user.email = it->value.GetString();
  }

  it = doc.FindMember("name");
  if (it != doc.MemberEnd() && it->value.IsString()) {
    ctxt->user.name = it->value.GetString();
  }

  authorization_manager_->get_user_manager()->user_invalidate(
      ctxt->user.user_id);
  if (!authorization_manager_->get_user_manager()->user_get(
          &ctxt->user, &ctxt->sql_session_cache)) {
    throw http::Error(HttpStatusCode::InternalError);
  }
  auto session = authorization_manager_->get_current_session(
      get_service_id(), ctxt->request->get_input_headers(), &ctxt->cookies);

  if (session) session->user = ctxt->user;

  return {};
}

void HandlerUser::fill_authorization(Object &ojson, const AuthUser &user,
                                     const std::vector<AuthRole> &roles) {
  if (user.has_user_id) {
    fill_the_user_data(ojson, user, roles);
  }
}

void HandlerUser::authorization(RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

}  // namespace rest
}  // namespace mrs
