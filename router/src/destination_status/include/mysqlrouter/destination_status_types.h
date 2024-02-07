/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_DESTINATION_STATUS_TYPES_INCLUDED
#define MYSQLROUTER_DESTINATION_STATUS_TYPES_INCLUDED

#include <memory>
#include <vector>

#include "tcp_address.h"

struct QuarantineRoutingCallbacks {
  std::function<std::vector<mysql_harness::TCPAddress>(const std::string &)>
      on_get_destinations;
  std::function<void(const std::string &)> on_start_acceptors;
  std::function<void(const std::string &)> on_stop_acceptors;

  void reset() {
    on_get_destinations =
        [](const std::string &) -> std::vector<mysql_harness::TCPAddress> {
      return {};
    };

    on_start_acceptors = [](const std::string &) -> void {};
    on_stop_acceptors = [](const std::string &) -> void {};
  }
};

struct AvailableDestination {
  AvailableDestination(mysql_harness::TCPAddress a, std::string i)
      : address{std::move(a)}, id{std::move(i)} {}

  mysql_harness::TCPAddress address;
  std::string id;
};

using AllowedNodes = std::vector<AvailableDestination>;

#endif  // MYSQLROUTER_DESTINATION_STATUS_TYPES_INCLUDED
