/*
  Copyright (c) 2016, 2026, Oracle and/or its affiliates.

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

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

#include <errmsg.h>
#include <mysql.h>
#include <rapidjson/error/en.h>  // GetParseError_En

#include "configuration_update_schema.h"
#include "group_replication_metadata.h"
#include "log_suppressor.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/event_state_tracker.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysqlrouter/cluster_metadata_instance_attributes.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"  // strtoi_checked
#include "mysqlrouter/utils_sqlstring.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

using metadata_cache::LogSuppressor;
using mysql_harness::EventStateTracker;
using mysql_harness::logging::LogLevel;
using mysqlrouter::MySQLSession;
using mysqlrouter::sqlstring;
using namespace std::string_literals;
IMPORT_LOG_FUNCTIONS()

/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
std::string as_string(const char *input_str) {
  return {input_str == nullptr ? "" : input_str};
}

ClusterMetadata::ClusterMetadata(
    const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
    const mysqlrouter::SSLOptions &ssl_options)
    : session_config_(session_config) {
  if (ssl_options.mode.empty()) {
    ssl_mode_ = SSL_MODE_PREFERRED;  // default mode
  } else {
    try {
      ssl_mode_ = MySQLSession::parse_ssl_mode(ssl_options.mode);
      log_info("Connections using ssl_mode '%s'", ssl_options.mode.c_str());
    } catch (const std::logic_error &) {
      throw metadata_cache::metadata_error(
          "Error initializing metadata cache: invalid configuration item "
          "'ssl_mode=" +
          ssl_options.mode + "'");
    }
  }
  ssl_options_ = ssl_options;
}

/** @brief Destructor
 *
 * Disconnect and release the connection to the metadata node.
 * (RAII will close the connection in metadata_connection_)
 */
ClusterMetadata::~ClusterMetadata() = default;

bool ClusterMetadata::do_connect(MySQLSession &connection,
                                 const metadata_cache::metadata_server_t &mi) {
  try {
    connection.set_ssl_options(ssl_mode_, ssl_options_.tls_version,
                               ssl_options_.cipher, ssl_options_.ca,
                               ssl_options_.capath, ssl_options_.crl,
                               ssl_options_.crlpath);
    connection.connect(mi.hostname(), static_cast<unsigned int>(mi.port()),
                       session_config_.user_credentials.username,
                       session_config_.user_credentials.password,
                       "" /* unix-socket */, "" /* default-schema */,
                       session_config_.connect_timeout,
                       session_config_.read_timeout);
    return true;
  } catch (const MySQLSession::Error & /*e*/) {
    return false;  // error is logged in calling function
  }
}

bool ClusterMetadata::is_connected_to(
    const metadata_cache::metadata_server_t &metadata_server) const {
  if (metadata_connection_ && metadata_connection_->is_connected() &&
      metadata_connection_->get_address() == metadata_server.str()) {
    if (0 == metadata_connection_->ping()) {
      return true;
    }
  }

  return false;
}

stdx::expected<std::shared_ptr<mysqlrouter::MySQLSession>, std::string>
ClusterMetadata::make_connection_shared(
    const metadata_cache::metadata_server_t &metadata_server) {
  // use the existing metadata-connection if it goes to the same server.
  if (is_connected_to(metadata_server)) {
    return metadata_connection_;
  }

  auto connection = std::make_shared<MySQLSession>();

  if (!do_connect(*connection, metadata_server)) {
    return stdx::unexpected("Could not connect to the cluster member");
  }

  const auto result = mysqlrouter::setup_metadata_session(*connection);
  if (!result) {
    return stdx::unexpected("could not set up the metadata session: " +
                            result.error());
  }

  return connection;
}

bool ClusterMetadata::connect_and_setup_session(
    const metadata_cache::metadata_server_t &metadata_server) noexcept {
  if (is_connected_to(metadata_server)) {
    return true;
  }

  // Get a clean metadata server connection object
  // (RAII will close the old one if needed).
  try {
    metadata_connection_ = std::make_shared<MySQLSession>();
  } catch (const std::logic_error &e) {
    // defensive programming, shouldn't really happen
    log_error("Failed connecting with Metadata Server: %s", e.what());
    return false;
  }

  const bool connect_res = do_connect(*metadata_connection_, metadata_server);
  const auto connect_state =
      connect_res ? 0 : metadata_connection_->last_errno();
  const bool connect_res_changed = EventStateTracker::instance().state_changed(
      connect_state, EventStateTracker::EventId::MetadataServerConnectedOk,
      metadata_server.str());
  if (connect_res) {
    const auto result =
        mysqlrouter::setup_metadata_session(*metadata_connection_);
    if (result) {
      const auto log_level =
          connect_res_changed ? LogLevel::kInfo : LogLevel::kDebug;

      log_custom(log_level, "Connected with metadata server running on %s",
                 metadata_server.str().c_str());
      return true;
    } else {
      log_warning("Failed setting up the session on Metadata Server %s: %s",
                  metadata_server.str().c_str(), result.error().c_str());
    }

  } else {
    // connection attempt failed
    const auto log_level =
        connect_res_changed ? LogLevel::kWarning : LogLevel::kDebug;

    log_custom(log_level, "Failed connecting with Metadata Server %s: %s (%i)",
               metadata_server.str().c_str(),
               metadata_connection_->last_error(),
               metadata_connection_->last_errno());
  }

  metadata_connection_.reset();
  return false;
}

mysqlrouter::MetadataSchemaVersion
ClusterMetadata::get_and_check_metadata_schema_version(
    mysqlrouter::MySQLSession &session) {
  const auto version = mysqlrouter::get_metadata_schema_version(&session);

  if (version == mysqlrouter::kUpgradeInProgressMetadataVersion) {
    throw mysqlrouter::MetadataUpgradeInProgressException();
  }

  if (!metadata_schema_version_is_compatible(
          mysqlrouter::kRequiredRoutingMetadataSchemaVersion, version)) {
    throw metadata_cache::metadata_error(mysql_harness::utility::string_format(
        "Instance '%s': %s", session.get_address().c_str(),
        mysqlrouter::get_metadata_schema_uncompatible_msg(version).c_str()));
  }

  return version;
}

bool set_instance_ports(metadata_cache::ManagedInstance &instance,
                        const mysqlrouter::MySQLSession::Row &row,
                        const size_t classic_port_column,
                        const size_t x_port_column) {
  {
    const std::string classic_port = as_string(row[classic_port_column]);

    auto make_res = mysql_harness::make_tcp_destination(classic_port);
    if (!make_res) {
      log_warning(
          "Error parsing host:port in metadata for instance %s: '%s': %s",
          instance.mysql_server_uuid.c_str(), row[classic_port_column],
          make_res.error().message().c_str());
      return false;
    }

    instance.host = make_res->hostname();
    instance.port = make_res->port() != 0 ? make_res->port() : 3306;
  }

  // X protocol support is not mandatory
  if (row[x_port_column] && *row[x_port_column]) {
    const std::string x_port = as_string(row[x_port_column]);
    auto make_res = mysql_harness::make_tcp_destination(x_port);
    if (!make_res) {
      // There is a Shell bug (#27677227) that can cause the mysqlx port be
      // invalid in the metadata (>65535). For the backward compatibility we
      // need to tolerate this and still let the node be used for classic
      // connections (as the older Router versions did).

      // log_warning(
      //   "Error parsing host:xport in metadata for instance %s:"
      //   "'%s': %s",
      //   instance.mysql_server_uuid.c_str(), row[x_port_column],
      //   e.what());
      instance.xport = 0;
    } else {
      instance.xport = make_res->port() != 0 ? make_res->port() : 33060;
    }
  } else {
    instance.xport = instance.port * 10;
  }

  return true;
}

static sqlstring get_last_check_in_query(
    const mysqlrouter::MetadataSchemaVersion &md_version) {
  if (md_version >= mysqlrouter::kRouterStatsMetadataVersion) {
    return "INSERT INTO mysql_innodb_cluster_metadata.router_stats(router_id, "
           "last_check_in) VALUES (?, NOW()) ON DUPLICATE KEY UPDATE "
           "last_check_in = NOW()";
  } else {
    return "UPDATE mysql_innodb_cluster_metadata.v2_routers set "
           "last_check_in=NOW() where router_id = ?";
  }
}

bool ClusterMetadata::update_router_attributes(
    const metadata_cache::metadata_server_t &rw_server,
    const unsigned router_id,
    const metadata_cache::RouterAttributes &router_attributes) {
  auto connection_res = make_connection_shared(rw_server);
  if (!connection_res) {
    log_warning("Updating the router attributes in metadata failed: %s",
                connection_res.error().c_str());

    return false;
  }

  auto connection = std::move(*connection_res);

  MySQLSession::Transaction transaction(connection.get());
  // throws metadata_cache::metadata_error and
  // MetadataUpgradeInProgressException
  const auto md_version = get_and_check_metadata_schema_version(*connection);
  sqlstring query =
      "UPDATE mysql_innodb_cluster_metadata.v2_routers "
      "SET version = ?, attributes = "
      "JSON_SET(JSON_SET(JSON_SET(JSON_SET(JSON_SET(JSON_SET(JSON_SET( "
      "IF(attributes IS NULL, '{}', attributes), "
      "'$.RWEndpoint', ?), "
      "'$.ROEndpoint', ?), "
      "'$.RWSplitEndpoint', ?), "
      "'$.RWXEndpoint', ?), "
      "'$.ROXEndpoint', ?), "
      "'$.MetadataUser', ?), "
      "'$.Configuration', CAST(? as JSON)) "
      "WHERE router_id = ?";

  const auto &ra{router_attributes};
  query << MYSQL_ROUTER_VERSION << ra.rw_classic_port << ra.ro_classic_port
        << ra.rw_split_classic_port << ra.rw_x_port << ra.ro_x_port
        << ra.metadata_user_name
        << mysql_harness::DynamicConfig::instance().get_json_as_string(
               mysql_harness::DynamicConfig::ValueType::ConfiguredValue)
        << router_id << sqlstring::end;

  connection->execute(query);

  query = get_last_check_in_query(md_version);
  query << router_id << sqlstring::end;

  connection->execute(query);

  transaction.commit();

  return true;
}

bool ClusterMetadata::update_router_last_check_in(
    const metadata_cache::metadata_server_t &rw_server,
    const unsigned router_id) {
  auto connection_res = make_connection_shared(rw_server);
  if (!connection_res) {
    log_warning("Updating the last_check_in in metadata failed: %s",
                connection_res.error().c_str());

    return false;
  }

  auto connection = std::move(*connection_res);

  MySQLSession::Transaction transaction(connection.get());
  // throws metadata_cache::metadata_error and
  // MetadataUpgradeInProgressException
  const auto md_version = get_and_check_metadata_schema_version(*connection);

  sqlstring query = get_last_check_in_query(md_version);
  query << router_id << sqlstring::end;
  try {
    connection->execute(query);
  } catch (const std::exception &e) {
    log_warning("Updating the router last_check_in in metadata failed: %s",
                e.what());
  }

  transaction.commit();
  return true;
}

void ClusterMetadata::report_guideline_name(
    const std::string &guideline_name,
    const metadata_cache::metadata_server_t &rw_server,
    const unsigned router_id) {
  try {
    auto connection_res = make_connection_shared(rw_server);
    if (!connection_res) {
      log_warning("Updating the router guideline name in metadata failed: %s",
                  connection_res.error().c_str());

      return;
    }

    auto connection = std::move(*connection_res);

    MySQLSession::Transaction transaction(connection.get());
    // throws metadata_cache::metadata_error and
    // MetadataUpgradeInProgressException
    get_and_check_metadata_schema_version(*connection);

    sqlstring query(
        "UPDATE mysql_innodb_cluster_metadata.v2_routers SET "
        "attributes = JSON_SET(attributes, '$.CurrentRoutingGuideline', ?) "
        "WHERE router_id = ? ");

    if (guideline_name.empty()) {
      query << sqlstring::null;
    } else {
      query << guideline_name;
    }

    query << router_id << sqlstring::end;

    connection->execute(query);
    transaction.commit();
  } catch (const std::exception &e) {
    log_warning("Updating the routing guideline name in metadata failed: %s",
                e.what());
  }
}

static std::string get_limit_target_cluster_clause(
    const mysqlrouter::TargetCluster &target_cluster,
    const mysqlrouter::ClusterType &cluster_type,
    mysqlrouter::MySQLSession &session) {
  switch (target_cluster.target_type()) {
    case mysqlrouter::TargetCluster::TargetType::ByUUID:
      if (cluster_type == mysqlrouter::ClusterType::RS_V2) {
        return session.quote(target_cluster.to_string());
      } else {
        return "(SELECT cluster_id FROM "
               "mysql_innodb_cluster_metadata.v2_gr_clusters C WHERE "
               "C.group_name = " +
               session.quote(target_cluster.to_string()) + ")";
      }
    case mysqlrouter::TargetCluster::TargetType::ByName:
      return "(SELECT cluster_id FROM "
             "mysql_innodb_cluster_metadata.v2_clusters WHERE cluster_name=" +
             session.quote(target_cluster.to_string()) + ")";
    default:
      assert(mysqlrouter::TargetCluster::TargetType::ByPrimaryRole ==
             target_cluster.target_type());
      return "(SELECT C.cluster_id FROM "
             "mysql_innodb_cluster_metadata.v2_gr_clusters C left join "
             "mysql_innodb_cluster_metadata.v2_cs_members CSM on "
             "CSM.cluster_id = "
             "C.cluster_id WHERE CSM.member_role = 'PRIMARY' and "
             "CSM.clusterset_id = " +
             session.quote(target_cluster.to_string()) + ")";
  }
}

ClusterMetadata::auth_credentials_t ClusterMetadata::fetch_auth_credentials(
    const metadata_cache::metadata_server_t &md_server,
    const mysqlrouter::TargetCluster &target_cluster) {
  ClusterMetadata::auth_credentials_t auth_credentials;

  auto connection_res = make_connection_shared(md_server);
  if (!connection_res) {
    log_warning("Fetching the auth credentials from metadata failed: %s",
                connection_res.error().c_str());

    return {};
  }

  auto connection = std::move(*connection_res);

  const std::string query =
      "SELECT user, authentication_string, privileges, authentication_method "
      "FROM mysql_innodb_cluster_metadata.v2_router_rest_accounts WHERE "
      "cluster_id="s +
      get_limit_target_cluster_clause(target_cluster, get_cluster_type(),
                                      *connection.get());

  auto result_processor =
      [&auth_credentials](const MySQLSession::Row &row) -> bool {
    JsonDocument privileges;
    if (row[2] != nullptr) privileges.Parse<0>(as_string(row[2]));

    const auto username = as_string(row[0]);
    if (privileges.HasParseError()) {
      log_warning(
          "Skipping user '%s': invalid privilege format '%s', authentication "
          "will not be possible",
          username.c_str(), as_string(row[2]).c_str());
    } else if (as_string(row[3]) != "modular_crypt_format") {
      log_warning(
          "Skipping user '%s': authentication method '%s' is not supported for "
          "metadata_cache authentication",
          username.c_str(), as_string(row[3]).c_str());
    } else {
      auth_credentials[username] =
          std::make_pair(as_string(row[1]), std::move(privileges));
    }
    return true;
  };

  connection->query(query, result_processor);
  return auth_credentials;
}

stdx::expected<std::string, std::error_code>
ClusterMetadata::fetch_routing_guidelines_document(const uint16_t router_id) {
  if (!metadata_connection_) {
    log_debug(
        "Could not fetch routing guidelines file: no metadata connection");
    return stdx::unexpected(make_error_code(
        metadata_cache::metadata_errc::no_metadata_server_reached));
  }

  const auto &version =
      get_and_check_metadata_schema_version(*metadata_connection_);

  const auto &query = get_select_routing_guidelines_query(version, router_id);
  if (!query) {
    const bool first_time = EventStateTracker::instance().state_changed(
        true, EventStateTracker::EventId::GuidelinesNotSupported);
    if (first_time) {
      log_warning(
          "Could not fetch routing guidelines: metadata schema version not "
          "supported");
    }
    return stdx::unexpected(query.error());
  }

  stdx::expected<std::string, std::error_code> result;
  auto result_processor = [&result](const MySQLSession::Row &row) {
    if (as_string(row[0]).empty()) {
      result = stdx::unexpected(
          make_error_code(routing_guidelines::routing_guidelines_errc::
                              empty_routing_guidelines));
      return false;
    }

    JsonDocument guidelines;
    if (row[0] != nullptr) guidelines.Parse<0>(as_string(row[0]));

    if (guidelines.HasParseError()) {
      log_warning("Unable to parse routing guidelines document: %s",
                  rapidjson::GetParseError_En(guidelines.GetParseError()));
      result = stdx::unexpected(make_error_code(
          routing_guidelines::routing_guidelines_errc::parse_error));
      return false;
    }

    if (guidelines.HasMember("version") && !guidelines["version"].IsString()) {
      log_warning(
          "Unable to parse routing guidelines document: 'version' must be a "
          "string value");
      result = stdx::unexpected(make_error_code(
          routing_guidelines::routing_guidelines_errc::parse_error));
      return false;
    }
    const std::string version = guidelines.HasMember("version")
                                    ? guidelines["version"].GetString()
                                    : "1.0";

    const auto &supported_version =
        mysqlrouter::get_routing_guidelines_supported_version();
    if (!mysqlrouter::routing_guidelines_version_is_compatible(
            supported_version,
            mysqlrouter::routing_guidelines_version_from_string(version))) {
      log_warning(
          "Update guidelines failed - routing guidelines version not "
          "supported. Router supported version is %s but got %s",
          to_string(supported_version).c_str(), version.c_str());
      result = stdx::unexpected(make_error_code(
          routing_guidelines::routing_guidelines_errc::unsupported_version));
      return false;
    }

    result = as_string(row[0]);
    return true;
  };

  metadata_connection_->query(query.value(), result_processor);
  return result;
}

std::optional<routing_guidelines::Router_info>
ClusterMetadata::fetch_router_info(const uint16_t router_id) {
  routing_guidelines::Router_info router_info;
  if (!metadata_connection_) {
    return std::nullopt;
  }
  sqlstring query =
      "SELECT address, "
      "attributes->>'$.ROEndpoint', "
      "attributes->>'$.RWEndpoint', "
      "attributes->>'$.RWSplitEndpoint', "
      "attributes->>'$.ROXEndpoint', "
      "attributes->>'$.RWXEndpoint', "
      "attributes->>'$.LocalCluster', "
      "options, router_name FROM mysql_innodb_cluster_metadata.v2_routers "
      "WHERE router_id=?";
  query << router_id << sqlstring::end;

  auto result_processor = [&router_info](const MySQLSession::Row &row) -> bool {
    router_info.hostname = as_string(row[0]);

    const auto ro_port = mysqlrouter::strtoi_checked(row[1]);
    router_info.port_ro =
        ro_port != 0 ? ro_port : mysqlrouter::strtoi_checked(row[4]);
    const auto rw_port = mysqlrouter::strtoi_checked(row[2]);
    router_info.port_rw =
        rw_port != 0 ? rw_port : mysqlrouter::strtoi_checked(row[5]);
    router_info.port_rw_split = mysqlrouter::strtoi_checked(row[3]);

    router_info.local_cluster = as_string(row[6]);

    if (row[7] != nullptr) {
      std::string tags_str = as_string(row[7]);
      if (!tags_str.empty()) {
        const auto &tags_result =
            mysqlrouter::InstanceAttributes::get_tags(tags_str);
        if (tags_result) {
          router_info.tags = *tags_result;
        } else {
          log_warning("Error parsing router tags JSON string: %s",
                      tags_result.error().c_str());
        }
      }
    }

    router_info.name = as_string(row[8]);

    return true;
  };

  metadata_connection_->query(query, result_processor);

  return router_info;
}

std::optional<metadata_cache::metadata_server_t>
ClusterMetadata::find_rw_server(
    const std::vector<metadata_cache::ManagedInstance> &instances) {
  for (const auto &instance : instances) {
    if (instance.mode == metadata_cache::ServerMode::ReadWrite) {
      return metadata_cache::metadata_server_t{instance.host, instance.port};
    }
  }

  return {};
}

std::optional<metadata_cache::metadata_server_t>
ClusterMetadata::find_rw_server(
    const std::vector<metadata_cache::ManagedCluster> &clusters) {
  for (const auto &cluster : clusters) {
    if (cluster.is_primary && cluster.has_quorum && !cluster.is_invalidated)
      return find_rw_server(cluster.members);
  }

  return {};
}

void set_instance_attributes(metadata_cache::ManagedInstance &instance,
                             const std::string &attributes) {
  auto &log_suppressor = LogSuppressor::instance();

  instance.attributes = attributes;

  const auto &tags_result =
      mysqlrouter::InstanceAttributes::get_tags(attributes);
  if (tags_result) instance.tags = tags_result.value();

  // we want to log the warning only when it's changing
  const std::string tags_msg =
      tags_result
          ? "Successfully parsed tags from attributes JSON string"
          : "Error parsing attributes JSON string: " + tags_result.error();

  log_suppressor.log_message(LogSuppressor::MessageId::kServerTags,
                             instance.mysql_server_uuid, tags_msg,
                             !tags_result);

  const auto default_instance_type = instance.type;
  const auto type_attr = mysqlrouter::InstanceAttributes::get_instance_type(
      attributes, default_instance_type);

  if (type_attr) {
    instance.type = *type_attr;
  }

  // we want to log the warning only when it's changing
  const std::string message =
      type_attr
          ? "Successfully parsed instance_type from attributes JSON string"
          : "Error parsing instance_type from attributes JSON string: " +
                type_attr.error();

  log_suppressor.log_message(LogSuppressor::MessageId::kInstanceType,
                             instance.mysql_server_uuid, message, !type_attr);

  if (instance.type == mysqlrouter::InstanceType::ReadReplica) {
    instance.mode = metadata_cache::ServerMode::ReadOnly;
  }

  const auto hidden_attr = mysqlrouter::InstanceAttributes::get_hidden(
      instance.tags, mysqlrouter::kNodeTagHiddenDefault);

  instance.hidden =
      hidden_attr ? hidden_attr.value() : mysqlrouter::kNodeTagHiddenDefault;

  // we want to log the warning only when it's changing
  const std::string message2 =
      hidden_attr ? "Successfully parsed _hidden from attributes JSON string"
                  : "Error parsing _hidden from attributes JSON string: " +
                        hidden_attr.error();

  log_suppressor.log_message(LogSuppressor::MessageId::kHidden,
                             instance.mysql_server_uuid, message2,
                             !hidden_attr);

  const auto disconnect_existing_sessions_when_hidden_attr = mysqlrouter::
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          instance.tags, mysqlrouter::kNodeTagDisconnectWhenHiddenDefault);

  instance.disconnect_existing_sessions_when_hidden =
      disconnect_existing_sessions_when_hidden_attr
          ? disconnect_existing_sessions_when_hidden_attr.value()
          : mysqlrouter::kNodeTagDisconnectWhenHiddenDefault;

  // we want to log the warning only when it's changing
  const std::string message3 =
      disconnect_existing_sessions_when_hidden_attr
          ? "Successfully parsed _disconnect_existing_sessions_when_hidden "
            "from attributes JSON string"
          : "Error parsing _disconnect_existing_sessions_when_hidden from "
            "attributes JSON string: " +
                disconnect_existing_sessions_when_hidden_attr.error();

  log_suppressor.log_message(
      LogSuppressor::MessageId::kDisconnectExistingSessionsWhenHidden,
      instance.mysql_server_uuid, message3,
      !disconnect_existing_sessions_when_hidden_attr);
}
