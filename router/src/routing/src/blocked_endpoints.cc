/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "blocked_endpoints.h"

#include <mutex>

uint64_t BlockedEndpoints::error_count(
    const net::ip::tcp::endpoint &endpoint) const {
  const auto &addr = endpoint.address();
  const bool is_v4 = addr.is_v4();

  std::lock_guard<std::mutex> lock(mutex_conn_errors_);
  if (is_v4) {
    const auto it = conn_error_counters_v4_.find(addr.to_v4());
    return (it != conn_error_counters_v4_.end()) ? it->second : 0;
  } else {
    const auto it = conn_error_counters_v6_.find(addr.to_v6());
    return (it != conn_error_counters_v6_.end()) ? it->second : 0;
  }
}

uint64_t BlockedEndpoints::increment_error_count(
    const net::ip::tcp::endpoint &endpoint) {
  const auto &addr = endpoint.address();
  const bool is_v4 = addr.is_v4();

  std::lock_guard<std::mutex> lock(mutex_conn_errors_);
  if (is_v4) {
    return ++conn_error_counters_v4_[addr.to_v4()];
  } else {
    return ++conn_error_counters_v6_[addr.to_v6()];
  }
}

uint64_t BlockedEndpoints::reset_error_count(
    const net::ip::tcp::endpoint &endpoint) {
  const auto &addr = endpoint.address();
  const bool is_v4 = addr.is_v4();

  std::lock_guard<std::mutex> lock(mutex_conn_errors_);
  if (is_v4) {
    auto it = conn_error_counters_v4_.find(addr.to_v4());
    return (it != conn_error_counters_v4_.end()) ? std::exchange(it->second, 0)
                                                 : 0;
  } else {
    auto it = conn_error_counters_v6_.find(addr.to_v6());
    return (it != conn_error_counters_v6_.end()) ? std::exchange(it->second, 0)
                                                 : 0;
  }
}

bool BlockedEndpoints::is_blocked(
    const net::ip::tcp::endpoint &endpoint) const {
  return error_count(endpoint) >= max_connect_errors();
}

std::vector<std::string> BlockedEndpoints::get_blocked_client_hosts() const {
  std::lock_guard<std::mutex> lock(mutex_conn_errors_);

  std::vector<std::string> result;
  for (const auto &client_ip : conn_error_counters_v4_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first.to_string());
    }
  }
  for (const auto &client_ip : conn_error_counters_v6_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first.to_string());
    }
  }

  return result;
}
