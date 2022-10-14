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
#include <memory>

#include "mrs/authentication/auth_handler_factory.h"
#include "mrs/authentication/track_authorize_handler.h"
#include "mrs/rest/handler_authorize.h"
#include "mrs/rest/handler_authorize_ok.h"
#include "mrs/rest/handler_is_authorized.h"
#include "mrs/rest/handler_unauthorize.h"

#include "helper/container/generic.h"
#include "helper/container/map.h"
#include "helper/make_shared_ptr.h"
#include "helper/replace_string.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using Handlers = AuthorizeManager::AuthHandlers;
using AuthorizeHandlerPtr = AuthorizeManager::AuthorizeHandlerPtr;

static std::string get_session_cookie_key_name(
    const AuthorizeManager::ServiceId id) {
  using namespace std::literals::string_literals;
  return "session_"s + std::to_string(id);
}

AuthorizeManager::AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                                   AuthHandlerFactoryPtr factory)
    : cache_manager_{cache_manager}, factory_{factory} {}

AuthorizeManager::AuthorizeManager(collector::MysqlCacheManager *cache_manager)
    : cache_manager_{cache_manager},
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
  auto it = std::find_if(
      container_.begin(), container_.end(),
      [auth_id](auto &i) { return i->get_service_id() == auth_id; });

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
      e.id, e.service_name, path1, e.options, redirect, this);
  auto status_handler = std::make_shared<mrs::rest::HandlerIsAuthorized>(
      e.id, e.service_name, path2, e.options, this);
  auto unauth_handler = std::make_shared<mrs::rest::HandlerUnauthorize>(
      e.id, e.service_name, path3, e.options, this);
  auto auth_ok_handler = std::make_shared<mrs::rest::HandlerAuthorizeOk>(
      e.id, e.service_name, path4, e.options, e.redirection_default_page, this);

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

    service_authorize_[handler->get_service_id()] = service_authorization;
    return;
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

bool AuthorizeManager::authorize(ServiceId service_id, http::Cookie *cookies,
                                 http::Url *url, SqlSessionCached *sql_session,
                                 HttpHeaders &input_headers,
                                 AuthUser *out_user) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = cookies->get(session_cookie_key);

  AuthorizeHandlerPtr selected_handler;

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

  Session *session;

  if (session_identifier.empty()) {
    session = session_manager_.new_session(selected_handler->get_id());
    cookies->set(session_cookie_key, session->get_session_id(),
                 session_manager_.get_timeout(),
                 selected_handler->get_entry().service_name);
  } else {
    session = session_manager_.get_session(session_identifier);
  }

  assert(nullptr != session);

  return selected_handler->authorize(session, url, sql_session, input_headers,
                                     out_user);
}

bool AuthorizeManager::is_authorized(ServiceId service_id,
                                     http::Cookie *cookies, AuthUser *user) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = cookies->get(session_cookie_key);

  if (session_identifier.empty()) return false;

  auto session = session_manager_.get_session(session_identifier);

  if (!session) return false;

  AuthorizeHandlerPtr out_handler;
  if (!get_handler_by_id(session->get_authorization_handler_id(), out_handler))
    return false;

  return out_handler->is_authorized(session, user);
}

}  // namespace authentication
}  // namespace mrs
