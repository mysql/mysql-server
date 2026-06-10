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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTHORIZE_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTHORIZE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "helper/mysql_time.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/database/entry/universal_id.h"
#include "mrs/http/cookie.h"
#include "mrs/http/session_manager.h"
#include "mrs/interface/authorize_handler.h"
#include "mrs/users/user_manager.h"

#include "http/base/headers.h"

namespace collector {

class MysqlCacheManager;

}  // namespace collector

namespace mrs {

namespace rest {
struct RequestContext;
}  // namespace rest

namespace interface {

class AuthorizeManager {
 public:
  using SessionId = http::SessionManager::SessionId;
  using Session = http::SessionManager::Session;
  using SessionPtr = http::SessionManager::SessionPtr;
  using SqlSessionCached = collector::MysqlCacheManager::CachedObject;
  using AuthorizeHandlerPtr = std::shared_ptr<AuthorizeHandler>;
  using AuthHandlers = std::vector<AuthorizeHandlerPtr>;
  using AuthApp = database::entry::AuthApp;
  using AuthUser = database::entry::AuthUser;
  using Entries = std::vector<AuthApp>;
  using ServiceId = UniversalId;
  using Container = std::vector<AuthorizeHandlerPtr>;
  using HttpHeaders = ::http::base::Headers;
  using ChangedUsersIds = mrs::users::UserManager::ChangedUsersIds;

  virtual ~AuthorizeManager() = default;

  virtual void update(const Entries &entries) = 0;

  virtual bool authorize(const std::string &proto, const std::string &host,
                         ServiceId id, bool passthrough_db_user,
                         rest::RequestContext &ctxt, AuthUser *out_user) = 0;
  virtual bool is_authorized(ServiceId id, rest::RequestContext &ctxt,
                             AuthUser *user) = 0;
  virtual bool unauthorize(const SessionPtr &session,
                           http::Cookie *cookies) = 0;
  virtual void configure(const std::string &options) = 0;
  virtual std::string get_jwt_token(ServiceId service_id,
                                    const SessionPtr &s) = 0;

  virtual users::UserManager *get_user_manager() = 0;
  virtual Container get_supported_authentication_applications(ServiceId id) = 0;

  virtual void discard_current_session(ServiceId, http::Cookie *) {}

  virtual collector::MysqlCacheManager *get_cache() = 0;
  virtual void clear() = 0;
  virtual void update_users_cache(const ChangedUsersIds &changed_users_ids) = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTHORIZE_MANAGER_H_
