/*
  Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_DESTINATION_INCLUDED
#define MYSQLROUTER_DESTINATION_INCLUDED

#include <list>          // list
#include <memory>        // unique_ptr
#include <string>        // string
#include <system_error>  // error_code

#include "mysql/harness/destination.h"
#include "mysqlrouter/datatypes.h"  // ServerMode
#include "routing_guidelines/routing_guidelines.h"

/**
 * Destination to forward client connections to.
 *
 * It is used between the DestinationManager implementations and MySQLRouting
 */
class ROUTING_EXPORT Destination {
 public:
  Destination(
      mysql_harness::Destination dest,
      routing_guidelines::Server_info server_info,
      std::string routing_guidelines_route_name,
      const std::optional<bool> connection_sharing_allowed = std::nullopt)
      : dest_(std::move(dest)), server_info_(std::move(server_info)) {
    guidelines_route_info_.route_name =
        std::move(routing_guidelines_route_name);
    guidelines_route_info_.connection_sharing_allowed =
        connection_sharing_allowed;
  }

  Destination() = default;
  Destination(const Destination &) = default;
  Destination &operator=(const Destination &) = default;
  Destination(Destination &&) = default;
  Destination &operator=(Destination &&) = default;
  virtual ~Destination() = default;

  struct Guidelines_route_info {
    std::optional<bool> connection_sharing_allowed;
    std::string route_name;
  };

  const mysql_harness::Destination &destination() const {
    return dest_.value();
  }

  /**
   * Get server UUID.
   */
  const std::string &server_uuid() const { return server_info_.uuid; }

  /**
   * Get server information.
   */
  const routing_guidelines::Server_info &get_server_info() const {
    return server_info_;
  }

  /**
   * Get name of the route that was used to reach this destination.
   *
   * @return route name
   */
  const std::string &route_name() const {
    return guidelines_route_info_.route_name;
  }

  /**
   * Set name of the route that was used to reach this destination.
   *
   * @param name route name
   */
  void set_route_name(std::string name) {
    guidelines_route_info_.route_name = std::move(name);
  }

  /**
   * server-mode of the destination.
   *
   * may be: unavailable, read-only or read-write.
   */
  virtual mysqlrouter::ServerMode server_mode() const;

  const Guidelines_route_info &guidelines_route_info() const {
    return guidelines_route_info_;
  }

  /**
   * Disable connection sharing if sharing prerequisites cannot be met.
   */
  void disable_connection_sharing() {
    guidelines_route_info_.connection_sharing_allowed = false;
  }

 private:
  std::optional<mysql_harness::Destination> dest_;
  routing_guidelines::Server_info server_info_{};
  Guidelines_route_info guidelines_route_info_;
};

#endif
