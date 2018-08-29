/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "dest_metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "utils.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#ifndef _WIN32
#include <netdb.h>
#include <netinet/tcp.h>
#endif

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/utils.h"
#include "tcp_address.h"

using mysqlrouter::to_string;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::out_of_range;
using std::runtime_error;

using metadata_cache::ManagedInstance;
IMPORT_LOG_FUNCTIONS()

// if client wants a primary and there's none, we can wait up to this amount of
// seconds until giving up and disconnecting the client
// TODO: possibly this should be made into a configurable option
static const int kPrimaryFailoverTimeout = 10;

static const std::set<std::string> supported_params{
    "role", "allow_primary_reads", "disconnect_on_promoted_to_primary",
    "disconnect_on_metadata_unavailable"};

namespace {

DestMetadataCacheGroup::ServerRole get_server_role_from_uri(
    const mysqlrouter::URIQuery &uri) {
  if (uri.find("role") == uri.end())
    throw runtime_error("Missing 'role' in routing destination specification");

  const std::string name = uri.at("role");
  std::string name_lc = name;
  std::transform(name.begin(), name.end(), name_lc.begin(), ::tolower);

  if (name_lc == "primary")
    return DestMetadataCacheGroup::ServerRole::Primary;
  else if (name_lc == "secondary")
    return DestMetadataCacheGroup::ServerRole::Secondary;
  else if (name_lc == "primary_and_secondary")
    return DestMetadataCacheGroup::ServerRole::PrimaryAndSecondary;

  throw std::runtime_error("Invalid server role in metadata cache routing '" +
                           name + "'");
}

std::string get_server_role_name(
    const DestMetadataCacheGroup::ServerRole role) {
  switch (role) {
    case DestMetadataCacheGroup::ServerRole::Primary:
      return "primary";
    case DestMetadataCacheGroup::ServerRole::Secondary:
      return "secondary";
    case DestMetadataCacheGroup::ServerRole::PrimaryAndSecondary:
      return "primary_and_secondary";
  }

  return "unknown";
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

/** @brief check that mode (if present) is correct for the role */
bool mode_is_valid(const routing::AccessMode mode,
                   const DestMetadataCacheGroup::ServerRole role) {
  // no mode given, that's ok, nothing to check
  if (mode == routing::AccessMode::kUndefined) {
    return true;
  }

  switch (role) {
    case DestMetadataCacheGroup::ServerRole::Primary:
      return mode == routing::AccessMode::kReadWrite;
    case DestMetadataCacheGroup::ServerRole::Secondary:
    case DestMetadataCacheGroup::ServerRole::PrimaryAndSecondary:
      return mode == routing::AccessMode::kReadOnly;
    default:;  //
               /* fall-through, no acces mode is valid for that role */
  }

  return false;
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
                               "' is valid only for mode=SECONDARY");
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

}  // namespace

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// doxygen confuses 'const mysqlrouter::URIQuery &query' with
// 'std::map<std::string, std::string>'
DestMetadataCacheGroup::DestMetadataCacheGroup(
    const std::string &metadata_cache, const std::string &replicaset,
    const routing::RoutingStrategy routing_strategy,
    const mysqlrouter::URIQuery &query, const Protocol::Type protocol,
    const routing::AccessMode access_mode,
    metadata_cache::MetadataCacheAPIBase *cache_api,
    routing::RoutingSockOpsInterface *routing_sock_ops)
    : RouteDestination(protocol, routing_sock_ops),
      cache_name_(metadata_cache),
      ha_replicaset_(replicaset),
      uri_query_(query),
      current_pos_(0),
      routing_strategy_(routing_strategy),
      access_mode_(access_mode),
      server_role_(get_server_role_from_uri(query)),
      cache_api_(cache_api),
      disconnect_on_promoted_to_primary_(
          get_disconnect_on_promoted_to_primary(query, server_role_)),
      disconnect_on_metadata_unavailable_(
          get_disconnect_on_metadata_unavailable(query)) {
  init();
}
#endif

DestMetadataCacheGroup::AvailableDestinations
DestMetadataCacheGroup::get_available(
    const metadata_cache::LookupResult &managed_servers,
    bool for_new_connections) {
  DestMetadataCacheGroup::AvailableDestinations result;

  bool primary_fallback{false};
  const auto &managed_servers_vec = managed_servers.instance_vector;
  if (routing_strategy_ == routing::RoutingStrategy::kRoundRobinWithFallback) {
    // if there are no secondaries available we fall-back to primaries
    auto secondary =
        std::find_if(managed_servers_vec.begin(), managed_servers_vec.end(),
                     [](const metadata_cache::ManagedInstance &i) {
                       return i.mode == metadata_cache::ServerMode::ReadOnly;
                     });

    primary_fallback = secondary == managed_servers_vec.end();
  }

  // if we are gathering the nodes for the decision about keeping existing
  // connections we look also at the disconnect_on_promoted_to_primary_ setting
  // if set to 'no' we need to allow primaries for role=SECONDARY
  if (!for_new_connections && server_role_ == ServerRole::Secondary &&
      !disconnect_on_promoted_to_primary_) {
    primary_fallback = true;
  }

  for (const auto &it : managed_servers_vec) {
    if (!(it.role == "HA")) {
      continue;
    }
    auto port = (protocol_ == Protocol::Type::kXProtocol)
                    ? static_cast<uint16_t>(it.xport)
                    : static_cast<uint16_t>(it.port);

    // role=PRIMARY_AND_SECONDARY
    if ((server_role_ == ServerRole::PrimaryAndSecondary) &&
        (it.mode == metadata_cache::ServerMode::ReadWrite ||
         it.mode == metadata_cache::ServerMode::ReadOnly)) {
      result.address.push_back(mysql_harness::TCPAddress(it.host, port));
      result.id.push_back(it.mysql_server_uuid);
      continue;
    }

    // role=SECONDARY
    if (server_role_ == ServerRole::Secondary &&
        it.mode == metadata_cache::ServerMode::ReadOnly) {
      result.address.push_back(mysql_harness::TCPAddress(it.host, port));
      result.id.push_back(it.mysql_server_uuid);
      continue;
    }

    // role=PRIMARY
    if ((server_role_ == ServerRole::Primary || primary_fallback) &&
        it.mode == metadata_cache::ServerMode::ReadWrite) {
      result.address.push_back(mysql_harness::TCPAddress(it.host, port));
      result.id.push_back(it.mysql_server_uuid);
      continue;
    }
  }

  return result;
}

void DestMetadataCacheGroup::init() {
  // check if URI does not contain parameters that we don't understand
  for (const auto &uri_param : uri_query_) {
    if (supported_params.count(uri_param.first) == 0) {
      throw std::runtime_error(
          "Unsupported 'metadata-cache' parameter in URI: '" + uri_param.first +
          "'");
    }
  }

  // if the routing strategy is set we don't allow mode to be set
  if (routing_strategy_ != routing::RoutingStrategy::kUndefined &&
      access_mode_ != routing::AccessMode::kUndefined) {
    throw std::runtime_error(
        "option 'mode' is not allowed together with 'routing_strategy' option");
  }

  bool routing_strategy_default{false};
  // if the routing_strategy is not set we go with the default based on the role
  if (routing_strategy_ == routing::RoutingStrategy::kUndefined) {
    routing_strategy_ = get_default_routing_strategy(server_role_);
    routing_strategy_default = true;
  }

  // check that mode (if present) is correct for the role
  // we don't actually use it but support it for backward compatibility
  // and parity with STANDALONE routing destinations
  if (!mode_is_valid(access_mode_, server_role_)) {
    throw std::runtime_error(
        "mode '" + routing::get_access_mode_name(access_mode_) +
        "' is not valid for 'role=" + get_server_role_name(server_role_) + "'");
  }

  // this is for backward compatibility
  // old(allow_primary_reads + role=SECONDARY) = new
  // (role=PRIMARY_AND_SECONDARY)
  auto query_part = uri_query_.find("allow_primary_reads");
  if (query_part != uri_query_.end()) {
    if (server_role_ != ServerRole::Secondary) {
      throw std::runtime_error(
          "allow_primary_reads is supported only for SECONDARY routing");
    }
    if (!routing_strategy_default) {
      throw std::runtime_error(
          "allow_primary_reads is only supported for backward compatibility: "
          "without routing_strategy but with mode defined, use "
          "role=PRIMARY_AND_SECONDARY instead");
    }
    auto value = query_part->second;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    if (value == "yes") {
      server_role_ = ServerRole::PrimaryAndSecondary;
    } else if (value == "no") {
      // it's a default but we allow it for consistency
    } else {
      throw std::runtime_error(
          "Invalid value for allow_primary_reads option: '" +
          query_part->second + "'");
    }
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
  using namespace std::placeholders;

  cache_api_->add_listener(ha_replicaset_, this);
  subscribed_for_metadata_cache_changes_ = true;
}

DestMetadataCacheGroup::~DestMetadataCacheGroup() {
  if (subscribed_for_metadata_cache_changes_) {
    cache_api_->remove_listener(ha_replicaset_, this);
  }
}

size_t DestMetadataCacheGroup::get_next_server(
    const DestMetadataCacheGroup::AvailableDestinations &available) {
  std::lock_guard<std::mutex> lock(mutex_update_);
  size_t result = 0;

  switch (routing_strategy_) {
    case routing::RoutingStrategy::kFirstAvailable:
      result = current_pos_;
      break;
    case routing::RoutingStrategy::kRoundRobin:
    case routing::RoutingStrategy::kRoundRobinWithFallback:
      result = current_pos_;
      if (result >= available.address.size()) {
        result = 0;
        current_pos_ = 0;
      }
      ++current_pos_;
      if (current_pos_ >= available.address.size()) {
        current_pos_ = 0;
      }
      break;
    default:
      assert(0);
      // impossible we verify this in init()
      ;
  }

  return result;
}

int DestMetadataCacheGroup::get_server_socket(
    std::chrono::milliseconds connect_timeout, int *error,
    mysql_harness::TCPAddress *address) noexcept {
  while (true) {
    try {
      auto available = get_available(
          cache_api_->lookup_replicaset(ha_replicaset_).instance_vector);
      if (available.address.empty()) {
        log_warning(
            "No available servers found for '%s' %s routing",
            ha_replicaset_.c_str(),
            server_role_ == ServerRole::Primary ? "primary" : "secondary");
        return -1;
      }

      size_t next_up = get_next_server(available);
      int fd = get_mysql_socket(available.address.at(next_up), connect_timeout);
      if (fd < 0) {
        // Signal that we can't connect to the instance
        cache_api_->mark_instance_reachability(
            available.id.at(next_up),
            metadata_cache::InstanceStatus::Unreachable);
        // if we're looking for a primary member, wait for there to be at least
        // one
        if (server_role_ == ServerRole::Primary &&
            cache_api_->wait_primary_failover(ha_replicaset_,
                                              kPrimaryFailoverTimeout)) {
          log_info("Retrying connection for '%s' after possible failover",
                   ha_replicaset_.c_str());
          continue;  // retry
        }
      }
      if (address) *address = available.address.at(next_up);
      return fd;
    } catch (std::runtime_error &re) {
      log_error("Failed getting managed servers from the Metadata server: %s",
                re.what());
      break;
    }
  }

  *error = errno;
  return -1;
}

void DestMetadataCacheGroup::on_instances_change(
    const metadata_cache::LookupResult &instances,
    const bool md_servers_reachable) {
  // we got notified that the metadata has changed.
  // If instances is empty then (most like is empty)
  // the metadata-cache cannot connect to the metadata-servers
  // In that case we only trigger the callbacks (resulting in disconnects) if
  // the user configured that it should happen
  // (disconnect_on_metadata_unavailable_ == true)
  if (!md_servers_reachable && !disconnect_on_metadata_unavailable_) return;

  const std::string reason =
      md_servers_reachable ? "metadata change" : "metadata unavailable";

  const auto &available_nodes =
      get_available(instances, /*for_new_connections=*/false);
  std::lock_guard<std::mutex> lock(allowed_nodes_change_callbacks_mtx_);
  // notify all the registered listeneres about the list of available nodes
  // change
  for (auto &clb : allowed_nodes_change_callbacks_) {
    clb(available_nodes.address, reason);
  }
}

void DestMetadataCacheGroup::notify(
    const metadata_cache::LookupResult &instances,
    const bool md_servers_reachable) noexcept {
  on_instances_change(instances, md_servers_reachable);
}

void DestMetadataCacheGroup::start(const mysql_harness::PluginFuncEnv *env) {
  // before using metadata-cache we need to make sure it is initialized
  unsigned count = 0;
  while (!cache_api_->is_initialized() && (!env || is_running(env))) {
    if (count++ > 100) {
      throw std::runtime_error(
          "Timed out waiting for metadata-cache to initialize.");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (!env || is_running(env)) {
    subscribe_for_metadata_cache_changes();
  }
}
