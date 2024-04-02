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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_REST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_REST_HANDLER_H_

#include "mrs/interface/rest_handler.h"

class MockRestHandler : public mrs::interface::RestHandler {
 public:
  using RequestContext = mrs::rest::RequestContext;
  using Parameters = std::map<std::string, std::string>;

  MOCK_METHOD(bool, may_check_access, (), (const, override));
  MOCK_METHOD(Authorization, requires_authentication, (), (const, override));
  MOCK_METHOD(mrs::UniversalId, get_service_id, (), (const, override));
  MOCK_METHOD(mrs::UniversalId, get_db_object_id, (), (const, override));
  MOCK_METHOD(mrs::UniversalId, get_schema_id, (), (const, override));
  MOCK_METHOD(uint32_t, get_access_rights, (), (const, override));
  MOCK_METHOD(bool, is_json_response, (), (const, override));
  MOCK_METHOD(HttpResult, handle_get, (RequestContext * ctxt), (override));
  MOCK_METHOD(HttpResult, handle_post,
              (RequestContext * ctxt, const std::vector<uint8_t> &document),
              (override));
  MOCK_METHOD(HttpResult, handle_delete, (RequestContext * ctxt), (override));
  MOCK_METHOD(HttpResult, handle_put, (RequestContext * ctxt), (override));

  MOCK_METHOD(void, destroy, (), ());

  MOCK_METHOD(void, authorization, (RequestContext * ctxt), (override));
  MOCK_METHOD(bool, request_begin, (RequestContext * ctxt), (override));
  MOCK_METHOD(void, request_end, (RequestContext * ctxt), (override));
  MOCK_METHOD(bool, request_error,
              (RequestContext * ctxt, const mrs::http::Error &e), (override));
  MOCK_METHOD(const mrs::interface::Options &, get_options, (),
              (const, override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_REST_HANDLER_H_
