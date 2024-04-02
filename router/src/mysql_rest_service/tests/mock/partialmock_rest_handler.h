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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_PARTIALMOCK_REST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_PARTIALMOCK_REST_HANDLER_H_

#include "mrs/rest/handler.h"

class PartialMockRestHandler : public mrs::rest::Handler {
 public:
  PartialMockRestHandler(const std::string &url,
                         const std::string &rest_path_matcher,
                         mrs::interface::AuthorizeManager *auth_manager)
      : Handler{url, {rest_path_matcher}, "", auth_manager} {}

  MOCK_METHOD(bool, may_check_access, (), (const, override));
  MOCK_METHOD(Authorization, requires_authentication, (), (const, override));
  MOCK_METHOD(mrs::UniversalId, get_service_id, (), (const, override));
  MOCK_METHOD(mrs::UniversalId, get_db_object_id, (), (const, override));
  MOCK_METHOD(mrs::UniversalId, get_schema_id, (), (const, override));
  MOCK_METHOD(uint32_t, get_access_rights, (), (const, override));

  MOCK_METHOD(HttpResult, handle_get, (mrs::rest::RequestContext * ctxt),
              (override));
  MOCK_METHOD(HttpResult, handle_post,
              (mrs::rest::RequestContext * ctxt,
               const std::vector<uint8_t> &document),
              (override));
  MOCK_METHOD(HttpResult, handle_delete, (mrs::rest::RequestContext * ctxt),
              (override));
  MOCK_METHOD(HttpResult, handle_put, (mrs::rest::RequestContext * ctxt),
              (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_PARTIALMOCK_REST_HANDLER_H_
