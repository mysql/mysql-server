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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTH_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTH_MANAGER_H_

#include <memory>
#include <vector>

#include "helper/mysql_time.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/id_type.h"
#include "mrs/interface/auth_handler.h"

namespace collector {

class MysqlCacheManager;

}  // namespace collector

namespace mrs {
namespace interface {

class AuthManager {
 public:
  using AuthHandler = AuthHandler;
  using HandlerPtr = std::shared_ptr<AuthHandler>;
  using AuthHandlers = std::vector<HandlerPtr>;
  using AuthApp = database::entry::AuthApp;
  using Entries = std::vector<AuthApp>;

  virtual ~AuthManager() = default;

  virtual void update(const Entries &entries) = 0;
  virtual AuthHandlers get_handlers_by_id(
      const std::pair<IdType, uint64_t> &id) = 0;
  virtual collector::MysqlCacheManager *get_cache() = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTH_MANAGER_H_
