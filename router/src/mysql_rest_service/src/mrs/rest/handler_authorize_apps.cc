/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_authorize_apps.h"

#include <cassert>

#include "helper/json/serializer_to_text.h"
#include "mrs/http/cookie.h"
#include "mrs/http/url.h"
#include "mrs/http/utilities.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using Result = HandlerAuthorizeApps::Result;
using Route = mrs::interface::Object;

HandlerAuthorizeApps::HandlerAuthorizeApps(
    const UniversalId service_id, const std::string &url,
    const std::string &rest_path_matcher, const std::string &options,
    const std::string &redirection, interface::AuthorizeManager *auth_manager)
    : Handler(url, {rest_path_matcher}, options, auth_manager),
      service_id_{service_id},
      redirection_{redirection} {}

Handler::Authorization HandlerAuthorizeApps::requires_authentication() const {
  return Authorization::kNotNeeded;
}

UniversalId HandlerAuthorizeApps::get_service_id() const { return service_id_; }

UniversalId HandlerAuthorizeApps::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerAuthorizeApps::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

uint32_t HandlerAuthorizeApps::get_access_rights() const {
  return Route::kRead;
}

Result HandlerAuthorizeApps::handle_get(RequestContext *) {
  helper::json::SerializerToText serializer;
  serializer.add_array().add(
      authorization_manager_->get_supported_authentication_applications(
          service_id_));
  return {serializer.get_result(), Result::Type::typeJson};
}

Result HandlerAuthorizeApps::handle_post(RequestContext *,
                                         const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerAuthorizeApps::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerAuthorizeApps::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerAuthorizeApps::may_check_access() const { return false; }

}  // namespace rest
}  // namespace mrs
