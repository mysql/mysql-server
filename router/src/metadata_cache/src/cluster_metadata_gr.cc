/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

// enable using Rapidjson library with std::string
#define RAPIDJSON_HAS_STDSTRING 1

#include "cluster_metadata_gr.h"

#include <algorithm>
#include <optional>

#include <rapidjson/writer.h>

#include "dim.h"
#include "group_replication_metadata.h"
#include "mysql/harness/event_state_tracker.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"  // strtoui_checked

using mysql_harness::EventStateTracker;
using mysql_harness::logging::LogLevel;
using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using mysqlrouter::strtoui_checked;
using mysqlrouter::strtoull_checked;
IMPORT_LOG_FUNCTIONS()

using ConnectCallback =
    std::function<bool(mysqlrouter::MySQLSession &connection,
                       const metadata_cache::ManagedInstance &mi)>;

std::vector<metadata_cache::metadata_servers_list_t> get_all_metadata_servers(
    const metadata_cache::metadata_servers_list_t &metadata_servers) {
  // by default use them all, that's what works for non-clusterset backends
  std::vector<metadata_cache::metadata_servers_list_t> result;
  for (const auto &server : metadata_servers) {
    result.push_back({server});
  }

  return result;
}

/* Abstract class for the GR metadata connection to hide the diffences between
 * the metadata versions. */
class GRMetadataBackend {
 public:
  GRMetadataBackend(GRClusterMetadata *metadata, ConnectCallback &connect_clb)
      : metadata_(metadata), connect_clb_(connect_clb) {}

  virtual ~GRMetadataBackend();

  /** @brief Queries the metadata server for the list of instances that belong
   * to the desired cluster.
   */
  virtual metadata_cache::ClusterTopology fetch_instances_from_metadata_server(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &group_name, const std::string &clusterset_id = "") = 0;

  virtual mysqlrouter::ClusterType get_cluster_type() = 0;

  virtual stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      MySQLSession::Transaction &transaction,
      const mysqlrouter::MetadataSchemaVersion &schema_version,
      mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
      const metadata_cache::metadata_server_t &metadata_server,
      bool needs_writable_node, const std::string &group_name,
      const std::string &clusterset_id, bool whole_topology);

  virtual void fetch_periodic_stats_update_frequency(
      const mysqlrouter::MetadataSchemaVersion & /*schema_version*/,
      const unsigned /*router_id*/) {
    periodic_stats_update_frequency_ = std::nullopt;
  }

  virtual std::vector<metadata_cache::metadata_servers_list_t>
  get_metadata_servers(
      const metadata_cache::metadata_servers_list_t &metadata_servers) {
    // by default use them all, that's what works for non-clusterset backends
    return get_all_metadata_servers(metadata_servers);
  }

  virtual std::optional<std::chrono::seconds>
  get_periodic_stats_update_frequency() noexcept {
    return periodic_stats_update_frequency_;
  }

  virtual void reset() {}

 protected:
  GRClusterMetadata *metadata_;
  ConnectCallback connect_clb_;
  std::optional<std::chrono::seconds> periodic_stats_update_frequency_{};
};

GRMetadataBackend::~GRMetadataBackend() = default;

/* Connection to the GR metadata version 1.x */
class GRMetadataBackendV1 : public GRMetadataBackend {
 public:
  GRMetadataBackendV1(GRClusterMetadata *metadata, ConnectCallback &connect_clb)
      : GRMetadataBackend(metadata, connect_clb) {}

  /** @brief Queries the metadata server for the list of instances that belong
   * to the desired cluster.
   */
  metadata_cache::ClusterTopology fetch_instances_from_metadata_server(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &group_name,
      const std::string &clusterset_id = "") override;

  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::GR_V1;
  }
};

/* Connection to the GR metadata version 2.x */
class GRMetadataBackendV2 : public GRMetadataBackend {
 public:
  GRMetadataBackendV2(GRClusterMetadata *metadata, ConnectCallback &connect_clb)
      : GRMetadataBackend(metadata, connect_clb) {}

  /** @brief Queries the metadata server for the list of instances that belong
   * to the desired cluster.
   */
  metadata_cache::ClusterTopology fetch_instances_from_metadata_server(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &group_name,
      const std::string &clusterset_id = "") override;

  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::GR_V2;
  }

  virtual void fetch_periodic_stats_update_frequency(
      const mysqlrouter::MetadataSchemaVersion &schema_version,
      const unsigned router_id) override;

 protected:
  virtual std::string get_cluster_type_specific_id_limit_sql(
      const std::string &group_name,
      const std::string & /*clusterset_id*/ = "");
};

/* Connection to the GR metadata clusterset */
class GRClusterSetMetadataBackend : public GRMetadataBackendV2 {
 public:
  GRClusterSetMetadataBackend(GRClusterMetadata *metadata,
                              ConnectCallback &connect_clb)
      : GRMetadataBackendV2(metadata, connect_clb) {}

  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::GR_CS;
  }

  /** @brief Returns cluster defined in the metadata given set of the
   *         metadata servers (cluster members)
   *
   * @param transaction transaction to be used for SQL queries required by this
   * function
   * @param schema_version current metadata schema version
   * @param [in,out] target_cluster object identifying the Cluster this
   * operation refers to
   * @param router_id id of the router in the cluster metadata
   * @param metadata_server info about the metadata server we are querying
   * @param needs_writable_node flag indicating if the caller needs us to query
   * for writable node
   * @param group_name Cluster Replication Group name (if bootstrapped as a
   * single Cluster)
   * @param clusterset_id UUID of the ClusterSet the Cluster belongs to (if
   * bootstrapped as a ClusterSet)
   * @param whole_topology return all usable nodes, ignore potential metadata
   * filters or policies (like target_cluster etc.)
   * @return object containing cluster topology information in case of success,
   * or error code in case of failure
   * @throws metadata_cache::metadata_error
   */
  stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      MySQLSession::Transaction &transaction,
      const mysqlrouter::MetadataSchemaVersion &schema_version,
      mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
      const metadata_cache::metadata_server_t &metadata_server,
      bool needs_writable_node, const std::string &group_name,
      const std::string &clusterset_id = "",
      bool whole_topology = false) override;

  void reset() override { metadata_read_ = false; }

  std::vector<metadata_cache::metadata_servers_list_t> get_metadata_servers(
      const metadata_cache::metadata_servers_list_t &metadata_servers)
      override {
    std::vector<metadata_cache::metadata_servers_list_t> result;

    if (cluster_topology_) {
      // We already know the latest ClusterSet topology so we return the
      // servers grouped by the Cluster they belong to
      for (const auto &cluster : (*cluster_topology_).clusters_data) {
        metadata_cache::metadata_servers_list_t nodes;
        for (const auto &node : cluster.members) {
          nodes.push_back({node.host, node.port});
        }
        if (!nodes.empty()) {
          result.push_back(nodes);
        }
      }
    }

    // if did not read the metadata yet we have the list from the state file
    // we don't know which server belongs to which Cluster at this point so we
    // have to assume the safest scenario: each is from different Cluster, we
    // have to check metadata on each of them
    if (result.empty()) {
      for (const auto &server : metadata_servers) {
        result.push_back({server});
      }
    }

    return result;
  }

  virtual std::optional<std::chrono::seconds>
  get_periodic_stats_update_frequency() noexcept override {
    using namespace std::chrono_literals;
    return periodic_stats_update_frequency_ ? periodic_stats_update_frequency_
                                            : 0s;
  }

 private:
  // Depending whether the bootstrap happened when the Cluster was already a
  // part of a ClusterSet or not we will have either GR name or clusterset_id
  // from the state file
  std::string get_cluster_type_specific_id_limit_sql(
      const std::string &group_name,
      const std::string &clusterset_id = "") override;

  uint64_t view_id_{0};
  bool metadata_read_{false};

  /** @brief Returns vector of the cluster members according to the metadata of
   * the selected server.
   *
   * @param session active connection to the member that is checked for the
   * metadata
   * @param cluster_id ID of the cluster this operation refers to
   * @return vector of the cluster members
   */
  metadata_cache::cluster_nodes_list_t
  fetch_target_cluster_instances_from_metadata_server(
      mysqlrouter::MySQLSession &session, const std::string &cluster_id);

  // returns cluster_id
  std::string get_target_cluster_info_from_metadata_server(
      mysqlrouter::MySQLSession &session,
      mysqlrouter::TargetCluster &target_cluster,
      const std::string &clusterset_id);

  /** @brief Queries the metada for the current ClusterSet topology. Stores the
   * topology in the class state. Returns the set of the metadata server for the
   * ClusterSet.
   *
   * @param session active connection to the member that is checked for the
   * metadata
   * @param clusterset_id ID of the ClusterSet this operation refers to
   * @param view_id view id of the metadata
   * @return set of the servers that contains metadata for the ClusterSet
   */
  metadata_cache::ClusterTopology
  update_clusterset_topology_from_metadata_server(
      mysqlrouter::MySQLSession &session, const std::string &clusterset_id,
      uint64_t view_id);

  /** @brief Finds the writable node within the currently known ClusterSet
   * topology.
   *
   * @return address of the current primary node if found
   * @return std::nullopt if did not find any writable node
   * primary
   */
  std::optional<metadata_cache::metadata_server_t> find_rw_server();

  std::string router_cs_options_string{""};
  std::optional<metadata_cache::ClusterTopology> cluster_topology_{};
};

namespace {

// represents the Router options in v2_cs_router_options view in the metadata
// schema
class RouterClusterSetOptions {
 public:
  bool read_from_metadata(mysqlrouter::MySQLSession &session,
                          const unsigned router_id) {
    const std::string query =
        "SELECT router_options FROM "
        "mysql_innodb_cluster_metadata.v2_cs_router_options where router_id "
        "= " +
        std::to_string(router_id);

    std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
    if (!row) {
      log_error(
          "Error reading router.options from v2_cs_router_options: did not "
          "find router entry for router_id '%u'",
          router_id);
      return false;
    }

    options_str_ = ::get_string((*row)[0]);

    return true;
  }

  std::string get_string() const { return options_str_; }

  std::optional<mysqlrouter::TargetCluster> get_target_cluster(
      const unsigned router_id) const {
    std::string out_error;
    // check if we have a target cluster assigned in the metadata
    std::string target_cluster_str =
        get_router_option_str(options_str_, "target_cluster", "", out_error);
    mysqlrouter::TargetCluster target_cluster;

    if (!out_error.empty()) {
      log_error("Error reading target_cluster from the router.options: %s",
                out_error.c_str());
      return {};
    }

    const std::string invalidated_cluster_routing_policy_str =
        get_router_option_str(options_str_, "invalidated_cluster_policy", "",
                              out_error);

    if (invalidated_cluster_routing_policy_str == "accept_ro") {
      target_cluster.invalidated_cluster_routing_policy(
          mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::
              AcceptRO);
    } else {
      // this is the default strategy
      target_cluster.invalidated_cluster_routing_policy(
          mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::DropAll);
    }

    const bool target_cluster_in_options = !target_cluster_str.empty();
    const bool target_cluster_in_options_changed =
        EventStateTracker::instance().state_changed(
            target_cluster_in_options,
            EventStateTracker::EventId::TargetClusterPresentInOptions);

    if (!target_cluster_in_options) {
      const auto log_level = target_cluster_in_options_changed
                                 ? LogLevel::kWarning
                                 : LogLevel::kDebug;
      log_custom(log_level,
                 "Target cluster for router_id=%d not set, using 'primary' as "
                 "a target cluster",
                 router_id);
      target_cluster_str = "primary";
    }

    if (target_cluster_str == "primary") {
      target_cluster.target_type(
          mysqlrouter::TargetCluster::TargetType::ByPrimaryRole);
      target_cluster.target_value("");
    } else {
      target_cluster.target_type(
          mysqlrouter::TargetCluster::TargetType::ByUUID);
      target_cluster.target_value(target_cluster_str);
    }

    return target_cluster;
  }

  std::chrono::seconds get_stats_updates_frequency() const {
    using namespace std::chrono_literals;
    std::string out_error;
    auto stats_updates_frequency = std::chrono::seconds(get_router_option_uint(
        options_str_, "stats_updates_frequency", 0, out_error));
    if (!out_error.empty()) {
      log_warning(
          "Error parsing stats_updates_frequency from the router.options: %s. "
          "Using default value %u",
          out_error.c_str(), 0);
      return 0s;
    }

    return stats_updates_frequency;
  }

  bool get_use_replica_primary_as_rw() const {
    std::string out_error;
    auto result = get_router_option_bool(
        options_str_, "use_replica_primary_as_rw", false, out_error);
    if (!out_error.empty()) {
      log_warning(
          "Error parsing use_replica_primary_as_rw from the router.options: "
          "%s. Using default value 'false'",
          out_error.c_str());
      return false;
    }

    return result;
  }

 private:
  std::string get_router_option_str(const std::string &options,
                                    const std::string &name,
                                    const std::string &default_value,
                                    std::string &out_error) const {
    out_error = "";
    if (options.empty()) return default_value;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (json_doc.HasParseError() || !json_doc.IsObject()) {
      out_error = "not a valid JSON object";
      return default_value;
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsString()) {
      out_error = "options." + name + " not a string";
      return default_value;
    }

    return it->value.GetString();
  }

  uint32_t get_router_option_uint(const std::string &options,
                                  const std::string &name,
                                  const uint32_t &default_value,
                                  std::string &out_error) const {
    out_error = "";
    if (options.empty()) return default_value;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (json_doc.HasParseError() || !json_doc.IsObject()) {
      out_error = "not a valid JSON object";
      return default_value;
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsUint()) {
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      it->value.Accept(writer);
      out_error =
          "options." + name + "='" + sb.GetString() + "'; not an unsigned int";
      return default_value;
    }

    return it->value.GetUint();
  }

  uint32_t get_router_option_bool(const std::string &options,
                                  const std::string &name,
                                  const bool &default_value,
                                  std::string &out_error) const {
    out_error = "";
    if (options.empty()) return default_value;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (json_doc.HasParseError() || !json_doc.IsObject()) {
      out_error = "not a valid JSON object";
      return default_value;
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsBool()) {
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      it->value.Accept(writer);
      out_error =
          "options." + name + "='" + sb.GetString() + "'; not a boolean";
      return default_value;
    }

    return it->value.GetBool();
  }

  std::string options_str_;
};

}  // namespace

GRClusterMetadata::GRClusterMetadata(
    const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
    const mysqlrouter::SSLOptions &ssl_options,
    const bool use_cluster_notifications)
    : ClusterMetadata(session_config, ssl_options) {
  if (use_cluster_notifications) {
    gr_notifications_listener_.reset(
        new GRNotificationListener(session_config.user_credentials));
  }
}

void GRClusterMetadata::update_cluster_status(
    metadata_cache::ManagedCluster
        &cluster) {  // throws metadata_cache::metadata_error

  log_debug("Updating cluster status from GR for '%s'", cluster.name.c_str());

  // iterate over all candidate nodes until we find the node that is part of
  // quorum
  bool found_quorum = false;
  std::shared_ptr<MySQLSession> gr_member_connection;
  for (const metadata_cache::ManagedInstance &mi : cluster.members) {
    const std::string mi_addr = mi.host + ":" + std::to_string(mi.port);

    auto connection = get_connection();

    // connect to node
    if (mi_addr == connection->get_address()) {  // optimisation: if node is the
                                                 // same as metadata server,
      gr_member_connection = connection;  //               share the established
                                          //               connection
    } else {
      try {
        gr_member_connection =
            mysql_harness::DIM::instance().new_MySQLSession();
      } catch (const std::logic_error &e) {
        // defensive programming, shouldn't really happen. If it does, there's
        // nothing we can do really, we give up
        log_error(
            "While updating metadata, could not initialise MySQL connection "
            "structure");
        throw metadata_cache::metadata_error(e.what());
      }

      const bool connect_res = do_connect(*gr_member_connection, mi);
      const bool connect_res_changed =
          EventStateTracker::instance().state_changed(
              connect_res, EventStateTracker::EventId::GRMemberConnectedOk,
              mi_addr);
      if (!connect_res) {
        const auto log_level =
            connect_res_changed ? LogLevel::kWarning : LogLevel::kDebug;
        log_custom(
            log_level,
            "While updating metadata, could not establish a connection to "
            "cluster '%s' through %s",
            cluster.name.c_str(), mi_addr.c_str());
        continue;  // server down, next!
      }
    }

    log_debug("Connected to cluster '%s' through %s", cluster.name.c_str(),
              mi_addr.c_str());

    try {
      bool single_primary_mode = true;

      // this node's perspective: give status of all nodes you see
      std::map<std::string, GroupReplicationMember> member_status =
          fetch_group_replication_members(
              *gr_member_connection,
              single_primary_mode);  // throws metadata_cache::metadata_error
      log_debug("Cluster '%s' has %zu members in metadata, %zu in status table",
                cluster.name.c_str(), cluster.members.size(),
                member_status.size());

      // check status of all nodes; updates instances
      // ------------------vvvvvvvvvvvvvvvvvv
      bool metadata_gr_discrepancy{false};
      const auto status = check_cluster_status(cluster.members, member_status,
                                               metadata_gr_discrepancy);
      switch (status) {
        case GRClusterStatus::AvailableWritable:  // we have
                                                  // quorum,
                                                  // good!
          found_quorum = true;
          break;
        case GRClusterStatus::AvailableReadOnly:  // have
                                                  // quorum,
                                                  // but only
                                                  // RO
          found_quorum = true;
          break;
        case GRClusterStatus::UnavailableRecovering:  // have quorum, but only
                                                      // with recovering nodes
                                                      // (cornercase)
          log_warning(
              "quorum for cluster '%s' consists only of recovering nodes!",
              cluster.name.c_str());
          found_quorum = true;  // no point in futher search
          break;
        case GRClusterStatus::Unavailable:  // we have nothing
          log_warning("%s is not part of quorum for cluster '%s'",
                      mi_addr.c_str(), cluster.name.c_str());
          continue;  // this server is no good, next!
      }

      if (found_quorum) {
        cluster.single_primary_mode = single_primary_mode;
        cluster.md_discrepancy = metadata_gr_discrepancy;
        break;  // break out of the member iteration loop
      }

    } catch (const metadata_cache::metadata_error &e) {
      log_warning(
          "Unable to fetch live group_replication member data from %s from "
          "cluster '%s': %s",
          mi_addr.c_str(), cluster.name.c_str(), e.what());
      continue;  // faulty server, next!
    } catch (const std::exception &e) {
      log_warning(
          "Unable to fetch live group_replication member data from %s from "
          "cluster '%s': %s",
          mi_addr.c_str(), cluster.name.c_str(), e.what());
      continue;  // faulty server, next!
    }

  }  // for (const metadata_cache::ManagedInstance& mi : instances)
  log_debug("End updating cluster for '%s'", cluster.name.c_str());

  if (!found_quorum) {
    std::string msg(
        "Unable to fetch live group_replication member data from any server in "
        "cluster '");
    msg += cluster.name + "'";
    log_error("%s", msg.c_str());

    // if we don't have a quorum, we want to give "nothing" to the Routing
    // plugin, so it doesn't route anything. Routing plugin is dumb, it has no
    // idea what a quorum is, etc.
    cluster.members.clear();
  }
}

GRClusterStatus GRClusterMetadata::check_cluster_status(
    std::vector<metadata_cache::ManagedInstance> &instances,
    const std::map<std::string, GroupReplicationMember> &member_status,
    bool &metadata_gr_discrepancy) const noexcept {
  // In ideal world, the best way to write this function would be to completely
  // ignore nodes in `instances` and operate on information from `member_status`
  // only. However, there is one problem: the host:port information contained
  // there may not be accurate (localhost vs external addressing issues), and we
  // are forced to use the host:port from `instances` instead. This leads to
  // nasty corner-cases if inconsistencies exist between the two sets, however.

  // Therefore, this code will work well only under one assumption:
  // All nodes in `member_status` are present in `instances`. This assumption
  // should hold unless a user "manually" adds new nodes to the cluster
  // without adding them to metadata (and the user is not allowed to do that).

  // Detect violation of above assumption (alarm if there's a node in
  // `member_status` not present in `instances`). It's O(n*m), but the CPU time
  // is negligible while keeping code simple.

  using metadata_cache::ServerMode;
  using metadata_cache::ServerRole;
  using GR_State = GroupReplicationMember::State;
  using GR_Role = GroupReplicationMember::Role;

  metadata_gr_discrepancy = false;
  auto number_of_all_members = member_status.size();
  for (const auto &status_node : member_status) {
    using MI = metadata_cache::ManagedInstance;
    auto found = std::find_if(instances.begin(), instances.end(),
                              [&status_node](const MI &metadata_node) {
                                return status_node.first ==
                                       metadata_node.mysql_server_uuid;
                              });
    const bool node_in_metadata = found != instances.end();
    const bool node_in_metadata_changed =
        EventStateTracker::instance().state_changed(
            node_in_metadata, EventStateTracker::EventId::GRNodeInMetadata,
            status_node.first);
    if (!node_in_metadata) {
      if (status_node.second.state == GR_State::Recovering) {
        const auto log_level =
            node_in_metadata_changed ? LogLevel::kInfo : LogLevel::kDebug;
        log_custom(log_level,
                   "GR member %s:%d (%s) Recovering, missing in the metadata, "
                   "ignoring",
                   status_node.second.host.c_str(), status_node.second.port,
                   status_node.first.c_str());
        // if the node is Recovering and it is missing in the metadata it can't
        // increase the pool used for quorum calculations. This is for example
        // important when we have single node cluster and we add another node
        // with cloning. While cloning the new node will be present in the GR
        // tables but missing in the metadata.
        --number_of_all_members;
      } else {
        const auto log_level =
            node_in_metadata_changed ? LogLevel::kWarning : LogLevel::kDebug;
        log_custom(
            log_level, "GR member %s:%d (%s) %s, missing in the metadata",
            status_node.second.host.c_str(), status_node.second.port,
            status_node.first.c_str(), to_string(status_node.second.state));
      }

      // we want to set this in both cases as it increases the metadata refresh
      // rate
      metadata_gr_discrepancy = true;
    }
  }

  // we do two things here:
  // 1. for all `instances`, set .mode according to corresponding .status found
  // in `member_status`
  // 2. count nodes which are part of quorum (online/recovering nodes)
  unsigned int quorum_count = 0;
  bool have_primary_instance = false;
  bool have_secondary_instance = false;
  for (auto &member : instances) {
    auto status = member_status.find(member.mysql_server_uuid);
    const bool node_in_gr = status != member_status.end();
    const bool node_in_gr_changed = EventStateTracker::instance().state_changed(
        node_in_gr, EventStateTracker::EventId::MetadataNodeInGR,
        member.mysql_server_uuid);

    if (node_in_gr) {
      switch (status->second.state) {
        case GR_State::Online:
          switch (status->second.role) {
            case GR_Role::Primary:
              have_primary_instance = true;
              member.role = ServerRole::Primary;
              member.mode = ServerMode::ReadWrite;
              quorum_count++;
              break;
            case GR_Role::Secondary:
              have_secondary_instance = true;
              member.role = ServerRole::Secondary;
              member.mode = ServerMode::ReadOnly;
              quorum_count++;
              break;
          }
          break;
        case GR_State::Recovering:
        case GR_State::Unreachable:
        case GR_State::Offline:  // online node with disabled GR maps to this
        case GR_State::Error:
        case GR_State::Other:
          // This could be done with a fallthrough but latest gcc (7.1)
          // generates a warning for that and there is no sane and portable way
          // to suppress it.
          if (GR_State::Recovering == status->second.state) quorum_count++;
          member.role = ServerRole::Unavailable;
          member.mode = ServerMode::Unavailable;
          break;
      }
    } else {
      member.role = ServerRole::Unavailable;
      member.mode = ServerMode::Unavailable;
      metadata_gr_discrepancy = true;
      const auto log_level =
          node_in_gr_changed ? LogLevel::kWarning : LogLevel::kDebug;
      log_custom(log_level,
                 "Member %s:%d (%s) defined in metadata not found in actual "
                 "Group Replication",
                 member.host.c_str(), member.port,
                 member.mysql_server_uuid.c_str());
    }
  }

  // quorum_count is based on nodes from `instances` instead of `member_status`.
  // This is okay, because all nodes in `member_status` are present in
  // `instances` (our assumption described at the top)
  bool have_quorum = (quorum_count > number_of_all_members / 2);

  // if we don't have quorum, we don't allow any access. Some configurations
  // might allow RO access in this case, but we don't support it at the momemnt
  if (!have_quorum) return GRClusterStatus::Unavailable;

  // if we have quorum but no primary/secondary instances, it means the quorum
  // is composed purely of recovering nodes (this is an unlikely cornercase)
  if (!(have_primary_instance || have_secondary_instance))
    return GRClusterStatus::UnavailableRecovering;

  // if primary node was not elected yet, we can only allow reads (typically
  // this is a temporary state shortly after a node failure, but could also be
  // more permanent)
  return have_primary_instance
             ? GRClusterStatus::AvailableWritable   // typical case
             : GRClusterStatus::AvailableReadOnly;  // primary not elected yet
}

void GRClusterMetadata::reset_metadata_backend(const ClusterType type) {
  ConnectCallback connect_clb = [this](
                                    mysqlrouter::MySQLSession &sess,
                                    const metadata_cache::ManagedInstance &mi) {
    return do_connect(sess, mi);
  };

  switch (type) {
    case ClusterType::GR_V1:
      metadata_backend_ =
          std::make_unique<GRMetadataBackendV1>(this, connect_clb);
      break;
    case ClusterType::GR_V2:
      metadata_backend_ =
          std::make_unique<GRMetadataBackendV2>(this, connect_clb);
      break;
    case ClusterType::GR_CS:
      metadata_backend_ =
          std::make_unique<GRClusterSetMetadataBackend>(this, connect_clb);
      break;
    default:
      throw std::runtime_error(
          "Invalid cluster type '" + mysqlrouter::to_string(type) +
          "'. Configured '" + mysqlrouter::to_string(ClusterType::GR_V1) + "'");
  }
}

mysqlrouter::ClusterType GRClusterMetadata::get_cluster_type() {
  if (!metadata_backend_) return ClusterType::GR_V1;
  return metadata_backend_->get_cluster_type();
}

GRClusterMetadata::auth_credentials_t GRClusterMetadata::fetch_auth_credentials(
    const mysqlrouter::TargetCluster &target_cluster,
    const std::string &cluster_type_specific_id) {
  if (!metadata_backend_) return {};
  switch (metadata_backend_->get_cluster_type()) {
    case mysqlrouter::ClusterType::GR_V1:
      log_warning(
          "metadata_cache authentication backend is not supported for metadata "
          "version 1.0");
      return {};
    default:
      return ClusterMetadata::fetch_auth_credentials(target_cluster,
                                                     cluster_type_specific_id);
  }
}

metadata_cache::ClusterTopology
GRClusterMetadata::fetch_instances_from_metadata_server(
    const mysqlrouter::TargetCluster &target_cluster,
    const std::string &cluster_type_specific_id) {
  return metadata_backend_->fetch_instances_from_metadata_server(
      target_cluster, cluster_type_specific_id);
}

namespace {
// returns true if the backends can be changed in the runtime
bool backends_compatible(const ClusterType a, const ClusterType b) {
  return a != ClusterType::GR_CS && b != ClusterType::GR_CS;
}
}  // namespace

void GRClusterMetadata::update_backend(
    const mysqlrouter::MetadataSchemaVersion &version, unsigned int router_id) {
  const auto cluster_type = mysqlrouter::get_cluster_type(
      version, metadata_connection_.get(), router_id);

  // if the current backend does not fit the metadata version that we just
  // discovered, we need to recreate it
  if (!metadata_backend_ ||
      cluster_type != metadata_backend_->get_cluster_type()) {
    if (metadata_backend_) {
      if (!backends_compatible(cluster_type,
                               metadata_backend_->get_cluster_type())) {
        return;
      }
      log_info(
          "Metadata version change was discovered. New metadata version is "
          "%d.%d.%d",
          version.major, version.minor, version.patch);
    }
    reset_metadata_backend(cluster_type);
  }
}

std::optional<std::chrono::seconds>
GRClusterMetadata::get_periodic_stats_update_frequency() noexcept {
  if (!metadata_backend_) return {};

  return metadata_backend_->get_periodic_stats_update_frequency();
}

stdx::expected<metadata_cache::ClusterTopology, std::error_code>
GRMetadataBackend::fetch_cluster_topology(
    MySQLSession::Transaction &transaction,
    const mysqlrouter::MetadataSchemaVersion &schema_version,
    mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
    const metadata_cache::metadata_server_t & /*metadata_server*/,
    bool needs_writable_node, const std::string &group_name,
    const std::string &clusterset_id = "", bool /*whole_topology*/ = false) {
  metadata_cache::ClusterTopology result;

  // fetch cluster topology from the metadata server (this is
  // the topology that was configured, it will be compared later against
  // current topology reported by (a server in) Group Replication)
  result = fetch_instances_from_metadata_server(
      target_cluster, group_name,
      clusterset_id);  // throws metadata_cache::metadata_error

  fetch_periodic_stats_update_frequency(schema_version, router_id);

  // we are done with querying metadata
  transaction.commit();

  auto &cluster = result.clusters_data[0];

  // now connect to the cluster and query it for the list and status of its
  // members. (more precisely: search and connect to a
  // member which is part of quorum to retrieve this data)
  metadata_->update_cluster_status(
      cluster);  // throws metadata_cache::metadata_error

  if (needs_writable_node) {
    result.writable_server = metadata_->find_rw_server(cluster.members);
  } else {
    result.writable_server = std::nullopt;
  }

  return result;
}

stdx::expected<metadata_cache::ClusterTopology, std::error_code>
GRClusterMetadata::fetch_cluster_topology(
    const std::atomic<bool> &terminated,
    mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
    const metadata_cache::metadata_servers_list_t &metadata_servers,
    bool needs_writable_node, const std::string &group_name,
    const std::string &clusterset_id, bool whole_topology,
    std::size_t &instance_id) {
  log_debug("Updating metadata information for cluster '%s'",
            target_cluster.c_str());
  stdx::expected<metadata_cache::ClusterTopology, std::error_code> result{
      stdx::make_unexpected(make_error_code(
          metadata_cache::metadata_errc::no_metadata_server_reached))},
      result_tmp;
  instance_id = 0;
  bool backend_reset{false};

  const auto servers_by_cluster =
      metadata_backend_
          ? metadata_backend_->get_metadata_servers(metadata_servers)
          : get_all_metadata_servers(metadata_servers);

  size_t last_fetch_cluster_id{std::numeric_limits<size_t>::max()};

  for (size_t i = 0; i < servers_by_cluster.size(); ++i) {
    const auto &cluster_servers = servers_by_cluster[i];
    for (const auto &metadata_server : cluster_servers) {
      // if this is a ClusterSet and we already read metadata from one of the
      // nodes of this Cluster we skip, we only check the metadata on one node
      // of a Cluster
      if (metadata_backend_ &&
          metadata_backend_->get_cluster_type() ==
              mysqlrouter::ClusterType::GR_CS &&
          last_fetch_cluster_id == i)
        continue;

      result_tmp = stdx::make_unexpected(make_error_code(
          metadata_cache::metadata_errc::no_metadata_read_successful));

      if (terminated) {
        return stdx::make_unexpected(make_error_code(
            metadata_cache::metadata_errc::metadata_refresh_terminated));
      }

      try {
        if (!connect_and_setup_session(metadata_server)) {
          continue;
        }

        MySQLSession::Transaction transaction(metadata_connection_.get());

        // throws metadata_cache::metadata_error and
        // MetadataUpgradeInProgressException
        const auto version =
            get_and_check_metadata_schema_version(*metadata_connection_);

        update_backend(version, router_id);

        if (!backend_reset) {
          metadata_backend_->reset();
          backend_reset = true;
        }

        result_tmp = metadata_backend_->fetch_cluster_topology(
            transaction, version, target_cluster, router_id, metadata_server,
            needs_writable_node, group_name, clusterset_id, whole_topology);

        last_fetch_cluster_id = i;
      } catch (const mysqlrouter::MetadataUpgradeInProgressException &) {
        throw;
      } catch (const std::exception &e) {
        log_warning(
            "Failed fetching metadata from metadata server on %s:%d - %s",
            metadata_server.address().c_str(), metadata_server.port(),
            e.what());
      }

      if (result_tmp) {
        const auto metadata_server_it = std::find(
            metadata_servers.begin(), metadata_servers.end(), metadata_server);
        if (metadata_server_it != metadata_servers.end()) {
          instance_id = metadata_server_it - metadata_servers.begin();
        }

        // for a standalone Cluster which is not part of a ClusterSet we are ok
        // with first successful read for a ClusterSet we need to go over all
        // of them to find highiest view_id
        result = std::move(result_tmp);
        if (metadata_backend_->get_cluster_type() !=
            mysqlrouter::ClusterType::GR_CS) {
          return result;
        }
      } else {
        if (!result) {
          result = std::move(result_tmp);
        }
      }
    }
  }

  return result;
}

// throws metadata_cache::metadata_error
metadata_cache::ClusterTopology
GRMetadataBackendV1::fetch_instances_from_metadata_server(
    const mysqlrouter::TargetCluster &target_cluster,
    const std::string &group_name, const std::string & /*clusterset_id*/) {
  auto connection = metadata_->get_connection();

  std::string limit_cluster;
  if (target_cluster.target_type() ==
      mysqlrouter::TargetCluster::TargetType::ByName) {
    limit_cluster = "F.cluster_name = ";
  } else {
    limit_cluster = "F.cluster_id = ";
  }
  limit_cluster += connection->quote(target_cluster.to_string());

  // If we have group replication id we also want to limit the results only
  // for that group replication. For backward compatibility we need to check
  // if it is not empty, we didn't store that information before introducing
  // dynamic state file.
  std::string limit_group_replication;
  if (!group_name.empty()) {
    limit_group_replication =
        " AND R.attributes->>'$.group_replication_group_name' = " +
        connection->quote(group_name);
  }

  // Get expected topology (what was configured) from metadata server. This
  // will later be compared against current topology (what exists NOW)
  // obtained from one of the nodes belonging to a quorum. Note that this
  // topology will also be successfully returned when a particular metadata
  // server is not part of GR, as serving metadata and being part of
  // replicaset are two orthogonal ideas.
  std::string query(
      "SELECT "
      "F.cluster_id, F.cluster_name, "
      "R.replicaset_name, "
      "I.mysql_server_uuid, "
      "I.addresses->>'$.mysqlClassic', "
      "I.addresses->>'$.mysqlX' "
      "FROM "
      "mysql_innodb_cluster_metadata.clusters AS F "
      "JOIN mysql_innodb_cluster_metadata.replicasets AS R "
      "ON F.cluster_id = R.cluster_id "
      "JOIN mysql_innodb_cluster_metadata.instances AS I "
      "ON R.replicaset_id = I.replicaset_id "
      "WHERE " +
      limit_cluster + limit_group_replication);

  metadata_cache::ManagedCluster cluster;
  auto result_processor = [&cluster](const MySQLSession::Row &row) -> bool {
    if (row.size() != 6) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 6, got = " +
          std::to_string(row.size()));
    }

    metadata_cache::ManagedInstance s{
        metadata_cache::InstanceType::GroupMember};
    s.mysql_server_uuid = get_string(row[3]);
    if (!set_instance_ports(s, row, 4, 5)) {
      return true;  // next row
    }

    cluster.members.push_back(s);
    cluster.single_primary_mode =
        true;  // actual value set elsewhere from GR metadata
    cluster.id = get_string(row[0]);
    cluster.name = get_string(row[1]);

    return true;  // false = I don't want more rows
  };

  assert(connection->is_connected());

  try {
    connection->query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  metadata_cache::ClusterTopology result;
  result.clusters_data.push_back(cluster);
  result.target_cluster_pos = 0;

  // for Cluster that is not part of the ClusterSet we assume metadata
  // servers are just Cluster nodes
  for (const auto &cluster_node : cluster.members) {
    result.metadata_servers.emplace_back(cluster_node.host, cluster_node.port);
  }

  return result;
}

std::string GRMetadataBackendV2::get_cluster_type_specific_id_limit_sql(
    const std::string &group_name, const std::string & /*clusterset_id*/) {
  auto connection = metadata_->get_connection();

  std::string result;
  if (!group_name.empty()) {
    result = " AND C.group_name = " + connection->quote(group_name);
  }

  return result;
}

// throws metadata_cache::metadata_error
metadata_cache::ClusterTopology
GRMetadataBackendV2::fetch_instances_from_metadata_server(
    const mysqlrouter::TargetCluster &target_cluster,
    const std::string &group_name, const std::string &clusterset_id) {
  auto connection = metadata_->get_connection();

  std::string limit_cluster;
  if (target_cluster.target_type() ==
      mysqlrouter::TargetCluster::TargetType::ByName) {
    limit_cluster = "C.cluster_name = ";
  } else {
    limit_cluster = "C.cluster_id = ";
  }
  limit_cluster += connection->quote(target_cluster.to_string());

  std::string limit_group_replication =
      get_cluster_type_specific_id_limit_sql(group_name, clusterset_id);

  // Get expected topology (what was configured) from metadata server. This
  // will later be compared against current topology (what exists NOW)
  // obtained from one of the nodes belonging to a quorum. Note that this
  // topology will also be successfully returned when a particular metadata
  // server is not part of GR, as serving metadata and being part of
  // replicaset are two orthogonal ideas.
  std::string query(
      "select C.cluster_id, C.cluster_name, I.mysql_server_uuid, I.endpoint, "
      "I.xendpoint, I.attributes "
      "from "
      "mysql_innodb_cluster_metadata.v2_instances I join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
      "C.cluster_id where " +
      limit_cluster + limit_group_replication);

  metadata_cache::ManagedCluster cluster;
  auto result_processor = [&cluster](const MySQLSession::Row &row) -> bool {
    if (row.size() != 6) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 6, got = " +
          std::to_string(row.size()));
    }

    metadata_cache::ManagedInstance instance{
        metadata_cache::InstanceType::GroupMember};
    instance.mysql_server_uuid = get_string(row[2]);
    if (!set_instance_ports(instance, row, 3, 4)) {
      return true;  // next row
    }
    set_instance_attributes(instance, get_string(row[5]));

    cluster.id = get_string(row[0]);
    cluster.name = get_string(row[1]);
    cluster.members.push_back(instance);
    cluster.single_primary_mode =
        true;  // actual value set elsewhere from GR metadata

    return true;  // false = I don't want more rows
  };

  try {
    connection->query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  metadata_cache::ClusterTopology result;
  result.clusters_data.push_back(cluster);
  result.target_cluster_pos = 0;

  // for Cluster that is not part of the ClusterSet we assume metadata
  // servers are just Cluster nodes
  for (const auto &cluster_node : cluster.members) {
    result.metadata_servers.emplace_back(cluster_node.host, cluster_node.port);
  }

  return result;
}

void GRMetadataBackendV2::fetch_periodic_stats_update_frequency(
    const mysqlrouter::MetadataSchemaVersion &schema_version,
    const unsigned router_id) {
  if (schema_version >= mysqlrouter::kClusterSetsMetadataVersion) {
    const auto connection = metadata_->get_connection();
    const bool is_part_of_cluster_set =
        mysqlrouter::is_part_of_cluster_set(connection.get());
    if (is_part_of_cluster_set) {
      RouterClusterSetOptions router_clusterset_options;
      if (router_clusterset_options.read_from_metadata(*connection.get(),
                                                       router_id)) {
        periodic_stats_update_frequency_ =
            router_clusterset_options.get_stats_updates_frequency();
        return;
      }
    }
  }

  periodic_stats_update_frequency_ = std::nullopt;
}

GRClusterMetadata::~GRClusterMetadata() = default;

//////////////////////////////////////
// class GRClusterSetMetadataBackend
//////////////////////////////////////

std::string GRClusterSetMetadataBackend::get_cluster_type_specific_id_limit_sql(
    const std::string &group_name, const std::string &clusterset_id) {
  auto connection = metadata_->get_connection();

  std::string result;
  if (!clusterset_id.empty()) {
    result =
        " AND C.cluster_id IN (select CSM.cluster_id from "
        "mysql_innodb_cluster_metadata.v2_cs_members CSM "
        "where CSM.clusterset_id=" +
        connection->quote(clusterset_id) + ")";
  }

  if (!group_name.empty()) {
    result = " AND C.group_name = " + connection->quote(group_name);
  }

  return result;
}

static stdx::expected<uint64_t, std::error_code> get_member_view_id(
    mysqlrouter::MySQLSession &session, const std::string &clusterset_id) {
  const std::string query =
      "select view_id from mysql_innodb_cluster_metadata.v2_cs_clustersets "
      "where clusterset_id = " +
      session.quote(clusterset_id);

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    return stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::cluster_not_found));
  }

  return strtoull_checked((*row)[0]);
}

static stdx::expected<std::string, std::error_code> get_clusterset_id(
    mysqlrouter::MySQLSession &session, const std::string &group_name) {
  const std::string query =
      "select CSM.clusterset_id from "
      "mysql_innodb_cluster_metadata.v2_cs_members CSM join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on CSM.cluster_id = "
      "C.cluster_id where C.group_name = " +
      session.quote(group_name);

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    return stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::cluster_not_found));
  }

  return get_string((*row)[0]);
}

static std::string get_limit_target_cluster_clause(
    const mysqlrouter::TargetCluster &target_cluster,
    mysqlrouter::MySQLSession &session) {
  switch (target_cluster.target_type()) {
    case mysqlrouter::TargetCluster::TargetType::ByUUID:
      return "C.attributes->>'$.group_replication_group_name' = " +
             session.quote(target_cluster.to_string());
    case mysqlrouter::TargetCluster::TargetType::ByName:
      return "C.cluster_name = " + session.quote(target_cluster.to_string());
    default:
      // case mysqlrouter::TargetCluster::TargetType::ByPrimaryRole:
      return "CSM.member_role = 'PRIMARY'";
  }
}

std::string
GRClusterSetMetadataBackend::get_target_cluster_info_from_metadata_server(
    mysqlrouter::MySQLSession &session,
    mysqlrouter::TargetCluster &target_cluster,
    const std::string &clusterset_id) {
  std::string result;

  std::string query =
      "select C.cluster_id, C.cluster_name "
      "from mysql_innodb_cluster_metadata.v2_gr_clusters C join "
      "mysql_innodb_cluster_metadata.v2_cs_members CSM on "
      "CSM.cluster_id = C.cluster_id left join "
      "mysql_innodb_cluster_metadata.v2_cs_clustersets CS on "
      "CSM.clusterset_id = CS.clusterset_id where";

  const std::string limit_target_cluster =
      get_limit_target_cluster_clause(target_cluster, session);

  query += " " + limit_target_cluster;

  if (!clusterset_id.empty()) {
    query += " and CS.clusterset_id = " + session.quote(clusterset_id);
  }

  auto result_processor =
      [&result, &target_cluster](const MySQLSession::Row &row) -> bool {
    if (row.size() != 2) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 2, got = " +
          std::to_string(row.size()));
    }

    result = get_string(row[0]);

    target_cluster.target_type(mysqlrouter::TargetCluster::TargetType::ByName);
    target_cluster.target_value(get_string(row[1]));

    return false;
  };

  try {
    session.query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  return result;
}

metadata_cache::ClusterTopology
GRClusterSetMetadataBackend::update_clusterset_topology_from_metadata_server(
    mysqlrouter::MySQLSession &session, const std::string &clusterset_id,
    uint64_t view_id) {
  metadata_cache::ClusterTopology result;
  result.view_id = view_id;

  std::string query =
      "select I.mysql_server_uuid, I.endpoint, I.xendpoint, I.attributes, "
      "C.cluster_id, C.cluster_name, CSM.member_role, CSM.invalidated, "
      "CS.domain_name "
      "from mysql_innodb_cluster_metadata.v2_instances I join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
      "C.cluster_id join mysql_innodb_cluster_metadata.v2_cs_members CSM on "
      "CSM.cluster_id = C.cluster_id left join "
      "mysql_innodb_cluster_metadata.v2_cs_clustersets CS on "
      "CSM.clusterset_id = CS.clusterset_id";

  if (!clusterset_id.empty()) {
    query += " where CS.clusterset_id = " + session.quote(clusterset_id);
  }

  query += " order by C.cluster_id";

  try {
    session.query(
        query, [&result](const std::vector<const char *> &row) -> bool {
          const std::string node_uuid = get_string(row[0]);
          const std::string node_addr_classic = get_string(row[1]);
          const std::string node_addr_x = get_string(row[2]);
          const std::string node_attributes = get_string(row[3]);
          const std::string cluster_id = get_string(row[4]);
          const std::string cluster_name = get_string(row[5]);
          const bool cluster_is_primary = get_string(row[6]) == "PRIMARY";
          const bool cluster_is_invalidated = strtoui_checked(row[7]) == 1;

          if (result.clusters_data.empty() ||
              result.clusters_data.back().name != cluster_name) {
            metadata_cache::ManagedCluster cluster;
            cluster.id = cluster_id;
            cluster.name = cluster_name;
            cluster.is_primary = cluster_is_primary;
            cluster.is_invalidated = cluster_is_invalidated;
            result.clusters_data.push_back(cluster);
          }

          mysqlrouter::URI uri_classic("mysql://" + node_addr_classic);
          mysqlrouter::URI uri_x("mysql://" + node_addr_x);
          result.clusters_data.back().members.emplace_back(
              metadata_cache::InstanceType::GroupMember, node_uuid,
              metadata_cache::ServerMode::ReadOnly,
              metadata_cache::ServerRole::Secondary, uri_classic.host,
              uri_classic.port, uri_x.port);

          set_instance_attributes(result.clusters_data.back().members.back(),
                                  node_attributes);

          result.metadata_servers.emplace_back(uri_classic.host,
                                               uri_classic.port);
          if (result.name.empty()) {
            result.name = get_string(row[8]);
          }
          return true;
        });
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }

  return result;
}

static void log_target_cluster_warnings(
    const metadata_cache::ManagedCluster &cluster,
    const mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy
        invalidated_cluster_policy) {
  const bool is_invalidated = cluster.is_invalidated;
  const bool state_changed = EventStateTracker::instance().state_changed(
      is_invalidated, EventStateTracker::EventId::ClusterInvalidatedInMetadata,
      cluster.id);

  if (is_invalidated) {
    const auto log_level =
        state_changed ? LogLevel::kWarning : LogLevel::kDebug;

    if (invalidated_cluster_policy ==
        mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::DropAll) {
      log_custom(
          log_level,
          "Target cluster '%s' invalidated in the metadata - blocking all "
          "connections",
          cluster.name.c_str());
    } else {
      // mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::AcceptRO
      log_custom(log_level,
                 "Target cluster '%s' invalidated in the metadata - accepting "
                 "only RO connections",
                 cluster.name.c_str());
    }
  }
}

static bool is_cluster_usable(
    const metadata_cache::ManagedCluster &cluster,
    const mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy
        &invalidated_cluster_policy) {
  return (!cluster.is_invalidated) ||
         (invalidated_cluster_policy !=
          mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::DropAll);
}

static std::optional<size_t> target_cluster_pos(
    const metadata_cache::ClusterTopology &topology,
    const std::string &target_cluster_id) {
  size_t i{};
  for (const auto &cluster : topology.clusters_data) {
    if (cluster.id == target_cluster_id) {
      return i;
    }
    i++;
  }

  return std::nullopt;
}

std::optional<metadata_cache::metadata_server_t>
GRClusterSetMetadataBackend::find_rw_server() {
  for (auto &cluster : (*cluster_topology_).clusters_data) {
    if (!cluster.is_primary) continue;

    log_debug("Updating the status of cluster '%s' to find the writable node",
              cluster.name.c_str());

    // we need to connect to the Primary Cluster and query its GR status to
    // figure out the current Primary node
    metadata_->update_cluster_status(cluster);

    return metadata_->find_rw_server(cluster.members);
  }

  return std::nullopt;
}

stdx::expected<metadata_cache::ClusterTopology, std::error_code>
GRClusterSetMetadataBackend::fetch_cluster_topology(
    MySQLSession::Transaction &transaction,
    const mysqlrouter::MetadataSchemaVersion & /*schema_version*/,
    mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
    const metadata_cache::metadata_server_t &metadata_server,
    bool needs_writable_node, const std::string &group_name,
    const std::string &clusterset_id, bool whole_topology) {
  metadata_cache::ClusterTopology result;
  auto connection = metadata_->get_connection();

  std::string cs_id;
  if (!clusterset_id.empty()) {
    cs_id = clusterset_id;
  } else {
    const auto cluster_id_res = get_clusterset_id(*connection, group_name);
    if (!cluster_id_res) {
      log_warning(
          "Failed fetching clusterset_id from the metadata server on %s:%d - "
          "could not find Cluster with group name '%s' in the metadata",
          metadata_server.address().c_str(), metadata_server.port(),
          group_name.c_str());

      return stdx::make_unexpected(cluster_id_res.error());
    }
    cs_id = cluster_id_res.value();
  }

  const auto view_id_res = get_member_view_id(*connection, cs_id);
  if (!view_id_res) {
    log_warning(
        "Failed fetching view_id from the metadata server on %s:%d - "
        "could not find ClusterSet with ID '%s' in the metadata",
        metadata_server.address().c_str(), metadata_server.port(),
        cs_id.c_str());
    return stdx::make_unexpected(view_id_res.error());
  }
  const uint64_t view_id = view_id_res.value();

  log_debug("Read view_id = %" PRIu64 ", current view_id = %" PRIu64
            ", metadata_read=%s",
            view_id, this->view_id_, metadata_read_ ? "yes" : "no");

  if (view_id < this->view_id_) {
    log_info("Metadata server %s:%d has outdated metadata view_id = %" PRIu64
             ", current view_id = %" PRIu64 ", ignoring",
             metadata_server.address().c_str(), metadata_server.port(), view_id,
             this->view_id_);

    return stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::outdated_view_id));
  }

  if (view_id == this->view_id_ && metadata_read_) {
    return stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::outdated_view_id));
  }

  // check if router options did not change in the metadata
  RouterClusterSetOptions router_clusterset_options;
  if (!router_clusterset_options.read_from_metadata(*connection, router_id)) {
    return stdx::make_unexpected(make_error_code(
        metadata_cache::metadata_errc::no_metadata_read_successful));
  }

  if (router_clusterset_options.get_string() != router_cs_options_string) {
    log_info("New router options read from the metadata '%s', was '%s'",
             router_clusterset_options.get_string().c_str(),
             router_cs_options_string.c_str());
    router_cs_options_string = router_clusterset_options.get_string();
  }

  periodic_stats_update_frequency_ =
      router_clusterset_options.get_stats_updates_frequency();

  // get target_cluster info
  auto new_target_cluster_op =
      router_clusterset_options.get_target_cluster(router_id);
  if (!new_target_cluster_op) {
    return stdx::make_unexpected(make_error_code(
        metadata_cache::metadata_errc::no_metadata_read_successful));
  }
  auto new_target_cluster = *new_target_cluster_op;
  const auto target_cluster_id = get_target_cluster_info_from_metadata_server(
      *connection, new_target_cluster, cs_id);

  const bool target_cluster_changed =
      target_cluster.target_type() != new_target_cluster.target_type() ||
      target_cluster.to_string() != new_target_cluster.to_string();

  target_cluster = new_target_cluster;

  if (target_cluster_id.empty()) {
    log_error("Could not find target_cluster '%s' in the metadata",
              target_cluster.c_str());
    return stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::cluster_not_found));
  } else {
    if (target_cluster_changed) {
      log_info("New target cluster assigned in the metadata: '%s'",
               target_cluster.c_str());
    }
  }

  // update the clusterset topology
  result = update_clusterset_topology_from_metadata_server(*connection, cs_id,
                                                           view_id);

  // we are done with querying metadata
  transaction.commit();

  this->cluster_topology_ = result;

  result.target_cluster_pos = target_cluster_pos(result, target_cluster_id);

  // if we are supposed to only work with single target_cluster clean
  // all other clusters from the result
  if (!whole_topology) {
    if (result.target_cluster_pos)
      result.clusters_data = {result.clusters_data[*result.target_cluster_pos]};
    else
      result.clusters_data.clear();
  }

  for (auto &cluster : result.clusters_data) {
    if (!whole_topology) {
      log_target_cluster_warnings(
          cluster, target_cluster.invalidated_cluster_routing_policy());
      if (!is_cluster_usable(
              cluster, target_cluster.invalidated_cluster_routing_policy())) {
        cluster.members.clear();
        continue;
      }
    }

    // connect to the cluster and query for the list and status of its
    // members. (more precisely: search and connect to a
    // member which is part of quorum to retrieve this data)
    metadata_->update_cluster_status(
        cluster);  // throws metadata_cache::metadata_error

    // change the mode of RW node(s) reported by the GR to RO if the
    // Cluster is Replica (and 'use_replica_primary_as_rw' option is not set)
    // or if our target cluster is invalidated
    if (!whole_topology) {
      if ((!cluster.is_primary &&
           !router_clusterset_options.get_use_replica_primary_as_rw()) ||
          cluster.is_invalidated) {
        for (auto &member : cluster.members) {
          if (member.mode == metadata_cache::ServerMode::ReadWrite) {
            member.mode = metadata_cache::ServerMode::ReadOnly;
          }
        }
      }
    }
  }

  if (needs_writable_node) {
    result.writable_server =
        metadata_->find_rw_server((*this->cluster_topology_).clusters_data);
    if (!result.writable_server) {
      result.writable_server = find_rw_server();
    }

    log_debug("Writable server is: %s",
              result.writable_server
                  ? result.writable_server.value().str().c_str()
                  : "(not found)");
  } else {
    result.writable_server = std::nullopt;
  }

  this->view_id_ = view_id;
  this->metadata_read_ = true;
  return result;
}