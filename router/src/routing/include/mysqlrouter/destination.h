/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_DESTINATION_INCLUDED
#define MYSQLROUTER_DESTINATION_INCLUDED

#include <cstddef>       // uint16_t
#include <list>          // list
#include <memory>        // unique_ptr
#include <string>        // string
#include <system_error>  // error_code

#include "tcp_address.h"

/**
 * Destination to forward client connections to.
 *
 * It is used between the RouteDestination implementations and MySQLRouting
 */
class Destination {
 public:
  Destination(std::string id, std::string hostname, uint16_t port)
      : id_{std::move(id)}, hostname_{std::move(hostname)}, port_{port} {}

  virtual ~Destination() = default;

  /**
   * unique, opaque identifier of a destination.
   *
   * used by connection container to find allowed destinations.
   */
  std::string id() const { return id_; }

  /**
   * hostname to connect to.
   */
  std::string hostname() const { return hostname_; }

  /**
   * TCP port to connect to.
   */
  uint16_t port() const noexcept { return port_; }

  /**
   * check if the destination is "good".
   *
   * If the destination is not "good", it will be skipped by MySQLRouting.
   *
   * @retval false if destination is known to be bad
   * @retval true otherwise
   */
  virtual bool good() const { return true; }

  /**
   * status of the last failed connect().
   *
   * called by MySQLRouting after a connect() to all addresses
   * of the destination failed.
   */
  virtual void connect_status(std::error_code /* ec */) {}

 private:
  const std::string id_;
  const std::string hostname_;
  const uint16_t port_;
};

/**
 * A forward iterable container of destinations.
 *
 * a PRIMARY destination set won't be failover from.
 *
 * @see RouteDestination::refresh_destinations()
 */
class Destinations {
 public:
  using value_type = std::unique_ptr<Destination>;
  using container_type = std::list<value_type>;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;
  using size_type = typename container_type::size_type;

  iterator begin() { return destinations_.begin(); }
  const_iterator begin() const { return destinations_.begin(); }
  iterator end() { return destinations_.end(); }
  const_iterator end() const { return destinations_.end(); }

  /**
   * emplace a Destination at the back of the container.
   */
  template <class... Args>
  auto emplace_back(Args &&... args) {
    return destinations_.emplace_back(std::forward<Args>(args)...);
  }

  void push_back(value_type &&v) { destinations_.push_back(std::move(v)); }

  /**
   * check if destination container is empty.
   *
   * @retval true if container is empty.
   */
  bool empty() const { return destinations_.empty(); }

  /**
   * clear all values.
   */
  void clear() { destinations_.clear(); }

  /**
   * number of destinations.
   */
  size_type size() const { return destinations_.size(); }

  /**
   * Check if we already used the primaries and don't want to fallback.
   *
   * @retval true primaries already used
   * @retval false primaries are not yet used
   */
  bool primary_already_used() const { return primary_already_used_; }

  /**
   * Mark that the primary destinations are already used.
   *
   * @param p true if PRIMARY destinations are already used.
   */
  void primary_already_used(const bool p) { primary_already_used_ = p; }

  /**
   * Check if destinations are primary destinations.
   *
   * @retval true destinations are primary destinations.
   * @retval false destinations are secondary destinations.
   */
  bool is_primary_destination() const { return is_primary_destination_; }

  /**
   * Mark that the destinations are primary destinations.
   *
   * @param p true if desitnations are PRIMARY destinations.
   */
  void set_is_primary_destination(const bool p) { is_primary_destination_ = p; }

 private:
  container_type destinations_;

  bool primary_already_used_{false};
  bool is_primary_destination_{false};
};

#endif
