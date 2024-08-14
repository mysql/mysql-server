/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "cluster_metadata.h"

#include <cstring>  // strcmp
#include <stdexcept>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "common.h"  // get_from_map
#include "configuration_update_schema.h"
#include "harness_assert.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/event_state_tracker.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"
#include "mysqlrouter/cluster_metadata_instance_attributes.h"
#include "mysqlrouter/utils.h"  // strtoui_checked
#include "mysqlrouter/utils_sqlstring.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

IMPORT_LOG_FUNCTIONS()

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

using namespace std::string_literals;
using mysql_harness::EventStateTracker;
using mysql_harness::get_from_map;
using mysql_harness::logging::LogLevel;

static constexpr const std::string_view kClusterSet{"clusterset"};
static constexpr const std::string_view kCreateClusterUrl{
    "https://dev.mysql.com/doc/mysql-shell/en/"
    "deploying-production-innodb-cluster.html"};

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

static void do_verify_router_id_is_ours(
    const uint32_t router_id, const std::string &hostname_override,
    MySQLSession *mysql,
    mysql_harness::SocketOperationsBase *socket_operations) {
  // query metadata for this router_id
  sqlstring query(
      "SELECT address FROM mysql_innodb_cluster_metadata.v2_routers WHERE "
      "router_id = ?");
  query << router_id << sqlstring::end;

  std::unique_ptr<MySQLSession::ResultRow> row(mysql->query_one(query));
  if (!row) {
    // log_warning("router_id %u not in metadata", router_id);
    throw std::runtime_error("router_id " + std::to_string(router_id) +
                             " not found in metadata");
  }

  // get_local_hostname() throws LocalHostnameResolutionError
  // (std::runtime_error)
  std::string hostname = hostname_override.empty()
                             ? socket_operations->get_local_hostname()
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

void ClusterMetadataGRV2::verify_router_id_is_ours(
    uint32_t router_id, const std::string &hostname_override) {
  do_verify_router_id_is_ours(router_id, hostname_override, mysql_,
                              socket_operations_);
}

void ClusterMetadataAR::verify_router_id_is_ours(
    uint32_t router_id, const std::string &hostname_override) {
  do_verify_router_id_is_ours(router_id, hostname_override, mysql_,
                              socket_operations_);
}

namespace {

std::string to_string_md(const ClusterType cluster_type) {
  switch (cluster_type) {
    case ClusterType::GR_V2:
      return "cluster";
    case ClusterType::RS_V2:
      return "replicaset";
    case ClusterType::GR_CS:
      return "clusterset";
  }

  return "unknown";
}

class ConfigurationDefaults {
 public:
  ConfigurationDefaults(const mysqlrouter::ClusterType cluster_type,
                        const std::string &id, MySQLSession *session)
      : cluster_type_(cluster_type), id_(id), session_(*session) {
    switch (cluster_type) {
      case mysqlrouter::ClusterType::GR_CS:
        table_name_ = "v2_cs_clustersets";
        id_field_ = "clusterset_id";
        break;
      case mysqlrouter::ClusterType::GR_V2:
        table_name_ = "v2_gr_clusters";
        id_field_ = "cluster_id";
        break;
      default:
        // case mysqlrouter::ClusterType::RS_V2:
        table_name_ = "v2_ar_clusters";
        id_field_ = "cluster_id";
        break;
    }
  }

  void update() {
    if (!is_stored()) {
      store();
    }
  }

 private:
  bool is_stored() {
    sqlstring query(
        "select JSON_EXTRACT(router_options, '$.Configuration.\"!\"') IS NULL "
        "from mysql_innodb_cluster_metadata.! where ! = ?",
        {mysqlrouter::QuoteOnlyIfNeeded});

    query << MYSQL_ROUTER_VERSION << table_name_ << id_field_ << id_
          << sqlstring::end;

    if (auto row = session_.query_one(query)) {
      return get_string((*row)[0]) != "1";
    }

    return false;
  }

  void store() {
    sqlstring query(
        "UPDATE mysql_innodb_cluster_metadata.! SET router_options = "
        "JSON_MERGE_PATCH("
        "IF(router_options IS NULL, '{}', router_options), "
        "CAST(? as JSON))"
        " WHERE ! = ?",
        {mysqlrouter::QuoteOnlyIfNeeded});

    query << table_name_ << get_config_json_as_string() << id_field_ << id_
          << sqlstring::end;

    session_.execute(query);
  }

  std::string get_config_json_as_string() {
    using JsonAllocator = rapidjson::CrtAllocator;
    using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
    using JsonDocument =
        rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;

    JsonValue version_obj(rapidjson::kObjectType);
    JsonValue config_obj(rapidjson::kObjectType);
    JsonValue result_obj(rapidjson::kObjectType);

    // defaults
    const auto defaults_type =
        (cluster_type_ == mysqlrouter::ClusterType::GR_CS)
            ? mysql_harness::DynamicConfig::ValueType::DefaultForClusterSet
            : mysql_harness::DynamicConfig::ValueType::DefaultForCluster;
    auto defaults =
        mysql_harness::DynamicConfig::instance().get_json(defaults_type);
    auto allocator = defaults.GetAllocator();

    // update schema
    const auto update_schema_str =
        std::string(ConfigurationUpdateJsonSchema::data(),
                    ConfigurationUpdateJsonSchema::size());
    JsonDocument changes_schema;
    changes_schema.Parse<0>(update_schema_str);

    version_obj.AddMember("Defaults", defaults, allocator);
    version_obj.AddMember("ConfigurationChangesSchema", changes_schema,
                          allocator);
    config_obj.AddMember(MYSQL_ROUTER_VERSION, version_obj, allocator);
    result_obj.AddMember("Configuration", config_obj, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    result_obj.Accept(writer);

    return buffer.GetString();
  }

  mysqlrouter::ClusterType cluster_type_;
  std::string id_;
  MySQLSession &session_;
  std::string table_name_;
  std::string id_field_;
};

void update_router_info_v2(
    const mysqlrouter::ClusterType cluster_type, const uint32_t router_id,
    const std::string &cluster_id, const std::string &target_cluster,
    const std::string &rw_endpoint, const std::string &ro_endpoint,
    const std::string &rw_split_endpoint, const std::string &rw_x_endpoint,
    const std::string &ro_x_endpoint, const std::string &username,
    MySQLSession *mysql) {
  const std::string cluster_id_field =
      cluster_type == mysqlrouter::ClusterType::GR_CS ? "clusterset_id"
                                                      : "cluster_id";
  sqlstring query(
      "UPDATE mysql_innodb_cluster_metadata.v2_routers"
      " SET attributes = "
      "JSON_SET(JSON_SET(JSON_SET(JSON_SET(JSON_SET(JSON_SET(JSON_SET("
      "JSON_SET("
      "IF(attributes IS NULL, '{}', attributes),"
      "    '$.RWEndpoint', ?),"
      "    '$.ROEndpoint', ?),"
      "    '$.RWSplitEndpoint', ?),"
      "    '$.RWXEndpoint', ?),"
      "    '$.ROXEndpoint', ?),"
      "    '$.MetadataUser', ?),"
      "    '$.bootstrapTargetType', ?),"
      "    '$.Configuration', CAST(? as JSON)),"
      " version = ?, ! = ?"
      " WHERE router_id = ?",
      {mysqlrouter::QuoteOnlyIfNeeded});

  query << rw_endpoint << ro_endpoint << rw_split_endpoint << rw_x_endpoint
        << ro_x_endpoint << username << to_string_md(cluster_type)
        << mysql_harness::DynamicConfig::instance().get_json_as_string(
               mysql_harness::DynamicConfig::ValueType::ConfiguredValue)
        << MYSQL_ROUTER_VERSION << cluster_id_field << cluster_id << router_id
        << sqlstring::end;

  mysql->execute(query);

  if (!target_cluster.empty()) {
    sqlstring query_options(
        "UPDATE mysql_innodb_cluster_metadata.v2_routers"
        " SET options = JSON_SET(IF(options IS NULL, '{}', options),"
        " '$.target_cluster', ?)"
        " WHERE router_id = ?");

    query_options << target_cluster << router_id << sqlstring::end;

    mysql->execute(query_options);
  }

  ConfigurationDefaults(cluster_type, cluster_id, mysql).update();
}

}  // namespace

void ClusterMetadataGRV2::update_router_info(
    const uint32_t router_id, const std::string &cluster_id,
    const std::string &target_cluster, const std::string &rw_endpoint,
    const std::string &ro_endpoint, const std::string &rw_split_endpoint,
    const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
    const std::string &username) {
  update_router_info_v2(mysqlrouter::ClusterType::GR_V2, router_id, cluster_id,
                        target_cluster, rw_endpoint, ro_endpoint,
                        rw_split_endpoint, rw_x_endpoint, ro_x_endpoint,
                        username, mysql_);
}

void ClusterMetadataAR::update_router_info(
    const uint32_t router_id, const std::string &cluster_id,
    const std::string &target_cluster, const std::string &rw_endpoint,
    const std::string &ro_endpoint, const std::string &rw_split_endpoint,
    const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
    const std::string &username) {
  update_router_info_v2(mysqlrouter::ClusterType::RS_V2, router_id, cluster_id,
                        target_cluster, rw_endpoint, ro_endpoint,
                        rw_split_endpoint, rw_x_endpoint, ro_x_endpoint,
                        username, mysql_);
}

void ClusterMetadataGRInClusterSet::update_router_info(
    const uint32_t router_id, const std::string &cluster_id,
    const std::string &target_cluster, const std::string &rw_endpoint,
    const std::string &ro_endpoint, const std::string &rw_split_endpoint,
    const std::string &rw_x_endpoint, const std::string &ro_x_endpoint,
    const std::string &username) {
  update_router_info_v2(mysqlrouter::ClusterType::GR_CS, router_id, cluster_id,
                        target_cluster, rw_endpoint, ro_endpoint,
                        rw_split_endpoint, rw_x_endpoint, ro_x_endpoint,
                        username, mysql_);
}

namespace {

uint32_t register_router_v2(
    const std::string &router_name, const bool overwrite,
    const std::string &hostname_override, MySQLSession *mysql,
    mysql_harness::SocketOperationsBase *socket_operations) {
  // get_local_hostname() throws LocalHostnameResolutionError
  // (std::runtime_error)
  std::string hostname = hostname_override.empty()
                             ? socket_operations->get_local_hostname()
                             : hostname_override;

  // now insert the router and get the router id
  sqlstring query = sqlstring(
      "INSERT INTO mysql_innodb_cluster_metadata.v2_routers"
      "        (address, product_name, router_name)"
      " VALUES (?, ?, ?)");

  query << hostname << "MySQL Router" << router_name << sqlstring::end;
  try {
    // insert and return the router_id we just created
    mysql->execute(query);
    return static_cast<uint32_t>(mysql->last_insert_id());
  } catch (const MySQLSession::Error &e) {
    if (e.code() == ER_DUP_ENTRY && overwrite) {
      // log_warning("Replacing instance %s (hostname %s) of router",
      //            router_name.c_str(), hostname.c_str());
      query = sqlstring(
          "SELECT router_id FROM mysql_innodb_cluster_metadata.v2_routers"
          " WHERE router_name = ? and address = ?");
      query << router_name << hostname << sqlstring::end;
      std::unique_ptr<MySQLSession::ResultRow> row(mysql->query_one(query));
      if (row) {
        return static_cast<uint32_t>(strtoui_checked((*row)[0]));
      }
    }
    throw;
  }
}

}  // namespace

uint32_t ClusterMetadataGRV2::register_router(
    const std::string &router_name, const bool overwrite,
    const std::string &hostname_override) {
  return register_router_v2(router_name, overwrite, hostname_override, mysql_,
                            socket_operations_);
}

uint32_t ClusterMetadataAR::register_router(
    const std::string &router_name, const bool overwrite,
    const std::string &hostname_override) {
  return register_router_v2(router_name, overwrite, hostname_override, mysql_,
                            socket_operations_);
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

std::string get_metadata_schema_uncompatible_msg(
    const mysqlrouter::MetadataSchemaVersion &version) {
  return "The target Cluster's Metadata version ('" + to_string(version) +
         "') is not supported. Please use the latest MySQL Shell to upgrade it "
         "using 'dba.upgradeMetadata()'. Expected metadata version compatible "
         "with '" +
         to_string(mysqlrouter::kRequiredRoutingMetadataSchemaVersion) + "'";
}

std::string to_string(const MetadataSchemaVersion &version) {
  return std::to_string(version.major) + "." + std::to_string(version.minor) +
         "." + std::to_string(version.patch);
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
     * 'mysql_innodb_cluster_metadata' (1049). We handle both codes the same
     * way here.
     */
    if (e.code() == ER_NO_SUCH_TABLE || e.code() == ER_BAD_DB_ERROR) {
      // unknown database mysql_innodb_cluster_metata
      throw std::runtime_error(
          std::string("Expected MySQL Server '") + mysql->get_address() +
          "' to contain the metadata of MySQL InnoDB Cluster, but the schema "
          "does not exist.\n" +
          "Checking version of the metadata schema failed with: " + e.what() +
          "\n\n" + "See " + std::string(kCreateClusterUrl) +
          " for instructions on setting up a "
          "MySQL Server to act as an InnoDB Cluster Metadata server\n");
    } else {
      throw;
    }
  }

  size_t result_size = result->size();
  if (result_size != 3 && result_size != 2) {
    throw std::out_of_range(
        "Invalid number of values returned from "
        "mysql_innodb_cluster_metadata.schema_version: expected 2 or 3, got " +
        std::to_string(result_size));
  }
  auto major = strtoui_checked((*result)[0]);
  auto minor = strtoui_checked((*result)[1]);

  // Initially shell used to create version number with 2 digits only (1.0)
  // It has since moved to 3 digit numbers. We normalize it to 1.0.0 here
  // for simplicity and backwards compatibility.
  auto patch = result_size == 3 ? strtoui_checked((*result)[2]) : 0;

  return {major, minor, patch};
}

bool check_group_replication_online(MySQLSession *mysql) {
  std::string q =
      "SELECT member_state"
      " FROM performance_schema.replication_group_members"
      " WHERE CAST(member_id AS char ascii) = CAST(@@server_uuid AS char "
      "ascii)";
  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql->query_one(q));  // throws MySQLSession::Error
  if (result && (*result)[0]) {
    if (strcmp((*result)[0], "ONLINE") == 0) return true;
    // log_warning("Member state for current server is %s", (*result)[0]);
    return false;
  }
  throw std::logic_error("No result returned for metadata query");
}

bool check_group_has_quorum(MySQLSession *mysql) {
  std::string q =
      "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, "
      "SUM(IF(member_state = 'RECOVERING', 1, 0)) as num_recovering, "
      "COUNT(*) as num_total "
      "FROM performance_schema.replication_group_members";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql->query_one(q));  // throws MySQLSession::Error
  if (result) {
    if (result->size() != 3) {
      throw std::out_of_range(
          "Invalid number of values returned from "
          "performance_schema.replication_group_members: "
          "expected 3 got " +
          std::to_string(result->size()));
    }
    int online = strtoi_checked((*result)[0]);
    int recovering = strtoi_checked((*result)[1]);
    int all = strtoi_checked((*result)[2]);

    if ((online + recovering) > all / 2) return true;
    return false;
  }

  throw std::logic_error("No result returned for metadata query");
}

void ClusterMetadata::require_metadata_is_ok() {
  uint64_t cluster_count =
      query_cluster_count();  // throws MySQLSession::Error,
                              // std::out_of_range,
                              // std::logic_error
  if (cluster_count == 0) {
    throw std::runtime_error(
        "Expected the metadata server to contain configuration for one "
        "cluster, found none.\n\nSee " +
        std::string(kCreateClusterUrl) + " about how to create a cluster.");
  } else if (cluster_count != 1) {
    throw std::runtime_error(
        "The metadata server contains configuration for more than 1 Cluster: " +
        std::to_string(cluster_count) +
        ". If it was a part of a ClusterSet previously, the metadata should be "
        "recreated using dba.dropMetadataSchema() and dba.createCluster() with "
        "adoptFromGR parameter set to true.");
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
          "Checking metadata state failed with: " + e.what() + "\n\n" + "See " +
          std::string(kCreateClusterUrl) +
          " for instructions on setting up a "
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

std::vector<std::tuple<std::string, unsigned long>>
ClusterMetadataGR::fetch_cluster_hosts() {
  // Query the name of the replicaset, the servers in the replicaset and the
  // router credentials using the URL of a server in the replicaset.
  //
  // order by member_role (in 8.0 and later) to sort PRIMARY over SECONDARY
  const std::string query =
      "SELECT member_host, member_port "
      "  FROM performance_schema.replication_group_members "
      " /*!80002 ORDER BY member_role */";

  try {
    std::vector<std::tuple<std::string, unsigned long>> gr_servers;

    mysql_->query(
        query, [&gr_servers](const std::vector<const char *> &row) -> bool {
          size_t end_pos;
          const std::string port_str{row[1]};
          auto port = std::stoul(port_str, &end_pos);

          if (end_pos != port_str.size()) {
            throw std::runtime_error(
                "Error querying metadata: expected cluster_host query to "
                "return a positive integer for member_port, got " +
                port_str);
          }

          gr_servers.emplace_back(std::string(row[0]), port);
          return true;  // don't stop
        });

    return gr_servers;
  } catch (const MySQLSession::Error &e) {
    // log_error("MySQL error: %s (%u)", e.what(), e.code());
    // log_error("    Failed query: %s", query.str().c_str());
    throw std::runtime_error("Error querying metadata: "s + e.what());
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

static uint64_t query_gr_cluster_count(MySQLSession *mysql) {
  // check if there's only 1 GR cluster
  std::string query =
      "select count(*) from "
      "mysql_innodb_cluster_metadata.v2_gr_clusters";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql->query_one(query));  // throws MySQLSession::Error

  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from query for metadata support: "
          "expected 1 got " +
          std::to_string(result->size()));
    }
    return strtoui_checked((*result)[0]);
  }
  throw std::logic_error("No result returned for metadata query");
}

uint64_t ClusterMetadataGRV2::query_cluster_count() {
  return query_gr_cluster_count(mysql_);
}

static InstanceType get_instance_type(const std::string &attributes) {
  const auto default_type = InstanceType::GroupMember;
  const auto instance_type_attr =
      InstanceAttributes::get_instance_type(attributes, default_type);

  if (!instance_type_attr) {
    return default_type;
  }

  return instance_type_attr.value();
}

static ClusterInfo query_metadata_servers(
    MySQLSession *mysql, const mysqlrouter::ClusterType cluster_type) {
  // Query the uuid and name of the cluster, and the instance addresses
  std::string query;

  switch (cluster_type) {
    case mysqlrouter::ClusterType::RS_V2:
      query =
          "select c.cluster_id, c.cluster_id as uuid, c.cluster_name, "
          "i.address, i.attributes from "
          "mysql_innodb_cluster_metadata.v2_instances i join "
          "mysql_innodb_cluster_metadata.v2_clusters c on c.cluster_id = "
          "i.cluster_id";
      break;
    default:  // case mysqlrouter::ClusterType::GR_V2:
      query =
          "select c.cluster_id, c.group_name as uuid, c.cluster_name, "
          "i.address, i.attributes from "
          "mysql_innodb_cluster_metadata.v2_instances i join "
          "mysql_innodb_cluster_metadata.v2_gr_clusters c on c.cluster_id = "
          "i.cluster_id";
      break;
  }

  ClusterInfo result;

  try {
    mysql->query(
        query, [&result](const std::vector<const char *> &row) -> bool {
          const std::string attributes = get_string(row[4]);
          if (get_instance_type(attributes) == InstanceType::ReadReplica) {
            // we don't want Read Replicas as metadata servers
            return true;
          }
          if (result.cluster_id == "") {
            result.cluster_id = get_string(row[0]);
            result.cluster_type_specific_id = get_string(row[1]);
            result.name = get_string(row[2]);
          } else if (result.cluster_id != get_string(row[0])) {
            // metadata with more than 1 cluster not currently supported
            throw std::runtime_error("Metadata contains more than one cluster");
          }

          result.metadata_servers.push_back("mysql://" + get_string(row[3]));
          return true;
        });
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }
  if (result.name.empty())
    throw std::runtime_error("No clusters defined in metadata server");

  return result;
}

ClusterInfo ClusterMetadataGRV2::fetch_metadata_servers() {
  return query_metadata_servers(mysql_, ClusterType::GR_V2);
}

InstanceType ClusterMetadataGRV2::fetch_current_instance_type() {
  const std::string query =
      "select i.attributes from mysql_innodb_cluster_metadata.v2_this_instance "
      "ti left join mysql_innodb_cluster_metadata.v2_instances i on "
      "ti.instance_id = i.instance_id";

  try {
    std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
    if (!row) {
      return InstanceType::Unsupported;
    }

    return get_instance_type(get_string((*row)[0]));
  } catch (const std::exception &e) {
    throw std::runtime_error("Failed identifying instance type: "s + e.what());
  }
}

ClusterMetadataGRInClusterSet::ClusterMetadataGRInClusterSet(
    const MetadataSchemaVersion &schema_version, MySQLSession *mysql,
    const OptionsMap &options, mysql_harness::SocketOperationsBase *sockops)
    : ClusterMetadataGRV2(schema_version, mysql, sockops) {
  const std::string target_cluster_by_name =
      get_from_map(options, "target-cluster-by-name"s, ""s);
  if (!target_cluster_by_name.empty()) {
    target_cluster_type_ = TargetClusterType::targetClusterByName;
    target_cluster_name_ = target_cluster_by_name;
    return;
  }

  const std::string target_cluster =
      get_from_map(options, "target-cluster"s, "current"s);
  if (target_cluster == "current")
    target_cluster_type_ = TargetClusterType::targetClusterCurrent;
  else {
    harness_assert(target_cluster == "primary");
    target_cluster_type_ = TargetClusterType::targetClusterPrimary;
  }
}

ClusterInfo ClusterMetadataGRInClusterSet::fetch_metadata_servers() {
  ClusterInfo result;
  std::string query =
      "select C.cluster_id, C.group_name, CS.domain_name, CSM.member_role from "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C join "
      "mysql_innodb_cluster_metadata.v2_cs_members CSM on CSM.cluster_id = "
      "C.cluster_id join mysql_innodb_cluster_metadata.v2_cs_clustersets "
      "CS on CS.clusterset_id = CSM.clusterset_id";

  switch (target_cluster_type_) {
    case TargetClusterType::targetClusterByName:
      query += " where C.cluster_name = " + mysql_->quote(target_cluster_name_);
      break;
    case TargetClusterType::targetClusterCurrent:
      query = query +=
          " where C.cluster_id = (select cluster_id from "
          "mysql_innodb_cluster_metadata.v2_this_instance)";
      break;
    case TargetClusterType::targetClusterPrimary:
      query += " where CSM.member_role = 'PRIMARY'";
  }

  std::unique_ptr<MySQLSession::ResultRow> result_cluster_info(
      mysql_->query_one(query));

  if (!result_cluster_info) {
    switch (target_cluster_type_) {
      case TargetClusterType::targetClusterByName:
        throw std::runtime_error(
            "Could not find Cluster with selected name: '" +
            target_cluster_name_ + "'");
      case TargetClusterType::targetClusterCurrent:
        throw std::runtime_error(
            "The node used for bootstrap does not appear to be part of the "
            "InnoDB Cluster'");
      case TargetClusterType::targetClusterPrimary:
        throw std::runtime_error(
            "Could not reach Primary Cluster for the ClusterSet");
    }
  }
  if (result_cluster_info->size() != 4) {
    throw std::out_of_range(
        "Invalid number of values returned from query for cluster info: "
        "expected 4 got " +
        std::to_string(result_cluster_info->size()));
  }
  result.cluster_id = get_string((*result_cluster_info)[0]);
  result.cluster_type_specific_id = get_string((*result_cluster_info)[1]);
  result.name = get_string((*result_cluster_info)[2]);
  result.is_primary = get_string((*result_cluster_info)[3]) == "PRIMARY";

  // get all the nodes of all the Clusters that belong to the ClusterSet;
  // we want those that belong to the PRIMARY cluster to be first in the
  // resultset
  sqlstring query2 =
      "SELECT i.address, i.attributes, csm.member_role "
      "FROM mysql_innodb_cluster_metadata.v2_instances i "
      "LEFT JOIN mysql_innodb_cluster_metadata.v2_cs_members csm "
      "ON i.cluster_id = csm.cluster_id "
      "WHERE i.cluster_id IN ( "
      "   SELECT cluster_id "
      "   FROM mysql_innodb_cluster_metadata.v2_cs_members "
      "   WHERE clusterset_id = "
      "      (SELECT clusterset_id "
      "       FROM mysql_innodb_cluster_metadata.v2_cs_members "
      "       WHERE cluster_id = ?) "
      ")";

  query2 << result.cluster_id;
  std::vector<std::string> replica_clusters_nodes;
  try {
    mysql_->query(
        query2,
        [&result, &replica_clusters_nodes](
            const std::vector<const char *> &row) -> bool {
          const std::string attributes = get_string(row[1]);
          if (get_instance_type(attributes) == InstanceType::ReadReplica) {
            // we don't want Read Replicas as metadata servers
            return true;
          }
          // we want PRIMARY cluster nodes first, so we put them
          // directly in the result list, the non-PRIMARY ones we
          // buffer and append to the result at the end
          auto &servers = (get_string(row[2]) == "PRIMARY")
                              ? result.metadata_servers
                              : replica_clusters_nodes;
          servers.push_back("mysql://" + get_string(row[0]));
          return true;
        });
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }

  result.metadata_servers.insert(result.metadata_servers.end(),
                                 replica_clusters_nodes.begin(),
                                 replica_clusters_nodes.end());

  return result;
}

std::vector<std::tuple<std::string, unsigned long>>
ClusterMetadataGRInClusterSet::fetch_cluster_hosts() {
  std::vector<std::tuple<std::string, unsigned long>> result;

  const auto clusterset_servers = fetch_metadata_servers();
  for (const auto &server : clusterset_servers.metadata_servers) {
    mysqlrouter::URI uri(server);
    result.push_back(std::make_tuple(uri.host, uri.port));
  }

  return result;
}

std::string ClusterMetadataGRInClusterSet::get_cluster_type_specific_id() {
  const std::string q =
      "select CSM.clusterset_id from "
      "mysql_innodb_cluster_metadata.v2_cs_members CSM "
      "join mysql_innodb_cluster_metadata.v2_gr_clusters C on "
      "CSM.cluster_id = "
      "C.cluster_id where C.cluster_id = (select cluster_id from "
      "mysql_innodb_cluster_metadata.v2_this_instance)";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from clusterset_id query expected "
          "1 got " +
          std::to_string(result->size()));
    }
    return std::string((*result)[0]);
  }

  throw std::logic_error("No result returned for metadata query");
}

uint64_t ClusterMetadataGRInClusterSet::get_view_id(
    const std::string &clusterset_id) {
  std::string q =
      "select view_id from mysql_innodb_cluster_metadata.v2_cs_clustersets "
      "where clusterset_id = " +
      mysql_->quote(clusterset_id);

  std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from view_id query expected 1 "
          "got " +
          std::to_string(result->size()));
    }
    return strtoull_checked((*result)[0]);
  }

  throw std::logic_error("No result returned for metadata query");
}

static std::vector<std::string> do_get_routing_mode_queries(
    MySQLSession *mysql) {
  const std::string fetch_instances_query =
      "select C.cluster_id, C.cluster_name, I.mysql_server_uuid, "
      "I.endpoint, I.xendpoint, I.attributes "
      "from mysql_innodb_cluster_metadata.v2_instances I join "
      "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
      "C.cluster_id where C.cluster_name = " +
      mysql->quote("some_cluster_name");

  std::string gr_members_query;
  // replication_group_members.member_role field was introduced in 8.0.2,
  // otoh group_replication_primary_member gets removed in 8.3 so we need 2
  // different queries depending on a server version
  const bool has_member_role_field = mysql->server_version() >= 80002;
  if (has_member_role_field) {
    gr_members_query =
        "SELECT member_id, member_host, member_port, member_state, "
        "member_role, @@group_replication_single_primary_mode FROM "
        "performance_schema.replication_group_members"
        " WHERE channel_name = 'group_replication_applier'";
  } else {
    gr_members_query =
        "SELECT member_id, member_host, member_port, member_state, "
        "IF(g.primary_uuid = '' OR member_id = g.primary_uuid, 'PRIMARY', "
        "'SECONDARY') as member_role, "
        "@@group_replication_single_primary_mode "
        "FROM (SELECT IFNULL(variable_value, '') AS primary_uuid FROM "
        "performance_schema.global_status WHERE variable_name = "
        "'group_replication_primary_member') g, "
        "performance_schema.replication_group_members WHERE channel_name = "
        "'group_replication_applier'";
  }

  return {// MDC startup
          // source: mysqlrouter::get_group_replication_id(MySQLSession *mysql)
          "select @@group_replication_group_name",

          /* next 2 are called during MDC Refresh; they access all tables that
           * are GRANTed by bootstrap
           */

          // source: ClusterMetadata::fetch_instances_from_metadata_server()
          fetch_instances_query,

          // source: fetch_group_replication_members()
          gr_members_query};
}

std::vector<std::string> ClusterMetadataGRV2::get_routing_mode_queries() {
  return do_get_routing_mode_queries(mysql_);
}

uint64_t ClusterMetadataAR::query_cluster_count() {
  // check if there's only 1 cluster and that it is ar type
  std::string q =
      "select count(*) from "
      "mysql_innodb_cluster_metadata.v2_ar_clusters";

  std::unique_ptr<MySQLSession::ResultRow> result(
      mysql_->query_one(q));  // throws MySQLSession::Error

  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from query for metadata support: "
          "expected 1 got " +
          std::to_string(result->size()));
    }
    return strtoui_checked((*result)[0]);
  }
  throw std::logic_error("No result returned for metadata query");
}

ClusterInfo ClusterMetadataAR::fetch_metadata_servers() {
  return query_metadata_servers(mysql_, ClusterType::RS_V2);
}

std::string ClusterMetadataAR::get_cluster_type_specific_id() {
  std::string q =
      "select cluster_id from mysql_innodb_cluster_metadata.v2_ar_clusters";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from cluster_id query expected 1 "
          "got " +
          std::to_string(result->size()));
    }
    return std::string((*result)[0]);
  }

  throw std::logic_error("No result returned for metadata query");
}

std::vector<std::string> ClusterMetadataAR::get_routing_mode_queries() {
  return {// source: ClusterMetadata::fetch_instances_from_metadata_server()
          "select C.cluster_id, C.cluster_name, I.mysql_server_uuid, "
          "I.endpoint, I.xendpoint, I.attributes from "
          "mysql_innodb_cluster_metadata.v2_instances I join "
          "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
          "C.cluster_id where C.cluster_name = " +
          mysql_->quote("some_cluster_name") + ";"};
}

std::vector<std::tuple<std::string, unsigned long>>
ClusterMetadataAR::fetch_cluster_hosts() {
  // Query the name of the cluster, and the instance addresses
  const std::string query =
      "select i.address from "
      "mysql_innodb_cluster_metadata.v2_instances i join "
      "mysql_innodb_cluster_metadata.v2_clusters c on c.cluster_id = "
      "i.cluster_id";

  try {
    std::vector<std::tuple<std::string, unsigned long>> ar_servers;

    mysql_->query(query,
                  [&ar_servers](const std::vector<const char *> &row) -> bool {
                    mysqlrouter::URI u("mysql://"s + row[0]);
                    ar_servers.push_back(std::make_tuple(u.host, u.port));
                    return true;  // don't stop
                  });

    return ar_servers;
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Error querying metadata: "s + e.what());
  }
}

static ClusterType get_cluster_type(MySQLSession *mysql) {
  std::string q =
      "select cluster_type from "
      "mysql_innodb_cluster_metadata.v2_this_instance";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from v2_this_instance expected 1 "
          "got " +
          std::to_string(result->size()));
    }
    const auto type = std::string((*result)[0]);
    if (type == "gr") {
      return ClusterType::GR_V2;
    } else if (type == "ar") {
      return ClusterType::RS_V2;
    } else {
      throw std::runtime_error(
          "Unsupported cluster type found in the metadata: '" + type + "'");
    }
  }

  throw std::runtime_error(
      "No result returned for v2_this_instance metadata query");
}

bool is_part_of_cluster_set(MySQLSession *mysql) {
  std::string q =
      "select count(clusterset_id) from "
      "mysql_innodb_cluster_metadata.v2_this_instance i join "
      "mysql_innodb_cluster_metadata.v2_cs_members "
      "csm on i.cluster_id = csm.cluster_id where clusterset_id is not "
      "null";

  std::unique_ptr<MySQLSession::ResultRow> result(mysql->query_one(q));
  if (result) {
    if (result->size() != 1) {
      throw std::out_of_range(
          "Invalid number of values returned from "
          "is_part_of_cluster_set query expected 1 got " +
          std::to_string(result->size()));
    }

    return strtoui_checked((*result)[0]) > 0;
  }

  throw std::runtime_error(
      "No result returned for is_part_of_cluster_set metadata query");
}

static bool was_bootstrapped_as_clusterset(MySQLSession *mysql,
                                           const unsigned router_id) {
  // check if we have a target cluster assigned in the metadata
  const std::string query =
      "SELECT JSON_UNQUOTE(JSON_EXTRACT(r.attributes, "
      "'$.bootstrapTargetType')) FROM mysql_innodb_cluster_metadata.v2_routers "
      "r where r.router_id = " +
      std::to_string(router_id);

  std::unique_ptr<MySQLSession::ResultRow> row(mysql->query_one(query));
  if (!row) {
    return false;
  }

  return get_string((*row)[0]) == kClusterSet;
}

ClusterType get_cluster_type(const MetadataSchemaVersion &schema_version,
                             MySQLSession *mysql,
                             unsigned int router_id /*= 0*/) {
  const auto type = get_cluster_type(mysql);

  if (schema_version >= kClusterSetsMetadataVersion &&
      type == ClusterType::GR_V2) {
    bool part_of_cluster_set = is_part_of_cluster_set(mysql);
    if (part_of_cluster_set) {
      // The type of the cluster that we discovered in the metadata is
      // ClusterSet. Check if the Router was actually bootstrapped for a
      // ClusterSet. If not treat it as a standalone cluster and log a
      // warning.
      const bool was_bs_for_cs =
          (router_id == 0) || was_bootstrapped_as_clusterset(mysql, router_id);

      const bool was_bs_for_cs_changed =
          EventStateTracker::instance().state_changed(
              was_bs_for_cs, EventStateTracker::EventId::
                                 ClusterWasBootstrappedAgainstClusterset);

      if (!was_bs_for_cs) {
        const auto log_level =
            was_bs_for_cs_changed ? LogLevel::kWarning : LogLevel::kDebug;
        log_custom(
            log_level,
            "The target Cluster is part of a ClusterSet, but this Router was "
            "not bootstrapped to use the ClusterSet. Treating the Cluster as "
            "a standalone Cluster. Please bootstrap the Router again if you "
            "want to use ClusterSet capabilities.");
        part_of_cluster_set = false;
      }
    }

    return part_of_cluster_set ? ClusterType::GR_CS : ClusterType::GR_V2;
  }

  return type;
}

std::unique_ptr<ClusterMetadata> create_metadata(
    const MetadataSchemaVersion &schema_version, MySQLSession *mysql,
    const OptionsMap &options, mysql_harness::SocketOperationsBase *sockops) {
  if (!metadata_schema_version_is_compatible(kRequiredBootstrapSchemaVersion,
                                             schema_version) &&
      !metadata_schema_version_is_compatible(
          kRequiredRoutingMetadataSchemaVersion, schema_version)) {
    throw std::runtime_error(
        "This version of MySQL Router is not compatible with the provided "
        "MySQL InnoDB cluster metadata.");
  }

  std::unique_ptr<ClusterMetadata> result;

  const auto cluster_type = get_cluster_type(schema_version, mysql);
  switch (cluster_type) {
    case ClusterType::GR_V2:
      result.reset(new ClusterMetadataGRV2(schema_version, mysql, sockops));
      break;
    case ClusterType::RS_V2:
      result.reset(new ClusterMetadataAR(schema_version, mysql, sockops));
      break;
    case ClusterType::GR_CS:
      result.reset(new ClusterMetadataGRInClusterSet(schema_version, mysql,
                                                     options, sockops));
  }

  return result;
}

uint64_t ClusterMetadataAR::get_view_id(
    const std::string & /*cluster_type_specific_id*/) {
  std::string query =
      "select view_id from mysql_innodb_cluster_metadata.v2_ar_members where "
      "CAST(member_id AS char ascii) = CAST(@@server_uuid AS char ascii)";

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
  switch (cluster_type) {
    case ClusterType::RS_V2:
      return "rs";
    default:
      return "gr";
  }
}

static std::vector<std::string> do_get_grant_statements(
    const std::string &new_accounts) {
  return {
      "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO " +
          new_accounts,
      "GRANT SELECT ON performance_schema.replication_group_members TO " +
          new_accounts,
      "GRANT SELECT ON performance_schema.replication_group_member_stats "
      "TO " +
          new_accounts,
      "GRANT SELECT ON performance_schema.global_variables TO " + new_accounts,
      "GRANT INSERT, UPDATE, DELETE ON "
      "mysql_innodb_cluster_metadata.routers TO " +
          new_accounts,
      "GRANT INSERT, UPDATE, DELETE ON "
      "mysql_innodb_cluster_metadata.v2_routers TO " +
          new_accounts};
}

std::vector<std::string> ClusterMetadataGRV2::get_grant_statements(
    const std::string &new_accounts) const {
  return do_get_grant_statements(new_accounts);
}

std::vector<std::string> ClusterMetadataAR::get_grant_statements(
    const std::string &new_accounts) const {
  return do_get_grant_statements(new_accounts);
}

// default SQL_MODE as of 8.0.19
constexpr const char *kDefaultSqlMode =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,"
    "NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

stdx::expected<void, std::string> setup_metadata_session(
    MySQLSession &session) {
  try {
    session.execute(
        "SET @@SESSION.autocommit=1, @@SESSION.character_set_client=utf8, "
        "@@SESSION.character_set_results=utf8, "
        "@@SESSION.character_set_connection=utf8, @@SESSION.sql_mode='"s +
        kDefaultSqlMode + "', " +
        "@@SESSION.optimizer_switch='derived_merge=on'");

    try {
      session.execute("SET @@SESSION.group_replication_consistency='EVENTUAL'");
    } catch (const MySQLSession::Error &e) {
      if (e.code() == ER_UNKNOWN_SYSTEM_VARIABLE) {
        // ER_UNKNOWN_SYSTEM_VARIABLE is ok, means that this version does not
        // support group_replication_consistency so we don't have to worry about
        // it
      } else {
        throw e;
      }
    }
  } catch (const std::exception &e) {
    return stdx::unexpected(std::string(e.what()));
  }

  return {};
}

std::optional<InstanceType> str_to_instance_type(const std::string &s) {
  if (s == "group-member") {
    return InstanceType::GroupMember;
  } else if (s == "async-member") {
    return InstanceType::AsyncMember;
  } else if (s == "read-replica") {
    return InstanceType::ReadReplica;
  }

  return std::nullopt;
}

std::string to_string(const InstanceType instance_type) {
  switch (instance_type) {
    case InstanceType::GroupMember:
      return "group-member";
    case InstanceType::AsyncMember:
      return "async-member";
    case InstanceType::ReadReplica:
      return "read-replica";
    case InstanceType::Unsupported:
        /* fallthrough */;
  }

  return "unsupported";
}

std::string to_string(
    const TargetCluster::InvalidatedClusterRoutingPolicy policy) {
  if (policy == TargetCluster::InvalidatedClusterRoutingPolicy::AcceptRO) {
    return "accept_ro";
  }

  assert(policy == TargetCluster::InvalidatedClusterRoutingPolicy::DropAll);
  return "drop_all";
}

}  // namespace mysqlrouter
