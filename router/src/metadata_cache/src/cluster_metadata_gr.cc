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

#include "cluster_metadata_gr.h"

#include <algorithm>
#include <optional>

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
  virtual metadata_cache::ManagedCluster fetch_instances_from_metadata_server(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &group_name, const std::string &clusterset_id = "") = 0;

  virtual mysqlrouter::ClusterType get_cluster_type() = 0;

  virtual stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      MySQLSession::Transaction &transaction,
      mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
      const metadata_cache::metadata_server_t &metadata_server,
      bool needs_writable_node, const std::string &group_name,
      const std::string &clusterset_id);

  virtual std::vector<metadata_cache::metadata_servers_list_t>
  get_metadata_servers(
      const metadata_cache::metadata_servers_list_t &metadata_servers) {
    // by default use them all, that's what works for non-clusterset backends
    return get_all_metadata_servers(metadata_servers);
  }

  virtual void reset() {}

 protected:
  GRClusterMetadata *metadata_;
  ConnectCallback connect_clb_;
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
  metadata_cache::ManagedCluster fetch_instances_from_metadata_server(
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
  metadata_cache::ManagedCluster fetch_instances_from_metadata_server(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &group_name,
      const std::string &clusterset_id = "") override;

  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::GR_V2;
  }

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
   * @return object containing cluster topology information in case of success,
   * or error code in case of failure
   * @throws metadata_cache::metadata_error
   */
  stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      MySQLSession::Transaction &transaction,
      mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
      const metadata_cache::metadata_server_t &metadata_server,
      bool needs_writable_node, const std::string &group_name,
      const std::string &clusterset_id = "") override;

  void reset() override { metadata_read_ = false; }

  std::vector<metadata_cache::metadata_servers_list_t> get_metadata_servers(
      const metadata_cache::metadata_servers_list_t &metadata_servers)
      override {
    return clusterset_topology_.get_metadata_servers(metadata_servers);
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

  // returns cluster_id
  std::string get_target_cluster_info_from_metadata_server(
      mysqlrouter::MySQLSession &session,
      mysqlrouter::TargetCluster &target_cluster,
      const std::string &clusterset_id);

  /** @brief Queries the metada for the current ClusterSet topology. Stores the
   * topology in the class state.
   *
   * @param session active connection to the member that is checked for the
   * metadata
   * @param clusterset_id ID of the ClusterSet this operation refers to
   */
  void update_clusterset_topology_from_metadata_server(
      mysqlrouter::MySQLSession &session, const std::string &clusterset_id);

  /** @brief Finds the writable node within the currently known ClusterSet
   * topology.
   *
   * @return address of the current primary node if found
   * @return metadata_cache::metadata_errc::no_rw_node_found if did not find any
   * primary
   */
  stdx::expected<metadata_cache::metadata_server_t, std::error_code>
  find_rw_server() const;

  struct ClusterSetTopology {
    // we at least once successfully read the metadata from one of the metadata
    // servers we had stored in the state file; if true we have the
    // clusters-nodes assignement, we know which cluster is the primary etc. so
    // when refreshing the metadata we can only check one node per cluster for
    // highiest view_id
    bool is_set{false};

    // the key is Cluster UUID
    std::vector<metadata_cache::ManagedCluster> clusters_data;
    // index of the target cluster in the clusters_data vector
    std::optional<size_t> target_cluster_pos{};
    metadata_cache::metadata_servers_list_t metadata_servers;

    std::vector<metadata_cache::metadata_servers_list_t> get_metadata_servers(
        const metadata_cache::metadata_servers_list_t &metadata_servers) const {
      std::vector<metadata_cache::metadata_servers_list_t> result;

      if (is_set) {
        // We already know the latest ClusterSet topology so we return the
        // servers grouped by the Cluster they belong to
        for (const auto &cluster : clusters_data) {
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
  };

  /** @brief Returns index of the target cluster in the clusters vector in
   * topology.
   *
   * @param topology object containing ClusterSet topology to search
   * @param target_cluster_id UUID fo the target_cluster to find
   * @return index in the clusters array or std::nullopt if not found
   */
  static std::optional<size_t> target_cluster_pos(
      const ClusterSetTopology &topology, const std::string &target_cluster_id);

  ClusterSetTopology clusterset_topology_;
};

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
    const mysqlrouter::TargetCluster &target_cluster,
    metadata_cache::ManagedCluster
        &cluster) {  // throws metadata_cache::metadata_error

  log_debug("Updating cluster status from GR for '%s'", target_cluster.c_str());

  // iterate over all cadidate nodes until we find the node that is part of
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
            target_cluster.c_str(), mi_addr.c_str());
        continue;  // server down, next!
      }
    }

    log_debug("Connected to cluster '%s' through %s", target_cluster.c_str(),
              mi_addr.c_str());

    try {
      bool single_primary_mode = true;

      // this node's perspective: give status of all nodes you see
      std::map<std::string, GroupReplicationMember> member_status =
          fetch_group_replication_members(
              *gr_member_connection,
              single_primary_mode);  // throws metadata_cache::metadata_error
      log_debug("Cluster '%s' has %zu members in metadata, %zu in status table",
                target_cluster.c_str(), cluster.members.size(),
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
              target_cluster.c_str());
          found_quorum = true;  // no point in futher search
          break;
        case GRClusterStatus::Unavailable:  // we have nothing
          log_warning("%s is not part of quorum for cluster '%s'",
                      mi_addr.c_str(), target_cluster.c_str());
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
          mi_addr.c_str(), target_cluster.c_str(), e.what());
      continue;  // faulty server, next!
    } catch (const std::exception &e) {
      log_warning(
          "Unable to fetch live group_replication member data from %s from "
          "cluster '%s': %s",
          mi_addr.c_str(), target_cluster.c_str(), e.what());
      continue;  // faulty server, next!
    }

  }  // for (const metadata_cache::ManagedInstance& mi : instances)
  log_debug("End updating cluster for '%s'", target_cluster.c_str());

  if (!found_quorum) {
    std::string msg(
        "Unable to fetch live group_replication member data from any server in "
        "cluster '");
    msg += target_cluster.to_string() + "'";
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
              member.mode = ServerMode::ReadWrite;
              quorum_count++;
              break;
            case GR_Role::Secondary:
              have_secondary_instance = true;
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
          member.mode = ServerMode::Unavailable;
          break;
      }
    } else {
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
  // is composed purely of recovering nodes (this is an unlikey cornercase)
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

metadata_cache::ManagedCluster
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

stdx::expected<metadata_cache::ClusterTopology, std::error_code>
GRMetadataBackend::fetch_cluster_topology(
    MySQLSession::Transaction &transaction,
    mysqlrouter::TargetCluster &target_cluster, const unsigned /*router_id*/,
    const metadata_cache::metadata_server_t & /*metadata_server*/,
    bool needs_writable_node, const std::string &group_name,
    const std::string &clusterset_id = "") {
  metadata_cache::ClusterTopology result;

  // fetch cluster topology from the metadata server (this is
  // the topology that was configured, it will be compared later against
  // current topology reported by (a server in) Group Replication)
  result.cluster_data = fetch_instances_from_metadata_server(
      target_cluster, group_name,
      clusterset_id);  // throws metadata_cache::metadata_error

  // we are done with querying metadata
  transaction.commit();

  // now connect to the cluster and query it for the list and status of its
  // members. (more precisely: search and connect to a
  // member which is part of quorum to retrieve this data)
  metadata_->update_cluster_status(
      target_cluster,
      result.cluster_data);  // throws metadata_cache::metadata_error

  // for Cluster that is not part of the ClusterSet we assume metadata
  // servers are just Cluster nodes
  for (const auto &cluster_node : result.cluster_data.members) {
    result.metadata_servers.push_back({cluster_node.host, cluster_node.port});
  }

  if (needs_writable_node) {
    result.cluster_data.writable_server =
        metadata_->find_rw_server(result.cluster_data.members);
  } else {
    result.cluster_data.writable_server = stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::no_rw_node_needed));
  }

  return result;
}

stdx::expected<metadata_cache::ClusterTopology, std::error_code>
GRClusterMetadata::fetch_cluster_topology(
    const std::atomic<bool> &terminated,
    mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
    const metadata_cache::metadata_servers_list_t &metadata_servers,
    bool needs_writable_node, const std::string &group_name,
    const std::string &clusterset_id, std::size_t &instance_id) {
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
            transaction, target_cluster, router_id, metadata_server,
            needs_writable_node, group_name, clusterset_id);

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
        // with first successfull read for a ClusterSet we need to go over all
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
metadata_cache::ManagedCluster
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

  // example response
  // clang-format off
  // +-----------------+--------------------------------------+--------------------------------+--------------------------+
  // | replicaset_name | mysql_server_uuid                    | I.addresses->>'$.mysqlClassic' | I.addresses->>'$.mysqlX' |
  // +-----------------+--------------------------------------+--------------------------------+--------------------------+
  // | default         | 30ec658e-861d-11e6-9988-08002741aeb6 | localhost:3310                 | NULL                     |
  // | default         | 3acfe4ca-861d-11e6-9e56-08002741aeb6 | localhost:3320                 | NULL                     |
  // | default         | 4c08b4a2-861d-11e6-a256-08002741aeb6 | localhost:3330                 | NULL                     |
  // +-----------------+--------------------------------------+--------------------------------+--------------------------+
  // clang-format on
  //

  metadata_cache::ManagedCluster result;
  auto result_processor = [&result](const MySQLSession::Row &row) -> bool {
    if (row.size() != 4) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 4, got = " +
          std::to_string(row.size()));
    }

    metadata_cache::ManagedInstance s;
    s.mysql_server_uuid = get_string(row[1]);
    if (!set_instance_ports(s, row, 2, 3)) {
      return true;  // next row
    }

    result.members.push_back(s);
    result.single_primary_mode =
        true;  // actual value set elsewhere from GR metadata

    return true;  // false = I don't want more rows
  };

  assert(connection->is_connected());

  try {
    connection->query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
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
metadata_cache::ManagedCluster
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
      "select I.mysql_server_uuid, I.endpoint, I.xendpoint, I.attributes "
      "from "
      "mysql_innodb_cluster_metadata.v2_instances I join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
      "C.cluster_id where " +
      limit_cluster + limit_group_replication);

  // example response
  // clang-format off
  //  +--------------------------------------+----------------+-----------------+--------------------------------------------------------------------+
  //  | mysql_server_uuid                    | endpoint       | xendpoint       | attributes                                                         |
  //  +--------------------------------------+----------------+-----------------+--------------------------------------------------------------------+
  //  | 201eabcf-adfa-11e9-8205-0800276c00e7 | 127.0.0.1:5000 | 127.0.0.1:50000 | {"tags": {"_hidden": true}, "joinTime": "2020-03-18 09:36:50.416"} |
  //  | 351ea0ec-adfa-11e9-b348-0800276c00e7 | 127.0.0.1:5001 | 127.0.0.1:50010 | {"joinTime": "2020-03-18 09:36:51.000"}                            |
  //  | 559bd763-adfa-11e9-b2c3-0800276c00e7 | 127.0.0.1:5002 | 127.0.0.1:50020 | {"joinTime": "2020-03-18 09:36:53.456"}                            |
  //  +--------------------------------------+----------------+-----------------+--------------------------------------------------------------------+
  // clang-format on
  //
  metadata_cache::ManagedCluster result;
  auto result_processor = [&result](const MySQLSession::Row &row) -> bool {
    if (row.size() != 4) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 4, got = " +
          std::to_string(row.size()));
    }

    metadata_cache::ManagedInstance instance;
    instance.mysql_server_uuid = get_string(row[0]);
    if (!set_instance_ports(instance, row, 1, 2)) {
      return true;  // next row
    }
    set_instance_attributes(instance, get_string(row[3]));

    result.members.push_back(instance);
    result.single_primary_mode =
        true;  // actual value set elsewhere from GR metadata

    return true;  // false = I don't want more rows
  };

  try {
    connection->query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  return result;
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

static std::string get_router_option_str(const std::string &options,
                                         const std::string &name,
                                         const std::string &default_value,
                                         std::string &out_error) {
  out_error = "";
  if (options.empty()) return default_value;

  rapidjson::Document json_doc;
  json_doc.Parse(options.c_str(), options.length());

  if (!json_doc.IsObject()) {
    out_error = "not a valid JSON object";
    return default_value;
  }

  const auto it =
      json_doc.FindMember(rapidjson::Value{name.data(), name.size()});
  if (it == json_doc.MemberEnd()) {
    return default_value;
  }

  if (!it->value.IsString()) {
    out_error = "options." + name + " not a string";
    return default_value;
  }

  return it->value.GetString();
}

static bool update_router_options_from_metadata(
    mysqlrouter::MySQLSession &session, const unsigned router_id,
    mysqlrouter::TargetCluster &target_cluster) {
  // check if we have a target cluster assigned in the metadata
  const std::string query =
      "SELECT router_options FROM "
      "mysql_innodb_cluster_metadata.v2_cs_router_options where router_id = " +
      std::to_string(router_id);

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    log_error(
        "Error reading target_cluster from the router.options: did not find "
        "router entry for router_id '%u'",
        router_id);
    return false;
  }

  const std::string options_str = get_string((*row)[0]);
  target_cluster.options_string(options_str);

  std::string out_error;
  std::string target_cluster_str =
      get_router_option_str(options_str, "target_cluster", "", out_error);

  if (!out_error.empty()) {
    log_error("Error reading target_cluster from the router.options: %s",
              out_error.c_str());
    return false;
  }

  const std::string invalidated_cluster_routing_policy_str =
      get_router_option_str(options_str, "invalidated_cluster_policy", "",
                            out_error);

  if (invalidated_cluster_routing_policy_str == "accept_ro") {
    target_cluster.invalidated_cluster_routing_policy(
        mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::AcceptRO);
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
    log_custom(
        log_level,
        "Target cluster for router_id=%d not set, using 'primary' as a target "
        "cluster",
        router_id);
    target_cluster_str = "primary";
  }

  if (target_cluster_str == "primary") {
    target_cluster.target_type(
        mysqlrouter::TargetCluster::TargetType::ByPrimaryRole);
    target_cluster.target_value("");
  } else {
    target_cluster.target_type(mysqlrouter::TargetCluster::TargetType::ByUUID);
    target_cluster.target_value(target_cluster_str);
  }

  return true;
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

void GRClusterSetMetadataBackend::
    update_clusterset_topology_from_metadata_server(
        mysqlrouter::MySQLSession &session, const std::string &clusterset_id) {
  ClusterSetTopology result;

  std::string query =
      "select I.mysql_server_uuid, I.endpoint, I.xendpoint, I.attributes, "
      "C.cluster_id, C.cluster_name, CSM.member_role, CSM.invalidated "
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
              node_uuid, metadata_cache::ServerMode::ReadOnly, uri_classic.host,
              uri_classic.port, uri_x.port);

          set_instance_attributes(result.clusters_data.back().members.back(),
                                  node_attributes);

          result.metadata_servers.emplace_back(uri_classic.host,
                                               uri_classic.port);
          return true;
        });
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }
  result.is_set = true;
  clusterset_topology_ = std::move(result);
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

stdx::expected<metadata_cache::metadata_server_t, std::error_code>
GRClusterSetMetadataBackend::find_rw_server() const {
  for (const auto &cluster : clusterset_topology_.clusters_data) {
    if (!cluster.is_primary) continue;

    const auto cluster_uuid = cluster.id;
    metadata_cache::ManagedCluster primary_cluster;
    for (const auto &node : cluster.members) {
      primary_cluster.members.emplace_back(node);
    }

    log_debug("Updating the status of cluster '%s' to find the writable node",
              cluster_uuid.c_str());

    // we need to connect to the Primary Cluster and query its GR status to
    // figure out the current Primary node
    metadata_->update_cluster_status(
        {mysqlrouter::TargetCluster::TargetType::ByUUID, cluster_uuid},
        primary_cluster);

    return metadata_->find_rw_server(primary_cluster.members);
  }

  return stdx::make_unexpected(
      make_error_code(metadata_cache::metadata_errc::no_rw_node_needed));
}

static bool is_cluster_usable(
    const metadata_cache::ManagedCluster &cluster,
    const mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy
        &invalidated_cluster_policy) {
  return (!cluster.is_invalidated) ||
         (invalidated_cluster_policy !=
          mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::DropAll);
}

std::optional<size_t> GRClusterSetMetadataBackend::target_cluster_pos(
    const GRClusterSetMetadataBackend::ClusterSetTopology &topology,
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

stdx::expected<metadata_cache::ClusterTopology, std::error_code>
GRClusterSetMetadataBackend::fetch_cluster_topology(
    MySQLSession::Transaction &transaction,
    mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
    const metadata_cache::metadata_server_t &metadata_server,
    bool needs_writable_node, const std::string &group_name,
    const std::string &clusterset_id) {
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
  mysqlrouter::TargetCluster new_target_cluster;
  if (!update_router_options_from_metadata(*connection, router_id,
                                           new_target_cluster)) {
    return stdx::make_unexpected(make_error_code(
        metadata_cache::metadata_errc::no_metadata_read_successful));
  }

  if (new_target_cluster.options_string() != target_cluster.options_string()) {
    log_info("New router options read from the metadata '%s', was '%s'",
             new_target_cluster.options_string().c_str(),
             target_cluster.options_string().c_str());
  }

  // get target_cluster info
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
  update_clusterset_topology_from_metadata_server(*connection, cs_id);

  // we are done with querying metadata, this transaction commit has to be
  // done unconditionally, regardless of the target cluster usability
  transaction.commit();

  const auto target_cluster_idx =
      target_cluster_pos(clusterset_topology_, target_cluster_id);

  if (target_cluster_idx)
    result.cluster_data =
        clusterset_topology_.clusters_data[*target_cluster_idx];
  else
    result.cluster_data.name = target_cluster.to_string();

  log_target_cluster_warnings(
      result.cluster_data, target_cluster.invalidated_cluster_routing_policy());

  result.view_id = view_id;
  result.cluster_data.single_primary_mode = true;
  result.metadata_servers = clusterset_topology_.metadata_servers;

  if (!is_cluster_usable(result.cluster_data,
                         target_cluster.invalidated_cluster_routing_policy())) {
    result.cluster_data.members.clear();
  } else {
    // connect to the cluster and query for the list and status of its
    // members. (more precisely: search and connect to a
    // member which is part of quorum to retrieve this data)
    metadata_->update_cluster_status(
        target_cluster,
        result.cluster_data);  // throws metadata_cache::metadata_error

    // change the mode of RW node(s) reported by the GR to RO if the
    // Cluster is Replica or if our target cluster is invalidated
    if (!result.cluster_data.is_primary || result.cluster_data.is_invalidated) {
      for (auto &member : result.cluster_data.members) {
        if (member.mode == metadata_cache::ServerMode::ReadWrite) {
          member.mode = metadata_cache::ServerMode::ReadOnly;
        }
      }
    }
  }

  if (needs_writable_node) {
    if (result.cluster_data.is_primary) {
      // if our target cluster is PRIMARY we grab the writable node; we
      // already know which one is it as we just checked it
      result.cluster_data.writable_server =
          metadata_->find_rw_server(result.cluster_data.members);
    } else {
      result.cluster_data.writable_server = find_rw_server();
    }

    log_debug("Writable server is: %s",
              result.cluster_data.writable_server
                  ? result.cluster_data.writable_server.value().str().c_str()
                  : "(not found)");
  } else {
    result.cluster_data.writable_server = stdx::make_unexpected(
        make_error_code(metadata_cache::metadata_errc::no_rw_node_needed));
  }

  this->view_id_ = view_id;
  this->metadata_read_ = true;
  return result;
}
