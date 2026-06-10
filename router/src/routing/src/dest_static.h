/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTING_DEST_STATIC_INCLUDED
#define ROUTING_DEST_STATIC_INCLUDED

#include "destination.h"  // DestinationManager

/**
 * Base class for routing strategy handler.
 */
class StrategyHandler {
 public:
  virtual ~StrategyHandler() = default;

  virtual std::optional<std::uint32_t> get_destination_index(
      const bool last_connection_successful,
      const std::uint32_t dest_pool_size) = 0;

 protected:
  std::uint32_t index_pos_{0};
};

/**
 * First-available strategy. Move to next destination only if the last
 * connection was unsuccessful. After successful connection attempt always try
 * from the beginning.
 */
class First_available_strategy : public StrategyHandler {
 public:
  std::optional<std::uint32_t> get_destination_index(
      const bool last_connection_successful,
      const std::uint32_t dest_pool_size) override;
};

/**
 * First-available strategy. Move to the next destination if the last
 * connection was unsuccessful. Keep the current position if connection is
 * successful (not going back, might exhaust the destination list).
 */
class Next_available_strategy : public StrategyHandler {
 public:
  std::optional<std::uint32_t> get_destination_index(
      const bool last_connection_successful,
      const std::uint32_t dest_pool_size) override;
};

/**
 * Round-robin strategy. Move to next destination after each connection attempt.
 * If end of the destination candidates list is reached then loop around. If
 * destination candidates list is exhausted (after unsuccessful connection we
 * tried every destinaion from the list and eventually went back to the position
 * that failed at first) then fail the connection.
 */
class Round_robin_strategy : public StrategyHandler {
 public:
  std::optional<std::uint32_t> get_destination_index(
      const bool last_connection_successful,
      const std::uint32_t dest_pool_size) override;

 private:
  bool started_{false};
  std::optional<std::uint32_t> failed_instance_index_;
};

class StaticDestinationsManager final : public DestinationManager {
 public:
  StaticDestinationsManager(routing::RoutingStrategy strategy,
                            net::io_context &io_ctx,
                            MySQLRoutingContext &routing_ctx);

  /** @brief Adds a destination
   *
   * Adds a destination using the given address and port number.
   *
   * @param dest destination address
   */
  void add(const mysql_harness::Destination &dest);

  void start(const mysql_harness::PluginFuncEnv *) override;

  std::vector<mysql_harness::Destination> get_destination_candidates()
      const override {
    return destinations_;
  }

  bool refresh_destinations(const routing_guidelines::Session_info &) override {
    return false;
  }

  void handle_sockets_acceptors() override {}

  std::unique_ptr<Destination> get_next_destination(
      const routing_guidelines::Session_info &) override;

  std::unique_ptr<Destination> get_last_used_destination() const override {
    return std::make_unique<Destination>(last_destination_);
  }

  stdx::expected<void, std::error_code> init_destinations(
      const routing_guidelines::Session_info &) override {
    return {};
  }

  void connect_status(std::error_code ec) override;

  bool has_read_write() const override { return !destinations_.empty(); }
  bool has_read_only() const override { return !destinations_.empty(); }

 private:
  /** Get destination index based on routing strategy for static routes*/
  std::unique_ptr<StrategyHandler> strategy_handler_;

  /** @brief List of destinations */
  DestVector destinations_;
  Destination last_destination_;

  /** @brief Protocol for the endpoint */
  Protocol::Type protocol_;
};

#endif  // ROUTING_DEST_STATIC_INCLUDED
