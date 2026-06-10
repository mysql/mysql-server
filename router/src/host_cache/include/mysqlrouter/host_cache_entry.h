/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_HOST_CACHE_ENTRY_H_
#define MYSQLROUTER_HOST_CACHE_ENTRY_H_

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "mysql/harness/net_ts/internet.h"

namespace host_cache {

class Entry {
 public:
  using steady_clock = std::chrono::steady_clock;
  using time_point = steady_clock::time_point;

  std::string hostname_;
  std::vector<net::ip::address> addresses_;
  std::chrono::seconds ttl_;
  time_point creation_time_{steady_clock::now()};
  std::atomic<uint64_t> cache_hits_{0};
  std::atomic<uint64_t> resolve_waiters_peak_{0};

  Entry() {}

  Entry(const Entry &other) { *this = other; }

  Entry &operator=(const Entry &other) {
    hostname_ = other.hostname_;
    addresses_ = other.addresses_;
    ttl_ = other.ttl_;
    creation_time_ = other.creation_time_;
    cache_hits_ = other.cache_hits_.load();
    resolve_waiters_peak_ = other.resolve_waiters_peak_.load();

    return *this;
  }
};

}  // namespace host_cache

#endif  // MYSQLROUTER_HOST_CACHE_ENTRY_H_
