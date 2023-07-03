/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_DEST_ROUND_ROBIN_INCLUDED
#define ROUTING_DEST_ROUND_ROBIN_INCLUDED

#include <algorithm>
#include <condition_variable>
#include <future>
#include <mutex>

#include "destination.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/routing.h"

class DestRoundRobin : public RouteDestination {
 public:
  /** @brief Default constructor
   *
   * @param io_ctx context for io operations
   * @param protocol Protocol for the destination, defaults to value returned
   *        by Protocol::get_default()
   */
  DestRoundRobin(net::io_context &io_ctx,
                 Protocol::Type protocol = Protocol::get_default())
      : RouteDestination(io_ctx, protocol) {}

  /** @brief Destructor */
  ~DestRoundRobin() override = default;

  Destinations destinations() override;

  routing::RoutingStrategy get_strategy() override {
    return routing::RoutingStrategy::kRoundRobin;
  }

 protected:
  // MUST take the RouteDestination Mutex
  size_t start_pos_{};
};

#endif  // ROUTING_DEST_ROUND_ROBIN_INCLUDED
