/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "dest_metadata_cache.h"

#include <algorithm>
#include <cctype>  // toupper
#include <chrono>
#include <iterator>  // advance
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/routing.h"
#include "tcp_address.h"

using namespace std::chrono_literals;
using namespace std::string_view_literals;

IMPORT_LOG_FUNCTIONS()

// if client wants a PRIMARY and there's none, we can wait up to this amount of
// seconds until giving up and disconnecting the client
// TODO: possibly this should be made into a configurable option
static const auto kPrimaryFailoverTimeout = 10s;

// we keep the allow_primary_reads on this list even though we no longer support
// it, so that we give more specific error message for it
static constexpr std::array supported_params{
    "role", "allow_primary_reads", "disconnect_on_promoted_to_primary",
    "disconnect_on_metadata_unavailable"};

namespace {

const constexpr std::array<
    std::pair<std::string_view, DestMetadataCacheGroup::ServerRole>, 3>
    known_roles{{
        {"PRIMARY", DestMetadataCacheGroup::ServerRole::Primary},
        {"SECONDARY", DestMetadataCacheGroup::ServerRole::Secondary},
        {"PRIMARY_AND_SECONDARY",
         DestMetadataCacheGroup::ServerRole::PrimaryAndSecondary},
    }};
}

DestMetadataCacheGroup::ServerRole get_server_role_from_uri(
    const mysqlrouter::URIQuery &uri) {
  const auto it = uri.find("role");
  if (it == uri.end()) {
    throw std::runtime_error(
        "Missing 'role' in routing destination specification");
  }

  const std::string name = it->second;
  std::string name_uc;
  name_uc.resize(name.size());
  std::transform(name.begin(), name.end(), name_uc.begin(), ::toupper);

  auto role_it =
      std::find_if(known_roles.begin(), known_roles.end(),
                   [name = name_uc](const auto &p) { return p.first == name; });

  if (role_it == known_roles.end()) {
    std::string valid_names;
    for (auto role : known_roles) {
      if (!valid_names.empty()) {
        valid_names += ", ";
      }

      valid_names += role.first;
    }

    throw std::runtime_error(
        "The role in '?role=" + name +
        "' does not contain one of the valid role names: " + valid_names);
  }

  return role_it->second;
}

routing::RoutingStrategy get_default_routing_strategy(
    const DestMetadataCacheGroup::ServerRole role) {
  switch (role) {
    case DestMetadataCacheGroup::ServerRole::Primary:
    case DestMetadataCacheGroup::ServerRole::PrimaryAndSecondary:
    case DestMetadataCacheGroup::ServerRole::Secondary:
      return routing::RoutingStrategy::kRoundRobin;
  }

  return routing::RoutingStrategy::kUndefined;
}

// throws:
// - runtime_error if invalid value for the option was discovered
// - check_option_allowed() throws std::runtime_error (it is expected to throw
// if the given
//   option is not allowed (because of wrong combination with other params.
//   etc.))
bool get_yes_no_option(const mysqlrouter::URIQuery &uri,
                       const std::string &option_name, const bool defalut_res,
                       const std::function<void()> &check_option_allowed) {
  if (uri.find(option_name) == uri.end()) return defalut_res;

  check_option_allowed();  // this should throw if this option is not allowed
                           // for given configuration

  std::string value_lc = uri.at(option_name);
  std::transform(value_lc.begin(), value_lc.end(), value_lc.begin(), ::tolower);

  if (value_lc == "no")
    return false;
  else if (value_lc == "yes")
    return true;
  else
    throw std::runtime_error("Invalid value for option '" + option_name +
                             "'. Allowed are 'yes' and 'no'");
}

// throws runtime_error if the parameter has wrong value or is not allowed for
// given configuration
bool get_disconnect_on_promoted_to_primary(
    const mysqlrouter::URIQuery &uri,
    const DestMetadataCacheGroup::ServerRole &role) {
  const std::string kOptionName = "disconnect_on_promoted_to_primary";
  auto check_option_allowed = [&]() {
    if (role != DestMetadataCacheGroup::ServerRole::Secondary) {
      throw std::runtime_error("Option '" + kOptionName +
                               "' is valid only for role=SECONDARY");
    }
  };

  return get_yes_no_option(uri, kOptionName, /*default=*/false,
                           check_option_allowed);
}

// throws runtime_error if the parameter has wrong value or is not allowed for
// given configuration
bool get_disconnect_on_metadata_unavailable(const mysqlrouter::URIQuery &uri) {
  const std::string kOptionName = "disconnect_on_metadata_unavailable";
  auto check_option_allowed = [&]() {};  // always allowed

  return get_yes_no_option(uri, kOptionName, /*default=*/false,
                           check_option_allowed);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// doxygen confuses 'const mysqlrouter::URIQuery &query' with
// 'std::map<std::string, std::string>'
DestMetadataCacheGroup::DestMetadataCacheGroup(
    net::io_context &io_ctx, const std::string &metadata_cache,
    const routing::RoutingStrategy routing_strategy,
    const mysqlrouter::URIQuery &query, const Protocol::Type protocol,
    metadata_cache::MetadataCacheAPIBase *cache_api)
    : RouteDestination(io_ctx, protocol),
      cache_name_(metadata_cache),
      uri_query_(query),
      routing_strategy_(routing_strategy),
      server_role_(get_server_role_from_uri(query)),
      cache_api_(cache_api),
      disconnect_on_promoted_to_primary_(
          get_disconnect_on_promoted_to_primary(query, server_role_)),
      disconnect_on_metadata_unavailable_(
          get_disconnect_on_metadata_unavailable(query)) {
  init();
}
#endif

std::pair<metadata_cache::cluster_nodes_list_t, bool>
DestMetadataCacheGroup::get_available(
    const metadata_cache::cluster_nodes_list_t &instances,
    bool for_new_connections) const {
  metadata_cache::cluster_nodes_list_t result;

  bool primary_fallback{false};
  if (routing_strategy_ == routing::RoutingStrategy::kRoundRobinWithFallback) {
    // if there are no secondaries available we fall-back to primaries
    std::lock_guard<std::mutex> lock(
        query_quarantined_destinations_callback_mtx_);
    auto secondary = std::find_if(
        instances.begin(), instances.end(),
        [&](const metadata_cache::ManagedInstance &i) {
          if (for_new_connections && query_quarantined_destinations_callback_) {
            return i.mode == metadata_cache::ServerMode::ReadOnly &&
                   !i.hidden && !i.ignore &&
                   !query_quarantined_destinations_callback_(i);
          } else {
            return i.mode == metadata_cache::ServerMode::ReadOnly &&
                   !i.hidden && !i.ignore;
          }
        });

    primary_fallback = secondary == instances.end();
  }
  // if we are gathering the nodes for the decision about keeping existing
  // connections we look also at the disconnect_on_promoted_to_primary_ setting
  // if set to 'no' we need to allow primaries for role=SECONDARY
  if (!for_new_connections && server_role_ == ServerRole::Secondary &&
      !disconnect_on_promoted_to_primary_) {
    primary_fallback = true;
  }

  for (const auto &it : instances) {
    if (it.ignore) continue;
    if (for_new_connections) {
      // for new connections skip (do not include) the node if it is hidden - it
      // is not allowed
      if (it.hidden) continue;
    } else {
      // for the existing connections skip (do not include) the node if it is
      // hidden and disconnect_existing_sessions_when_hidden is true
      if (it.hidden && it.disconnect_existing_sessions_when_hidden) continue;
    }

    // role=PRIMARY_AND_SECONDARY
    if ((server_role_ == ServerRole::PrimaryAndSecondary) &&
        (it.mode == metadata_cache::ServerMode::ReadWrite ||
         it.mode == metadata_cache::ServerMode::ReadOnly)) {
      result.emplace_back(it);
      continue;
    }

    // role=SECONDARY
    if (server_role_ == ServerRole::Secondary &&
        it.mode == metadata_cache::ServerMode::ReadOnly) {
      result.emplace_back(it);
      continue;
    }

    // role=PRIMARY
    if ((server_role_ == ServerRole::Primary || primary_fallback) &&
        it.mode == metadata_cache::ServerMode::ReadWrite) {
      result.emplace_back(it);
      continue;
    }
  }

  return {result, primary_fallback};
}

metadata_cache::cluster_nodes_list_t
DestMetadataCacheGroup::get_available_primaries(
    const metadata_cache::cluster_nodes_list_t &managed_servers) const {
  metadata_cache::cluster_nodes_list_t result;

  for (const auto &it : managed_servers) {
    if (it.hidden || it.ignore) continue;

    if (it.mode == metadata_cache::ServerMode::ReadWrite) {
      result.emplace_back(it);
    }
  }

  return result;
}

void DestMetadataCacheGroup::init() {
  // check if URI does not contain parameters that we don't understand
  for (const auto &uri_param : uri_query_) {
    if (std::find(supported_params.begin(), supported_params.end(),
                  uri_param.first) == supported_params.end()) {
      throw std::runtime_error(
          "Unsupported 'metadata-cache' parameter in URI: '" + uri_param.first +
          "'");
    }
  }

  // if the routing_strategy is not set we go with the default based on the role
  if (routing_strategy_ == routing::RoutingStrategy::kUndefined) {
    routing_strategy_ = get_default_routing_strategy(server_role_);
  }

  auto query_part = uri_query_.find("allow_primary_reads");
  if (query_part != uri_query_.end()) {
    throw std::runtime_error(
        "allow_primary_reads is no longer supported, use "
        "role=PRIMARY_AND_SECONDARY instead");
  }

  // validate routing strategy:
  switch (routing_strategy_) {
    case routing::RoutingStrategy::kRoundRobinWithFallback:
      if (server_role_ != ServerRole::Secondary) {
        throw std::runtime_error(
            "Strategy 'round-robin-with-fallback' is supported only for "
            "SECONDARY routing");
      }
      break;
    case routing::RoutingStrategy::kFirstAvailable:
    case routing::RoutingStrategy::kRoundRobin:
      break;
    default:
      throw std::runtime_error(
          "Unsupported routing strategy: " +
          routing::get_routing_strategy_name(routing_strategy_));
  }
}

void DestMetadataCacheGroup::subscribe_for_metadata_cache_changes() {
  cache_api_->add_state_listener(this);
  subscribed_for_metadata_cache_changes_ = true;
}

void DestMetadataCacheGroup::subscribe_for_acceptor_handler() {
  cache_api_->add_acceptor_handler_listener(this);
}

void DestMetadataCacheGroup::subscribe_for_md_refresh_handler() {
  cache_api_->add_md_refresh_listener(this);
}

DestMetadataCacheGroup::~DestMetadataCacheGroup() {
  if (subscribed_for_metadata_cache_changes_) {
    cache_api_->remove_state_listener(this);
    cache_api_->remove_acceptor_handler_listener(this);
    cache_api_->remove_md_refresh_listener(this);
  }
}

class MetadataCacheDestination : public Destination {
 public:
  MetadataCacheDestination(std::string id, std::string addr, uint16_t port,
                           DestMetadataCacheGroup *balancer,
                           std::string server_uuid,
                           mysqlrouter::ServerMode server_mode)
      : Destination(std::move(id), std::move(addr), port),
        balancer_{balancer},
        server_uuid_{std::move(server_uuid)},
        server_mode_{server_mode} {}

  void connect_status(std::error_code ec) override {
    last_ec_ = ec;

    if (ec != std::error_code{}) {
      // the tests
      //
      // - NodeUnavailable/NodeUnavailableTest.NodeUnavailable/1, where
      //   GetParam() = "round-robin"
      // - NodeUnavailable/NodeUnavailableTest.NodeUnavailable/2, where
      //   GetParam() = "round-robin-with-fallback"
      //
      // rely on moving the ndx forward in case of failure.

      balancer_->advance(1);
    }
  }

  std::string server_uuid() const { return server_uuid_; }

  std::error_code last_error_code() const { return last_ec_; }

  mysqlrouter::ServerMode server_mode() const override { return server_mode_; }

 private:
  DestMetadataCacheGroup *balancer_;

  std::string server_uuid_;

  mysqlrouter::ServerMode server_mode_;

  std::error_code last_ec_;
};

// the first round of destinations didn't succeed.
//
// try to fallback.
std::optional<Destinations> DestMetadataCacheGroup::refresh_destinations(
    const Destinations &previous_dests) {
  if (cache_api_->cluster_type() == mysqlrouter::ClusterType::RS_V2) {
    // ReplicaSet
    if (routing_strategy_ ==
            routing::RoutingStrategy::kRoundRobinWithFallback &&
        !previous_dests.primary_already_used()) {
      // get the primaries
      return primary_destinations();
    }
  } else {
    // Group Replication
    if (server_role() == DestMetadataCacheGroup::ServerRole::Primary) {
      // verify preconditions.
      assert(!previous_dests.empty() &&
             "previous destinations MUST NOT be empty");

      assert(previous_dests.is_primary_destination() &&
             "previous destinations MUST be primary destinations");

      if (previous_dests.empty()) {
        return std::nullopt;
      }

      if (!previous_dests.is_primary_destination()) {
        return std::nullopt;
      }

      // if connecting to the primary failed differentiate between:
      //
      // - network failure
      // - member failure
      //
      // On network failure (timeout, network-not-reachable, ...), fail
      // directly.
      //
      // On member failure (connection refused, ...) wait for failover and use
      // the new primary.

      auto const *primary_member = dynamic_cast<MetadataCacheDestination *>(
          previous_dests.begin()->get());

      const auto err = primary_member->last_error_code();
      log_debug("refresh_destinations(): %s:%s", err.category().name(),
                err.message().c_str());

      if (err == make_error_condition(std::errc::timed_out) ||
          err == make_error_condition(std::errc::no_such_file_or_directory)) {
        return std::nullopt;
      }

      if (cache_api_->wait_primary_failover(primary_member->server_uuid(),
                                            kPrimaryFailoverTimeout)) {
        return primary_destinations();
      }
    }
  }

  return std::nullopt;
}

void DestMetadataCacheGroup::advance(size_t n) {
  std::lock_guard<std::mutex> lk(mutex_update_);

  start_pos_ += n;
}

Destinations DestMetadataCacheGroup::balance(
    const metadata_cache::cluster_nodes_list_t &available,
    bool primary_fallback) {
  Destinations dests;

  auto from_instance = [this](const auto &dest) {
    mysql_harness::TCPAddress addr(
        dest.host,
        (protocol_ == Protocol::Type::kXProtocol) ? dest.xport : dest.port);

    return std::make_unique<MetadataCacheDestination>(
        addr.str(), addr.address(), addr.port(), this, dest.mysql_server_uuid,
        dest.mode);
  };

  std::lock_guard<std::mutex> lk(mutex_update_);

  switch (routing_strategy_) {
    case routing::RoutingStrategy::kFirstAvailable: {
      for (auto const &dest : available) {
        dests.push_back(from_instance(dest));
      }

      break;
    }
    case routing::RoutingStrategy::kRoundRobinWithFallback:
    case routing::RoutingStrategy::kRoundRobin: {
      if (available.empty()) break;

      const auto sz = available.size();
      const auto end = available.end();
      const auto begin = available.begin();

      // start pos moves forward with each call to balance().
      //
      // ensure it wraps around.

      if (start_pos_ >= sz) start_pos_ = 0;
      if (rw_start_pos_ >= sz) rw_start_pos_ = 0;
      if (ro_start_pos_ >= sz) ro_start_pos_ = 0;

      // Goal:
      //
      // - writes round-robins over read-write-servers
      // - reads  round-robins over read-only servers
      //
      // Example:
      //
      // available  = [ W1, R1, R2, W2, R3 ]
      // writers    = [ W1, W2 ]
      // readers    = [ R1, R2, R3 ]
      //
      // Each group is rotated independently and then appened
      // depending on the server-mode of the current "start_pos".
      //
      // input:         -> output         (auth, write, read)
      //
      // W1 R1 R2 W2 R3 -> W1 W2 R1 R2 R3 (a: W1, w: W1, r: R1)
      // R1 R2 W2 R3 W1 -> R2 R3 R1 W2 W1 (a: R2: w: W2, r: R2)
      // R2 W2 R3 W1 R1 -> R3 R1 R2 W1 W2 (a: R3: w: W1, r: R3)
      // W2 R3 W1 R1 R2 -> W2 W1 R1 R2 R3 (a: W2: w: W2, r: R1)
      // R3 W1 R1 R2 W2 -> R2 R3 R1 W1 W2 (a: R2: w: W1, r: R2)
      // W1 R1 R2 W2 R3 -> W2 W1 R3 R1 R2 (a: W2: w: W2, r: R3)
      //
      // ^^- start-pos -> initial-server-mode.

      //
      const auto initial_server_mode = available[start_pos_].mode;

      auto &initial_server_mode_start_pos =
          initial_server_mode == mysqlrouter::ServerMode::ReadOnly
              ? ro_start_pos_
              : rw_start_pos_;

      auto &other_server_mode_start_pos =
          initial_server_mode == mysqlrouter::ServerMode::ReadOnly
              ? rw_start_pos_
              : ro_start_pos_;

      {
        if (initial_server_mode_start_pos >= sz) {
          initial_server_mode_start_pos = 0;
        }

        // move iterator forward and remember the position as 'last'
        auto cur = begin;
        std::advance(cur, initial_server_mode_start_pos);
        auto last = cur;

        auto pos = initial_server_mode_start_pos;
        bool is_first{true};

        // for start_pos == 2:
        //
        // 0 1 2 3 4 x
        // ^   ^     ^
        // |   |     `- end
        // |   `- last|cur
        // `- begin

        // from last to end;
        //
        // dests = [2 3 4]
        for (; cur != end; ++cur, ++pos) {
          if (cur->mode == initial_server_mode) {
            if (is_first) {
              initial_server_mode_start_pos = pos;
              is_first = false;
            }
            dests.push_back(from_instance(*cur));
          }
        }

        // from begin to before-last
        //
        // dests = [2 3 4] + [0 1]
        for (cur = begin, pos = 0; cur != last; ++cur, ++pos) {
          if (cur->mode == initial_server_mode) {
            if (is_first) {
              initial_server_mode_start_pos = pos;
              is_first = false;
            }
            dests.push_back(from_instance(*cur));
          }
        }

        if (++initial_server_mode_start_pos >= sz) {
          initial_server_mode_start_pos = 0;
        }
      }

      // ... and now the other server-mode set:
      {
        if (other_server_mode_start_pos >= sz) {
          other_server_mode_start_pos = 0;
        }

        // move iterator forward and remember the position as 'last'
        auto cur = begin;
        std::advance(cur, other_server_mode_start_pos);
        auto last = cur;

        auto pos = other_server_mode_start_pos;
        bool is_first{true};

        // from last to end;
        //
        // dests = [2 3 4]
        for (; cur != end; ++cur, ++pos) {
          if (cur->mode != initial_server_mode) {
            if (is_first) {
              other_server_mode_start_pos = pos;
              is_first = false;
            }
            dests.push_back(from_instance(*cur));
          }
        }

        // from begin to before-last
        //
        // dests = [2 3 4] + [0 1]
        for (cur = begin, pos = 0; cur != last; ++cur, ++pos) {
          if (cur->mode != initial_server_mode) {
            if (is_first) {
              other_server_mode_start_pos = pos;
              is_first = false;
            }
            dests.push_back(from_instance(*cur));
          }
        }

        if (++other_server_mode_start_pos >= sz) {
          other_server_mode_start_pos = 0;
        }
      }
      // NOTE: AsyncReplicasetTest.SecondaryAdded from
      // routertest_component_async_replicaset depends on the start_pos_ is
      // capped here.
      //
      // replacing it with:
      //
      //    ++start_pos_;
      //
      // would be correct too, but change the order of destinations that the
      // test expects.
      if (++start_pos_ >= sz) start_pos_ = 0;

      break;
    }
    case routing::RoutingStrategy::kNextAvailable:
    case routing::RoutingStrategy::kUndefined:
      assert(0);
      break;
  }

  if (dests.empty()) {
    log_warning("No available servers found for %s routing",
                server_role_ == ServerRole::Primary ? "PRIMARY" : "SECONDARY");

    // return an empty list
    return dests;
  }

  if (primary_fallback) {
    // announce that we already use the primaries and don't want to fallback
    dests.primary_already_used(true);
  }

  if (server_role() == DestMetadataCacheGroup::ServerRole::Primary) {
    dests.set_is_primary_destination(true);
  }

  return dests;
}

Destinations DestMetadataCacheGroup::destinations() {
  if (!cache_api_->is_initialized()) return {};

  const auto &all_replicaset_nodes = cache_api_->get_cluster_nodes();

  auto [available, primary_failover] = get_available(all_replicaset_nodes);

  return balance(available, primary_failover);
}

Destinations DestMetadataCacheGroup::primary_destinations() {
  if (!cache_api_->is_initialized()) return {};

  const auto &all_replicaset_nodes = cache_api_->get_cluster_nodes();

  auto available = get_available_primaries(all_replicaset_nodes);

  return balance(available, true);
}

DestMetadataCacheGroup::AddrVector DestMetadataCacheGroup::get_destinations()
    const {
  // don't call lookup if the cache-api is not ready yet.
  if (!cache_api_->is_initialized()) return {};

  auto available = get_available(cache_api_->get_cluster_nodes()).first;

  AddrVector addresses;
  for (const auto &dest : available) {
    addresses.emplace_back(dest.host, protocol_ == Protocol::Type::kXProtocol
                                          ? dest.xport
                                          : dest.port);
  }

  return addresses;
}

void DestMetadataCacheGroup::on_instances_change(
    const metadata_cache::ClusterTopology &cluster_topology,
    const bool md_servers_reachable) {
  // we got notified that the metadata has changed.
  // If instances is empty then (most like is empty)
  // the metadata-cache cannot connect to the metadata-servers
  // In that case we only disconnect clients if
  // the user configured that it should happen
  // (disconnect_on_metadata_unavailable_ == true)
  const bool disconnect =
      md_servers_reachable || disconnect_on_metadata_unavailable_;

  const auto instances = cluster_topology.get_all_members();
  const std::string reason =
      md_servers_reachable ? "metadata change" : "metadata unavailable";

  auto from_instances = [this](const auto &dests) {
    std::vector<AvailableDestination> result;

    for (auto &dest : dests) {
      mysql_harness::TCPAddress addr(
          dest.host,
          (protocol_ == Protocol::Type::kXProtocol) ? dest.xport : dest.port);

      result.emplace_back(addr, dest.mysql_server_uuid);
    }

    return result;
  };

  auto nodes_for_new_connections = from_instances(
      get_available(instances, /*for_new_connections=*/true).first);

  auto nodes_for_existing_connections = from_instances(
      get_available(instances, /*for_new_connections=*/false).first);

  std::lock_guard<std::mutex> lock(allowed_nodes_change_callbacks_mtx_);

  // notify all the registered listeners about the list of available nodes
  // change
  for (auto &clb : allowed_nodes_change_callbacks_) {
    clb(nodes_for_existing_connections, nodes_for_new_connections, disconnect,
        reason);
  }
}

void DestMetadataCacheGroup::notify_instances_changed(
    const metadata_cache::ClusterTopology &cluster_topology,
    const bool md_servers_reachable, const uint64_t /*view_id*/) noexcept {
  on_instances_change(cluster_topology, md_servers_reachable);
}

bool DestMetadataCacheGroup::update_socket_acceptor_state(
    const metadata_cache::cluster_nodes_list_t &instances) noexcept {
  const auto &nodes_for_new_connections =
      get_available(instances, /*for_new_connections=*/true).first;

  {
    std::lock_guard<std::mutex> lock(socket_acceptor_handle_callbacks_mtx);
    if (!nodes_for_new_connections.empty() &&
        start_router_socket_acceptor_callback_) {
      const auto &start_acceptor_res = start_router_socket_acceptor_callback_();
      return start_acceptor_res ? true : false;
    }

    if (nodes_for_new_connections.empty() &&
        stop_router_socket_acceptor_callback_) {
      stop_router_socket_acceptor_callback_();
      return true;
    }
  }

  return true;
}

void DestMetadataCacheGroup::on_md_refresh(
    const bool nodes_changed,
    const metadata_cache::ClusterTopology &cluster_topology) {
  const auto instances = cluster_topology.get_all_members();

  auto from_instances = [this](const auto &dests) {
    std::vector<AvailableDestination> result;

    for (auto &dest : dests) {
      mysql_harness::TCPAddress addr(
          dest.host,
          (protocol_ == Protocol::Type::kXProtocol) ? dest.xport : dest.port);

      result.emplace_back(addr, dest.mysql_server_uuid);
    }

    return result;
  };

  const auto available_nodes = from_instances(
      get_available(instances, /*for_new_connections=*/true).first);

  std::lock_guard<std::mutex> lock(md_refresh_callback_mtx_);
  if (md_refresh_callback_) {
    md_refresh_callback_(nodes_changed, available_nodes);
  }
}

void DestMetadataCacheGroup::start(const mysql_harness::PluginFuncEnv *env) {
  // before using metadata-cache we need to wait for it to be initialized
  while (!cache_api_->is_initialized() && (!env || is_running(env))) {
    std::this_thread::sleep_for(1ms);
  }

  if (!env || is_running(env)) {
    subscribe_for_metadata_cache_changes();
    subscribe_for_acceptor_handler();
    subscribe_for_md_refresh_handler();
  }
}
