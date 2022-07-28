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

#ifndef MYSQLROUTER_CONNECTION_POOL_COMPONENT_INCLUDED
#define MYSQLROUTER_CONNECTION_POOL_COMPONENT_INCLUDED

#include <memory>  // shared_ptr
#include <mutex>
#include <unordered_map>
#include <vector>

#include "mysqlrouter/connection_pool_export.h"

class ConnectionPool;

class CONNECTION_POOL_EXPORT ConnectionPoolComponent {
 public:
  static ConnectionPoolComponent &get_instance();

  using key_type = std::string;

  // disable copy, as we are a single-instance
  ConnectionPoolComponent(ConnectionPoolComponent const &) = delete;
  void operator=(ConnectionPoolComponent const &) = delete;

  // no move either
  ConnectionPoolComponent(ConnectionPoolComponent &&) = delete;
  void operator=(ConnectionPoolComponent &&) = delete;

  template <class... Args>
  void emplace(Args &&... args) {
    pools_.emplace(std::forward<Args>(args)...);
  }

  void erase(const key_type &name);

  void clear();

  std::shared_ptr<ConnectionPool> get(const key_type &name);

  std::vector<std::string> pool_names() const;

  static std::string default_pool_name();

 private:
  ConnectionPoolComponent() = default;

  std::unordered_map<key_type, std::shared_ptr<ConnectionPool>> pools_;
};

#endif
