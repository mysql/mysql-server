/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_

#include <chrono>
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
#include "mrs/interface/endpoint_configuration.h"
#include "mrs/interface/query_factory.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/users/user_manager.h"

namespace mrs {
namespace authentication {

class AuthorizeManager : public mrs::interface::AuthorizeManager,
                         private helper::AuthorizeHandlerCallbakcs {
 public:
  using AuthHandler = mrs::interface::AuthorizeHandler;
  using RestHandler = mrs::interface::RestHandler;
  using AuthHandlerFactoryPtr =
      std::shared_ptr<mrs::interface::AuthHandlerFactory>;
  using RestHandlerPtr = std::shared_ptr<RestHandler>;
  using QueryFactory = mrs::interface::QueryFactory;
  using EndpointConfiguration = mrs::interface::EndpointConfiguration;
  using EndpointConfigurationPtr = std::shared_ptr<EndpointConfiguration>;
  using Container = std::vector<AuthorizeHandlerPtr>;
  using minutes = std::chrono::minutes;
  using steady_clock = std::chrono::steady_clock;

 public:
  AuthorizeManager(const EndpointConfigurationPtr &configuration,
                   collector::MysqlCacheManager *cache_manager,
                   const std::string &jwt_secret, QueryFactory *query_factory,
                   AuthHandlerFactoryPtr factory);

  void update(const Entries &entries) override;
  void configure(const std::string &options) override;

  bool unauthorize(const SessionPtr &session, http::Cookie *cookies) override;
  bool authorize(const std::string &proto, const std::string &host,
                 ServiceId id, bool passthrough_db_user,
                 rest::RequestContext &ctxt, AuthUser *out_user) override;
  bool is_authorized(ServiceId id, rest::RequestContext &ctxt,
                     AuthUser *user) override;

  std::string get_jwt_token(UniversalId service_id,
                            const SessionPtr &) override;
  void discard_current_session(ServiceId id, http::Cookie *cookies) override;
  users::UserManager *get_user_manager() override;
  collector::MysqlCacheManager *get_cache() override { return cache_manager_; }
  Container get_supported_authentication_applications(ServiceId id) override;
  void clear() override;
  void update_users_cache(const ChangedUsersIds &changed_users_ids) override;

  void collect_garbage();

 private:
  AuthorizeHandlerPtr create_authentication_application(const AuthApp &entry);
  Container get_handlers_by_service_id(const UniversalId service_id);
  SessionPtr get_session_id_from_cookie(const UniversalId &service_id,
                                        http::Cookie &cookies);
  std::vector<std::pair<std::string, SessionId>> get_session_ids_cookies(
      const UniversalId &service_id, http::Cookie *cookies);
  std::vector<SessionId> get_session_ids_from_cookies(
      const UniversalId &service_id, http::Cookie *cookies);
  bool get_handler_by_id(const UniversalId auth_id, Container::iterator *it);
  bool get_handler_by_id(const UniversalId auth_id,
                         AuthorizeHandlerPtr &out_it);
  void remove_unreferenced_service_authorizators();
  AuthorizeHandlerPtr choose_authentication_handler(
      rest::RequestContext &ctxt, ServiceId service_id,
      const std::optional<std::string> &app_name);

  /**
   * Validate the JWT token, and get/create session for it.
   *
   * @returns session pointer
   */
  SessionPtr authorize_jwt(const UniversalId service_id,
                           const helper::Jwt &jwt);

 private:  // AuthorizeHandlerCallbacks
  void pre_authorize_account(interface::AuthorizeHandler *handler,
                             const std::string &account) override;

  class ServiceAuthorize {
   public:
    RestHandlerPtr authorize_handler_;
    RestHandlerPtr status_handler_;
    RestHandlerPtr unauthorize_handler_;
    RestHandlerPtr authorization_result_handler_;
    RestHandlerPtr user_handler_;
    RestHandlerPtr list_handler_;
  };

  using ServiceAuthorizePtr = std::shared_ptr<ServiceAuthorize>;
  using MapOfServices = std::map<ServiceId, ServiceAuthorizePtr>;

  EndpointConfigurationPtr configuration_;
  collector::MysqlCacheManager *cache_manager_;
  users::UserManager user_manager_;
  http::SessionManager session_manager_;
  Container container_;
  std::string jwt_secret_;
  AuthHandlerFactoryPtr factory_;
  RateControlFor<std::string> accounts_rate_;
  RateControlFor<std::string> hosts_rate_;
  minutes jwt_expire_timeout;
  steady_clock::time_point last_garbage_collection_{};
  uint32_t passthrough_db_user_session_pool_size_{0};

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
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_
