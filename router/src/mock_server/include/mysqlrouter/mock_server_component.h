/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_MOCK_SERVER_COMPONENT_INCLUDED
#define MYSQLROUTER_MOCK_SERVER_COMPONENT_INCLUDED

#include <memory>
#include <vector>

#include "mysqlrouter/mock_server_export.h"
#include "mysqlrouter/mock_server_global_scope.h"

namespace server_mock {
class MySQLServerMock;
}

class MOCK_SERVER_EXPORT MockServerComponent {
  // disable copy, as we are a single-instance
  MockServerComponent(MockServerComponent const &) = delete;
  void operator=(MockServerComponent const &) = delete;

  std::vector<std::weak_ptr<server_mock::MySQLServerMock>> srvs_;

  MockServerComponent() = default;

 public:
  static MockServerComponent &get_instance();

  void register_server(std::shared_ptr<server_mock::MySQLServerMock> srv);

  std::shared_ptr<MockServerGlobalScope> get_global_scope();
  void close_all_connections();
};

#endif
