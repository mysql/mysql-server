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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTHENTICATION_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTHENTICATION_HANDLER_H_

#include "collector/mysql_cache_manager.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/http/session_manager.h"
#include "mrs/interface/universal_id.h"

namespace mrs {

namespace rest {
struct RequestContext;
}  // namespace rest

namespace interface {

class AuthorizeHandler;

class AuthorizeHandler {
 public:
  using SqlSessionCached = collector::MysqlCacheManager::CachedObject;
  using AuthUser = mrs::database::entry::AuthUser;
  using AuthApp = mrs::database::entry::AuthApp;
  using RequestContext = rest::RequestContext;
  using Session = http::SessionManager::Session;

 public:
  virtual ~AuthorizeHandler() = default;

  virtual bool redirects() const = 0;
  virtual UniversalId get_service_id() const = 0;
  virtual UniversalId get_id() const = 0;
  virtual const AuthApp &get_entry() const = 0;

  virtual bool is_authorized(Session *session, AuthUser *user) = 0;
  virtual bool authorize(RequestContext &ctxt, Session *session,
                         AuthUser *out_user) = 0;

  virtual void pre_authorize_account(
      [[maybe_unused]] AuthorizeHandler *handler,
      [[maybe_unused]] const std::string &account) {}
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_AUTHENTICATION_HANDLER_H_
