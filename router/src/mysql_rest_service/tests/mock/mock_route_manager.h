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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_ROUTE_MANAGER_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_ROUTE_MANAGER_H_

#include <vector>

#include "mrs/interface/endpoint_manager.h"

class MockRouteManager : public mrs::interface::EndpointManager {
 public:
  using UniversalId = mrs::database::entry::UniversalId;
  MOCK_METHOD(void, turn, (const mrs::State state, const std::string &options),
              (override));

  MOCK_METHOD(void, update, (const std::vector<UrlHost> &paths), (override));
  MOCK_METHOD(void, update, (const std::vector<DbObjectLite> &paths),
              (override));
  MOCK_METHOD(void, update, (const std::vector<DbSchema> &schemas), (override));
  MOCK_METHOD(void, update, (const std::vector<DbService> &services),
              (override));
  MOCK_METHOD(void, update, (const std::vector<ContentFileLite> &files),
              (override));
  MOCK_METHOD(void, update, (const std::vector<ContentSet> &set), (override));

  MOCK_METHOD(void, schema_not_used, (RouteSchema * route), (override));
  MOCK_METHOD(void, clear, (), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_ROUTE_MANAGER_H_
