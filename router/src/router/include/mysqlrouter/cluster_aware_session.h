/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CLUSTER_AWARE_SESSION_INCLUDED
#define MYSQLROUTER_CLUSTER_AWARE_SESSION_INCLUDED

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"
#include "mysqlrouter/cluster_metadata.h"
IMPORT_LOG_FUNCTIONS()

/**
 * Error codes for MySQL Errors that we handle specifically
 *
 * @todo extend to other MySQL Error codes that need to be handled specifically
 *       and move into a place where other can access it too
 */
enum class MySQLErrorc {
  kSuperReadOnly = ER_OPTION_PREVENTS_STATEMENT,  // 1290
  kLostConnection = CR_SERVER_LOST,               // 2013
};

/**
 * Cluster (GR or AR)-aware decorator for MySQL Sessions.
 */
class ROUTER_CLUSTER_EXPORT ClusterAwareDecorator {
 public:
  ClusterAwareDecorator(
      mysqlrouter::ClusterMetadata &metadata,
      const std::string &cluster_initial_username,
      const std::string &cluster_initial_password,
      const std::string &cluster_initial_hostname,
      unsigned long cluster_initial_port,
      const std::string &cluster_initial_socket,
      unsigned long connection_timeout,
      std::set<MySQLErrorc> failure_codes = {MySQLErrorc::kSuperReadOnly,
                                             MySQLErrorc::kLostConnection})
      : metadata_(metadata),
        cluster_initial_username_(cluster_initial_username),
        cluster_initial_password_(cluster_initial_password),
        cluster_initial_hostname_(cluster_initial_hostname),
        cluster_initial_port_(cluster_initial_port),
        cluster_initial_socket_(cluster_initial_socket),
        connection_timeout_(connection_timeout),
        failure_codes_(std::move(failure_codes)) {}

  /**
   * Cluster (GR or AR) aware failover.
   *
   * @param wrapped_func function will be called
   *
   * assumes:
   *
   * - actively connected mysql_ session
   * - all nodes in the group have the same user/pass combination
   * - wrapped_func throws MySQLSession::Error with .code in .failure_codes
   */
  template <class R>
  R failover_on_failure(std::function<R()> wrapped_func) {
    bool fetched_cluster_servers = false;
    std::vector<std::tuple<std::string, unsigned long>> cluster_servers;

    auto cluster_servers_it = cluster_servers.begin();
    const auto cluster_specific_initial_id =
        metadata_.get_cluster_type_specific_id();

    bool initial_node = true;
    do {
      bool skip_node = false;
      if (!initial_node) {
        // let's check if the node we failed over to belongs to the same cluster
        // the user is bootstaping against
        const auto cluster_specific_id =
            metadata_.get_cluster_type_specific_id();

        if (cluster_specific_id != cluster_specific_initial_id) {
          log_warning(
              "Node on '%s' that the bootstrap failed over to, seems to belong "
              "to different cluster(%s != %s), skipping...",
              metadata_.get_session().get_address().c_str(),
              cluster_specific_initial_id.c_str(), cluster_specific_id.c_str());
          skip_node = true;
        }
      } else {
        initial_node = false;
      }

      if (!skip_node) {
        try {
          return wrapped_func();
        } catch (const mysqlrouter::MySQLSession::Error &e) {
          MySQLErrorc ec = static_cast<MySQLErrorc>(e.code());

          log_debug(
              "Executing statements failed with: '%s' (%d), trying to connect "
              "to "
              "another node",
              e.what(), e.code());

          // code not in failure-set
          if (failure_codes_.find(ec) == failure_codes_.end()) {
            throw;
          }
        }
      }

      // bootstrap not successful, checking next node to fail over to
      do {
        if (!fetched_cluster_servers) {
          // lazy fetch the GR members
          //
          fetched_cluster_servers = true;

          log_info("Fetching Cluster Members");

          for (auto &cluster_node : metadata_.fetch_cluster_hosts()) {
            auto const &node_host = std::get<0>(cluster_node);
            auto node_port = std::get<1>(cluster_node);

            // if we connected through TCP/IP, ignore the initial host
            if (cluster_initial_socket_.size() == 0 &&
                (node_host == cluster_initial_hostname_ &&
                 node_port == cluster_initial_port_)) {
              continue;
            }

            log_debug("added cluster node: %s:%ld", node_host.c_str(),
                      node_port);
            cluster_servers.emplace_back(node_host, node_port);
          }

          // get a new iterator as the old one is now invalid
          cluster_servers_it = cluster_servers.begin();
        } else {
          std::advance(cluster_servers_it, 1);
        }

        if (cluster_servers_it == cluster_servers.end()) {
          throw std::runtime_error(
              "no more nodes to fail-over too, giving up.");
        }

        if (metadata_.get_session().is_connected()) {
          log_debug("%s", "disconnecting from mysql-server");
          metadata_.get_session().disconnect();
        }

        auto const &tp = *cluster_servers_it;

        auto const &host = std::get<0>(tp);
        auto port = std::get<1>(tp);

        log_info("trying to connect to mysql-server at %s:%ld", host.c_str(),
                 port);

        try {
          connect(metadata_.get_session(), host, port);
        } catch (const std::exception &inner_e) {
          log_info("Failed connecting to %s:%ld: %s, trying next", host.c_str(),
                   port, inner_e.what());
          continue;
        }

        const auto result =
            mysqlrouter::setup_metadata_session(metadata_.get_session());
        if (!result) {
          metadata_.get_session().disconnect();
          log_info(
              "Failed setting up a metadata session %s:%ld: %s, trying next",
              host.c_str(), port, result.error().c_str());
        }

        // if this fails, we should just skip it and go to the next
      } while (!metadata_.get_session().is_connected());
    } while (true);
  }

  virtual ~ClusterAwareDecorator() = default;

 protected:
  void connect(mysqlrouter::MySQLSession &session, const std::string &host,
               const unsigned port);

  mysqlrouter::ClusterMetadata &metadata_;
  const std::string &cluster_initial_username_;
  const std::string &cluster_initial_password_;
  const std::string &cluster_initial_hostname_;
  unsigned long cluster_initial_port_;
  const std::string &cluster_initial_socket_;
  unsigned long connection_timeout_;
  std::set<MySQLErrorc> failure_codes_;
};

#endif  // MYSQLROUTER_CLUSTER_AWARE_SESSION_INCLUDED
