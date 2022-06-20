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

#ifndef ROUTER_CLUSTER_METADATA_INCLUDED
#define ROUTER_CLUSTER_METADATA_INCLUDED

#include "mysqlrouter/router_export.h"

#include <stdexcept>

#include "config_generator.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "socket_operations.h"

namespace mysqlrouter {

struct ClusterInfo {
  std::vector<std::string> metadata_servers;
  std::string cluster_id;
  // GR name for GR cluster
  std::string cluster_type_specific_id;
  // name of the cluster (or clusterset in case of the clusterset)
  std::string name;
  // whether this cluster is a primary cluster in case it is a member of a
  // ClusterSet
  bool is_primary{false};

  std::string get_cluster_type_specific_id() const {
    return cluster_type_specific_id.empty() ? cluster_id
                                            : cluster_type_specific_id;
  }
};

using OptionsMap = std::map<std::string, std::string>;

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

  virtual void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_x_endpoint,
      const std::string &ro_x_endpoint, const std::string &username) = 0;

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
   * checks that the server
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

  virtual uint64_t get_view_id(
      const std::string & /*cluster_type_specific_id*/) {
    return 0;
  }

 protected:
  // throws MySQLSession::Error, std::out_of_range, std::logic_error
  virtual uint64_t query_cluster_count() = 0;

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

  ~ClusterMetadataGR() override = default;

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

  ~ClusterMetadataGRV1() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::GR_V1;
  }

  ClusterInfo fetch_metadata_servers() override;

  std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) override;

  void verify_router_id_is_ours(
      const uint32_t router_id,
      const std::string &hostname_override = "") override;

  void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_x_endpoint,
      const std::string &ro_x_endpoint, const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

 protected:
  uint64_t query_cluster_count() override;
};

class ClusterMetadataGRV2 : public ClusterMetadataGR {
 public:
  ClusterMetadataGRV2(const MetadataSchemaVersion &schema_version,
                      MySQLSession *mysql,
                      mysql_harness::SocketOperationsBase *sockops =
                          mysql_harness::SocketOperations::instance())
      : ClusterMetadataGR(schema_version, mysql, sockops) {}

  ~ClusterMetadataGRV2() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::GR_V2;
  }

  ClusterInfo fetch_metadata_servers() override;

  std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) override;

  void verify_router_id_is_ours(
      uint32_t router_id, const std::string &hostname_override = "") override;

  void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_x_endpoint,
      const std::string &ro_x_endpoint, const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

 protected:
  uint64_t query_cluster_count() override;
};

class ClusterMetadataGRInClusterSet : public ClusterMetadataGRV2 {
 public:
  ClusterMetadataGRInClusterSet(
      const MetadataSchemaVersion &schema_version, MySQLSession *mysql,
      const OptionsMap & /*options*/,
      mysql_harness::SocketOperationsBase *sockops =
          mysql_harness::SocketOperations::instance());

  ~ClusterMetadataGRInClusterSet() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::GR_CS;
  }

  // nothing specific to check for ClusterSet
  void require_metadata_is_ok() override {}

  ClusterInfo fetch_metadata_servers() override;

  std::vector<std::tuple<std::string, unsigned long>> fetch_cluster_hosts()
      override;

  enum class TargetClusterType {
    // target should be the cluster on which we bootstrap
    targetClusterCurrent,
    // target should be the Priamry Cluster
    targetClusterPrimary,
    // target should be the Cluster with the given name
    targetClusterByName
  };

  std::string get_cluster_type_specific_id() override;
  uint64_t get_view_id(const std::string &cluster_type_specific_id) override;

  void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_x_endpoint,
      const std::string &ro_x_endpoint, const std::string &username) override;

 protected:
  TargetClusterType target_cluster_type_;
  std::string target_cluster_name_;
};

class ClusterMetadataAR : public ClusterMetadata {
 public:
  ClusterMetadataAR(const MetadataSchemaVersion &schema_version,
                    MySQLSession *mysql,
                    mysql_harness::SocketOperationsBase *sockops =
                        mysql_harness::SocketOperations::instance())
      : ClusterMetadata(schema_version, mysql, sockops) {}

  ~ClusterMetadataAR() override = default;

  mysqlrouter::ClusterType get_type() override {
    return mysqlrouter::ClusterType::RS_V2;
  }

  void require_cluster_is_ok() override {
    // Nothing specific to check for ReplicaSet cluster
  }

  ClusterInfo fetch_metadata_servers() override;

  std::string get_cluster_type_specific_id() override;

  uint64_t get_view_id(
      const std::string & /*cluster_type_specific_id*/) override;

  std::vector<std::string> get_routing_mode_queries(
      const std::string &cluster_name) override;

  void verify_router_id_is_ours(
      uint32_t router_id, const std::string &hostname_override = "") override;

  void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_x_endpoint,
      const std::string &ro_x_endpoint, const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

  std::vector<std::tuple<std::string, unsigned long>> fetch_cluster_hosts()
      override;

 protected:
  uint64_t query_cluster_count() override;
};

std::unique_ptr<ClusterMetadata> ROUTER_LIB_EXPORT
create_metadata(const MetadataSchemaVersion &schema_version,
                MySQLSession *mysql, const OptionsMap &options = {},
                mysql_harness::SocketOperationsBase *sockops =
                    mysql_harness::SocketOperations::instance());

}  // namespace mysqlrouter

#endif  // ROUTER_CLUSTER_METADATA_INCLUDED
