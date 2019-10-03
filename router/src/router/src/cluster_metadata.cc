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

#include "cluster_metadata.h"

#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/utils_sqlstring.h"
IMPORT_LOG_FUNCTIONS()

#include <string.h>
#include <stdexcept>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

namespace mysqlrouter {

/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
static std::string get_string(const char *input_str) {
  return input_str == nullptr ? "" : input_str;
}

void ClusterMetadata::verify_router_id_is_ours(
    uint32_t router_id, const std::string &hostname_override) {
  // query metadata for this router_id
  sqlstring query(
      "SELECT address FROM mysql_innodb_cluster_metadata.v2_routers WHERE "
      "router_id = ?");
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

  if ((*row)[0] && strcasecmp((*row)[0], hostname.c_str()) == 0) {
    // host_name matches our router_id, check passed
    return;
  }

  // if the host doesn't match, we force a new router_id to be generated
  throw std::runtime_error("router_id " + std::to_string(router_id) +
                           " is associated with a different host ('" +
                           (*row)[0] + "' vs '" + hostname + "')");
}

void ClusterMetadata::update_router_info(uint32_t router_id,
                                         std::string cluster_id,
                                         const std::string &rw_endpoint,
                                         const std::string &ro_endpoint,
                                         const std::string &rw_x_endpoint,
                                         const std::string &ro_x_endpoint) {
  sqlstring query(
      "UPDATE mysql_innodb_cluster_metadata.v2_routers"
      " SET attributes = "
      "   JSON_SET(JSON_SET(JSON_SET(JSON_SET(IF(attributes IS NULL, '{}', "
      "attributes),"
      "    '$.RWEndpoint', ?),"
      "    '$.ROEndpoint', ?),"
      "    '$.RWXEndpoint', ?),"
      "    '$.ROXEndpoint', ?),"
      " version = ?, cluster_id = ?"
      " WHERE router_id = ?");
  query << rw_endpoint;
  query << ro_endpoint;
  query << rw_x_endpoint;
  query << ro_x_endpoint;
  query << MYSQL_ROUTER_VERSION << cluster_id << router_id << sqlstring::end;

  mysql_->execute(query);
}

uint32_t ClusterMetadata::register_router(
    const std::string &router_name, bool overwrite,
    const std::string &hostname_override) {
  // get_local_hostname() throws LocalHostnameResolutionError
  // (std::runtime_error)
  std::string hostname = hostname_override.empty()
                             ? socket_operations_->get_local_hostname()
                             : hostname_override;

  // now insert the router and get the router id
  sqlstring query = sqlstring(
      "INSERT INTO mysql_innodb_cluster_metadata.v2_routers"
      "        (address, product_name, router_name)"
      " VALUES (?, ?, ?)");

  query << hostname << "MySQL Router" << router_name << sqlstring::end;
  try {
    // insert and return the router_id we just created
    mysql_->execute(query);
    return static_cast<uint32_t>(mysql_->last_insert_id());
  } catch (const MySQLSession::Error &e) {
    if (e.code() == ER_DUP_ENTRY && overwrite) {
      // log_warning("Replacing instance %s (hostname %s) of router",
      //            router_name.c_str(), hostname.c_str());
      query = sqlstring(
          "SELECT router_id FROM mysql_innodb_cluster_metadata.v2_routers"
          " WHERE router_name = ?");
      query << router_name << sqlstring::end;
      std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
      if (row) {
        return static_cast<uint32_t>(strtoui_checked((*row)[0]));
      }
    }
    throw;
  }
}

bool metadata_schema_version_is_compatible(
    const MetadataSchemaVersion &required,
    const MetadataSchemaVersion &available) {
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

MetadataSchemaVersion get_metadata_schema_version(MySQLSession *mysql) {
  std::unique_ptr<MySQLSession::ResultRow> result;

  try {
    result = mysql->query_one(
        "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
    if (!result) {
      throw std::runtime_error("Invalid MySQL InnoDB cluster metadata");
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
    if (e.code() == ER_NO_SUCH_TABLE || e.code() == ER_BAD_DB_ERROR) {
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
    } else {
      throw;
    }
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

void ClusterMetadata::require_metadata_is_ok() {
  if (!check_metadata_is_supported()) {  // throws MySQLSession::Error,
                                         // std::out_of_range,
                                         // std::logic_error
    throw std::runtime_error(
        "The provided server contains an unsupported cluster "
        "metadata.");
  }
}

void ClusterMetadataGR::require_cluster_is_ok() {
  try {
    if (!check_group_replication_online(
            mysql_)) {  // throws std::logic_error, MySQLSession::Error
      throw std::runtime_error(
          "The provided server is currently not an ONLINE member of a InnoDB "
          "cluster.");
    }
  } catch (const MySQLSession::Error &e) {
    if (e.code() == ER_NO_SUCH_TABLE) {
      //  Table 'performance_schema.replication_group_members' doesn't exist
      //  (1146) means
      // that group replication is not configured
      throw std::runtime_error(
          std::string("Expected MySQL Server '") + mysql_->get_address() +
          "' to have Group Replication running.\n" +
          "Checking metadata state failed with: " + e.what() + "\n\n" +
          "See https://dev.mysql.com/doc/refman/en/"
          "mysql-innodb-cluster-creating.html for instructions on setting up a "
          "MySQL Server to act as an InnoDB Cluster Metadata server\n");
    } else {
      throw;
    }
  }

  if (!check_group_has_quorum(mysql_)) {  // throws MySQLSession::Error,
                                          // std::logic_error, std::out_of_range
    throw std::runtime_error(
        "The provided server is currently not in a InnoDB cluster group with "
        "quorum and thus may contain inaccurate or outdated data.");
  }
}

std::string ClusterMetadataGR::get_cluster_type_specific_id() {
  std::string q = "select @@group_replication_group_name";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from "
          "@@group_replication_group_name "
          "expected 1 got " +
          std::to_string(result->size()));
    }
    return std::string((*result)[0]);
  }

  throw std::logic_error("No result returned for metadata query");
}

bool ClusterMetadataGR::check_metadata_is_supported() {
  // check if there's only 1 cluster and that it is gr type
  std::string q =
      "select ((select count(*) from "
      "mysql_innodb_cluster_metadata.v2_gr_clusters)=1) as has_one_gr_cluster";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql_->query_one(q));  // throws MySQLSession::Error

  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from query for metadata support: "
          "expected 1 got " +
          std::to_string(result->size()));
    }
    const bool has_one_cluster = strtoi_checked((*result)[0]) == 1;

    return has_one_cluster;
  }
  throw std::logic_error("No result returned for metadata query");
}

static ClusterInfo query_metadata_servers(MySQLSession *mysql) {
  // Query the name of the cluster, and the instance addresses
  const std::string query =
      "select c.cluster_id, c.cluster_name, i.address from "
      "mysql_innodb_cluster_metadata.v2_instances i join "
      "mysql_innodb_cluster_metadata.v2_clusters c on c.cluster_id = "
      "i.cluster_id";

  ClusterInfo result;

  try {
    mysql->query(
        query, [&result](const std::vector<const char *> &row) -> bool {
          if (result.metadata_cluster_id == "") {
            result.metadata_cluster_id = get_string(row[0]);
            result.metadata_cluster_name = get_string(row[1]);
          } else if (result.metadata_cluster_id != get_string(row[0])) {
            // metadata with more than 1 cluster not currently supported
            throw std::runtime_error("Metadata contains more than one cluster");
          }

          result.metadata_servers.push_back("mysql://" + get_string(row[2]));
          return true;
        });
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }
  if (result.metadata_cluster_name.empty())
    throw std::runtime_error("No clusters defined in metadata server");

  return result;
}

ClusterInfo ClusterMetadataGR::fetch_metadata_servers() {
  return query_metadata_servers(mysql_);
}

std::vector<std::string> ClusterMetadataGR::get_routing_mode_queries(
    const std::string &cluster_name) {
  return {
      // MDC startup
      // source: mysqlrouter::get_group_replication_id(MySQLSession *mysql)
      "select @@group_replication_group_name",

      /* next 3 are called during MDC Refresh; they access all tables that are
       * GRANTed by bootstrap
       */

      // source: ClusterMetadata::fetch_instances_from_metadata_server()
      "select I.mysql_server_uuid, I.endpoint, I.xendpoint from "
      "mysql_innodb_cluster_metadata.v2_instances I join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
      "C.cluster_id where C.cluster_name = " +
          mysql_->quote(cluster_name),

      // source: find_group_replication_primary_member()
      "show status like 'group_replication_primary_member'",

      // source: fetch_group_replication_members()
      "SELECT member_id, member_host, member_port, member_state, "
      "@@group_replication_single_primary_mode FROM "
      "performance_schema.replication_group_members WHERE channel_name = "
      "'group_replication_applier'",
  };
}

bool ClusterMetadataAR::check_metadata_is_supported() {
  // check if there's only 1 cluster and that it is ar type
  std::string q =
      "select ((select count(*) from "
      "mysql_innodb_cluster_metadata.v2_ar_clusters)=1) as has_one_ar_cluster";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql_->query_one(q));  // throws MySQLSession::Error

  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from query for metadata support: "
          "expected 1 got " +
          std::to_string(result->size()));
    }
    const bool has_one_cluster = strtoi_checked((*result)[0]) == 1;

    return has_one_cluster;
  }
  throw std::logic_error("No result returned for metadata query");
}

ClusterInfo ClusterMetadataAR::fetch_metadata_servers() {
  return query_metadata_servers(mysql_);
}

std::string ClusterMetadataAR::get_cluster_type_specific_id() {
  std::string q =
      "select cluster_id from mysql_innodb_cluster_metadata.v2_ar_clusters";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from "
          "@@group_replication_group_name "
          "expected 1 got " +
          std::to_string(result->size()));
    }
    return std::string((*result)[0]);
  }

  throw std::logic_error("No result returned for metadata query");
}

std::vector<std::string> ClusterMetadataAR::get_routing_mode_queries(
    const std::string &cluster_name) {
  return {// source: ClusterMetadata::fetch_instances_from_metadata_server()
          "select I.mysql_server_uuid, I.endpoint, I.xendpoint from "
          "mysql_innodb_cluster_metadata.v2_instances I join "
          "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
          "C.cluster_id where C.cluster_name = " +
          mysql_->quote(cluster_name) + ";"};
}

static ClusterType get_cluster_type(MySQLSession *mysql) {
  std::string q =
      "select cluster_type from "
      "mysql_innodb_cluster_metadata.v2_this_instance";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from "
          "v2_this_instance expected 1 got " +
          std::to_string(result->size()));
    }
    const auto type = std::string((*result)[0]);
    if (type == "gr") {
      return ClusterType::GR_V2;
    } else if (type == "ar") {
      return ClusterType::AR_V2;
    } else {
      throw std::runtime_error(
          "Unsupported cluster type found in the metadata: '" + type + "'");
    }
  }

  throw std::logic_error(
      "No result returned for v2_this_instance metadata query");
}

ClusterType get_cluster_type(const MetadataSchemaVersion &schema_version,
                             MySQLSession *mysql) {
  if (schema_version < kNewMetadataVersion) {
    return ClusterType::GR_V1;
  } else {
    return get_cluster_type(mysql);
  }
}

std::unique_ptr<ClusterMetadata> create_metadata(
    const MetadataSchemaVersion &schema_version, MySQLSession *mysql,
    mysql_harness::SocketOperationsBase *sockops) {
  if (!metadata_schema_version_is_compatible(kRequiredBootstrapSchemaVersion,
                                             schema_version)) {
    throw std::runtime_error(
        "This version of MySQL Router is not compatible with the provided "
        "MySQL InnoDB cluster metadata.");
  }

  std::unique_ptr<ClusterMetadata> result;

  const auto cluster_type = get_cluster_type(schema_version, mysql);
  switch (cluster_type) {
    case ClusterType::GR_V2:
      result.reset(new ClusterMetadataGR(schema_version, mysql, sockops));
      break;
    default:
      result.reset(new ClusterMetadataAR(schema_version, mysql, sockops));
  }

  return result;
}

unsigned ClusterMetadataAR::get_view_id() {
  std::string query =
      "select view_id from mysql_innodb_cluster_metadata.v2_ar_members where "
      "member_id = @@server_uuid";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one(query));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from view_id expected 1 got " +
          std::to_string(result->size()));
    }
    return strtoui_checked((*result)[0]);
  }

  throw std::logic_error("No result returned for view_id metadata query");
}

std::string to_string(const ClusterType cluster_type) {
  if (cluster_type == ClusterType::AR_V2) {
    return "ar";
  } else {
    return "gr";
  }
}

}  // namespace mysqlrouter
