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

#include "dest_static.h"

StaticDestinationsManager::StaticDestinationsManager(
    routing::RoutingStrategy strategy, net::io_context &io_ctx,
    MySQLRoutingContext &routing_ctx)
    : DestinationManager(io_ctx, routing_ctx),
      protocol_{routing_ctx.get_protocol()} {
  switch (strategy) {
    case routing::RoutingStrategy::kRoundRobin:
      strategy_handler_ = std::make_unique<Round_robin_strategy>();
      break;
    case routing::RoutingStrategy::kFirstAvailable:
      strategy_handler_ = std::make_unique<First_available_strategy>();
      break;
    case routing::RoutingStrategy::kNextAvailable:
      strategy_handler_ = std::make_unique<Next_available_strategy>();
      break;
    default:
      throw std::runtime_error(
          "Strategy round-robin-with-fallback is not supported for static "
          "routing");
  }
}

void StaticDestinationsManager::start(const mysql_harness::PluginFuncEnv *) {
  std::lock_guard<std::mutex> lock(state_mtx_);

  if (destinations_.empty()) {
    throw std::runtime_error("No destinations available");
  }
}

void StaticDestinationsManager::add(const mysql_harness::Destination &dest) {
  std::lock_guard<std::mutex> lock(state_mtx_);

  auto dest_end = destinations_.end();

  if (std::find(destinations_.begin(), dest_end, dest) == dest_end) {
    destinations_.push_back(dest);
  }
}

void StaticDestinationsManager::connect_status(std::error_code ec) {
  std::lock_guard<std::mutex> lock(state_mtx_);
  last_ec_ = ec;
}

std::unique_ptr<Destination> StaticDestinationsManager::get_next_destination(
    const routing_guidelines::Session_info &) {
  std::lock_guard<std::mutex> lock(state_mtx_);
  const bool last_connect_successful = !last_ec_;

  const auto index = strategy_handler_->get_destination_index(
      last_connect_successful, destinations_.size());

  if (!index.has_value() || *index >= destinations_.size()) return nullptr;

  const auto dest = destinations_[*index];

  routing_guidelines::Server_info server_info;

  if (dest.is_tcp()) {
    const auto dest_tcp = dest.as_tcp();
    server_info.address = dest_tcp.hostname();
    if (protocol_ == Protocol::Type::kClassicProtocol) {
      server_info.port = dest_tcp.port();
    } else {
      server_info.port_x = dest_tcp.port();
    }
  }

  last_destination_ = Destination(dest, server_info, "");
  return std::make_unique<Destination>(last_destination_);
}

std::optional<std::uint32_t> First_available_strategy::get_destination_index(
    const bool last_connection_successful,
    const std::uint32_t /*dest_pool_size*/) {
  if (last_connection_successful) {
    index_pos_ = 0;
  } else {
    index_pos_++;
  }
  return index_pos_;
}

std::optional<std::uint32_t> Next_available_strategy::get_destination_index(
    const bool last_connection_successful,
    const std::uint32_t /*dest_pool_size*/) {
  if (!last_connection_successful) index_pos_++;

  return index_pos_;
}

std::optional<std::uint32_t> Round_robin_strategy::get_destination_index(
    const bool last_connection_successful, const std::uint32_t dest_pool_size) {
  if (!started_) {
    started_ = true;
    return index_pos_;
  }

  if (!last_connection_successful && !failed_instance_index_) {
    // Store first failing
    failed_instance_index_ = index_pos_;
  } else if (last_connection_successful) {
    failed_instance_index_ = std::nullopt;
  }

  index_pos_++;
  if (index_pos_ >= dest_pool_size) index_pos_ = 0;

  // Each destination that we tried has failed, there are no more destinations
  // that could be used.
  if (failed_instance_index_.has_value() &&
      failed_instance_index_.value() == index_pos_)
    return std::nullopt;

  return index_pos_;
}
