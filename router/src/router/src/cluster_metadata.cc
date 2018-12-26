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

#include "cluster_metadata.h"
#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/utils_sqlstring.h"
IMPORT_LOG_FUNCTIONS()

#include <string.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

using mysqlrouter::MySQLInnoDBClusterMetadata;
using mysqlrouter::MySQLSession;
using mysqlrouter::sqlstring;
using mysqlrouter::strtoi_checked;
using mysqlrouter::strtoui_checked;

bool mysqlrouter::metadata_schema_version_is_compatible(
    const mysqlrouter::MetadataSchemaVersion &required,
    const mysqlrouter::MetadataSchemaVersion &available) {
  // incompatible metadata
  if (available.major != required.major ||
      // metadata missing stuff we need
      (available.minor < required.minor) ||
      // metadata missing bugfixes we're expecting
      (available.minor == required.minor && available.patch < required.patch)) {
    return false;
  }
  return true;
}

mysqlrouter::MetadataSchemaVersion mysqlrouter::get_metadata_schema_version(
    MySQLSession *mysql) {
  std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(
      "SELECT * FROM mysql_innodb_cluster_metadata.schema_version"));
  if (!result) {
    throw std::runtime_error("Invalid MySQL InnoDB cluster metadata");
  }

  size_t result_size = result->size();
  if (result_size != 3 && result_size != 2) {
    throw std::out_of_range(
        "Invalid number of values returned from "
        "mysql_innodb_cluster_metadata.schema_version: "
        "expected 2 or 3, got " +
        std::to_string(result_size));
  }
  auto major = strtoui_checked((*result)[0]);
  auto minor = strtoui_checked((*result)[1]);

  // Initially shell used to create version number with 2 digits only (1.0)
  // It has since moved to 3 digit numbers. We normalize it to 1.0.0 here for
  // simplicity and backwards compatibility.
  auto patch = result_size == 3 ? strtoui_checked((*result)[2]) : 0;

  return {major, minor, patch};
}

// TODO std::logic_error probably should be replaced by std::runtime_error
// throws std::logic_error, MySQLSession::Error
static bool check_group_replication_online(MySQLSession *mysql) {
  std::string q =
      "SELECT member_state"
      " FROM performance_schema.replication_group_members"
      " WHERE member_id = @@server_uuid";
  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql->query_one(q));  // throws MySQLSession::Error
  if (result && (*result)[0]) {
    if (strcmp((*result)[0], "ONLINE") == 0) return true;
    // log_warning("Member state for current server is %s", (*result)[0]);
    return false;
  }
  throw std::logic_error("No result returned for metadata query");
}

// throws MySQLSession::Error, std::logic_error, std::out_of_range
static bool check_group_has_quorum(MySQLSession *mysql) {
  std::string q =
      "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) "
      "as num_total"
      " FROM performance_schema.replication_group_members";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql->query_one(q));  // throws MySQLSession::Error
  if (result) {
    if (result->size() != 2) {
      throw std::out_of_range(
          "Invalid number of values returned from "
          "performance_schema.replication_group_members: "
          "expected 2 got " +
          std::to_string(result->size()));
    }
    int online = strtoi_checked((*result)[0]);
    int all = strtoi_checked((*result)[1]);
    // log_info("%d members online out of %d", online, all);
    if (online >= all / 2 + 1) return true;
    return false;
  }

  throw std::logic_error("No result returned for metadata query");
}

// TODO: std::out_of_range/std::logic_error should probably get replaced by
// std::runtime_error equivalents throws MySQLSession::Error, std::out_of_range,
// std::logic_error
static bool check_metadata_is_supported(
    MySQLSession *mysql, const mysqlrouter::MetadataSchemaVersion &version) {
  // check if there's only 1 cluster and 1 replicaset and that this member
  // is in that replicaset
  std::string q =
      "SELECT "
      " ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1"
      "  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) "
      "<= 1) as has_one_replicaset,"
      " (SELECT attributes->>'$.group_replication_group_name' FROM "
      "mysql_innodb_cluster_metadata.replicasets)"
      "  = @@group_replication_group_name as replicaset_is_ours";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql->query_one(q));  // throws MySQLSession::Error

  if (result) {
    if (result->size() != 2) {
      throw std::out_of_range(
          "Invalid number of values returned from query for metadata support: "
          "expected 2 got " +
          std::to_string(result->size()));
    }
    bool has_only_one_replicaset = strtoi_checked((*result)[0]) == 1;
    bool replicaset_is_ours = true;
    if (metadata_schema_version_is_compatible({1, 0, 1}, version))
      replicaset_is_ours = strtoi_checked((*result)[1]) == 1;

    // log_info("Replicaset/Cluster is unique = %i, Replicaset is our own = %i",
    //           has_only_one_replicaset, replicaset_is_ours);
    return has_only_one_replicaset && replicaset_is_ours;
  }
  throw std::logic_error("No result returned for metadata query");
}

/**
 * ensure MySQL Server we are connected to valid for InnoDB Cluster operators
 *
 * @throws std::runtime_error
 * @throws MySQLSession::Error
 * @throws std::out_of_range
 * @throws std::logic_error
 *
 * checks that the server
 *
 * - has the metadata in the correct version
 * - contains metadata for the group it's in
 *   (metadata server group must be same as managed group currently)
 */
void mysqlrouter::require_innodb_metadata_is_ok(MySQLSession *mysql) {
  try {
    MetadataSchemaVersion mdversion = get_metadata_schema_version(mysql);

    if (!metadata_schema_version_is_compatible(required_metadata_schema_version,
                                               mdversion)) {
      throw std::runtime_error(
          "This version of MySQL Router is not compatible with the provided "
          "MySQL InnoDB cluster metadata.");
    }

    if (!check_metadata_is_supported(
            mysql, mdversion)) {  // throws MySQLSession::Error,
                                  // std::out_of_range, std::logic_error
      throw std::runtime_error(
          "The provided server contains an unsupported InnoDB cluster "
          "metadata.");
    }
  } catch (const MySQLSession::Error &e) {
    /*
     * If the metadata schema is missing:
     * - MySQL server before version 8.0 returns error: Table
     * 'mysql_innodb_cluster_metadata.schema_version' doesn't exist (1146)
     * - MySQL server version 8.0 returns error: Unknown database
     * 'mysql_innodb_cluster_metadata' (1049). We handle both codes the same way
     * here.
     */
    if (e.code() == 1146 || e.code() == 1049) {
      // unknown database mysql_innodb_cluster_metata
      throw std::runtime_error(
          std::string("Expected MySQL Server '") + mysql->get_address() +
          "' to contain the metadata of MySQL InnoDB Cluster, but the schema "
          "does not exist.\n" +
          "Checking version of the metadata schema failed with: " + e.what() +
          "\n\n" +
          "See "
          "https://dev.mysql.com/doc/refman/en/"
          "mysql-innodb-cluster-creating.html for instructions on setting up a "
          "MySQL Server to act as an InnoDB Cluster Metadata server\n");
    } else if (e.code() == 1193) {
      // Unknown system variable 'group_replication_group_name' (1193) means
      // that group replication is not configured
      throw std::runtime_error(
          std::string("Expected MySQL Server '") + mysql->get_address() +
          "' to have Group Replication running.\n" +
          "Checking metadata state failed with: " + e.what() + "\n\n" +
          "See "
          "https://dev.mysql.com/doc/refman/en/"
          "mysql-innodb-cluster-creating.html for instructions on setting up a "
          "MySQL Server to act as an InnoDB Cluster Metadata server\n");
    } else {
      throw;
    }
  }
}

/**
 * ensure MySQL Server we are connected to has Group Replication ready
 *
 * @throws MySQLSession::Error
 * @throws std::runtime_error
 * @throws std::out_of_range
 * @throws std::logic_error
 *
 * check that the server we're bootstrapping from
 *
 * - has GR enabled
 * - has quorum
 */
void mysqlrouter::require_innodb_group_replication_is_ok(MySQLSession *mysql) {
  if (!check_group_replication_online(
          mysql)) {  // throws std::logic_error, MySQLSession::Error
    throw std::runtime_error(
        "The provided server is currently not an ONLINE member of a InnoDB "
        "cluster.");
  }

  if (!check_group_has_quorum(mysql)) {  // throws MySQLSession::Error,
                                         // std::logic_error, std::out_of_range
    throw std::runtime_error(
        "The provided server is currently not in a InnoDB cluster group with "
        "quorum and thus may contain inaccurate or outdated data.");
  }
}

void MySQLInnoDBClusterMetadata::check_router_id(
    uint32_t router_id, const std::string &hostname_override) {
  // query metadata for this router_id
  sqlstring query(
      "SELECT h.host_id, h.host_name"
      " FROM mysql_innodb_cluster_metadata.routers r"
      " JOIN mysql_innodb_cluster_metadata.hosts h"
      "    ON r.host_id = h.host_id"
      " WHERE r.router_id = ?");
  query << router_id << sqlstring::end;
  std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
  if (!row) {
    // log_warning("router_id %u not in metadata", router_id);
    throw std::runtime_error("router_id " + std::to_string(router_id) +
                             " not found in metadata");
  }

  // get_local_hostname() throws LocalHostnameResolutionError
  // (std::runtime_error)
  std::string hostname = hostname_override.empty()
                             ? socket_operations_->get_local_hostname()
                             : hostname_override;

  if ((*row)[1] && strcasecmp((*row)[1], hostname.c_str()) == 0) {
    return;
  }
  // log_warning("router_id %u maps to an instance at hostname %s, while this
  // hostname is %s",
  //                router_id, row[1], hostname.c_str());

  // if the host doesn't match, we force a new router_id to be generated
  throw std::runtime_error("router_id " + std::to_string(router_id) +
                           " is associated with a different host ('" +
                           (*row)[1] + "' vs '" + hostname + "')");
}

void MySQLInnoDBClusterMetadata::update_router_info(
    uint32_t router_id, const std::string &rw_endpoint,
    const std::string &ro_endpoint, const std::string &rw_x_endpoint,
    const std::string &ro_x_endpoint) {
  sqlstring query(
      "UPDATE mysql_innodb_cluster_metadata.routers"
      " SET attributes = "
      "   JSON_SET(JSON_SET(JSON_SET(JSON_SET(attributes,"
      "    'RWEndpoint', ?),"
      "    'ROEndpoint', ?),"
      "    'RWXEndpoint', ?),"
      "    'ROXEndpoint', ?)"
      " WHERE router_id = ?");
  query << rw_endpoint;
  query << ro_endpoint;
  query << rw_x_endpoint;
  query << ro_x_endpoint;
  query << router_id;
  query << sqlstring::end;

  mysql_->execute(query);
}

uint32_t MySQLInnoDBClusterMetadata::register_router(
    const std::string &router_name, bool overwrite,
    const std::string &hostname_override) {
  uint32_t host_id;

  // get_local_hostname() throws LocalHostnameResolutionError
  // (std::runtime_error)
  std::string hostname = hostname_override.empty()
                             ? socket_operations_->get_local_hostname()
                             : hostname_override;

  // check if the host already exists in the metadata schema and if so, get
  // our host_id.. if it doesn't, insert it and get the host_id
  sqlstring query(
      "SELECT host_id, host_name, ip_address"
      " FROM mysql_innodb_cluster_metadata.hosts"
      " WHERE host_name = ?"
      " LIMIT 1");
  query << hostname << sqlstring::end;
  {
    std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
    if (!row) {
      // host is not known to the metadata, register it
      query = sqlstring(
          "INSERT INTO mysql_innodb_cluster_metadata.hosts"
          "        (host_name, location, attributes)"
          " VALUES (?, '', "
          "         JSON_OBJECT('registeredFrom', 'mysql-router'))");
      query << hostname << sqlstring::end;
      mysql_->execute(query);
      host_id = static_cast<uint32_t>(mysql_->last_insert_id());
      // log_info("host_id for local host '%s' newly registered as '%u'",
      //        hostname.c_str(), host_id);
    } else {
      host_id = static_cast<uint32_t>(std::strtoul((*row)[0], NULL, 10));
      // log_info("host_id for local host '%s' already registered as '%u'",
      //        hostname.c_str(), host_id);
    }
  }
  // now insert the router and get the router id
  query = sqlstring(
      "INSERT INTO mysql_innodb_cluster_metadata.routers"
      "        (host_id, router_name)"
      " VALUES (?, ?)");
  // log_info("Router instance '%s' registered with id %u", router_name.c_str(),
  // router_id);
  query << host_id << router_name << sqlstring::end;
  try {
    mysql_->execute(query);
  } catch (MySQLSession::Error &e) {
    if (e.code() == 1062 && overwrite) {
      // log_warning("Replacing instance %s (host_id %i) of router",
      //            router_name.c_str(), host_id);
      query = sqlstring(
          "SELECT router_id FROM mysql_innodb_cluster_metadata.routers"
          " WHERE host_id = ? AND router_name = ?");
      query << host_id << router_name << sqlstring::end;
      std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
      if (row) {
        return static_cast<uint32_t>(std::stoul((*row)[0]));
      }
    }
    throw;
  }
  return static_cast<uint32_t>(mysql_->last_insert_id());
}
