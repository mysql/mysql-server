/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_AUTH_MANAGER_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_AUTH_MANAGER_H_

#include "mrs/authentication/authorize_manager.h"

class MockAuthManager : public mrs::interface::AuthorizeManager {
 public:
  MOCK_METHOD(std::string, get_jwt_token, (ServiceId id, Session *s),
              (override));
  MOCK_METHOD(void, update, (const Entries &entries), (override));

  MOCK_METHOD(void, configure, (const std::string &), (override));
  MOCK_METHOD(bool, authorize,
              (ServiceId id, mrs::rest::RequestContext &ctxt,
               AuthUser *out_user),
              (override));
  MOCK_METHOD(bool, is_authorized,
              (ServiceId id, mrs::rest::RequestContext &ctxt, AuthUser *user),
              (override));
  MOCK_METHOD(bool, unauthorize, (ServiceId id, mrs::http::Cookie *cookies),
              (override));

  MOCK_METHOD(Session *, get_current_session,
              (ServiceId id, const HttpHeaders &input_headers,
               mrs::http::Cookie *cookies),
              (override));
  MOCK_METHOD(collector::MysqlCacheManager *, get_cache, (), (override));
  MOCK_METHOD(mrs::users::UserManager *, get_user_manager, (), (override));
  MOCK_METHOD(Container, get_supported_authentication_applications, (ServiceId),
              (override));
  MOCK_METHOD(void, clear, (), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_AUTH_MANAGER_H_
