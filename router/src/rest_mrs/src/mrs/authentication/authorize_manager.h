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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "helper/mysql_time.h"
#include "mrs/authentication/helper/authorize_handler_callbacks.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/http/session_manager.h"
#include "mrs/interface/auth_handler_factory.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/rest_handler.h"

namespace mrs {
namespace authentication {

class AuthorizeManager : public mrs::interface::AuthorizeManager,
                         private helper::AuthorizeHandlerCallbakcs {
  using Session = http::SessionManager::Session;
  using AuthHandler = mrs::interface::AuthorizeHandler;
  using RestHandler = mrs::interface::RestHandler;
  using AuthHandlerFactoryPtr =
      std::shared_ptr<mrs::interface::AuthHandlerFactory>;
  using RestHandlerPtr = std::shared_ptr<RestHandler>;

  class ServiceAuthorize {
   public:
    uint64_t references_{1};
    RestHandlerPtr authorize_handler_;
    RestHandlerPtr status_handler_;
    RestHandlerPtr unauthorize_handler_;
    RestHandlerPtr authorization_result_handler_;
  };

  using ServiceAuthorizePtr = std::shared_ptr<ServiceAuthorize>;

  using Container = std::vector<AuthorizeHandlerPtr>;
  using MapOfServices = std::map<ServiceId, ServiceAuthorizePtr>;

 public:
  AuthorizeManager(collector::MysqlCacheManager *cache_manager);
  AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                   AuthHandlerFactoryPtr factory);

  void update(const Entries &entries) override;

  bool unauthorize(ServiceId id, http::Cookie *cookies) override;
  bool authorize(ServiceId id, http::Cookie *cookies, http::Url *url,
                 SqlSessionCached *sql_session, HttpHeaders &input_headers,
                 AuthUser *out_user) override;
  bool is_authorized(ServiceId id, http::Cookie *cookies,
                     AuthUser *user) override;

  collector::MysqlCacheManager *get_cache() override { return cache_manager_; }

 private:
  AuthorizeHandlerPtr make_auth(const AuthApp &entry);
  Container get_handlers_by_service_id(const uint64_t service_id);
  bool get_handler_by_id(const uint64_t auth_id, Container::iterator *it);
  bool get_handler_by_id(const uint64_t auth_id, AuthorizeHandlerPtr &out_it);
  void remove_unreferenced_service_authorizators();
  void fill_service(const AuthApp &e, ServiceAuthorize &sa);

 private:  // AuthorizeHandlerCallbacks
  void acquire(interface::AuthorizeHandler *) override;
  void destroy(interface::AuthorizeHandler *) override;

  std::mutex service_authorize_mutext_;
  MapOfServices service_authorize_;
  collector::MysqlCacheManager *cache_manager_;
  AuthHandlerFactoryPtr factory_;
  http::SessionManager session_manager_;
  Container container_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_
