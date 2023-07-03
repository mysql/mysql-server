/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef METADATA_CACHE_CLUSTER_METADATA_INCLUDED
#define METADATA_CACHE_CLUSTER_METADATA_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/metadata.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/mysql_session.h"
#include "tcp_address.h"

#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GroupReplicationMember;

namespace mysqlrouter {
class MySQLSession;
}
namespace xcl {
class XSession;
}

using ConnectCallback =
    std::function<bool(mysqlrouter::MySQLSession &connection,
                       const metadata_cache::ManagedInstance &mi)>;

/** @class ClusterMetadata
 *
 * The `ClusterMetadata` class encapsulates a connection to the Metadata server.
 * It uses the mysqlrouter::MySQLSession to setup, manage and retrieve results.
 *
 */
class METADATA_CACHE_EXPORT ClusterMetadata : public MetaData {
 public:
  /** @brief Constructor
   *
   * @param session_config Metadata MySQL session configuration
   * @param ssl_options SSL related options to use for MySQL connections)
   */
  ClusterMetadata(
      const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
      const mysqlrouter::SSLOptions &ssl_options);

  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit ClusterMetadata(const ClusterMetadata &) = delete;
  ClusterMetadata &operator=(const ClusterMetadata &) = delete;

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  ~ClusterMetadata() override;

  /** @brief Connects with the Metadata server and sets up the session
   * parameters
   *
   *
   * @param metadata_server the server instance for which the connection
   *                        should be attempted.
   *
   * @return a boolean to indicate if the connection and session parameters
   * setup was successful.
   */
  bool connect_and_setup_session(const metadata_cache::metadata_server_t
                                     &metadata_server) noexcept override;

  /** @brief Disconnects from the Metadata server
   *
   * This is a no-op, as MySQLSession object used underneath for
   * connection handling employs RAII, making this method unnecessary.
   */
  void disconnect() noexcept override {}

  /** @brief Gets the object representing the session to the metadata server
   */
  std::shared_ptr<mysqlrouter::MySQLSession> get_connection() override {
    return metadata_connection_;
  }

  bool update_router_attributes(
      const metadata_cache::metadata_server_t &rw_server,
      const unsigned router_id,
      const metadata_cache::RouterAttributes &router_attributes) override;

  bool update_router_last_check_in(
      const metadata_cache::metadata_server_t &rw_server,
      const unsigned router_id) override;

  auth_credentials_t fetch_auth_credentials(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &cluster_type_specific_id) override;

  std::optional<metadata_cache::metadata_server_t> find_rw_server(
      const std::vector<metadata_cache::ManagedInstance> &instances);

  std::optional<metadata_cache::metadata_server_t> find_rw_server(
      const std::vector<metadata_cache::ManagedCluster> &clusters);

  std::optional<std::chrono::seconds>
  get_periodic_stats_update_frequency() noexcept override {
    return {};
  }

 protected:
  /** Connects a MYSQL connection to the given instance
   */
  bool do_connect(mysqlrouter::MySQLSession &connection,
                  const metadata_cache::metadata_server_t &mi);

  // throws metadata_cache::metadata_error and
  // MetadataUpgradeInProgressException
  mysqlrouter::MetadataSchemaVersion get_and_check_metadata_schema_version(
      mysqlrouter::MySQLSession &session);

  // Metadata node generic information
  mysql_ssl_mode ssl_mode_;
  mysqlrouter::SSLOptions ssl_options_;

  metadata_cache::MetadataCacheMySQLSessionConfig session_config_;

#if 0  // not used so far
  // The number of times we should try connecting to the metadata server if a
  // connection attempt fails.
  int connection_attempts_;
#endif

  // connection to metadata server (it may also be shared with GR status queries
  // for optimisation purposes)
  std::shared_ptr<mysqlrouter::MySQLSession> metadata_connection_;
};

std::string get_string(const char *input_str);

bool set_instance_ports(metadata_cache::ManagedInstance &instance,
                        const mysqlrouter::MySQLSession::Row &row,
                        const size_t classic_port_column,
                        const size_t x_port_column);

void set_instance_attributes(metadata_cache::ManagedInstance &instance,
                             const std::string &attributes);

bool get_hidden(const std::string &attributes, std::string &out_warning);
bool get_disconnect_existing_sessions_when_hidden(const std::string &attributes,
                                                  std::string &out_warning);

#endif  // METADATA_CACHE_CLUSTER_METADATA_INCLUDED
