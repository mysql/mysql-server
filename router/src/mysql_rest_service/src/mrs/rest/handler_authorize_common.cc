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

#include "mrs/rest/handler_authorize_common.h"

#include <cassert>

#include "helper/http/url.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/http/utilities.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace rest {

using HttpResult = HandlerAuthorizeCommon::HttpResult;
using Route = mrs::interface::Object;

HandlerAuthorizeCommon::HandlerAuthorizeCommon(
    const UniversalId service_id, const std::string &url,
    const std::string &rest_path_matcher, const std::string &options,
    const std::string &redirection, interface::AuthorizeManager *auth_manager)
    : Handler(url, {rest_path_matcher}, options, auth_manager),
      service_id_{service_id},
      redirection_{redirection} {}

Handler::Authorization HandlerAuthorizeCommon::requires_authentication() const {
  return Authorization::kCheck;
}

UniversalId HandlerAuthorizeCommon::get_service_id() const {
  return service_id_;
}

UniversalId HandlerAuthorizeCommon::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerAuthorizeCommon::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

uint32_t HandlerAuthorizeCommon::get_access_rights() const {
  return Route::kRead;
}

void HandlerAuthorizeCommon::authorization(RequestContext *) {
  //  http::Url url{ctxt->request->get_uri()};
  //  database::entry::AuthUser user;
  //  if (!authorization_manager_->authorize(
  //          get_service_id(), &ctxt->cookies, &url, &ctxt->sql_session_cache,
  //          ctxt->request->get_input_headers(), &user)) {
  //    auto uri = append_status_parameters(ctxt, {HttpStatusCode::Ok});
  //    // Generate the response by the default handler.
  //    http::redirect_and_throw(ctxt->request, uri);
  //  }
}

HttpResult HandlerAuthorizeCommon::handle_get(
    RequestContext *ctxt) {  // TODO(lkotula): Add status to redirection URL:
                             // (Shouldn't be in review)
  // ?status=ok|failure
  // &status=ok|failure
  auto uri = append_status_parameters(ctxt, {HttpStatusCode::Ok});
  // Generate the response by the default handler.
  http::redirect_and_throw(ctxt->request, uri);
  return {};
}

HttpResult HandlerAuthorizeCommon::handle_post(RequestContext *,
                                               const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeCommon::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeCommon::handle_put(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

static const char *get_authentication_status(HttpStatusCode::key_type code) {
  switch (code) {
    case HttpStatusCode::Ok:
      return "authorized";
    case HttpStatusCode::Unauthorized:
      return "unauthorized";
    default:
      return "error";
  }
}

std::string HandlerAuthorizeCommon::append_status_parameters(
    RequestContext *ctxt, const http::Error &error) {
  HttpUri uri{redirection_};

  Url::append_query_parameter(uri, "status",
                              get_authentication_status(error.status));

  if (HttpStatusCode::Ok == error.status) {
    Url::append_query_parameter(uri, "user_id", ctxt->user.user_id.to_string());
    Url::append_query_parameter(uri, "user_name", ctxt->user.name);
  } else if (HttpStatusCode::Unauthorized != error.status) {
    Url::append_query_parameter(uri, "message", error.message);
  }

  return uri.join();
}

bool HandlerAuthorizeCommon::request_error(RequestContext *ctxt,
                                           const http::Error &error) {
  // Oauth2 authentication may redirect, allow it.
  if (error.status == HttpStatusCode::TemporaryRedirect) return false;

  // Redirect to original/first page that redirected to us.
  auto uri = append_status_parameters(ctxt, error);
  ctxt->request->send_reply(http::redirect(ctxt->request, uri.c_str()));
  return true;
}

bool HandlerAuthorizeCommon::may_check_access() const { return false; }

}  // namespace rest
}  // namespace mrs
