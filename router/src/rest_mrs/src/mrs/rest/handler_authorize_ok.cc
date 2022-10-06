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

#include "mrs/rest/handler_authorize_ok.h"

#include <cassert>

#include "mrs/http/cookie.h"
#include "mrs/http/url.h"
#include "mrs/http/utilities.h"
#include "mrs/interface/route.h"
#include "mrs/rest/handler_request_context.h"

namespace mrs {
namespace rest {

using Result = HandlerAuthorizeOk::Result;
using Route = mrs::interface::Route;

// clang-format off
const std::string k_page_content_default = R"HEREDOC(
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>Login completed.</title>
    <style>
        html, body {
            height: 100%;
            overflow: hidden;
        }
        .main {
            display: flex;
            align-items: center;
            justify-content: center;
            height: 100%;
            font-family: Helvetica, Arial, sans-serif;
            font-weight: 200;
        }
    </style>
  </head>
  <body>
    <div class="main">
        <p>Login completed.</p>
    </div>
  </body>
</html>
)HEREDOC";
// clang-format on

HandlerAuthorizeOk::HandlerAuthorizeOk(const uint64_t id,
                                       const std::string &url,
                                       const std::string &rest_path_matcher,
                                       const std::string &options,
                                       const std::string &page_content_custom,
                                       interface::AuthManager *auth_manager)
    : Handler(url, rest_path_matcher, options, auth_manager),
      id_{id},
      page_content_custom_{page_content_custom} {}

Handler::Authorization HandlerAuthorizeOk::requires_authentication() const {
  return Authorization::kCheck;
}

std::pair<IdType, uint64_t> HandlerAuthorizeOk::get_id() const {
  return {IdType::k_id_type_auth_id, id_};
}

uint64_t HandlerAuthorizeOk::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return 0;
}

uint64_t HandlerAuthorizeOk::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return 0;
}

uint32_t HandlerAuthorizeOk::get_access_rights() const { return Route::kRead; }

Result HandlerAuthorizeOk::handle_get(RequestContext *) {
  if (page_content_custom_.empty())
    return {k_page_content_default, helper::MediaType::typeHtml};

  return {page_content_custom_, helper::MediaType::typeHtml};
}

Result HandlerAuthorizeOk::handle_post(RequestContext *,
                                       const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerAuthorizeOk::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerAuthorizeOk::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

}  // namespace rest
}  // namespace mrs
