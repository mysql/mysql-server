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

#include "mrs/endpoint/handler/authentication/handler_authorize_user.h"

#include <cassert>

#include "helper/json/rapid_json_iterator.h"
#include "helper/json/serializer_to_text.h"
#include "helper/json/text_to.h"
#include "mrs/database/query_entries_auth_role.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = HandlerAuthorizeUser::HttpResult;

uint32_t HandlerAuthorizeUser::get_access_rights() const {
  using Op = mrs::database::entry::Operation::Values;
  return Op::valueRead | Op::valueUpdate;
}

HttpResult HandlerAuthorizeUser::handle_put(RequestContext *ctxt) {
  using namespace std::string_literals;
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto vinput = input_buffer.pop_front(size);
  std::string input{vinput.begin(), vinput.end()};

  rapidjson::Document doc;

  if (!ctxt->session) {
    throw http::Error(HttpStatusCode::BadRequest,
                      "This endpoint requires authenticated user.");
  }

  if (!helper::json::text_to(&doc, input) || !doc.IsObject())
    throw http::Error(HttpStatusCode::BadRequest,
                      "PUT value isn't a JSON object.");

  const std::vector<std::string> allowed_members{"email"};
  for (auto kv : helper::json::member_iterator(doc)) {
    if (!mysql_harness::utility::container::has(allowed_members, kv.first))
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not supported member: "s + kv.first);
  }

  auto it = doc.FindMember("email");
  if (it != doc.MemberEnd()) {
    if (!it->value.IsString() && !it->value.IsNull())
      throw http::Error(HttpStatusCode::BadRequest,
                        "Wrong type for 'email' field.");
    std::optional<std::string> email;
    if (it->value.IsString()) email = it->value.GetString();

    try {
      authorization_manager_->get_user_manager()->update_email(
          &ctxt->sql_session_cache, ctxt->user.user_id, email);
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      if (ER_CHECK_CONSTRAINT_VIOLATED == e.code()) {
        throw http::Error(HttpStatusCode::BadRequest,
                          "Invalid value for 'email' field.");
      }
    }
    ctxt->user.email = email.value_or("");
  }

  authorization_manager_->update_users_cache({{ctxt->user.user_id, "UPDATE"}});

  ctxt->session->user = ctxt->user;

  return {"{}"};
}

void HandlerAuthorizeUser::fill_authorization(
    Object &ojson, const AuthUser &user, const std::vector<AuthRole> &roles) {
  if (user.has_user_id) {
    fill_the_user_data(ojson, user, roles);
  }
}

void HandlerAuthorizeUser::authorization(RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
