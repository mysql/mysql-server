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

#include "mrs/authentication/auth_handler_factory.h"

#include "mrs/authentication/basic_handler.h"
#include "mrs/authentication/oauth2_facebook_handler.h"
#include "mrs/authentication/oauth2_google_handler.h"
#include "mrs/authentication/oauth2_twitter_handler.h"

namespace mrs {
namespace authentication {

using AuthHandlerPtr = AuthHandlerFactory::AuthHandlerPtr;

AuthHandlerPtr AuthHandlerFactory::create_basic_auth_handler(
    const AuthApp &entry, MysqlCacheManager *cache_manager) const {
  return std::make_shared<BasicHandler>(entry, cache_manager);
}

AuthHandlerPtr AuthHandlerFactory::create_facebook_auth_handler(
    const AuthApp &entry, SessionManager *session_manager) const {
  return std::make_shared<Oauth2FacebookHandler>(entry, session_manager);
}

AuthHandlerPtr AuthHandlerFactory::create_twitter_auth_handler(
    const AuthApp &entry, SessionManager *session_manager) const {
  return std::make_shared<Oauth2TwitterHandler>(entry, session_manager);
}

AuthHandlerPtr AuthHandlerFactory::create_google_auth_handler(
    const AuthApp &entry, SessionManager *session_manager) const {
  return std::make_shared<Oauth2GoogleHandler>(entry, session_manager);
}

}  // namespace authentication
}  // namespace mrs
