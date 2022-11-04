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
#include "mrs/database/query_entries_auth_role.h"
#include "mrs/http/cookie.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using Result = HandlerIsAuthorized::Result;
using Route = mrs::interface::Object;

HandlerIsAuthorized::HandlerIsAuthorized(
    const uint64_t service_id, const std::string &url,
    const std::string &rest_path_matcher, const std::string &options,
    interface::AuthorizeManager *auth_manager)
    : Handler(url, rest_path_matcher, options, auth_manager),
      service_id_{service_id} {}

Handler::Authorization HandlerIsAuthorized::requires_authentication() const {
  return Authorization::kCheck;
}

uint64_t HandlerIsAuthorized::get_service_id() const { return service_id_; }

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
  log_debug("HandlerIsAuthorized::handle_get");
  helper::json::SerializerToText serializer;
  {
    database::QueryEntriesAuthRole roles;
    if (ctxt->user.has_user_id) {
      auto session = authorization_manager_->get_cache()->get_instance(
          collector::kMySQLConnectionMetadata);
      roles.query(session.get(), ctxt->user.user_id);
    }
    auto obj = serializer.add_object();
    obj->member_add_value(
        "status", ctxt->user.has_user_id ? "authorized" : "unauthorized");

    if (ctxt->user.has_user_id) {
      auto user = obj->member_add_object("user");
      user->member_add_value("name", ctxt->user.name);
      user->member_add_value("id", ctxt->user.user_id);

      auto roles_array = user->member_add_array("roles");
      for (const auto &r : roles.result) {
        roles_array->add_value(database::entry::to_string(r).c_str(),
                               helper::ColumnJsonTypes::kJson);
      }
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

bool HandlerIsAuthorized::may_check_access() const { return false; }

}  // namespace rest
}  // namespace mrs
