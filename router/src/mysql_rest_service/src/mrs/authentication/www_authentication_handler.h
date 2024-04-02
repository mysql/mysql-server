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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_WWW_AUTHENTICATION_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_WWW_AUTHENTICATION_HANDLER_H_

#include "mrs/interface/authorize_handler.h"

#include <optional>

#include "mrs/database/entry/auth_app.h"
#include "mrs/users/user_manager.h"

#include "mysql/harness/string_utils.h"

namespace mrs {
namespace authentication {

class WwwAuthenticationHandler : public interface::AuthorizeHandler {
 protected:
  using SessionManager = mrs::http::SessionManager;
  using UserManager = mrs::users::UserManager;
  using Session = mrs::http::SessionManager::Session;

  AuthApp entry_;
  UserManager um_{entry_.limit_to_registered_users, entry_.default_role_id};

  void throw_add_www_authenticate(const char *schema);

  bool redirects() const override;
  bool is_authorized(Session *session, AuthUser *user) override;
  bool authorize(RequestContext &ctxt, Session *session,
                 AuthUser *out_user) override;
  const AuthApp &get_entry() const override;

 public:
  WwwAuthenticationHandler(const AuthApp &entry) : entry_{entry} {}

  constexpr static char kAuthorization[] = "Authorization";
  constexpr static char kWwwAuthenticate[] = "WWW-Authenticate";

  virtual bool www_authorize(const std::string &token,
                             SqlSessionCached *out_cache,
                             AuthUser *out_user) = 0;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_WWW_AUTHENTICATION_HANDLER_H_
