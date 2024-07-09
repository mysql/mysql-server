/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CLUSTER_METADATA_INCLUDED
#define MYSQLROUTER_CLUSTER_METADATA_INCLUDED

#include "mysqlrouter/router_cluster_export.h"

#include <chrono>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/mysql_session.h"
#include "socket_operations.h"

namespace mysqlrouter {

class MySQLSession;

struct MetadataSchemaVersion {
  unsigned int major;
  unsigned int minor;
  unsigned int patch;

  bool operator<(const MetadataSchemaVersion &o) const {
    if (major == o.major) {
      if (minor == o.minor) {
        return patch < o.patch;
      } else {
        return minor < o.minor;
      }
    } else {
      return major < o.major;
    }
  }

  bool operator<=(const MetadataSchemaVersion &o) const {
    return operator<(o) || operator==(o);
  }

  bool operator>(const MetadataSchemaVersion &o) const {
    return operator!=(o) && !operator<(o);
  }

  bool operator>=(const MetadataSchemaVersion &o) const {
    return operator>(o) || operator==(o);
  }

  bool operator==(const MetadataSchemaVersion &o) const {
    return major == o.major && minor == o.minor && patch == o.patch;
  }

  bool operator!=(const MetadataSchemaVersion &o) const {
    return !operator==(o);
  }
};

std::string ROUTER_CLUSTER_EXPORT
to_string(const MetadataSchemaVersion &version);

// Semantic version numbers that this Router version supports for bootstrap mode
constexpr MetadataSchemaVersion kRequiredBootstrapSchemaVersion[]{{2, 0, 0}};

// Semantic version number that this Router version supports for routing mode
constexpr MetadataSchemaVersion kRequiredRoutingMetadataSchemaVersion[]{
    {2, 0, 0}};

// Version that introduced views and support for ReplicaSet cluster type
constexpr MetadataSchemaVersion kNewMetadataVersion{2, 0, 0};

// Version that introduced support for ClusterSets
constexpr MetadataSchemaVersion kClusterSetsMetadataVersion{2, 1, 0};

// Version that will be is set while the metadata is being updated
constexpr MetadataSchemaVersion kUpgradeInProgressMetadataVersion{0, 0, 0};

MetadataSchemaVersion ROUTER_CLUSTER_EXPORT
get_metadata_schema_version(MySQLSession *mysql);

bool ROUTER_CLUSTER_EXPORT metadata_schema_version_is_compatible(
    const mysqlrouter::MetadataSchemaVersion &required,
    const mysqlrouter::MetadataSchemaVersion &available);

std::string ROUTER_CLUSTER_EXPORT get_metadata_schema_uncompatible_msg(
    const mysqlrouter::MetadataSchemaVersion &version);

// throws std::logic_error, MySQLSession::Error
bool ROUTER_CLUSTER_EXPORT check_group_replication_online(MySQLSession *mysql);

// throws MySQLSession::Error, std::logic_error, std::out_of_range
bool ROUTER_CLUSTER_EXPORT check_group_has_quorum(MySQLSession *mysql);

template <size_t N>
bool metadata_schema_version_is_compatible(
    const mysqlrouter::MetadataSchemaVersion (&required)[N],
    const mysqlrouter::MetadataSchemaVersion &available) {
  for (size_t i = 0; i < N; ++i) {
    if (metadata_schema_version_is_compatible(required[i], available))
      return true;
  }

  return false;
}

template <size_t N>
std::string to_string(const mysqlrouter::MetadataSchemaVersion (&version)[N]) {
  std::string result;
  for (size_t i = 0; i < N; ++i) {
    result += to_string(version[i]);
    if (i != N - 1) {
      result += ", ";
    }
  }

  return result;
}

enum class ClusterType {
  GR_V2, /* based on Group Replication (metadata 2.x) */
  GR_CS, /* based on Group Replication, part of ClusterSet (metadata 2.1+) */
  RS_V2  /* ReplicaSet (metadata 2.x) */
};

ClusterType ROUTER_CLUSTER_EXPORT
get_cluster_type(const MetadataSchemaVersion &schema_version,
                 MySQLSession *mysql, unsigned int router_id = 0);

std::string ROUTER_CLUSTER_EXPORT to_string(const ClusterType cluster_type);

class MetadataUpgradeInProgressException : public std::exception {};

stdx::expected<void, std::string> ROUTER_CLUSTER_EXPORT
setup_metadata_session(MySQLSession &session);

bool ROUTER_CLUSTER_EXPORT is_part_of_cluster_set(MySQLSession *mysql);

class TargetCluster {
 public:
  enum class TargetType { ByUUID, ByName, ByPrimaryRole };
  enum class InvalidatedClusterRoutingPolicy { DropAll, AcceptRO };

  TargetCluster(const TargetType type = TargetType::ByPrimaryRole,
                const std::string &value = "")
      : target_type_(type), target_value_(value) {
    if (target_type_ == TargetType::ByPrimaryRole) target_value_ = "PRIMARY";
  }

  std::string to_string() const { return target_value_; }
  const char *c_str() const { return target_value_.c_str(); }

  TargetType target_type() const { return target_type_; }
  InvalidatedClusterRoutingPolicy invalidated_cluster_routing_policy() const {
    return invalidated_cluster_routing_policy_;
  }

  void target_type(const TargetType value) { target_type_ = value; }
  void target_value(const std::string &value) { target_value_ = value; }
  void invalidated_cluster_routing_policy(
      const InvalidatedClusterRoutingPolicy value) {
    invalidated_cluster_routing_policy_ = value;
  }

 private:
  TargetType target_type_;
  std::string target_value_;
  InvalidatedClusterRoutingPolicy invalidated_cluster_routing_policy_{
      InvalidatedClusterRoutingPolicy::DropAll};
};

constexpr const std::string_view kNodeTagHidden{"_hidden"};
constexpr const std::string_view kNodeTagDisconnectWhenHidden{
    "_disconnect_existing_sessions_when_hidden"};

constexpr const bool kNodeTagHiddenDefault{false};
constexpr const bool kNodeTagDisconnectWhenHiddenDefault{true};

enum class InstanceType { GroupMember, AsyncMember, ReadReplica, Unsupported };

std::optional<InstanceType> ROUTER_CLUSTER_EXPORT
str_to_instance_type(const std::string &);

std::string ROUTER_CLUSTER_EXPORT to_string(const InstanceType);
std::string ROUTER_CLUSTER_EXPORT
to_string(const TargetCluster::InvalidatedClusterRoutingPolicy);

constexpr const std::chrono::milliseconds kDefaultMetadataTTLCluster{500};
constexpr const std::chrono::milliseconds
    kDefaultMetadataTTLClusterGRNotificationsON =
        std::chrono::milliseconds(60 * 1000);
constexpr const std::chrono::milliseconds kDefaultMetadataTTLClusterSet{
    5000};  // default TTL for ClusterSet is 5 seconds regardless if GR
            // Notifications are used or not
const bool kDefaultUseGRNotificationsCluster = false;
const bool kDefaultUseGRNotificationsClusterSet = true;

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

class metadata_missing : public std::runtime_error {
 public:
  explicit metadata_missing(const std::string &msg) : std::runtime_error(msg) {}
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

  virtual void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_split_endpoint,
      const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
      const std::string &username) = 0;

  virtual std::vector<std::string> get_routing_mode_queries() = 0;

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

  virtual InstanceType fetch_current_instance_type() = 0;

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

  std::vector<std::string> get_routing_mode_queries() override;

  InstanceType fetch_current_instance_type() override;

  void verify_router_id_is_ours(
      uint32_t router_id, const std::string &hostname_override = "") override;

  void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_split_endpoint,
      const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
      const std::string &username) override;

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
      const std::string &ro_endpoint, const std::string &rw_split_endpoint,
      const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
      const std::string &username) override;

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

  InstanceType fetch_current_instance_type() override {
    return InstanceType::AsyncMember;
  }

  std::string get_cluster_type_specific_id() override;

  uint64_t get_view_id(
      const std::string & /*cluster_type_specific_id*/) override;

  std::vector<std::string> get_routing_mode_queries() override;

  void verify_router_id_is_ours(
      uint32_t router_id, const std::string &hostname_override = "") override;

  void update_router_info(
      const uint32_t router_id, const std::string &cluster_id,
      const std::string &target_cluster, const std::string &rw_endpoint,
      const std::string &ro_endpoint, const std::string &rw_split_endpoint,
      const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
      const std::string &username) override;

  uint32_t register_router(const std::string &router_name, const bool overwrite,
                           const std::string &hostname_override = "") override;

  std::vector<std::string> get_grant_statements(
      const std::string &new_accounts) const override;

  std::vector<std::tuple<std::string, unsigned long>> fetch_cluster_hosts()
      override;

 protected:
  uint64_t query_cluster_count() override;
};

std::unique_ptr<ClusterMetadata> ROUTER_CLUSTER_EXPORT
create_metadata(const MetadataSchemaVersion &schema_version,
                MySQLSession *mysql, const OptionsMap &options = {},
                mysql_harness::SocketOperationsBase *sockops =
                    mysql_harness::SocketOperations::instance());

}  // namespace mysqlrouter
#endif
