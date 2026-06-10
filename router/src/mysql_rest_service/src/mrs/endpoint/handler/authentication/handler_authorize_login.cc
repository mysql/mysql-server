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

#include "mrs/endpoint/handler/authentication/handler_authorize_login.h"

#include <cassert>

#include "helper/http/url.h"
#include "helper/json/to_string.h"
#include "mrs/http/cookie.h"
#include "mrs/http/error.h"
#include "mrs/http/utilities.h"
#include "mrs/rest/request_context.h"
#include "mysql/harness/regex_matcher.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using Session = HandlerAuthorizeLogin::Session;
using HttpResult = HandlerAuthorizeLogin::HttpResult;
using Url = helper::http::Url;

HandlerAuthorizeLogin::HandlerAuthorizeLogin(
    const Protocol protocol, const std::string &url_host,
    const UniversalId service_id, const std::string &service_path,
    const UriPathMatcher &rest_path_matcher, const std::string &options,
    const std::string &redirection,
    const std::optional<std::string> &redirection_validator,
    interface::AuthorizeManager *auth_manager)
    : HandlerAuthorizeBase(protocol, url_host, {rest_path_matcher}, options,
                           auth_manager),
      service_id_{service_id},
      service_path_{service_path},
      redirection_{redirection} {
  if (redirection_validator.has_value()) {
    redirection_validator_ = std::make_shared<mysql_harness::RegexMatcher>(
        redirection_validator.value());
    const auto is_valid = redirection_validator_->is_valid();
    if (!is_valid) {
      log_error(
          "Redirection pattern for 'onCompletionRedirect' parameter is "
          "invalid. "
          "Compilation returned: %s",
          is_valid.error().c_str());
    }
  }
}

mrs::rest::Handler::Authorization
HandlerAuthorizeLogin::requires_authentication() const {
  return Authorization::kRequires;
}

UniversalId HandlerAuthorizeLogin::get_service_id() const {
  return service_id_;
}

UniversalId HandlerAuthorizeLogin::get_schema_id() const { return {}; }

UniversalId HandlerAuthorizeLogin::get_db_object_id() const { return {}; }

const std::string &HandlerAuthorizeLogin::get_service_path() const {
  return service_path_;
}

const std::string &HandlerAuthorizeLogin::get_db_object_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

const std::string &HandlerAuthorizeLogin::get_schema_path() const {
  assert(0 && "is_object returns false, it is not allowed to call this method");
  return empty_path();
}

uint32_t HandlerAuthorizeLogin::get_access_rights() const {
  using Op = mrs::database::entry::Operation::Values;
  return Op::valueRead | Op::valueCreate;
}

void HandlerAuthorizeLogin::set_session_cookie(RequestContext *ctxt) const {
  assert(ctxt->session && "handle_get/post should have checked it");

  const auto &session_cookie_key = ctxt->session->get_holder_name();
  if (!session_cookie_key.empty()) {
    http::Cookie::SameSite same_site = http::Cookie::None;
    ctxt->cookies.set(session_cookie_key, ctxt->session->get_session_id(),
                      http::Cookie::duration{0}, "/", &same_site, true, true,
                      {});
  }
}

HttpResult HandlerAuthorizeLogin::handle_get(RequestContext *ctxt) {
  log_debug("HandlerAuthorizeLogin::handle_get - before redirects");

  if (!ctxt->session) {
    throw http::Error(HttpStatusCode::Unauthorized);
  }

  if (ctxt->selected_handler->redirects(*ctxt)) {
    if (!ctxt->session->generate_token) set_session_cookie(ctxt);

    auto uri = append_status_parameters(ctxt->session, {HttpStatusCode::Ok});
    http::redirect_and_throw(ctxt->request, uri);
  }

  log_debug(
      "HandlerAuthorizeLogin::handle_get - no redirects ("
      "generate_token:%s)",
      ctxt->session->generate_token ? "yes" : "no");

  if (!ctxt->session->generate_token) {
    set_session_cookie(ctxt);
    return HttpResult(HttpStatusCode::Ok, "{}", helper::MediaType::typeJson);
  }

  log_debug("HandlerAuthorizeLogin::handle_get - post");
  auto jwt_token =
      authorization_manager_->get_jwt_token(get_service_id(), ctxt->session);
  ctxt->session->generate_token = false;
  return HttpResult(HttpStatusCode::Ok,
                    helper::json::to_string({{"accessToken", jwt_token}}),
                    helper::MediaType::typeJson);
}

HttpResult HandlerAuthorizeLogin::handle_post(RequestContext *ctxt,
                                              const std::vector<uint8_t> &) {
  if (!ctxt->post_authentication) throw http::Error(HttpStatusCode::Forbidden);

  return handle_get(ctxt);
}

HttpResult HandlerAuthorizeLogin::handle_delete(RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerAuthorizeLogin::handle_put(RequestContext *) {
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

std::string HandlerAuthorizeLogin::append_status_parameters(
    const SessionPtr &session, const http::Error &error) const {
  std::string jwt_token;
  if (session && session->generate_token &&
      error.status == HttpStatusCode::Ok) {
    jwt_token =
        authorization_manager_->get_jwt_token(get_service_id(), session);
  }
  http::SessionManager::Session dummy{nullptr, "", UniversalId{}, ""};
  auto session_ptr = session ? session.get() : &dummy;

  ::http::base::Uri uri(
      session_ptr->users_on_complete_url_redirection.value_or(redirection_));

  if (!jwt_token.empty())
    Url::append_query_parameter(uri, "accessToken", jwt_token);
  if (!session_ptr->handler_name.empty())
    Url::append_query_parameter(uri, "authApp", session_ptr->handler_name);
  if (!session_ptr->users_on_complete_timeout.empty())
    Url::append_query_parameter(uri, "onCompletionClose",
                                session_ptr->users_on_complete_timeout);
  Url::append_query_parameter(uri, "login",
                              get_authentication_status(error.status));

  // Best practices of handling URL redirection point that
  // fragment should be blocked in some way.
  // We are not forwarding it.
  uri.set_fragment({});

  return uri.join();
}

bool HandlerAuthorizeLogin::request_begin(RequestContext *ctxt) {
  ctxt->redirection_validator = redirection_validator_;
  return true;
}

bool HandlerAuthorizeLogin::request_error(RequestContext *ctxt,
                                          const http::Error &error) {
  if (HandlerAuthorizeBase::request_error(ctxt, error)) return true;

  if (HttpMethod::Options == ctxt->request->get_method()) return false;

  if (ctxt->post_authentication) return false;

  if (error.status == HttpStatusCode::TemporaryRedirect ||
      error.status == HttpStatusCode::TooManyRequests)
    return false;
  // Oauth2 authentication may redirect, allow it.
  Url url(ctxt->request->get_uri());

  log_debug(
      "HandlerAuthorizeLogin::request_error - trying to overwrite  the error: "
      "%i with redirect",
      (int)error.status);

  // Redirect to original/first page that redirected to us.
  auto uri = append_status_parameters(ctxt->session, error);
  ctxt->request->send_reply(http::redirect(ctxt->request, uri.c_str()));
  authorization_manager_->discard_current_session(get_service_id(),
                                                  &ctxt->cookies);
  return true;
}

bool HandlerAuthorizeLogin::may_check_access() const { return false; }

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
