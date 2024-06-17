/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <initializer_list>
#include <optional>

#include "helper/container/generic.h"
#include "helper/plugin_monitor.h"
#include "helper/wait_variable.h"
#include "mysql/harness/logging/logging.h"

#include "mysql_rest_service_plugin_config.h"

#include "mysqlrouter/io_component.h"

IMPORT_LOG_FUNCTIONS()

namespace {

class DestinationStatic : public collector::DestinationProvider {
 public:
  using SslOptions = MySQLRoutingAPI::SslOptions;

 public:
  DestinationStatic(const std::vector<Node> &nodes, const SslConfiguration &ssl)
      : nodes_{nodes}, ssl_{ssl} {}

  std::optional<Node> get_node(const WaitingOp) override {
    // Waiting operation is ignored, the list is static.
    if (nodes_.empty()) return {};

    auto idx = nodes_idx_++ % nodes_.size();
    return nodes_[idx];
  }

  bool is_node_supported(const Node &node) override {
    for (auto &n : nodes_) {
      if (node.address() == n.address()) {
        if (node.port() == n.port()) return true;
      }
    }

    return false;
  }

  const SslConfiguration &get_ssl_configuration() override { return ssl_; }

 protected:
  std::vector<Node> nodes_;
  uint32_t nodes_idx_{0};
  SslConfiguration ssl_;
};

class DestinationDynamic : public DestinationStatic {
 private:
  enum State { kOk, kNoValidNodes, kStopped };

  MySQLRoutingAPI get_notifier(DestinationNodesStateNotifier **out_notifier) {
    assert(out_notifier);
    auto &routing_component = MySQLRoutingComponent::get_instance();
    auto routing = routing_component.api(routing_plugin_name_);

    if (routing) {
      *out_notifier = routing.get_destinations_state_notifier();
    }

    return routing;
  }

  static auto get_address(const AvailableDestination &dest) {
    return dest.address;
  }
  static auto get_address(const mysql_harness::TCPAddress &dest) {
    return dest;
  }

  static const std::set<State> &get_expected_state(
      bool apply_only_when_its_first_request = false) {
    static std::set<State> for_first_request{kNoValidNodes};
    static std::set<State> for_other_requests{kOk, kNoValidNodes};

    if (apply_only_when_its_first_request) return for_first_request;
    return for_other_requests;
  }

  template <typename Nodes>
  void callback_allowed_nodes_change(
      const Nodes &nodes_for_existing_connections [[maybe_unused]],
      const Nodes &nodes_for_new_connections,
      const bool disconnected [[maybe_unused]],
      const std::string &res [[maybe_unused]],
      bool apply_only_when_its_first_request = false) {
    auto is_valid = !nodes_for_new_connections.empty();

    log_debug("Received destination addresses update: %i",
              static_cast<int>(nodes_for_new_connections.size()));

    if (is_valid) {
      state_.exchange(get_expected_state(apply_only_when_its_first_request),
                      kOk, [this, &nodes_for_new_connections]() {
                        nodes_.resize(nodes_for_new_connections.size());
                        int idx{0};
                        for (auto &node : nodes_for_new_connections) {
                          nodes_[idx++] = get_address(node);
                        }
                      });
      return;
    }

    state_.exchange(get_expected_state(apply_only_when_its_first_request),
                    kNoValidNodes, [this]() { nodes_.clear(); });
  }

 public:
  DestinationDynamic(const std::string &routing_plugin_name,
                     const SslConfiguration &ssl)
      : DestinationStatic{{}, ssl}, routing_plugin_name_{routing_plugin_name} {
    DestinationNodesStateNotifier *notifier{nullptr};
    auto routing = get_notifier(&notifier);
    if (notifier) {
      it_ = notifier->register_allowed_nodes_change_callback(
          [this](auto &for_existing_con, auto &for_new_con, auto disc,
                 auto &reason) {
            callback_allowed_nodes_change(for_existing_con, for_new_con, disc,
                                          reason);
          });
      auto dest = routing.get_destinations();
      if (!dest.empty()) {
        std::string reason;
        const bool k_first_init = true;
        callback_allowed_nodes_change({}, dest, false, reason, k_first_init);
      }
    }
  }

  ~DestinationDynamic() override { stop(); }

  class CopyNodes {
   public:
    CopyNodes(DestinationDynamic *parent) : parent{parent} {}
    void operator()() const { nodes = parent->nodes_; }

    DestinationDynamic *parent;
    mutable std::vector<Node> nodes;
  };

  std::optional<Node> get_node(const WaitingOp op) override {
    CopyNodes copy_nodes(this);

    switch (op) {
      case WaitingOp::kNoWait:
        state_.is(kOk, copy_nodes);
        break;
      case WaitingOp::kWaitUntilAvaiable:
        state_.wait({kOk, kStopped}, copy_nodes);
        break;
      case WaitingOp::kWaitUntilTimeout:
        state_.wait_for(std::chrono::seconds(1), {kOk, kStopped}, copy_nodes);
        break;
    }

    if (!copy_nodes.nodes.empty())
      return copy_nodes.nodes[nodes_idx_++ % copy_nodes.nodes.size()];
    return {};
  }

  bool is_node_supported(const Node &node) override {
    bool result = false;
    // When its not kOk, the lets go with default value of result.
    // Use synchronization mechanism implemented in `state_`.
    state_.is(kOk, [this, node, &result]() {
      result = DestinationStatic::is_node_supported(node);
    });
    return result;
  }

  void stop() {
    if (!state_.is(kStopped)) {
      DestinationNodesStateNotifier *notifier{nullptr};
      auto routing = get_notifier(&notifier);
      if (notifier) {
        notifier->unregister_allowed_nodes_change_callback(it_.value());
      }
      state_.set(kStopped, [this]() { nodes_.clear(); });
    }
  }

 private:
  std::string routing_plugin_name_;
  // Keep state, allow application to synchronize using the variable.
  WaitableVariable<State> state_{kNoValidNodes};
  std::optional<AllowedNodesChangeCallbacksListIterator> it_;
};

mrs::SslConfiguration cast(const MySQLRoutingAPI::SslOptions &ssl) {
  mrs::SslConfiguration result;

  result.ssl_mode_ = ssl.ssl_mode;
  result.ssl_ca_file_ = ssl.ca;
  result.ssl_ca_path_ = ssl.capath;
  result.ssl_crl_file_ = ssl.crl;
  result.ssl_crl_path_ = ssl.crlpath;
  result.ssl_curves_ = ssl.curves;
  result.ssl_ciphers_ = ssl.ssl_cipher;

  return result;
}

std::shared_ptr<collector::DestinationProvider> create_destination(
    const std::string &routing_name,
    std::set<std::string> &out_wait_for_dynamic_destination_providers) {
  using namespace std::string_literals;

  if (routing_name.empty()) return {};

  auto &routing_component = MySQLRoutingComponent::get_instance();
  auto routing = routing_component.api(routing_name);
  auto destiantions_state = routing.get_destinations_state_notifier();
  auto ssl = cast(routing.get_destination_ssl_options());

  if (destiantions_state->is_dynamic()) {
    auto name = destiantions_state->get_dynamic_plugin_name();
    log_debug("Waiting for destination-provider:%s", name.c_str());
    out_wait_for_dynamic_destination_providers.insert(
        name.empty() ? "metadata_cache" : "metadata_cache:"s + name);

    return std::make_shared<DestinationDynamic>(routing_name, ssl);
    //    destiantions_state->register_allowed_nodes_change_callback(clb);
    // return {new DestinationStatic(desitnations, ssl)};
  } else {
    auto desitnations = routing.get_destinations();
    return std::make_shared<DestinationStatic>(desitnations, ssl);
  }
}

}  // namespace

namespace mrs {

class UserConfigurationInfo {
 public:
  void operator()(const char *variable) {
    log_error(
        "MySQL Server account: '%s', set in configuration file "
        "must have configured password in `MySQLRouters` keyring.",
        variable);
    log_info(
        "Please consult the MRS documentation on: how to configure MySQL "
        "Server accounts for MRS");
  }
};

PluginConfig::PluginConfig(const ConfigSection *section,
                           const std::vector<std::string> &routing_sections,
                           const std::string &router_name)
    : mysql_harness::BasePluginConfig(section) {
  static const char *kKeyringAttributePassword = "password";
  mysql_user_ = get_option(section, "mysql_user", StringOption{});
  mysql_user_data_access_ =
      get_option(section, "mysql_user_data_access", StringOption{});
  routing_rw_ = get_option(section, "mysql_read_write_route", StringOption{});
  routing_ro_ = get_option(section, "mysql_read_only_route", StringOption{});
  router_id_ =
      get_option_no_default(section, "router_id", IntOption<uint64_t>{});
  metadata_refresh_interval_ =
      get_option(section, k_option_metadata_refresh, SecondsOption{});
  router_name_ = router_name;

  account_autentication_rate_rps_ = get_option_no_default(
      section, "authentication_account_maximum_rate", IntOption<uint64_t>{});
  host_autentication_rate_rps_ = get_option_no_default(
      section, "authentication_host_maximum_rate", IntOption<uint64_t>{});
  authentication_rate_exceeded_block_for_ = get_option(
      section, "authentication_rate_exceeded_block_for", IntOption<uint64_t>{});

  if (mysql_user_data_access_.empty()) {
    mysql_user_data_access_ = mysql_user_;
  }

  if (metadata_refresh_interval_.count() == 0)
    throw std::logic_error(
        "`metadata_refresh_interval` option, must be greater than zero.");

  mysql_user_password_ = get_keyring_value<UserConfigurationInfo>(
      mysql_user_, kKeyringAttributePassword);
  mysql_user_data_access_password_ =
      get_keyring_value(mysql_user_data_access_, kKeyringAttributePassword);
  jwt_secret_ = get_keyring_value("rest-user", "jwt_secret");

  if (!helper::container::has(routing_sections, routing_rw_))
    throw std::logic_error(
        "Route name '" + routing_rw_ +
        "' specified for `mysql_read_write_route` option, doesn't exist.");
  if (!routing_ro_.empty()) {
    if (!helper::container::has(routing_sections, routing_ro_))
      throw std::logic_error(
          "Route name '" + routing_ro_ +
          "' specified for `mysql_read_only_route` option, doesn't exist.");
  }

  wait_for_metadata_schema_access_ =
      get_option(section, "wait_for_metadata_schema_access", SecondsOption{});
}

std::set<std::string> PluginConfig::get_waiting_for_routing_plugins() {
  std::set<std::string> result;

  result.insert(routing_rw_);
  if (!routing_ro_.empty()) result.insert(routing_ro_);

  return result;
}

bool PluginConfig::init_runtime_configuration() {
  std::set<std::string> waiting_for_metadatacache_plugin;
  provider_rw_ =
      create_destination(routing_rw_, waiting_for_metadatacache_plugin);
  provider_ro_ =
      create_destination(routing_ro_, waiting_for_metadatacache_plugin);

  log_debug("routing_rw_=%s", routing_rw_.c_str());
  log_debug("routing_ro_=%s", routing_ro_.c_str());
  log_debug("provider_rw_=%p", provider_rw_.get());
  log_debug("provider_ro_=%p", provider_ro_.get());

  if (!service_monitor_->wait_for_services(waiting_for_metadatacache_plugin))
    return false;

  if (!provider_ro_) provider_ro_ = provider_rw_;

  //    // This is going to happen for metadata-cache, lets connect to
  //    router. if (desitnations.empty()) {
  //      nodes_.emplace_back(r.get_bind_address(), r.get_bind_port());
  //    }

  is_https_ = HttpServerComponent::get_instance().is_ssl_configured();
  auto num_of_io_threads = IoComponent::get_instance().io_threads().size();
  default_mysql_cache_instances_ = num_of_io_threads + 3;
  return true;
}

bool PluginConfig::is_required(std::string_view option) const {
  if (option == "mysql_user") return true;
  if (option == "mysql_read_write_route") return true;
  if (option == "authentication") return true;

  return false;
}

std::string PluginConfig::get_default(std::string_view option) const {
  if (option == k_option_metadata_refresh) return "5";
  if (option == "authentication_rate_exceeded_block_for") return "60";
  if (option == "wait_for_metadata_schema_access") return "0";

  return {};
}

}  // namespace mrs
