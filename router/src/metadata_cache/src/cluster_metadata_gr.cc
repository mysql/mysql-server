/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#include "dim.h"
#include "group_replication_metadata.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_session.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using mysqlrouter::strtoi_checked;
using mysqlrouter::strtoui_checked;
IMPORT_LOG_FUNCTIONS()

using ConnectCallback =
    std::function<bool(mysqlrouter::MySQLSession &connection,
                       const metadata_cache::ManagedInstance &mi)>;

/* Abstract class for the GR metadata connection to hide the diffences between
 * the metadata versions. */
class GRMetadataBackend {
 public:
  GRMetadataBackend(MetaData *metadata, ConnectCallback &connect_clb)
      : metadata_(metadata), connect_clb_(connect_clb) {}

  virtual ~GRMetadataBackend();

  /** @brief Queries the metadata server for the list of instances and
   * replicasets that belong to the desired cluster.
   */
  virtual MetaData::ReplicaSetsByName fetch_instances_from_metadata_server(
      const std::string &cluster_name,
      const std::string &cluster_type_specific_id) = 0;

  virtual mysqlrouter::ClusterType get_cluster_type() = 0;

 protected:
  MetaData *metadata_;
  ConnectCallback connect_clb_;
};

GRMetadataBackend::~GRMetadataBackend() = default;

/* Connection to the GR metadata version 1.x */
class GRMetadataBackendV1 : public GRMetadataBackend {
 public:
  GRMetadataBackendV1(MetaData *metadata, ConnectCallback &connect_clb)
      : GRMetadataBackend(metadata, connect_clb) {}

  /** @brief Queries the metadata server for the list of instances and
   * replicasets that belong to the desired cluster.
   */
  MetaData::ReplicaSetsByName fetch_instances_from_metadata_server(
      const std::string &cluster_name,
      const std::string &cluster_type_specific_id) override;

  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::GR_V1;
  }
};

/* Connection to the GR metadata version 2.x */
class GRMetadataBackendV2 : public GRMetadataBackend {
 public:
  GRMetadataBackendV2(MetaData *metadata, ConnectCallback &connect_clb)
      : GRMetadataBackend(metadata, connect_clb) {}

  /** @brief Queries the metadata server for the list of instances and
   * replicasets that belong to the desired cluster.
   */
  MetaData::ReplicaSetsByName fetch_instances_from_metadata_server(
      const std::string &cluster_name,
      const std::string &cluster_type_specific_id) override;

  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::GR_V2;
  }
};

GRClusterMetadata::GRClusterMetadata(const std::string &user,
                                     const std::string &password,
                                     int connect_timeout, int read_timeout,
                                     int connection_attempts,
                                     const mysqlrouter::SSLOptions &ssl_options,
                                     const bool use_cluster_notifications)
    : ClusterMetadata(user, password, connect_timeout, read_timeout,
                      connection_attempts, ssl_options) {
  if (use_cluster_notifications) {
    gr_notifications_listener_.reset(
        new GRNotificationListener(user, password));
  }
}

void GRClusterMetadata::update_replicaset_status(
    const std::string &name,
    metadata_cache::ManagedReplicaSet
        &replicaset) {  // throws metadata_cache::metadata_error

  log_debug("Updating replicaset status from GR for '%s'", name.c_str());

  // iterate over all cadidate nodes until we find the node that is part of
  // quorum
  bool found_quorum = false;
  std::shared_ptr<MySQLSession> gr_member_connection;
  for (const metadata_cache::ManagedInstance &mi : replicaset.members) {
    std::string mi_addr = (mi.host == "localhost" ? "127.0.0.1" : mi.host) +
                          ":" + std::to_string(mi.port);

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

      if (!do_connect(*gr_member_connection, mi)) {
        log_warning(
            "While updating metadata, could not establish a connection to "
            "replicaset '%s' through %s",
            name.c_str(), mi_addr.c_str());
        continue;  // server down, next!
      }
    }

    log_debug("Connected to replicaset '%s' through %s", name.c_str(),
              mi_addr.c_str());

    try {
      bool single_primary_mode = true;

      // this node's perspective: give status of all nodes you see
      std::map<std::string, GroupReplicationMember> member_status =
          fetch_group_replication_members(
              *gr_member_connection,
              single_primary_mode);  // throws metadata_cache::metadata_error
      log_debug(
          "Replicaset '%s' has %zu members in metadata, %zu in status table",
          name.c_str(), replicaset.members.size(), member_status.size());

      // check status of all nodes; updates instances
      // ------------------vvvvvvvvvvvvvvvvvv
      bool metadata_gr_discrepancy{false};
      metadata_cache::ReplicasetStatus status = check_replicaset_status(
          replicaset.members, member_status, metadata_gr_discrepancy);
      switch (status) {
        case metadata_cache::ReplicasetStatus::AvailableWritable:  // we have
                                                                   // quorum,
                                                                   // good!
          found_quorum = true;
          break;
        case metadata_cache::ReplicasetStatus::AvailableReadOnly:  // have
                                                                   // quorum,
                                                                   // but only
                                                                   // RO
          found_quorum = true;
          break;
        case metadata_cache::ReplicasetStatus::
            UnavailableRecovering:  // have quorum, but only with recovering
                                    // nodes (cornercase)
          log_warning(
              "quorum for replicaset '%s' consists only of recovering nodes!",
              name.c_str());
          found_quorum = true;  // no point in futher search
          break;
        case metadata_cache::ReplicasetStatus::Unavailable:  // we have nothing
          log_warning("%s is not part of quorum for replicaset '%s'",
                      mi_addr.c_str(), name.c_str());
          continue;  // this server is no good, next!
      }

      if (found_quorum) {
        replicaset.single_primary_mode = single_primary_mode;
        replicaset.md_discrepancy = metadata_gr_discrepancy;
        break;  // break out of the member iteration loop
      }

    } catch (const metadata_cache::metadata_error &e) {
      log_warning(
          "Unable to fetch live group_replication member data from %s from "
          "replicaset '%s': %s",
          mi_addr.c_str(), name.c_str(), e.what());
      continue;  // faulty server, next!
    } catch (const std::exception &e) {
      log_warning(
          "Unable to fetch live group_replication member data from %s from "
          "replicaset '%s': %s",
          mi_addr.c_str(), name.c_str(), e.what());
      continue;  // faulty server, next!
    }

  }  // for (const metadata_cache::ManagedInstance& mi : instances)
  log_debug("End updating replicaset for '%s'", name.c_str());

  if (!found_quorum) {
    std::string msg(
        "Unable to fetch live group_replication member data from any server in "
        "replicaset '");
    msg += name + "'";
    log_error("%s", msg.c_str());

    // if we don't have a quorum, we want to give "nothing" to the Routing
    // plugin, so it doesn't route anything. Routing plugin is dumb, it has no
    // idea what a quorum is, etc.
    replicaset.members.clear();
  }
}

metadata_cache::ReplicasetStatus GRClusterMetadata::check_replicaset_status(
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
  // should hold unless a user "manually" adds new nodes to the replicaset
  // without adding them to metadata (and the user is not allowed to do that).

  // Detect violation of above assumption (alarm if there's a node in
  // `member_status` not present in `instances`). It's O(n*m), but the CPU time
  // is negligible while keeping code simple.

  metadata_gr_discrepancy = false;
  for (const auto &status_node : member_status) {
    using MI = metadata_cache::ManagedInstance;
    auto found = std::find_if(instances.begin(), instances.end(),
                              [&status_node](const MI &metadata_node) {
                                return status_node.first ==
                                       metadata_node.mysql_server_uuid;
                              });
    if (found == instances.end()) {
      metadata_gr_discrepancy = true;
      log_error(
          "Member %s:%d (%s) found in replicaset, yet is not defined in "
          "metadata!",
          status_node.second.host.c_str(), status_node.second.port,
          status_node.first.c_str());
    }
  }

  using metadata_cache::ReplicasetStatus;
  using metadata_cache::ServerMode;
  using GR_State = GroupReplicationMember::State;
  using GR_Role = GroupReplicationMember::Role;

  // we do two things here:
  // 1. for all `instances`, set .mode according to corresponding .status found
  // in `member_status`
  // 2. count nodes which are part of quorum (online/recovering nodes)
  unsigned int quorum_count = 0;
  bool have_primary_instance = false;
  bool have_secondary_instance = false;
  for (auto &member : instances) {
    auto status = member_status.find(member.mysql_server_uuid);
    if (status != member_status.end()) {
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
      log_warning(
          "Member %s:%d (%s) defined in metadata not found in actual "
          "replicaset",
          member.host.c_str(), member.port, member.mysql_server_uuid.c_str());
    }
  }

  // quorum_count is based on nodes from `instances` instead of `member_status`.
  // This is okay, because all nodes in `member_status` are present in
  // `instances` (our assumption described at the top)
  bool have_quorum = (quorum_count > member_status.size() / 2);

  // if we don't have quorum, we don't allow any access. Some configurations
  // might allow RO access in this case, but we don't support it at the momemnt
  if (!have_quorum) return ReplicasetStatus::Unavailable;

  // if we have quorum but no primary/secondary instances, it means the quorum
  // is composed purely of recovering nodes (this is an unlikey cornercase)
  if (!(have_primary_instance || have_secondary_instance))
    return ReplicasetStatus::UnavailableRecovering;

  // if primary node was not elected yet, we can only allow reads (typically
  // this is a temporary state shortly after a node failure, but could also be
  // more permanent)
  return have_primary_instance
             ? ReplicasetStatus::AvailableWritable   // typical case
             : ReplicasetStatus::AvailableReadOnly;  // primary not elected yet
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
    const std::string &cluster_name) {
  if (!metadata_backend_) return {};
  switch (metadata_backend_->get_cluster_type()) {
    case mysqlrouter::ClusterType::GR_V1:
      log_warning(
          "metadata_cache authentication backend is not supported for metadata "
          "version 1.0");
      return {};
    default:
      return ClusterMetadata::fetch_auth_credentials(cluster_name);
  }
}

GRClusterMetadata::ReplicaSetsByName
GRClusterMetadata::fetch_instances_from_metadata_server(
    const std::string &cluster_name,
    const std::string &cluster_type_specific_id) {
  return metadata_backend_->fetch_instances_from_metadata_server(
      cluster_name, cluster_type_specific_id);
}

void GRClusterMetadata::update_backend(
    const mysqlrouter::MetadataSchemaVersion &version) {
  const auto cluster_type =
      mysqlrouter::get_cluster_type(version, metadata_connection_.get());

  // if the current backend does not fit the metadata version that we just
  // discovered, we need to recreate it
  if (!metadata_backend_ ||
      cluster_type != metadata_backend_->get_cluster_type()) {
    if (metadata_backend_) {
      log_info(
          "Metadata version change was discovered. New metadata version is "
          "%d.%d.%d",
          version.major, version.minor, version.patch);
    }
    reset_metadata_backend(cluster_type);
  }
}

ClusterMetadata::ReplicaSetsByName GRClusterMetadata::fetch_instances(
    const std::string &cluster_name,
    const std::string &cluster_type_specific_id) {
  log_debug("Updating metadata information for cluster '%s'",
            cluster_name.c_str());

  MySQLSession::Transaction transaction(metadata_connection_.get());
  auto version =
      get_and_check_metadata_schema_version(*metadata_connection_.get());
  update_backend(version);

  // fetch existing replicasets in the cluster from the metadata server (this is
  // the topology that was configured, it will be compared later against current
  // topology reported by (a server in) replicaset)
  ReplicaSetsByName replicasets(fetch_instances_from_metadata_server(
      cluster_name,
      cluster_type_specific_id));  // throws metadata_cache::metadata_error

  // we are done with querying metadata
  transaction.commit();

  if (replicasets.empty())
    log_warning("No replicasets defined for cluster '%s'",
                cluster_name.c_str());

  // now connect to each replicaset and query it for the list and status of its
  // members. (more precisely, foreach replicaset: search and connect to a
  // member which is part of quorum to retrieve this data)
  for (auto &&rs : replicasets) {
    update_replicaset_status(
        rs.first, rs.second);  // throws metadata_cache::metadata_error
  }

  return replicasets;
}

// throws metadata_cache::metadata_error
ClusterMetadata::ReplicaSetsByName
GRMetadataBackendV1::fetch_instances_from_metadata_server(
    const std::string &cluster_name,
    const std::string &cluster_type_specific_id) {
  // If we have group replication id we also want to limit the results only for
  // that group replication. For backward compatibility we need to check if it
  // is not empty, we didn't store that information before introducing dynamic
  // state file.
  std::string limit_group_replication;
  auto connection = metadata_->get_connection();
  if (!cluster_type_specific_id.empty()) {
    limit_group_replication =
        " AND R.attributes->>'$.group_replication_group_name' = " +
        connection->quote(cluster_type_specific_id);
  }

  // Get expected topology (what was configured) from metadata server. This will
  // later be compared against current topology (what exists NOW) obtained from
  // one of the nodes belonging to a quorum. Note that this topology will also
  // be successfully returned when a particular metadata server is not part of
  // GR, as serving metadata and being part of replicaset are two orthogonal
  // ideas.
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
      "WHERE F.cluster_name = " +
      connection->quote(cluster_name) + limit_group_replication);

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
  // The following instance map stores a list of servers mapped to every
  // replicaset name.
  // {
  //   {replicaset_1:[host1:port1, host2:port2, host3:port3]},
  //   {replicaset_2:[host4:port4, host5:port5, host6:port6]},
  //   ...
  //   {replicaset_n:[hostj:portj, hostk:portk, hostl:portl]}
  // }
  MetaData::ReplicaSetsByName replicaset_map;

  // Deserialize the resultset into a map that stores a list of server
  // instance objects mapped to each replicaset.
  auto result_processor =
      [&replicaset_map](const MySQLSession::Row &row) -> bool {
    if (row.size() != 4) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 4, got = " +
          std::to_string(row.size()));
    }

    metadata_cache::ManagedInstance s;
    s.replicaset_name = get_string(row[0]);
    s.mysql_server_uuid = get_string(row[1]);
    if (!set_instance_ports(s, row, 2, 3)) {
      return true;  // next row
    }

    auto &rset(replicaset_map[s.replicaset_name]);
    rset.members.push_back(s);
    rset.name = s.replicaset_name;
    rset.single_primary_mode =
        true;  // actual value set elsewhere from GR metadata

    return true;  // false = I don't want more rows
  };

  assert(connection->is_connected());

  try {
    connection->query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  return replicaset_map;
}

// throws metadata_cache::metadata_error
ClusterMetadata::ReplicaSetsByName
GRMetadataBackendV2::fetch_instances_from_metadata_server(
    const std::string &cluster_name,
    const std::string &cluster_type_specific_id) {
  auto connection = metadata_->get_connection();

  std::string limit_group_replication;
  if (!cluster_type_specific_id.empty()) {
    limit_group_replication =
        " AND C.group_name = " + connection->quote(cluster_type_specific_id);
  }

  // Get expected topology (what was configured) from metadata server. This will
  // later be compared against current topology (what exists NOW) obtained from
  // one of the nodes belonging to a quorum. Note that this topology will also
  // be successfully returned when a particular metadata server is not part of
  // GR, as serving metadata and being part of replicaset are two orthogonal
  // ideas.
  std::string query(
      "select I.mysql_server_uuid, I.endpoint, I.xendpoint, I.attributes from "
      "mysql_innodb_cluster_metadata.v2_instances I join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
      "C.cluster_id where C.cluster_name = " +
      connection->quote(cluster_name) + limit_group_replication);

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
  // The following instance map stores a list of servers mapped to every
  // replicaset name.
  // {
  //   {replicaset_1:[host1:port1, host2:port2, host3:port3]},
  //   {replicaset_2:[host4:port4, host5:port5, host6:port6]},
  //   ...
  //   {replicaset_n:[hostj:portj, hostk:portk, hostl:portl]}
  // }
  MetaData::ReplicaSetsByName replicaset_map;

  // Deserialize the resultset into a map that stores a list of server
  // instance objects mapped to each replicaset.
  auto result_processor =
      [&replicaset_map](const MySQLSession::Row &row) -> bool {
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

    instance.replicaset_name = "default";

    auto &rset(replicaset_map[instance.replicaset_name]);
    rset.members.push_back(instance);
    rset.name = instance.replicaset_name;
    rset.single_primary_mode =
        true;  // actual value set elsewhere from GR metadata

    return true;  // false = I don't want more rows
  };

  try {
    connection->query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  return replicaset_map;
}

GRClusterMetadata::~GRClusterMetadata() = default;
