/*
 Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/authentication/authorize_manager.h"

#include <cassert>
#include <iomanip>
#include <memory>
#include <sstream>

#include "mrs/authentication/auth_handler_factory.h"
#include "mrs/authentication/track_authorize_handler.h"
#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/rest/handler_authorize.h"
#include "mrs/rest/handler_authorize_ok.h"
#include "mrs/rest/handler_is_authorized.h"
#include "mrs/rest/handler_unauthorize.h"

#include "helper/container/generic.h"
#include "helper/container/map.h"
#include "helper/make_shared_ptr.h"
#include "helper/replace_string.h"
#include "helper/token/jwt.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using JwtHolder = helper::JwtHolder;
using Jwt = helper::Jwt;
using Handlers = AuthorizeManager::AuthHandlers;
using AuthorizeHandlerPtr = AuthorizeManager::AuthorizeHandlerPtr;

static Jwt get_bearer_token_jwt(const HttpHeaders &headers) {
  auto authorization = headers.get(WwwAuthenticationHandler::kAuthorization);

  if (!authorization) return {};

  auto args = mysql_harness::split_string(authorization, ' ', false);
  std::string value = args.size() > 1 ? args[1] : "";

  log_debug("authorization: \"%s\"", authorization);

  JwtHolder holder;

  if (!helper::Jwt::parse(value, &holder)) return {};

  auto jwt = helper::Jwt::create(holder);

  return jwt;
}

static std::string get_session_cookie_key_name(
    const AuthorizeManager::ServiceId id) {
  using namespace std::literals::string_literals;
  return "session_"s + std::to_string(id);
}

AuthorizeManager::AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                                   const std::string &jwt_secret,
                                   AuthHandlerFactoryPtr factory)
    : cache_manager_{cache_manager},
      jwt_secret_{jwt_secret},
      factory_{factory} {
  log_info("JWT bearer authorization disabled, the signing secret is empty.");
}

AuthorizeManager::AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                                   const std::string &jwt_secret)
    : cache_manager_{cache_manager},
      jwt_secret_{jwt_secret},
      factory_{std::make_shared<AuthHandlerFactory>()} {}

void AuthorizeManager::update(const Entries &entries) {
  Container::iterator it;

  if (entries.size()) {
    log_debug("auth_app: Number of updated entries:%i", (int)entries.size());
  }

  for (const auto &e : entries) {
    log_debug("auth_app: Processing update of id=%i", (int)e.id);
    auto auth = make_auth(e);

    if (get_handler_by_id(e.id, &it)) {
      *it = auth;

      if (!auth) container_.erase(it);
    } else {
      if (auth) container_.push_back(auth);
    }
  }
}

AuthorizeManager::Container AuthorizeManager::get_handlers_by_service_id(
    const uint64_t service_id) {
  Container out_result;

  helper::container::copy_if(
      container_,
      [service_id](auto &element) {
        return element->get_service_id() == service_id;
      },
      out_result);

  return out_result;
}

bool AuthorizeManager::get_handler_by_id(const uint64_t auth_id,
                                         Container::iterator *out_it) {
  auto it = std::find_if(container_.begin(), container_.end(),
                         [auth_id](auto &i) { return i->get_id() == auth_id; });

  if (it != container_.end()) {
    *out_it = it;
    return true;
  }

  return false;
}

bool AuthorizeManager::get_handler_by_id(const uint64_t auth_id,
                                         AuthorizeHandlerPtr &out_it) {
  Container::iterator it;
  if (get_handler_by_id(auth_id, &it)) {
    out_it = *it;
    return true;
  }
  return false;
}

AuthorizeHandlerPtr AuthorizeManager::make_auth(const AuthApp &entry) {
  // TODO(lkotula): Rework this (Shouldn't be in review)
  AuthorizeHandlerPtr result;

  if (entry.deleted) return {};

  if (!entry.active) return {};

  if (entry.name == "MySQL Basic")
    result = factory_->create_basic_auth_handler(entry, cache_manager_);
  else if (entry.name == "Facebook")
    result = factory_->create_facebook_auth_handler(entry);
  else if (entry.name == "Twitter")
    result = factory_->create_twitter_auth_handler(entry);
  else if (entry.name == "Google")
    result = factory_->create_google_auth_handler(entry);

  if (result) {
    helper::AuthorizeHandlerCallbakcs *callbacks = this;
    result = std::make_shared<TrackAuthorizeHandler<
        AuthorizeManager *, helper::AuthorizeHandlerCallbakcs *>>(result, this,
                                                                  callbacks);
  }

  return result;
}

void AuthorizeManager::fill_service(const AuthApp &e, ServiceAuthorize &sa) {
  std::string auth_path =
      !e.auth_path.empty() ? e.auth_path : "/authentication";
  std::string path1 = "^" + e.service_name + auth_path + "/login$";
  std::string path2 = "^" + e.service_name + auth_path + "/status$";
  std::string path3 = "^" + e.service_name + auth_path + "/logout$";
  std::string path4 = "^" + e.service_name + auth_path + "/completed";
  std::string redirect = e.redirect;

  if (redirect.empty()) {
    redirect = e.host + e.service_name + auth_path + "/completed";
  }

  auto login_handler = std::make_shared<mrs::rest::HandlerAuthorize>(
      e.service_id, e.service_name, path1, e.options, redirect, this);
  auto status_handler = std::make_shared<mrs::rest::HandlerIsAuthorized>(
      e.service_id, e.service_name, path2, e.options, this);
  auto unauth_handler = std::make_shared<mrs::rest::HandlerUnauthorize>(
      e.service_id, e.service_name, path3, e.options, this);
  auto auth_ok_handler = std::make_shared<mrs::rest::HandlerAuthorizeOk>(
      e.service_id, e.service_name, path4, e.options,
      e.redirection_default_page, this);

  sa.authorize_handler_ = login_handler;
  sa.status_handler_ = status_handler;
  sa.unauthorize_handler_ = unauth_handler;
  sa.authorization_result_handler_ = auth_ok_handler;
}

void AuthorizeManager::acquire(interface::AuthorizeHandler *handler) {
  ServiceAuthorizePtr out_service_authorization;
  auto lock = std::unique_lock(service_authorize_mutext_);

  if (!helper::container::get_value(service_authorize_,
                                    handler->get_service_id(),
                                    &out_service_authorization)) {
    helper::MakeSharedPtr<ServiceAuthorize> service_authorization;

    fill_service(handler->get_entry(), *service_authorization);

    service_authorize_[handler->get_service_id()] =
        service_authorization.copy_base();
    return;
  } else {
    fill_service(handler->get_entry(), *out_service_authorization);
  }
  ++out_service_authorization->references_;
}

void AuthorizeManager::destroy(interface::AuthorizeHandler *handler) {
  auto lock = std::unique_lock(service_authorize_mutext_);
  auto &el = service_authorize_[handler->get_service_id()];

  if (0 == --el->references_) {
    service_authorize_.erase(handler->get_service_id());
  }
}

bool AuthorizeManager::unauthorize(ServiceId service_id,
                                   http::Cookie *cookies) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = cookies->get(session_cookie_key);

  if (session_identifier.empty()) return false;

  return session_manager_.remove_session(session_identifier);
}

static std::string current_timestamp(std::chrono::system_clock::duration d) {
  auto now = std::chrono::system_clock::now() + d;
  std::time_t tt = std::chrono::system_clock::to_time_t(now);

  std::ostringstream os;
  os << std::put_time(gmtime(&tt), "%F %T");

  return os.str();
}

static bool is_timestamp_in_past(const std::string ts) {
  std::tm t = {};
  std::istringstream ss(ts);
  ss >> std::get_time(&t, "%Y-%m-%d %T");
  if (ss.fail()) return true;

  auto past = timegm(&t);
  auto current =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  if (past == -1 || current == -1) return true;

  if (difftime(current, past) <= 0.0) return false;

  return true;
}

std::string AuthorizeManager::get_jwt_token(uint64_t service_id, Session *s) {
  using namespace rapidjson;
  Document payload;
  auto exp = current_timestamp(session_manager_.get_timeout());

  payload.SetObject();
  payload.AddMember(StringRef("user_id"), s->user.user_id,
                    payload.GetAllocator());
  payload.AddMember(StringRef("exp"), StringRef(exp.c_str()),
                    payload.GetAllocator());
  payload.AddMember(StringRef("service_id"), service_id,
                    payload.GetAllocator());

  auto jwt = helper::Jwt::create("HS256", payload);

  auto token = jwt.sign(jwt_secret_);

  std::string session_id = std::to_string(service_id) + "." +
                           std::to_string(s->user.user_id) + "." + exp;
  if (session_manager_.get_session(session_id)) return session_id;

  auto session = session_manager_.new_session(session_id);
  session->user = s->user;
  session->state = http::SessionManager::Session::kUserVerified;

  return token;
}

AuthorizeManager::Session *AuthorizeManager::get_current_session(
    ServiceId id, HttpHeaders &input_headers, http::Cookie *cookies) {
  auto session_cookie_key = get_session_cookie_key_name(id);
  auto session_identifier = cookies->get(session_cookie_key);

  if (session_identifier.empty()) {
    if (!jwt_secret_.empty()) {
      auto jwt = get_bearer_token_jwt(input_headers);

      session_identifier = authorize(id, jwt);
      if (session_identifier.empty()) return nullptr;
    }
  }

  auto session = session_manager_.get_session(session_identifier);
  log_debug("Current session state:%i", session->state);
  return session;
}

std::string AuthorizeManager::authorize(const uint64_t service_id,
                                        const helper::Jwt &jwt) {
  log_debug("Validating JWT token: %s", jwt.get_token().c_str());
  if (!jwt.is_valid()) {
    log_debug("JWT token is invalid");
    return {};
  }
  // We allow HS256, still it doesn't change much because
  // Jws supports only HS256 and none (just blocking use of none).
  if (jwt.get_header_claim_algorithm() != "HS256") {
    log_debug("JWT token not supported algorithm");
    return {};
  }
  if (!jwt.verify(jwt_secret_)) {
    log_debug("JWT token verification failed");
    return {};
  }

  auto claims = jwt.get_payload_claim_names();
  if (!helper::container::has(claims, "user_id")) return {};
  if (!helper::container::has(claims, "exp")) return {};
  if (!helper::container::has(claims, "service_id")) return {};

  auto json_uid = jwt.get_payload_claim_custom("user_id");
  auto json_exp = jwt.get_payload_claim_custom("exp");
  auto json_sid = jwt.get_payload_claim_custom("service_id");

  if (!json_uid->IsUint64()) return {};
  log_debug("JWT token  supported algorithm");
  if (!json_exp->IsString()) return {};
  if (!json_sid->IsUint64()) return {};

  auto user_id = json_uid->GetUint64();
  auto exp = json_exp->GetString();
  auto sid = json_sid->GetUint64();

  if (sid != service_id) {
    log_debug("Wrong service id.");
    return {};
  }

  if (is_timestamp_in_past(exp)) {
    log_debug("Token expired.");
    return {};
  }

  std::string session_id = std::to_string(user_id) + "." + exp;
  if (session_manager_.get_session(session_id)) {
    log_debug("Session for token already exsits: %s", session_id.c_str());
    return session_id;
  }

  auto session = session_manager_.new_session(session_id);

  auto instance =
      cache_manager_->get_instance(collector::kMySQLConnectionMetadata);
  if (user_manager_.user_get_by_id(user_id, &session->user, &instance)) {
    log_debug("Found user %i", static_cast<int>(user_id));
    session->state = http::SessionManager::Session::kUserVerified;
    return session_id;
  }
  log_debug("User not found");
  return {};
}

bool AuthorizeManager::authorize(ServiceId service_id, http::Cookie *cookies,
                                 http::Url *url, SqlSessionCached *sql_session,
                                 HttpHeaders &input_headers,
                                 AuthUser *out_user) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = cookies->get(session_cookie_key);

  AuthorizeHandlerPtr selected_handler;

  bool generate_jwt_token = url->get_query_parameter("sessionType") == "bearer";
  if (generate_jwt_token) url->remove_query_parameter("sessionType");

  if (generate_jwt_token && jwt_secret_.empty()) {
    throw http::Error{HttpStatusCode::BadRequest,
                      "Bad request - bearer not allowed."};
  }

  {
    auto handlers = get_handlers_by_service_id(service_id);
    if (handlers.empty())
      throw http::Error{
          HttpStatusCode::BadRequest,
          "Bad request - there is no authorization application available"};

    auto selected_app = url->get_query_parameter("app");

    if (selected_app.empty() && handlers.size() == 1) {
      selected_handler = handlers[0];
    } else {
      if (!helper::container::get_if(
              handlers,
              [&selected_app](const auto &handler) {
                return (selected_app == handler->get_entry().app_name);
              },
              &selected_handler))
        throw http::Error{
            HttpStatusCode::BadRequest,
            "Bad request - chosen authorization application no available"};
    }
  }

  // Ensure that all code paths, had selected the handlers.
  assert(nullptr != selected_handler.get());

  if (!session_identifier.empty()) {
    auto session = session_manager_.get_session(session_identifier);
    if (session) {
      if (session->get_authorization_handler_id() !=
          selected_handler->get_id()) {
        session_manager_.remove_session(session_identifier);
        session_identifier.clear();
      }
    } else {
      session_identifier.clear();
    }
  }

  using namespace std::literals::string_literals;
  Session *session;

  if (session_identifier.empty()) {
    session = session_manager_.new_session(selected_handler->get_id());
    session->generate_token = generate_jwt_token;
    http::Cookie::SameSite same_site = http::Cookie::None;
    cookies->set(session_cookie_key, session->get_session_id(),
                 http::Cookie::duration{0}, "/", &same_site, true, true, {});
    log_debug("new session id=%s", session->get_session_id().c_str());
  } else {
    session = session_manager_.get_session(session_identifier);
    if (generate_jwt_token) session->generate_token = true;
    log_debug("existing session id=%s", session_identifier.c_str());
  }

  assert(nullptr != session);
  session->handler_name = selected_handler->get_entry().app_name;

  if (selected_handler->authorize(session, url, sql_session, input_headers,
                                  out_user)) {
    return true;
  }

  return false;
}

bool AuthorizeManager::is_authorized(ServiceId service_id,
                                     http::Cookie *cookies,
                                     HttpHeaders &input_headers,
                                     AuthUser *user) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = cookies->get(session_cookie_key);

  log_debug(
      "AuthorizeManager::is_authorized(service_id:%i, session_id:%s, "
      "can_use_jwt:%s)",
      static_cast<int>(service_id), session_identifier.c_str(),
      (jwt_secret_.empty() ? "no" : "yes"));

  if (session_identifier.empty()) {
    if (!jwt_secret_.empty()) {
      auto jwt = get_bearer_token_jwt(input_headers);

      session_identifier = authorize(service_id, jwt);
    }

    if (session_identifier.empty()) return false;
  }

  auto session = session_manager_.get_session(session_identifier);

  if (!session) return false;

  if (session->state == Session::kUserVerified) {
    *user = session->user;
    return true;
  }

  return false;

  //  AuthorizeHandlerPtr out_handler;
  //  if (!get_handler_by_id(session->get_authorization_handler_id(),
  //  out_handler))
  //    return false;
  //
  //  return out_handler->is_authorized(session, user);
}

void AuthorizeManager::discard_current_session(ServiceId id,
                                               http::Cookie *cookies) {
  auto session_cookie_key = get_session_cookie_key_name(id);
  auto session_identifier = cookies->get(session_cookie_key);
  session_manager_.remove_session(session_identifier);
}

}  // namespace authentication
}  // namespace mrs
