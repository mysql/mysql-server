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

#ifndef ROUTING_BLOCKED_ENDPOINT_INCLUDED
#define ROUTING_BLOCKED_ENDPOINT_INCLUDED

#include <map>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/local.h"

class BlockedEndpoints {
 public:
  BlockedEndpoints(uint64_t max_connect_errors)
      : max_connect_errors_{max_connect_errors} {}

  uint64_t max_connect_errors() const { return max_connect_errors_; }

  /**
   * increments the error count of an endpoint.
   *
   * @param endpoint endpoint increment error counter for.
   * @returns new error count value.
   */
  uint64_t increment_error_count(const net::ip::tcp::endpoint &endpoint);

  /**
   * resets error counter for an endpoint.
   *
   * @sa increment_error_counter()
   *
   * @param endpoint endpoint
   * @returns previous value.
   */
  uint64_t reset_error_count(const net::ip::tcp::endpoint &endpoint);

  bool is_blocked(const net::ip::tcp::endpoint &endpoint) const;

  uint64_t error_count(const net::ip::tcp::endpoint &endpoint) const;

#ifdef NET_TS_HAS_UNIX_SOCKET
  uint64_t increment_error_count(const local::stream_protocol::endpoint &) {
    return 0;
  }

  uint64_t reset_error_count(const local::stream_protocol::endpoint &) {
    return 0;
  }

  bool is_blocked(const local::stream_protocol::endpoint &) const {
    return false;
  }

  uint64_t error_count(const local::stream_protocol::endpoint &) const {
    return 0;
  }
#endif

  /**
   * Returns list of blocked client hosts.
   */
  std::vector<std::string> get_blocked_client_hosts() const;

 private:
  mutable std::mutex mutex_conn_errors_;

  /** Max connect errors blocking hosts when handshake not completed. */
  const uint64_t max_connect_errors_;

  /** Connection error counters for IPv4 hosts. */
  std::map<net::ip::address_v4, uint64_t> conn_error_counters_v4_;

  /** Connection error counters for IPv4 hosts. */
  std::map<net::ip::address_v6, uint64_t> conn_error_counters_v6_;
};

#endif
