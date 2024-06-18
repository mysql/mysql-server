/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
#include <optional>
#include <vector>

#include "helper/mysql_time.h"
#include "helper/token/jwt.h"
#include "mrs/authentication/authorize_handler_callbacks.h"
#include "mrs/authentication/rate_control_for.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/interface/auth_handler_factory.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/users/user_manager.h"

namespace mrs {
namespace authentication {

class AuthorizeManager : public mrs::interface::AuthorizeManager,
                         private helper::AuthorizeHandlerCallbakcs {
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
    RestHandlerPtr user_handler_;
    RestHandlerPtr list_handler_;
  };

  using ServiceAuthorizePtr = std::shared_ptr<ServiceAuthorize>;
  using Container = std::vector<AuthorizeHandlerPtr>;
  using MapOfServices = std::map<ServiceId, ServiceAuthorizePtr>;

 public:
  AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                   const std::string &jwt_secret);
  AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                   const std::string &jwt_secret,
                   AuthHandlerFactoryPtr factory);

  void update(const Entries &entries) override;
  void configure(const std::string &options) override;

  bool unauthorize(ServiceId id, http::Cookie *cookies) override;
  bool authorize(ServiceId id, rest::RequestContext &ctxt,
                 AuthUser *out_user) override;
  bool is_authorized(ServiceId id, rest::RequestContext &ctxt,
                     AuthUser *user) override;

  std::string get_jwt_token(UniversalId service_id, Session *s) override;
  Session *get_current_session(ServiceId id, const HttpHeaders &input_headers,
                               http::Cookie *cookies) override;
  void discard_current_session(ServiceId id, http::Cookie *cookies) override;
  users::UserManager *get_user_manager() override;
  collector::MysqlCacheManager *get_cache() override { return cache_manager_; }
  Container get_supported_authentication_applications(ServiceId id) override;
  void clear() override;

 private:
  AuthorizeHandlerPtr make_auth(const AuthApp &entry);
  Container get_handlers_by_service_id(const UniversalId service_id);
  bool get_handler_by_id(const UniversalId auth_id, Container::iterator *it);
  bool get_handler_by_id(const UniversalId auth_id,
                         AuthorizeHandlerPtr &out_it);
  void remove_unreferenced_service_authorizators();
  void fill_service(const AuthApp &e, ServiceAuthorize &sa);
  AuthorizeHandlerPtr choose_authentication_handler(
      ServiceId service_id, const std::string &app_name);

  /**
   * Validate the jwt tokent, and get/create session_id for it.
   *
   * @returns session id, for found or just created session.
   */
  std::string authorize(const UniversalId service_id, const helper::Jwt &jwt);

 private:  // AuthorizeHandlerCallbacks
  void acquire(interface::AuthorizeHandler *) override;
  void destroy(interface::AuthorizeHandler *) override;
  void pre_authorize_account(interface::AuthorizeHandler *handler,
                             const std::string &account) override;

  std::mutex service_authorize_mutext_;
  MapOfServices service_authorize_;
  collector::MysqlCacheManager *cache_manager_;
  users::UserManager user_manager_{true, {}};
  http::SessionManager session_manager_;
  Container container_;
  std::string jwt_secret_;
  AuthHandlerFactoryPtr factory_;
  RateControlFor<std::string> accounts_rate_;
  RateControlFor<std::string> hosts_rate_;

  /*
   * Random data, created at `authorization_manager` creation.
   *
   * Those data should be used for fake keys generation. Which concated with
   * user name, may be used for the generation and user shouldn't be able to
   * detect the fake generation was used.
   *
   * Ideally, the data should be constant while the whole live of the service.
   * For example the attacker should be able to query the user and store its
   * "salt", and after logner time the same query should return the same salt
   * (even if the user doesn't exists).
   */
  const std::string random_data_;

  // TODO(lkotula): Take it from configuration structure (Shouldn't be in
  // review)
  std::optional<uint64_t> host_autentication_rate_rps_;
  std::optional<uint64_t> account_autentication_rate_rps_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_
