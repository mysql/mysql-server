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
#ifndef MOCK_METADATA_INCLUDED
#define MOCK_METADATA_INCLUDED

#include <vector>

#include "cluster_metadata.h"

#include "tcp_address.h"

#ifdef _WIN32
#ifdef metadata_cache_tests_DEFINE_STATIC
#define METADATA_TESTS_API
#else
#ifdef metadata_cache_tests_EXPORTS
#define METADATA_TESTS_API __declspec(dllexport)
#else
#define METADATA_TESTS_API __declspec(dllimport)
#endif
#endif
#else
#define METADATA_TESTS_API
#endif

/** @class MockNG
 *
 * Used for simulating NG metadata for testing purposes.
 *
 */

class METADATA_TESTS_API MockNG : public ClusterMetadata {
 public:
  /**
   * Objects representing the servers that are part of the topology.
   */
  metadata_cache::ManagedInstance ms1;
  metadata_cache::ManagedInstance ms2;
  metadata_cache::ManagedInstance ms3;
  metadata_cache::ManagedInstance ms4;
  metadata_cache::ManagedInstance ms5;
  metadata_cache::ManagedInstance ms6;
  metadata_cache::ManagedInstance ms7;
  metadata_cache::ManagedInstance ms8;
  metadata_cache::ManagedInstance ms9;

  /**
   * Server list for each replicaset in the topology. Each server object
   * represents all relevant information about the server that is
   * part of the topology.
   */
  std::vector<metadata_cache::ManagedInstance> replicaset_1_vector;
  std::vector<metadata_cache::ManagedInstance> replicaset_2_vector;
  std::vector<metadata_cache::ManagedInstance> replicaset_3_vector;

  /**
   * The information about the HA topology being managed.
   */
  ReplicaSetsByName replicaset_map;

  /** @brief Constructor
   * @param user The user name used to authenticate to the metadata server.
   * @param password The password used to authenticate to the metadata server.
   * @param connect_timeout The time after which trying to connect to the
   *                        metadata server should timeout.
   * @param read_timeout The time after which read from the metadata server
   *                     should timeout.
   * @param connection_attempts The number of times a connection to the metadata
   *                            server must be attempted, when a connection
   *                            attempt fails.
   * @param ttl The time to live of the data in the cache.
   */
  MockNG(
      const std::string &user, const std::string &password, int connect_timeout,
      int read_timeout, int connection_attempts, std::chrono::milliseconds ttl,
      const mysqlrouter::SSLOptions &ssl_options = mysqlrouter::SSLOptions());

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  virtual ~MockNG();

  /** @brief Mock connect method.
   *
   * Mock connect method, does nothing.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect(
      const metadata_cache::ManagedInstance &metadata_server) noexcept override;

  /** @brief Mock disconnect method.
   *
   * Mock method, does nothing.
   *
   */
  void disconnect() noexcept override;

  /**
   *
   * Returns relation as a std::map between replicaset ID and list of managed
   * servers.
   *
   * @return Map of replicaset ID, server list pairs.
   */
  ReplicaSetsByName fetch_instances(const std::string &farm_name) override;

#if 0  // not used so far
  /**
   *
   * Returns a mock refresh interval.
   *
   * @return refresh interval of the Metadata cache.
   */
  unsigned int fetch_ttl() override;
#endif
};

#endif  // MOCK_METADATA_INCLUDED
