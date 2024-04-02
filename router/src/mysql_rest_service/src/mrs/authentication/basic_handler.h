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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_BASIC_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_BASIC_HANDLER_H_

#include "collector/mysql_cache_manager.h"
#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/users/user_manager.h"

namespace mrs {
namespace authentication {

class BasicHandler : public WwwAuthenticationHandler {
 public:
  using AuthApp = database::entry::AuthApp;

 public:
  BasicHandler(const AuthApp &entry,
               collector::MysqlCacheManager *cache_manager);

  UniversalId get_service_id() const override;
  UniversalId get_id() const override;

  bool www_authorize(const std::string &token, SqlSessionCached *out_cache,
                     AuthUser *out_user) override;

 private:
  collector::MysqlCacheManager *cache_manager_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_BASIC_HANDLER_H_
