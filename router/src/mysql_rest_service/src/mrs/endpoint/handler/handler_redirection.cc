/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler/handler_redirection.h"

#include <map>
#include <string>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using namespace std::string_literals;

template <typename T>
static std::string as_string(const std::vector<T> &v) {
  return std::string(v.begin(), v.end());
}

HandlerRedirection::HandlerRedirection(
    const Protocol protocol, const UniversalId &service_id,
    const std::string &service_path, bool requires_authentication,
    const std::string &url_host, const std::string &path,
    const std::string &file_name, const std::string &file_new_location,
    mrs::interface::AuthorizeManager *auth_manager, bool pernament)
    : Handler(protocol, url_host, {{path, false, false}}, std::string{},
              auth_manager),
      service_id_{service_id},
      service_path_{service_path},
      requires_authentication_{requires_authentication},
      path_{path},
      file_name_{file_name},
      file_new_location_{file_new_location},
      pernament_{pernament} {}

UniversalId HandlerRedirection::get_service_id() const { return service_id_; }

UniversalId HandlerRedirection::get_schema_id() const { return {}; }

UniversalId HandlerRedirection::get_db_object_id() const { return {}; }

const std::string &HandlerRedirection::get_service_path() const {
  return service_path_;
}

const std::string &HandlerRedirection::get_schema_path() const {
  return empty_path();
}

const std::string &HandlerRedirection::get_db_object_path() const {
  return empty_path();
}

HandlerRedirection::Authorization HandlerRedirection::requires_authentication()
    const {
  return requires_authentication_ ? Authorization::kCheck
                                  : Authorization::kNotNeeded;
}

uint32_t HandlerRedirection::get_access_rights() const {
  return mrs::database::entry::Operation::valueRead;
}

void HandlerRedirection::authorization(rest::RequestContext *) {}

HandlerRedirection::HttpResult HandlerRedirection::handle_get(
    rest::RequestContext *ctx) {
  const auto request = ctx->request;

  std::string redirect = file_new_location_;
  if (!request->get_uri().get_query().empty()) {
    redirect += "?" + request->get_uri().get_query();
  }
  if (!request->get_uri().get_fragment().empty()) {
    redirect += "#" + request->get_uri().get_fragment();
  }

  throw http::ErrorRedirect(redirect, pernament_);
}

HandlerRedirection::HttpResult HandlerRedirection::handle_delete(
    rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HandlerRedirection::HttpResult HandlerRedirection::handle_post(
    rest::RequestContext *, const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HandlerRedirection::HttpResult HandlerRedirection::handle_put(
    rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
