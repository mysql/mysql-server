/*
  Copyright (c) 2016, 2021, Oracle and/or its affiliates.

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

#ifndef METADATA_CACHE_METADATA_INTERFACE_INCLUDED
#define METADATA_CACHE_METADATA_INTERFACE_INCLUDED

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>

#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/mysql_session.h"

#include <map>
#include <string>
#include <vector>

/**
 * The metadata class is used to create a pluggable transport layer
 * from which the metadata is fetched for the metadata cache.
 */
class METADATA_API MetaData {
 public:
  using ReplicaSetsByName =
      std::map<std::string, metadata_cache::ManagedReplicaSet>;
  using JsonAllocator = rapidjson::CrtAllocator;
  using JsonDocument = rapidjson::Document;
  // username as key, password hash and priviliges as value
  using auth_credentials_t =
      std::map<std::string, std::pair<std::string, JsonDocument>>;
  // fetch instances from connected server
  virtual ReplicaSetsByName fetch_instances(
      const std::string &cluster_name,
      const std::string &cluster_type_specific_id) = 0;

  // fetch instances from vector of servers
  virtual ReplicaSetsByName fetch_instances(
      const std::vector<metadata_cache::ManagedInstance> &instances,
      const std::string &cluster_type_specific_id,
      std::size_t &instance_id) = 0;

  virtual bool update_router_version(
      const metadata_cache::ManagedInstance &rw_instance,
      const unsigned router_id) = 0;

  virtual bool update_router_last_check_in(
      const metadata_cache::ManagedInstance &rw_instance,
      const unsigned router_id) = 0;

  virtual bool connect_and_setup_session(
      const metadata_cache::ManagedInstance &metadata_server) = 0;

  virtual void disconnect() = 0;

  virtual void setup_notifications_listener(
      const std::vector<metadata_cache::ManagedInstance> &instances,
      const std::function<void()> &callback) = 0;

  virtual void shutdown_notifications_listener() = 0;

  virtual std::shared_ptr<mysqlrouter::MySQLSession> get_connection() = 0;

  virtual mysqlrouter::ClusterType get_cluster_type() = 0;

  virtual auth_credentials_t fetch_auth_credentials(
      const std::string &cluster_name) = 0;

  MetaData() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit MetaData(const MetaData &) = delete;
  MetaData &operator=(const MetaData &) = delete;
  virtual ~MetaData() {}
};

#endif  // METADATA_CACHE_METADATA_INTERFACE_INCLUDED
