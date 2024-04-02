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

#include "mrs/rest/handler_authorize_ok.h"

#include <cassert>

#include "helper/http/url.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/http/utilities.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace rest {

using HttpResult = HandlerAuthorizeOk::HttpResult;
using Route = mrs::interface::Object;

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
        button {
          display: flex;
          flex-direction: column;
          align-items: center;
          padding: 6px 26px;
          font-family: -apple-system, BlinkMacSystemFont, 'Roboto', sans-serif;
          font-weight: 300;
          border-radius: 6px;
          border: none;
          background: #6E6D70;
          box-shadow: 0px 0.5px 1px rgba(0, 0, 0, 0.1), inset 0px 0.5px 0.5px rgba(255, 255, 255, 0.5), 0px 0px 0px 0.5px rgba(0, 0, 0, 0.12);
          color: #DFDEDF;
          user-select: none;
          -webkit-user-select: none;
          touch-action: manipulation;
        }
        button:active {
          box-shadow: 0px 1px 4px rgba(0, 0, 0, 0.3) inset;
        }
        .main {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100%;
            font-family: Helvetica, Arial, sans-serif;
            font-weight: 200;
        }
        .hidden {
          display: none;
        }
​
        @media (prefers-color-scheme: dark) {
          html, body {
            background-color: #181818;
            color: #aaa;
          }
          button {
            box-shadow: 0px 0.5px 1px rgba(0, 0, 0, 0.1);
            background-color: #ccc;
            color: #3D3D3D;
          }
        }
    </style>
  </head>
  <body>
    <div class="main">
        <p id="loginStatus">Login completed.</p>
        <button id="closeBtn" class="hidden" onClick="window.close();">Close</button>
    </div>
    <script>
      (function() {
        // Fetch URL parameters
        const params = new Proxy(new URLSearchParams(window.location.search), {
          get: (searchParams, prop) => searchParams.get(prop),
        });
​
        // Helper function to check if value is an integer
        function isInt(value) {
          return !isNaN(value) && 
                parseInt(Number(value)) == value && 
                !isNaN(parseInt(value, 10));
        }
​
        // Handle login parameter values
        if (params.login === "success") {
          document.getElementById("loginStatus").innerHTML = "Login completed successfully.";
        } else if (params.login) {
          document.getElementById("loginStatus").innerHTML = "Login failed.";
        }
​
        // Handle onCompletionClose parameter values. This only works in popup windows.
        if (params.onCompletionClose === "manual") {
          document.getElementById("closeBtn").classList.remove("hidden");
        } else if (isInt(params.onCompletionClose)) {
          setTimeout(() => { window.close(); }, 1000 * parseInt(params.onCompletionClose, 10));
        }
      }())
    </script>
  </body>
</html>
)HEREDOC";
// clang-format on

HandlerAuthorizeOk::HandlerAuthorizeOk(
    const UniversalId service_id, const std::string &url,
    const std::string &rest_path_matcher, const std::string &options,
    const std::string &page_content_custom,
    interface::AuthorizeManager *auth_manager)
    : Handler(url, {rest_path_matcher}, options, auth_manager),
      service_id_{service_id},
      page_content_custom_{page_content_custom} {}

Handler::Authorization HandlerAuthorizeOk::requires_authentication() const {
  return Authorization::kCheck;
}

UniversalId HandlerAuthorizeOk::get_service_id() const { return service_id_; }

UniversalId HandlerAuthorizeOk::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerAuthorizeOk::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

uint32_t HandlerAuthorizeOk::get_access_rights() const { return Route::kRead; }

HttpResult HandlerAuthorizeOk::handle_get(RequestContext *) {
  if (page_content_custom_.empty())
    return {k_page_content_default, helper::MediaType::typeHtml};

  return {page_content_custom_, helper::MediaType::typeHtml};
}

HttpResult HandlerAuthorizeOk::handle_post(RequestContext *,
                                           const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeOk::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeOk::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerAuthorizeOk::may_check_access() const { return false; }

}  // namespace rest
}  // namespace mrs
