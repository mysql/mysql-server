/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <stdexcept>
#include <string>

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

  bool operator==(const MetadataSchemaVersion &o) const {
    return major == o.major && minor == o.minor && patch == o.patch;
  }

  bool operator!=(const MetadataSchemaVersion &o) const {
    return !operator==(o);
  }
};

// Semantic version number that this Router version supports for bootstrap mode
constexpr MetadataSchemaVersion kRequiredBootstrapSchemaVersion{2, 0, 0};

// Semantic version number that this Router version supports for routing mode
constexpr MetadataSchemaVersion kRequiredRoutingMetadataSchemaVersion{1, 0, 0};

// Version that introduced views and support for Async Replicaset cluster type
constexpr MetadataSchemaVersion kNewMetadataVersion{2, 0, 0};

// Version that will be is set while the metadata is being updated
constexpr MetadataSchemaVersion kUpdateInProgressMetadataVersion{0, 0, 0};

MetadataSchemaVersion get_metadata_schema_version(MySQLSession *mysql);

bool metadata_schema_version_is_compatible(
    const mysqlrouter::MetadataSchemaVersion &required,
    const mysqlrouter::MetadataSchemaVersion &available);

enum class ClusterType {
  GR_V1, /* based on Group Replication (metadata 1.x) */
  GR_V2, /* based on Group Replication (metadata 2.x) */
  AR_V2  /* Async Replicaset (metadata 2.x) */
};

ClusterType get_cluster_type(const MetadataSchemaVersion &schema_version,
                             MySQLSession *mysql);

std::string to_string(const ClusterType cluster_type);

class UpdateInProgressException : public std::exception {};

}  // namespace mysqlrouter
#endif
