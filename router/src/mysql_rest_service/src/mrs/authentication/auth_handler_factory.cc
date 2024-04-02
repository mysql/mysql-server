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

#include "mrs/authentication/auth_handler_factory.h"

#include "mrs/authentication/basic_handler.h"
#include "mrs/authentication/oauth2_facebook_handler.h"
#include "mrs/authentication/oauth2_google_handler.h"
#include "mrs/authentication/oauth2_twitter_handler.h"
#include "mrs/authentication/scram_handler.h"
#include "mrs/authentication/track_authorize_handler.h"

namespace mrs {
namespace authentication {

using AuthHandlerPtr = AuthHandlerFactory::AuthHandlerPtr;

AuthHandlerPtr AuthHandlerFactory::create_basic_auth_handler(
    AuthorizeHandlerCallbakcs *cb, const AuthApp &entry,
    MysqlCacheManager *cache_manager) const {
  using Obj = TrackAuthorizeHandler<AuthorizeHandlerCallbakcs, BasicHandler>;
  return std::make_shared<Obj>(cb, entry, cache_manager);
}

AuthHandlerPtr AuthHandlerFactory::create_facebook_auth_handler(
    AuthorizeHandlerCallbakcs *cb, const AuthApp &entry) const {
  using Obj =
      TrackAuthorizeHandler<AuthorizeHandlerCallbakcs, Oauth2FacebookHandler>;
  return std::make_shared<Obj>(cb, entry);
}

AuthHandlerPtr AuthHandlerFactory::create_twitter_auth_handler(
    AuthorizeHandlerCallbakcs *cb, const AuthApp &entry) const {
  using Obj =
      TrackAuthorizeHandler<AuthorizeHandlerCallbakcs, Oauth2TwitterHandler>;
  return std::make_shared<Obj>(cb, entry);
}

AuthHandlerPtr AuthHandlerFactory::create_google_auth_handler(
    AuthorizeHandlerCallbakcs *cb, const AuthApp &entry) const {
  using Obj =
      TrackAuthorizeHandler<AuthorizeHandlerCallbakcs, Oauth2GoogleHandler>;
  return std::make_shared<Obj>(cb, entry);
}

AuthHandlerPtr AuthHandlerFactory::create_scram_auth_handler(
    AuthorizeHandlerCallbakcs *cb, const AuthApp &entry,
    const std::string &rd) const {
  using Obj = TrackAuthorizeHandler<AuthorizeHandlerCallbakcs, ScramHandler>;
  return std::make_shared<Obj>(cb, entry, rd);
}

}  // namespace authentication
}  // namespace mrs
