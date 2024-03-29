/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_METADATA_CACHE_DATATYPES_INCLUDED
#define MYSQLROUTER_METADATA_CACHE_DATATYPES_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

#include <algorithm>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "mysqlrouter/datatypes.h"  // UserCredentials
#include "tcp_address.h"

namespace metadata_cache {

enum class metadata_errc {
  ok,
  no_metadata_server_reached,
  no_metadata_read_successful,
  metadata_refresh_terminated,
  cluster_not_found,
  invalid_cluster_type,
  outdated_view_id
};
}  // namespace metadata_cache

namespace std {
template <>
struct is_error_code_enum<metadata_cache::metadata_errc>
    : public std::true_type {};
}  // namespace std

namespace metadata_cache {
inline const std::error_category &metadata_cache_category() noexcept {
  class metadata_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "metadata cache"; }
    std::string message(int ev) const override {
      switch (static_cast<metadata_errc>(ev)) {
        case metadata_errc::ok:
          return "ok";
        case metadata_errc::no_metadata_server_reached:
          return "no metadata server accessible";
        case metadata_errc::no_metadata_read_successful:
          return "did not successfully read metadata from any metadata server";
        case metadata_errc::metadata_refresh_terminated:
          return "metadata refresh terminated";
        case metadata_errc::cluster_not_found:
          return "cluster not found in the metadata";
        case metadata_errc::invalid_cluster_type:
          return "unexpected cluster type";
        case metadata_errc::outdated_view_id:
          return "highier view_id seen";
        default:
          return "unknown";
      }
    }
  };

  static metadata_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(metadata_errc e) noexcept {
  return std::error_code(static_cast<int>(e), metadata_cache_category());
}

constexpr const bool kNodeTagHiddenDefault{false};
constexpr const bool kNodeTagDisconnectWhenHiddenDefault{true};

enum class ServerMode { ReadWrite, ReadOnly, Unavailable };
enum class ServerRole { Primary, Secondary, Unavailable };
enum class InstanceType { GroupMember, AsyncMember, ReadReplica };

/** @class ManagedInstance
 *
 * Class ManagedInstance represents a server managed by the topology.
 */
class METADATA_CACHE_EXPORT ManagedInstance {
 public:
  ManagedInstance(InstanceType p_type, const std::string &p_mysql_server_uuid,
                  const ServerMode p_mode, const ServerRole p_role,
                  const std::string &p_host, const uint16_t p_port,
                  const uint16_t p_xport);

  using TCPAddress = mysql_harness::TCPAddress;
  explicit ManagedInstance(InstanceType p_type);
  explicit ManagedInstance(InstanceType p_type, const TCPAddress &addr);
  operator TCPAddress() const;
  bool operator==(const ManagedInstance &other) const;

  /** @brief Instance type */
  InstanceType type;
  /** @brief The uuid of the MySQL server */
  std::string mysql_server_uuid;
  /** @brief The mode of the server */
  ServerMode mode{ServerMode::Unavailable};
  /** @brief The role of the server */
  ServerRole role{ServerRole::Unavailable};
  /** @brief The host name on which the server is running */
  std::string host;
  /** The port number in which the server is running */
  uint16_t port{0};
  /** The X protocol port number in which the server is running */
  uint16_t xport{0};
  /** Node atributes as a json string from metadata */
  std::string attributes;
  /** Should the node be hidden from the application to use it */
  bool hidden{kNodeTagHiddenDefault};
  /** Should the Router disconnect existing client sessions to the node when it
   * is hidden */
  bool disconnect_existing_sessions_when_hidden{
      kNodeTagDisconnectWhenHiddenDefault};
};

using cluster_nodes_list_t = std::vector<ManagedInstance>;

using metadata_server_t = mysql_harness::TCPAddress;

using metadata_servers_list_t = std::vector<metadata_server_t>;

/** @class ManagedCluster
 * Represents a cluster (a GR group or AR members)
 */
class METADATA_CACHE_EXPORT ManagedCluster {
 public:
  /** @brief UUID in the metadata */
  std::string id;
  /** @brief Name of the cluster */
  std::string name;
  /** @brief List of the members that belong to the cluster */
  cluster_nodes_list_t members;
  /** @brief Whether the cluster is in single_primary_mode (from PFS in case of
   * GR) */
  bool single_primary_mode;

  /** @brief Metadata for the cluster is not consistent (only applicable for
   * the GR cluster when the data in the GR metadata is not consistent with the
   * cluster metadata)*/
  bool md_discrepancy{false};

  /** @brief Is this a PRIMARY Cluster in case of ClusterSet */
  bool is_primary{true};
  /** @brief Is the Cluster marked as invalid in the metadata */
  bool is_invalidated{false};

  bool empty() const noexcept { return members.empty(); }

  void clear() noexcept { members.clear(); }
};

/** @class ClusterTopology
 * Represents a cluster (a GR group or AR members) and its metadata servers
 */
struct METADATA_CACHE_EXPORT ClusterTopology {
  using clusters_list_t = std::vector<ManagedCluster>;

  clusters_list_t clusters_data;
  // index of the target cluster in the clusters_data vector
  std::optional<size_t> target_cluster_pos{};
  metadata_servers_list_t metadata_servers;

  /** @brief Id of the view this metadata represents (used for AR and
   * ClusterSets)*/
  uint64_t view_id{0};

  /** @brief name of the ClusterSet or empty in case of standalone Cluster */
  std::string name{};

  // address of the writable metadata server that can be used for updating the
  // metadata (router version, last_check_in), nullptr_t if not found
  std::optional<metadata_cache::metadata_server_t> writable_server{};

  cluster_nodes_list_t get_all_members() const {
    cluster_nodes_list_t result;

    for (const auto &cluster : clusters_data) {
      result.insert(result.end(), cluster.members.begin(),
                    cluster.members.end());
    }

    return result;
  }

  void clear_all_members() {
    for (auto &cluster : clusters_data) {
      cluster.members.clear();
    }
  }
};

/**
 * @brief Metadata MySQL session configuration
 */
struct METADATA_CACHE_EXPORT MetadataCacheMySQLSessionConfig {
  // User credentials used for the connecting to the metadata server.
  mysqlrouter::UserCredentials user_credentials;

  // The time in seconds after which trying to connect to metadata server should
  // time out.
  int connect_timeout;

  // The time in seconds after which read from metadata server should time out.
  int read_timeout;

  // Numbers of retries used before giving up the attempt to connect to the
  // metadata server (not used atm).
  int connection_attempts;
};

struct RouterAttributes {
  std::string metadata_user_name;
  std::string rw_classic_port;
  std::string ro_classic_port;
  std::string rw_x_port;
  std::string ro_x_port;
};

}  // namespace metadata_cache

#endif
