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

#include "mrs/endpoint/handler/authentication/handler_authorize_auth_apps.h"

#include <cassert>

#include "helper/http/url.h"
#include "helper/json/serializer_to_text.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/http/utilities.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = HandlerAuthorizeAuthApps::HttpResult;

HandlerAuthorizeAuthApps::HandlerAuthorizeAuthApps(
    const Protocol protocol, const std::string &url_host,
    const UniversalId service_id, const std::string &service_path,
    const UriPathMatcher &rest_path_matcher, const std::string &options,
    const std::string &redirection, interface::AuthorizeManager *auth_manager)
    : HandlerAuthorizeBase(protocol, url_host, {rest_path_matcher}, options,
                           auth_manager),
      service_id_{service_id},
      service_path_{service_path},
      redirection_{redirection} {}

mrs::rest::Handler::Authorization
HandlerAuthorizeAuthApps::requires_authentication() const {
  return Authorization::kNotNeeded;
}

UniversalId HandlerAuthorizeAuthApps::get_service_id() const {
  return service_id_;
}

UniversalId HandlerAuthorizeAuthApps::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerAuthorizeAuthApps::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

const std::string &HandlerAuthorizeAuthApps::get_service_path() const {
  return service_path_;
}

const std::string &HandlerAuthorizeAuthApps::get_schema_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

const std::string &HandlerAuthorizeAuthApps::get_db_object_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

uint32_t HandlerAuthorizeAuthApps::get_access_rights() const {
  using Op = mrs::database::entry::Operation::Values;
  return Op::valueRead;
}

HttpResult HandlerAuthorizeAuthApps::handle_get(RequestContext *) {
  helper::json::SerializerToText serializer;
  using namespace std::string_literals;
  using AuthorizeHandlerPtr =
      ::mrs::interface::AuthorizeManager::AuthorizeHandlerPtr;
  auto auth_apps =
      authorization_manager_->get_supported_authentication_applications(
          service_id_);

  // Stabilize the output
  std::sort(
      auth_apps.begin(), auth_apps.end(),
      [](const AuthorizeHandlerPtr &lhs, const AuthorizeHandlerPtr &rhs) {
        return lhs->get_entry().app_name.compare(rhs->get_entry().app_name) < 0;
      });
  {
    auto arr = serializer.add_array();
    for (auto &app : auth_apps) {
      auto obj = arr->add_object();
      auto &entry = app->get_entry();
      obj->member_add_value("name", entry.app_name);
      obj->member_add_value("vendorId", "0x"s + entry.vendor_id.to_string());
    }
  }

  return {serializer.get_result(), HttpResult::Type::typeJson};
}

HttpResult HandlerAuthorizeAuthApps::handle_post(RequestContext *,
                                                 const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeAuthApps::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeAuthApps::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerAuthorizeAuthApps::may_check_access() const { return false; }

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
