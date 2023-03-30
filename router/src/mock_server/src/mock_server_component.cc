/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/mock_server_component.h"

#include <memory>  // shared_ptr

#include "mysql_server_mock.h"
#include "mysqlrouter/mock_server_global_scope.h"

std::shared_ptr<MockServerGlobalScope> MockServerComponent::get_global_scope() {
  static std::shared_ptr<MockServerGlobalScope> instance{
      std::make_shared<MockServerGlobalScope>()};

  return instance;
}

void MockServerComponent::register_server(
    const std::string &name,
    std::shared_ptr<server_mock::MySQLServerMock> srv) {
  srvs_([&](auto srvs) { srvs.emplace(name, srv); });
}

MockServerComponent &MockServerComponent::get_instance() {
  static MockServerComponent instance;

  return instance;
}

void MockServerComponent::close_all_connections() {
  srvs_([&](auto srvs) {
    for (auto &srv : srvs) {
      // if we have a mock_server instance, call its close_all_connections()
      if (auto server = srv.second.lock()) {
        server->close_all_connections();
      }
    }
  });
}
