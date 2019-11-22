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

#ifndef ROUTER_CLUSTER_METADATA_INCLUDED
#define ROUTER_CLUSTER_METADATA_INCLUDED

#include <stdexcept>

#include "config_generator.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "socket_operations.h"

namespace mysqlrouter {

struct ClusterInfo {
  std::vector<std::string> metadata_servers;
  std::string metadata_cluster_id;
  std::string metadata_cluster_name;
  std::string metadata_replicaset;
};

class ClusterMetadata {
 public:
  ClusterMetadata(const MetadataSchemaVersion &schema_version,
                  MySQLSession *mysql,
                  mysql_harness::SocketOperationsBase *sockops =
                      mysql_harness::SocketOperations::instance())
      : mysql_(mysql),
        socket_operations_(sockops),
        schema_version_(schema_version) {}

  virtual ~ClusterMetadata() = default;

  virtual mysqlrouter::ClusterType get_type() = 0;

  /** @brief Checks if Router with given id is already registered in metadata
   *         database, and belongs to our machine
   *
   * @param router_id Router id
   * @param hostname_override If non-empty, this hostname will be used instead
   *        of getting queried from OS
   *
   * @throws LocalHostnameResolutionError(std::runtime_error) on hostname query
   *         failure
   * @throws std::runtime_error if router_id doesn't exist, or is associated
   *         with a different host
   * @throws MySQLSession::Error(std::runtime_error) on database error
   */
  virtual void verify_router_id_is_ours(
      const uint32_t router_id, const std::string &hostname_override = "") = 0;

  /** @brief Registers Router in metadata database
   *
   * @param router_name Router name
   * @param overwrite if Router name is already registered, allow this
   * registration to be "hijacked" instead of throwing
   * @param hostname_override If non-empty, this hostname will be used instead
   *        of getting queried from OS
   *
   * @returns newly-assigned router_id
   *
   * @throws LocalHostnameResolutionError(std::runtime_error) on hostname query
   *         failure, std::runtime_error on other failure
   */
  virtual uint32_t register_router(
      const std::string &router_name, const bool overwrite,
      const std::string &hostname_override = "") = 0;

  virtual void update_router_info(const uint32_t router_id,
                                  const std::string &cluster_id,
                                  const std::string &rw_endpoint,
                                  const std::string &ro_endpoint,
                                  const std::string &rw_x_endpoint,
                                  const std::string &ro_x_endpoint,
                                  const std::string &username) = 0;

  virtual std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) = 0;

  /** @brief Verify that host is a valid metadata server
   *
   *
   * @throws MySQLSession::Error
   * @throws std::runtime_error
   * @throws std::out_of_range
   * @throws std::logic_error
   *
   *  * checks that the server
   *
   * - has the metadata in the correct version
   * - contains metadata for the group it's in (in case of GR cluster)
   *   (metadata server group must be same as managed group currently)
   */
  virtual void require_metadata_is_ok();

  /** @brief Verify that host is a valid cluster member (either Group
   * Replication or ReplicaSet cluster)
   *
   * @throws MySQLSession::Error
   * @throws std::runtime_error
   * @throws std::out_of_range
   * @throws std::logic_error
   */
  virtual void require_cluster_is_ok() = 0;

  virtual std::string get_cluster_type_specific_id() = 0;

  virtual ClusterInfo fetch_metadata_servers() = 0;

  virtual std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const = 0;

  virtual std::vector<std::tuple<std::string, unsigned long>>
  fetch_cluster_hosts() = 0;

  MySQLSession &get_session() { return *mysql_; }

 protected:
  // throws MySQLSession::Error, std::out_of_range, std::logic_error
  virtual bool check_metadata_is_supported() = 0;

  MySQLSession *mysql_;
  mysql_harness::SocketOperationsBase *socket_operations_;
  mysqlrouter::MetadataSchemaVersion schema_version_;
};

class ClusterMetadataGR : public ClusterMetadata {
 public:
  ClusterMetadataGR(const MetadataSchemaVersion &schema_version,
                    MySQLSession *mysql,
                    mysql_harness::SocketOperationsBase *sockops =
                        mysql_harness::SocketOperations::instance())
      : ClusterMetadata(schema_version, mysql, sockops) {}

  virtual ~ClusterMetadataGR() override = default;

  // For GR cluster Group Replication ID
  std::string get_cluster_type_specific_id() override;

  void require_cluster_is_ok() override;

  std::vector<std::tuple<std::string, unsigned long>> fetch_cluster_hosts()
      override;
};

class ClusterMetadataGRV1 : public ClusterMetadataGR {
 public:
  ClusterMetadataGRV1(const MetadataSchemaVersion &schema_version,
                      MySQLSession *mysql,
                      mysql_harness::SocketOperationsBase *sockops =
                          mysql_harness::SocketOperations::instance())
      : ClusterMetadataGR(schema_version, mysql, sockops) {}

  virtual ~ClusterMetadataGRV1() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::GR_V1;
  }

  ClusterInfo fetch_metadata_servers() override;

  std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) override;

  void verify_router_id_is_ours(
      const uint32_t router_id,
      const std::string &hostname_override = "") override;

  void update_router_info(const uint32_t router_id,
                          const std::string &cluster_id,
                          const std::string &rw_endpoint,
                          const std::string &ro_endpoint,
                          const std::string &rw_x_endpoint,
                          const std::string &ro_x_endpoint,
                          const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

 protected:
  bool check_metadata_is_supported() override;
};

class ClusterMetadataGRV2 : public ClusterMetadataGR {
 public:
  ClusterMetadataGRV2(const MetadataSchemaVersion &schema_version,
                      MySQLSession *mysql,
                      mysql_harness::SocketOperationsBase *sockops =
                          mysql_harness::SocketOperations::instance())
      : ClusterMetadataGR(schema_version, mysql, sockops) {}

  virtual ~ClusterMetadataGRV2() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::GR_V2;
  }

  ClusterInfo fetch_metadata_servers() override;

  std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) override;

  void verify_router_id_is_ours(
      uint32_t router_id, const std::string &hostname_override = "") override;

  void update_router_info(const uint32_t router_id,
                          const std::string &cluster_id,
                          const std::string &rw_endpoint,
                          const std::string &ro_endpoint,
                          const std::string &rw_x_endpoint,
                          const std::string &ro_x_endpoint,
                          const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

 protected:
  bool check_metadata_is_supported() override;
};

class ClusterMetadataAR : public ClusterMetadata {
 public:
  ClusterMetadataAR(const MetadataSchemaVersion &schema_version,
                    MySQLSession *mysql,
                    mysql_harness::SocketOperationsBase *sockops =
                        mysql_harness::SocketOperations::instance())
      : ClusterMetadata(schema_version, mysql, sockops) {}

  virtual ~ClusterMetadataAR() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::RS_V2;
  }

  void require_cluster_is_ok() override {
    // Nothing specific to check for ReplicaSet cluster
  }

  ClusterInfo fetch_metadata_servers() override;

  std::string get_cluster_type_specific_id() override;

  unsigned int get_view_id();

  std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) override;

  void verify_router_id_is_ours(
      uint32_t router_id, const std::string &hostname_override = "") override;

  void update_router_info(const uint32_t router_id,
                          const std::string &cluster_id,
                          const std::string &rw_endpoint,
                          const std::string &ro_endpoint,
                          const std::string &rw_x_endpoint,
                          const std::string &ro_x_endpoint,
                          const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

  std::vector<std::tuple<std::string, unsigned long>> fetch_cluster_hosts()
      override;

 protected:
  bool check_metadata_is_supported() override;
};

MetadataSchemaVersion get_metadata_schema_version(MySQLSession *mysql);

std::unique_ptr<ClusterMetadata> create_metadata(
    const MetadataSchemaVersion &schema_version, MySQLSession *mysql,
    mysql_harness::SocketOperationsBase *sockops =
        mysql_harness::SocketOperations::instance());

}  // namespace mysqlrouter

#endif  // ROUTER_CLUSTER_METADATA_INCLUDED
