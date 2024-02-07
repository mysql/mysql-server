/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mysqlrouter/connection_pool_component.h"

#include <memory>
#include <mutex>
#include <string>

#include "mysqlrouter/connection_pool.h"

void ConnectionPoolComponent::erase(const key_type &name) {
  pools_.erase(name);
}

void ConnectionPoolComponent::clear() { pools_.clear(); }

std::shared_ptr<ConnectionPool> ConnectionPoolComponent::get(
    const key_type &name) {
  auto it = pools_.find(name);
  if (it == pools_.end()) return {};

  return it->second;
}

std::vector<std::string> ConnectionPoolComponent::pool_names() const {
  // one pool only for now.
  return {default_pool_name()};
}

std::string ConnectionPoolComponent::default_pool_name() { return "main"; }

ConnectionPoolComponent &ConnectionPoolComponent::get_instance() {
  static ConnectionPoolComponent instance;

  return instance;
}
