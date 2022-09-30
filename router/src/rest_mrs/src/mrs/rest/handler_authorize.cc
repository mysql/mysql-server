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

#include "mrs/rest/handler_authorize.h"

#include <cassert>

#include "mrs/http/cookie.h"
#include "mrs/http/url.h"
#include "mrs/http/utilities.h"
#include "mrs/interface/route.h"
#include "mrs/rest/handler_request_context.h"

namespace mrs {
namespace rest {

using Result = HandlerAuthorize::Result;
using Route = mrs::interface::Route;

HandlerAuthorize::HandlerAuthorize(const uint64_t id, const std::string &url,
                                   const std::string &rest_path_matcher,
                                   interface::AuthManager *auth_manager)
    : Handler(url, rest_path_matcher, auth_manager), id_{id} {}

Handler::Authorization HandlerAuthorize::requires_authentication() const {
  return Authorization::kRequires;
}

std::pair<IdType, uint64_t> HandlerAuthorize::get_id() const {
  return {IdType::k_id_type_auth_id, id_};
}

uint64_t HandlerAuthorize::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return 0;
}

uint64_t HandlerAuthorize::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return 0;
}

uint32_t HandlerAuthorize::get_access_rights() const { return Route::kRead; }

Result HandlerAuthorize::handle_get(
    RequestContext *ctxt) {  // TODO(lkotula): Add status to redirection URL:
                             // (Shouldn't be in review)
  // ?status=ok|failure
  // &status=ok|failure
  auto uri = append_status_parameters(ctxt, {HttpStatusCode::Ok});
  // Generate the response by the default handler.
  http::redirect_and_throw(ctxt->request, uri);
  return {};
}

Result HandlerAuthorize::handle_post(RequestContext *,
                                     const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerAuthorize::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Result HandlerAuthorize::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

bool HandlerAuthorize::request_begin(RequestContext *ctxt) {
  const char *kMrsRedirection = "mrs_auth_redirect";
  auto &url = ctxt->request->get_uri();
  http::Url url_parser(url);
  auto &redirection_value = ctxt->handler_authentication_redirection;

  redirection_value = url_parser.get_query_parameter("mrs_redirect");
  bool has_mrs_redirect = !redirection_value.empty();

  if (has_mrs_redirect) {
    using namespace std::literals::chrono_literals;

    http::Cookie::set(ctxt->request, kMrsRedirection, redirection_value, 15min);
  } else {
    redirection_value = http::Cookie::get(ctxt->request, kMrsRedirection);
  }

  if (redirection_value.empty()) {
    return false;
  }

  // We do not want any parameters on callback from Oauth2
  // Its the first redirect, oauth2 handlers on provider side
  // doesn't like custom query parameters.
  if (has_mrs_redirect) url.set_query("");

  return true;
}

void HandlerAuthorize::request_end(RequestContext *) {}

const char *get_authentication_status(HttpStatusCode::key_type code) {
  switch (code) {
    case HttpStatusCode::Ok:
      return "authorized";
    case HttpStatusCode::Unauthorized:
      return "unauthorized";
    default:
      return "error";
  }
}

std::string HandlerAuthorize::append_status_parameters(
    RequestContext *ctxt, const http::Error &error) {
  auto uri = HttpUri::parse(ctxt->handler_authentication_redirection);

  http::Url::append_query_parameter(uri, "status",
                                    get_authentication_status(error.status));

  if (HttpStatusCode::Ok == error.status) {
    http::Url::append_query_parameter(uri, "user_id",
                                      std::to_string(ctxt->user.user_id));
    http::Url::append_query_parameter(uri, "user_name", ctxt->user.name);
  } else if (HttpStatusCode::Unauthorized != error.status) {
    http::Url::append_query_parameter(uri, "message", error.message);
  }

  return uri.join();
}

bool HandlerAuthorize::request_error(RequestContext *ctxt,
                                     const http::Error &error) {
  // Oauth2 authentication may redirect, allow it.
  if (error.status == HttpStatusCode::TemporaryRedirect) return false;
  // If handler_authentication_redirection is empthy then the flow is broken,
  // just return an error to the user.
  if (ctxt->handler_authentication_redirection.empty()) {
    ctxt->request->send_error(error.status, error.message);
    return true;
  }

  // Redirect to original/first page that redirected to us.
  auto uri = append_status_parameters(ctxt, error);
  ctxt->request->send_reply(http::redirect(ctxt->request, uri.c_str()));
  return true;
}

}  // namespace rest
}  // namespace mrs
