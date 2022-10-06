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

#include "mrs/authentication/auth_manager.h"

#include "mrs/authentication/auth_handler_factory.h"
#include "mrs/rest/handler_authorize.h"
#include "mrs/rest/handler_authorize_ok.h"
#include "mrs/rest/handler_is_authorized.h"
#include "mrs/rest/handler_unauthorize.h"

#include "helper/replace_string.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using Handlers = AuthManager::AuthHandlers;
using HandlerPtr = AuthManager::HandlerPtr;

AuthManager::AuthManager(collector::MysqlCacheManager *cache_manager,
                         AuthHandlerFactoryPtr factory)
    : cache_manager_{cache_manager}, factory_{factory} {}

AuthManager::AuthManager(collector::MysqlCacheManager *cache_manager)
    : cache_manager_{cache_manager},
      factory_{std::make_shared<AuthHandlerFactory>()} {}

void AuthManager::update(const Entries &entries) {
  Container::iterator it;

  if (entries.size()) {
    log_debug("auth_app: Number of updated entries:%i", (int)entries.size());
  }

  for (const auto &e : entries) {
    log_debug("auth_app: Processing update of id=%i", (int)e.id);
    auto auth = make_auth(e);

    if (get_handler_by_id(e.id, &it)) {
      it->auth_handler_ = auth;
      if (!auth) container_.erase(it);
    } else {
      if (auth) {
        std::string auth_path =
            !e.auth_path.empty() ? e.auth_path : "/authorize";
        std::string path1 = "^" + e.service_name + auth_path + "/login$";
        std::string path2 = "^" + e.service_name + auth_path + "/status$";
        std::string path3 = "^" + e.service_name + auth_path + "/logout$";
        std::string path4 =
            "^" + e.service_name + auth_path + "/login_success$";
        std::string redirect = e.redirect;
        if (redirect.empty()) {
          redirect = e.host + e.service_name + auth_path + "/login_success";
        }

        auto rest_handler = std::make_shared<mrs::rest::HandlerAuthorize>(
            e.id, e.service_name, path1, e.options, redirect, this);
        auto status_handler = std::make_shared<mrs::rest::HandlerIsAuthorized>(
            e.id, e.service_name, path2, e.options, this);
        auto unauth_handler = std::make_shared<mrs::rest::HandlerUnauthorize>(
            e.id, e.service_name, path3, e.options, this);
        auto auth_ok_handler = std::make_shared<mrs::rest::HandlerAuthorizeOk>(
            e.id, e.service_name, path4, e.options, e.redirection_default_page,
            this);
        container_.emplace_back(rest_handler, auth, status_handler,
                                unauth_handler, auth_ok_handler);
      }
    }
  }
}

Handlers AuthManager::get_handlers_by_id(
    const std::pair<IdType, uint64_t> &id) {
  AuthHandlers handlers;

  uint64_t (AuthHandler::*get_id)() const =
      id.first == IdType::k_id_type_service_id ? &AuthHandler::get_service_id
                                               : &AuthHandler::get_id;
  for (auto &item : container_) {
    if ((item.auth_handler_.get()->*get_id)() == id.second)
      handlers.push_back(item.auth_handler_);
  }

  return handlers;
}

bool AuthManager::get_handler_by_id(const uint64_t auth_id,
                                    Container::iterator *out_it) {
  *out_it = std::find_if(container_.begin(), container_.end(),
                         [auth_id](ContainerItem &i) {
                           return i.auth_handler_->get_id() == auth_id;
                         });

  return *out_it != container_.end();
}

HandlerPtr AuthManager::make_auth(const AuthApp &entry) {
  // TODO(lkotula): Rework this (Shouldn't be in review)
  if (entry.deleted) return {};

  if (!entry.active) return {};

  if (entry.name == "MySQL Basic")
    return factory_->create_basic_auth_handler(entry, cache_manager_);
  if (entry.name == "Facebook")
    return factory_->create_facebook_auth_handler(entry, &session_manager_);
  if (entry.name == "Twitter")
    return factory_->create_twitter_auth_handler(entry, &session_manager_);
  if (entry.name == "Google")
    return factory_->create_google_auth_handler(entry, &session_manager_);

  return {};
}

}  // namespace authentication
}  // namespace mrs
