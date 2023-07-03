/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_MOCK_SERVER_GLOBAL_SCOPE_INCLUDED
#define MYSQLROUTER_MOCK_SERVER_GLOBAL_SCOPE_INCLUDED

#include <map>
#include <mutex>
#include <string>
#include <vector>

/**
 * stores global data as pair of <string, jsonfied-string>
 */
class MockServerGlobalScope {
 public:
  using key_type = std::string;
  using value_type = std::string;
  using type = std::map<key_type, value_type>;

  type get_all() {
    std::lock_guard<std::mutex> lk(global_mutex_);
    return global_;
  }

  std::vector<key_type> get_keys() {
    std::lock_guard<std::mutex> lk(global_mutex_);

    std::vector<key_type> keys;
    for (const auto &k : global_) {
      keys.emplace_back(k.first);
    }

    return keys;
  }

  void set(const key_type &key, const value_type &value) {
    std::lock_guard<std::mutex> lk(global_mutex_);

    global_[key] = value;
  }

  size_t erase(const key_type &key) {
    std::lock_guard<std::mutex> lk(global_mutex_);

    return global_.erase(key);
  }

  void reset(type globals) {
    std::lock_guard<std::mutex> lk(global_mutex_);
    global_ = globals;
  }

 private:
  type global_;
  std::mutex global_mutex_;
};

#endif
