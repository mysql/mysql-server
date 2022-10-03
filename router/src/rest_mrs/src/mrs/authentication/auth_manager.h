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

#include <memory>
#include <vector>

#include "helper/mysql_time.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/http/session_manager.h"
#include "mrs/interface/auth_handler_factory.h"
#include "mrs/interface/auth_manager.h"
#include "mrs/interface/rest_handler.h"

namespace mrs {
namespace authentication {

class AuthManager : public mrs::interface::AuthManager {
  using AuthHandler = mrs::interface::AuthHandler;
  using RestHandler = mrs::interface::RestHandler;
  using AuthHandlerFactoryPtr =
      std::shared_ptr<mrs::interface::AuthHandlerFactory>;
  using RestHandlerPtr = std::shared_ptr<RestHandler>;

  class ContainerItem {
   public:
    ContainerItem() {}
    ContainerItem(RestHandlerPtr rest_handler) : rest_handler_{rest_handler} {}
    ContainerItem(HandlerPtr auth_handler) : auth_handler_{auth_handler} {}
    ContainerItem(RestHandlerPtr rest_handler, HandlerPtr auth_handler,
                  RestHandlerPtr status_handler, RestHandlerPtr unauth_handler)
        : rest_handler_{rest_handler},
          auth_handler_{auth_handler},
          status_handler_{status_handler},
          unauth_handler_{unauth_handler} {}

    RestHandlerPtr rest_handler_;
    HandlerPtr auth_handler_;
    RestHandlerPtr status_handler_;
    RestHandlerPtr unauth_handler_;
  };

  using Container = std::vector<ContainerItem>;

 public:
  AuthManager(collector::MysqlCacheManager *cache_manager);
  AuthManager(collector::MysqlCacheManager *cache_manager,
              AuthHandlerFactoryPtr factory);

  void update(const Entries &entries) override;

  AuthHandlers get_handlers_by_id(
      const std::pair<IdType, uint64_t> &id) override;
  collector::MysqlCacheManager *get_cache() override { return cache_manager_; }

 private:
  HandlerPtr make_auth(const AuthApp &entry);
  bool get_handler_by_id(const uint64_t auth_id, Container::iterator *out_it);

  collector::MysqlCacheManager *cache_manager_;
  AuthHandlerFactoryPtr factory_;
  http::SessionManager session_manager_;
  Container container_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_AUTH_MANAGER_H_
