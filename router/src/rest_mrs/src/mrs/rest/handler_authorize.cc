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
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using Result = HandlerAuthorize::Result;
using Route = mrs::interface::Object;

HandlerAuthorize::HandlerAuthorize(const uint64_t service_id,
                                   const std::string &url,
                                   const std::string &rest_path_matcher,
                                   const std::string &options,
                                   const std::string &redirection,
                                   interface::AuthorizeManager *auth_manager)
    : Handler(url, rest_path_matcher, options, auth_manager),
      service_id_{service_id},
      redirection_{redirection} {}

Handler::Authorization HandlerAuthorize::requires_authentication() const {
  return Authorization::kRequires;
}

uint64_t HandlerAuthorize::get_service_id() const { return service_id_; }

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

static const char *get_authentication_status(HttpStatusCode::key_type code) {
  switch (code) {
    case HttpStatusCode::Ok:
      return "success";
    case HttpStatusCode::Unauthorized:
      return "fail";
    default:
      return "fail";
  }
}

std::string HandlerAuthorize::append_status_parameters(
    RequestContext *ctxt, const http::Error &error) {
  auto session = authorization_manager_->get_current_session(
      get_service_id(), ctxt->request->get_input_headers(), &ctxt->cookies);

  std::string jwt_token;
  if (session && session->generate_token &&
      error.status == HttpStatusCode::Ok) {
    jwt_token =
        authorization_manager_->get_jwt_token(get_service_id(), session);
    session->generate_token = false;
  }
  http::SessionManager::Session dummy{"", 0};
  session = session ? session : &dummy;

  auto uri = HttpUri::parse(session->users_on_complete_url_redirection.empty()
                                ? redirection_
                                : session->users_on_complete_url_redirection);

  if (!jwt_token.empty())
    http::Url::append_query_parameter(uri, "accessToken", jwt_token);
  if (!session->handler_name.empty())
    http::Url::append_query_parameter(uri, "app", session->handler_name);
  if (!session->users_on_complete_timeout.empty())
    http::Url::append_query_parameter(uri, "onCompletionClose",
                                      session->users_on_complete_timeout);
  http::Url::append_query_parameter(uri, "login",
                                    get_authentication_status(error.status));

  //  if (HttpStatusCode::Ok == error.status) {
  //    http::Url::append_query_parameter(uri, "user_id",
  //                                      std::to_string(ctxt->user.user_id));
  //    http::Url::append_query_parameter(uri, "user_name", ctxt->user.name);
  //  } else if (HttpStatusCode::Unauthorized != error.status) {
  //    http::Url::append_query_parameter(uri, "message", error.message);
  //  }

  return uri.join();
}

bool HandlerAuthorize::request_error(RequestContext *ctxt,
                                     const http::Error &error) {
  // Oauth2 authentication may redirect, allow it.
  http::Url url(ctxt->request->get_uri());

  auto session = authorization_manager_->get_current_session(
      get_service_id(), ctxt->request->get_input_headers(), &ctxt->cookies);

  if (session) {
    log_debug("session->onRedirect=url_param->onRedirect");
    url.get_if_query_parameter("onCompletionRedirect",
                               &session->users_on_complete_url_redirection);
    url.get_if_query_parameter("onCompletionClose",
                               &session->users_on_complete_timeout);
  }
  if (error.status == HttpStatusCode::TemporaryRedirect) return false;

  // Redirect to original/first page that redirected to us.
  auto uri = append_status_parameters(ctxt, error);
  ctxt->request->send_reply(http::redirect(ctxt->request, uri.c_str()));
  authorization_manager_->discard_current_session(get_service_id(),
                                                  &ctxt->cookies);
  return true;
}

bool HandlerAuthorize::may_check_access() const { return false; }

}  // namespace rest
}  // namespace mrs
