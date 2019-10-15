/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "metadata.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/mysql_session.h"
#include "tcp_address.h"

#include <string.h>
#include <chrono>
#include <map>
#include <memory>
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
class METADATA_API ClusterMetadata : public MetaData {
 public:
  /** @brief Constructor
   *
   * @param user The user name used to authenticate to the metadata server.
   * @param password The password used to authenticate to the metadata server.
   * @param connect_timeout The time after which trying to connect to the
   *                        metadata server should timeout (in seconds).
   * @param read_timeout The time after which read from metadata server should
   *                     timeout (in seconds).
   * @param connection_attempts The number of times a connection to metadata
   *                            must be attempted, when a connection attempt
   *                            fails.  NOTE: not used so far
   * @param ssl_options SSL related options to use for MySQL connections)
   */
  ClusterMetadata(const std::string &user, const std::string &password,
                  int connect_timeout, int read_timeout,
                  int connection_attempts,
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

  /** @brief Connects with the Metadata server
   *
   *
   * @param metadata_server the server instance for which the connection
   *                        should be attempted.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect(
      const metadata_cache::ManagedInstance &metadata_server) noexcept override;

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

  bool update_router_version(const metadata_cache::ManagedInstance &rw_instance,
                             const unsigned router_id) override;

  bool update_router_last_check_in(
      const metadata_cache::ManagedInstance &rw_instance,
      const unsigned router_id) override;

 protected:
  /** Connects a MYSQL connection to the given instance
   */
  bool do_connect(mysqlrouter::MySQLSession &connection,
                  const metadata_cache::ManagedInstance &mi);

  // throws metadata_cache::metadata_error and
  // MetadataUpgradeInProgressException
  mysqlrouter::MetadataSchemaVersion get_and_check_metadata_schema_version(
      mysqlrouter::MySQLSession &session);

  // Metadata node connection information
  std::string user_;
  std::string password_;

  // Metadata node generic information
  mysql_ssl_mode ssl_mode_;
  mysqlrouter::SSLOptions ssl_options_;

  std::string cluster_name_;
#if 0  // not used so far
  std::string metadata_uuid_;
  std::string message_;
#endif

  // The time after which trying to connect to the metadata server should
  // timeout.
  int connect_timeout_;
  // The time after which read from metadata server should timeout.
  int read_timeout_;

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

#endif  // METADATA_CACHE_CLUSTER_METADATA_INCLUDED
