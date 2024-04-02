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

#include "mrs/rest/handler_authorize.h"

#include <cassert>

#include "helper/http/url.h"
#include "helper/json/to_string.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/http/utilities.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using HttpResult = HandlerAuthorize::HttpResult;
using Route = mrs::interface::Object;
using Url = helper::http::Url;

HandlerAuthorize::HandlerAuthorize(const UniversalId service_id,
                                   const std::string &url,
                                   const std::string &rest_path_matcher,
                                   const std::string &options,
                                   const std::string &redirection,
                                   interface::AuthorizeManager *auth_manager)
    : Handler(url, {rest_path_matcher}, options, auth_manager),
      service_id_{service_id},
      redirection_{redirection} {}

Handler::Authorization HandlerAuthorize::requires_authentication() const {
  return Authorization::kRequires;
}

UniversalId HandlerAuthorize::get_service_id() const { return service_id_; }

UniversalId HandlerAuthorize::get_db_object_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

UniversalId HandlerAuthorize::get_schema_id() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return {};
}

uint32_t HandlerAuthorize::get_access_rights() const {
  return Route::kRead | Route::kCreate;
}

HttpResult HandlerAuthorize::handle_get(
    RequestContext *ctxt) {  // TODO(lkotula): Add status to redirection URL:
                             // (Shouldn't be in review)
  // ?status=ok|failure
  // &status=ok|failure
  auto uri = append_status_parameters(ctxt, {HttpStatusCode::Ok});
  // Generate the response by the default handler.

  log_debug("HandlerAuthorize::handle_get - before redirects");
  if (ctxt->selected_handler->redirects())
    http::redirect_and_throw(ctxt->request, uri);
  log_debug("HandlerAuthorize::handle_get - no redirects");

  auto session = authorization_manager_->get_current_session(
      get_service_id(), ctxt->request->get_input_headers(), &ctxt->cookies);

  if (session && session->generate_token) {
    log_debug("HandlerAuthorize::handle_get - post");
    auto jwt_token =
        authorization_manager_->get_jwt_token(get_service_id(), session);
    session->generate_token = false;
    return HttpResult(HttpStatusCode::Ok,
                      helper::json::to_string({{"accessToken", jwt_token}}),
                      helper::MediaType::typeJson);
  }

  return {};
}

HttpResult HandlerAuthorize::handle_post(RequestContext *ctxt,
                                         const std::vector<uint8_t> &) {
  if (!ctxt->post_authentication) throw http::Error(HttpStatusCode::Forbidden);

  return handle_get(ctxt);
}

HttpResult HandlerAuthorize::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorize::handle_put(RequestContext *) {
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
  }
  http::SessionManager::Session dummy{"", UniversalId{}};
  session = session ? session : &dummy;

  ::http::base::Uri uri(session->users_on_complete_url_redirection.empty()
                            ? redirection_
                            : session->users_on_complete_url_redirection);

  if (!jwt_token.empty())
    Url::append_query_parameter(uri, "accessToken", jwt_token);
  if (!session->handler_name.empty())
    Url::append_query_parameter(uri, "app", session->handler_name);
  if (!session->users_on_complete_timeout.empty())
    Url::append_query_parameter(uri, "onCompletionClose",
                                session->users_on_complete_timeout);
  Url::append_query_parameter(uri, "login",
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
  if (HttpMethod::Options == ctxt->request->get_method()) return false;
  // Oauth2 authentication may redirect, allow it.
  Url url(ctxt->request->get_uri());

  auto session = authorization_manager_->get_current_session(
      get_service_id(), ctxt->request->get_input_headers(), &ctxt->cookies);

  if (session) {
    log_debug("session->onRedirect=url_param->onRedirect");
    url.get_if_query_parameter("onCompletionRedirect",
                               &session->users_on_complete_url_redirection);
    url.get_if_query_parameter("onCompletionClose",
                               &session->users_on_complete_timeout);
  }
  if (error.status == HttpStatusCode::TemporaryRedirect ||
      error.status == HttpStatusCode::TooManyRequests)
    return false;

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
