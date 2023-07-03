/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CLUSTER_METADATA_INCLUDED
#define MYSQLROUTER_CLUSTER_METADATA_INCLUDED

#include "mysqlrouter/router_export.h"

#include <stdexcept>
#include <string>

#include "mysql/harness/stdx/expected.h"

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

std::string ROUTER_LIB_EXPORT to_string(const MetadataSchemaVersion &version);

// Semantic version numbers that this Router version supports for bootstrap mode
constexpr MetadataSchemaVersion kRequiredBootstrapSchemaVersion[]{{1, 0, 0},
                                                                  {2, 0, 0}};

// Semantic version number that this Router version supports for routing mode
constexpr MetadataSchemaVersion kRequiredRoutingMetadataSchemaVersion[]{
    {1, 0, 0}, {2, 0, 0}};

// Version that introduced views and support for ReplicaSet cluster type
constexpr MetadataSchemaVersion kNewMetadataVersion{2, 0, 0};

// Version that introduced support for ClusterSets
constexpr MetadataSchemaVersion kClusterSetsMetadataVersion{2, 1, 0};

// Version that will be is set while the metadata is being updated
constexpr MetadataSchemaVersion kUpgradeInProgressMetadataVersion{0, 0, 0};

MetadataSchemaVersion ROUTER_LIB_EXPORT
get_metadata_schema_version(MySQLSession *mysql);

bool ROUTER_LIB_EXPORT metadata_schema_version_is_compatible(
    const mysqlrouter::MetadataSchemaVersion &required,
    const mysqlrouter::MetadataSchemaVersion &available);

// throws std::logic_error, MySQLSession::Error
bool ROUTER_LIB_EXPORT check_group_replication_online(MySQLSession *mysql);

// throws MySQLSession::Error, std::logic_error, std::out_of_range
bool ROUTER_LIB_EXPORT check_group_has_quorum(MySQLSession *mysql);

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
  GR_V1, /* based on Group Replication (metadata 1.x) */
  GR_V2, /* based on Group Replication (metadata 2.x) */
  GR_CS, /* based on Group Replication, part of ClusterSet (metadata 2.1+) */
  RS_V2  /* ReplicaSet (metadata 2.x) */
};

ClusterType ROUTER_LIB_EXPORT
get_cluster_type(const MetadataSchemaVersion &schema_version,
                 MySQLSession *mysql, unsigned int router_id = 0);

std::string ROUTER_LIB_EXPORT to_string(const ClusterType cluster_type);

class MetadataUpgradeInProgressException : public std::exception {};

stdx::expected<void, std::string> ROUTER_LIB_EXPORT
setup_metadata_session(MySQLSession &session);

bool ROUTER_LIB_EXPORT is_part_of_cluster_set(MySQLSession *mysql);

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

}  // namespace mysqlrouter
#endif
