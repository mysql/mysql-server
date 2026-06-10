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

#include "mrs/endpoint/handler/authentication/handler_authorize_status.h"

#include <cassert>

#include "helper/json/serializer_to_text.h"
#include "mrs/database/query_entries_auth_role.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = HandlerAuthorizeStatus::HttpResult;

HandlerAuthorizeStatus::HandlerAuthorizeStatus(
    const Protocol protocol, const std::string &url_host,
    const UniversalId service_id, const std::string &service_path,
    const UriPathMatcher &rest_path_matcher, const std::string &options,
    interface::AuthorizeManager *auth_manager)
    : HandlerAuthorizeBase(protocol, url_host, {rest_path_matcher}, options,
                           auth_manager),
      service_id_{service_id},
      service_path_{service_path} {}

mrs::rest::Handler::Authorization
HandlerAuthorizeStatus::requires_authentication() const {
  return Authorization::kCheck;
}

UniversalId HandlerAuthorizeStatus::get_service_id() const {
  return service_id_;
}

UniversalId HandlerAuthorizeStatus::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerAuthorizeStatus::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

const std::string &HandlerAuthorizeStatus::get_service_path() const {
  return service_path_;
}

const std::string &HandlerAuthorizeStatus::get_schema_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

const std::string &HandlerAuthorizeStatus::get_db_object_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

uint32_t HandlerAuthorizeStatus::get_access_rights() const {
  using Op = mrs::database::entry::Operation::Values;
  return Op::valueRead;
}

void HandlerAuthorizeStatus::fill_the_user_data(
    Object &ojson, const AuthUser &user, const std::vector<AuthRole> &roles) {
  ojson->member_add_value("name", user.name);
  ojson->member_add_value("id", user.user_id.to_string());

  if (!user.email.empty()) ojson->member_add_value("email", user.email);

  auto roles_array = ojson->member_add_array("roles");
  for (const auto &r : roles) {
    roles_array->add_value(database::entry::to_string(r).c_str(),
                           helper::JsonType::kJson);
  }
}

void HandlerAuthorizeStatus::fill_authorization(
    Object &ojson, const AuthUser &user, const std::vector<AuthRole> &roles) {
  ojson->member_add_value("status",
                          user.has_user_id ? "authorized" : "unauthorized");

  if (user.has_user_id) {
    auto ouser = ojson->member_add_object("user");
    fill_the_user_data(ouser, user, roles);
  }
}

HttpResult HandlerAuthorizeStatus::handle_get(RequestContext *ctxt) {
  log_debug("HandlerAuthorizeStatus::handle_get");
  helper::json::SerializerToText serializer;
  {
    database::QueryEntriesAuthRole roles;
    if (ctxt->user.has_user_id) {
      auto session = authorization_manager_->get_cache()->get_instance(
          collector::kMySQLConnectionMetadataRO, false);
      roles.query_role(session.get(), ctxt->user.user_id);
    }
    auto obj = serializer.add_object();
    fill_authorization(obj, ctxt->user, roles.result);
  }

  return HttpResult(serializer.get_result(), helper::typeJson);
}

HttpResult HandlerAuthorizeStatus::handle_post(RequestContext *,
                                               const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeStatus::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeStatus::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerAuthorizeStatus::may_check_access() const { return false; }

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
