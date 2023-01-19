/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

// enable using Rapidjson library with std::string
#define RAPIDJSON_HAS_STDSTRING 1

#include "router_options.h"

#include <rapidjson/writer.h>

#include "log_suppressor.h"
#include "mysql/harness/event_state_tracker.h"
#include "mysql/harness/logging/logging.h"

using metadata_cache::LogSuppressor;
using mysql_harness::EventStateTracker;
using mysql_harness::logging::LogLevel;
using mysqlrouter::MySQLSession;

IMPORT_LOG_FUNCTIONS()

namespace {
class MetadataJsonOptions {
 public:
  static stdx::expected<std::optional<std::string>, std::string>
  get_router_option_str(const std::string &options, const std::string &name) {
    if (options.empty()) return std::nullopt;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (!json_doc.IsObject()) {
      return stdx::make_unexpected("not a valid JSON object");
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return std::nullopt;
    }

    if (!it->value.IsString()) {
      return stdx::make_unexpected("options." + name + " not a string");
    }

    return it->value.GetString();
  }

  static stdx::expected<uint32_t, std::string> get_router_option_uint(
      const std::string &options, const std::string &name,
      const uint32_t default_value) {
    if (options.empty()) return default_value;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (!json_doc.IsObject()) {
      return stdx::make_unexpected("not a valid JSON object");
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsUint()) {
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      it->value.Accept(writer);
      return stdx::make_unexpected("options." + name + "='" + sb.GetString() +
                                   "'; not an unsigned int");
    }

    return it->value.GetUint();
  }

  static stdx::expected<bool, std::string> get_router_option_bool(
      const std::string &options, const std::string &name,
      const bool default_value) {
    if (options.empty()) return default_value;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (json_doc.HasParseError() || !json_doc.IsObject()) {
      return stdx::make_unexpected("not a valid JSON object");
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsBool()) {
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      it->value.Accept(writer);

      return stdx::make_unexpected("options." + name + "='" + sb.GetString() +
                                   "'; not a boolean");
    }

    return it->value.GetBool();
  }
};

}  // namespace

bool RouterClusterSetOptions::read_from_metadata(
    mysqlrouter::MySQLSession &session, const unsigned router_id) {
  const std::string query =
      "SELECT router_options FROM "
      "mysql_innodb_cluster_metadata.v2_cs_router_options where router_id "
      "= " +
      std::to_string(router_id);

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    log_error(
        "Error reading router.options from v2_cs_router_options: did not "
        "find router entry for router_id '%u'",
        router_id);
    return false;
  }

  options_str_ = ::get_string((*row)[0]);

  return true;
}

std::optional<mysqlrouter::TargetCluster>
RouterClusterSetOptions::get_target_cluster(const unsigned router_id) const {
  std::string out_error;
  // check if we have a target cluster assigned in the metadata
  const auto target_cluster_op = MetadataJsonOptions::get_router_option_str(
      options_str_, "target_cluster");
  mysqlrouter::TargetCluster target_cluster;

  if (!target_cluster_op) {
    log_error("Error reading target_cluster from the router.options: %s",
              target_cluster_op.error().c_str());
    return {};
  }

  const auto invalidated_cluster_routing_policy_str =
      MetadataJsonOptions::get_router_option_str(options_str_,
                                                 "invalidated_cluster_policy");

  if (invalidated_cluster_routing_policy_str &&
      *invalidated_cluster_routing_policy_str == "accept_ro") {
    target_cluster.invalidated_cluster_routing_policy(
        mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::AcceptRO);
  } else {
    // this is the default strategy
    target_cluster.invalidated_cluster_routing_policy(
        mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::DropAll);
  }

  const bool target_cluster_in_options_changed =
      EventStateTracker::instance().state_changed(
          static_cast<bool>(target_cluster_op),
          EventStateTracker::EventId::TargetClusterPresentInOptions);

  std::string target_cluster_str;
  if (!target_cluster_op || !target_cluster_op.value() ||
      (*target_cluster_op.value()).empty()) {
    const auto log_level = target_cluster_in_options_changed
                               ? LogLevel::kWarning
                               : LogLevel::kDebug;
    log_custom(log_level,
               "Target cluster for router_id=%d not set, using 'primary' as "
               "a target cluster",
               router_id);
    target_cluster_str = "primary";
  } else {
    target_cluster_str = *target_cluster_op.value();
  }

  if (target_cluster_str == "primary") {
    target_cluster.target_type(
        mysqlrouter::TargetCluster::TargetType::ByPrimaryRole);
    target_cluster.target_value("");
  } else {
    target_cluster.target_type(mysqlrouter::TargetCluster::TargetType::ByUUID);
    target_cluster.target_value(target_cluster_str);
  }

  return target_cluster;
}

std::chrono::seconds RouterClusterSetOptions::get_stats_updates_frequency()
    const {
  using namespace std::chrono_literals;
  const auto stats_updates_frequency_op =
      MetadataJsonOptions::get_router_option_uint(options_str_,
                                                  "stats_updates_frequency", 0);
  if (!stats_updates_frequency_op) {
    log_warning(
        "Error parsing stats_updates_frequency from the router.options: %s. "
        "Using default value %u",
        stats_updates_frequency_op.error().c_str(), 0);
    return 0s;
  }

  return std::chrono::seconds(stats_updates_frequency_op.value());
}

bool RouterClusterSetOptions::get_use_replica_primary_as_rw() const {
  auto result = MetadataJsonOptions::get_router_option_bool(
      options_str_, "use_replica_primary_as_rw", false);
  if (!result) {
    log_warning(
        "Error parsing use_replica_primary_as_rw from the router.options: "
        "%s. Using default value 'false'",
        result.error().c_str());
    return false;
  }

  return result.value();
}

bool RouterOptions::read_from_metadata(mysqlrouter::MySQLSession &session,
                                       const unsigned router_id) {
  std::string query;
  const bool use_options_view =
      schema_version_ >= mysqlrouter::MetadataSchemaVersion{2, 2, 0};

  if (use_options_view) {
    query =
        "SELECT router_options FROM "
        "mysql_innodb_cluster_metadata.v2_router_options WHERE "
        "router_id = " +
        std::to_string(router_id);
  } else {
    query =
        "SELECT options FROM mysql_innodb_cluster_metadata.v2_routers WHERE "
        "router_id = " +
        std::to_string(router_id);
  }

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    log_error(
        "Error reading options from %s: did not "
        "find router entry for router_id '%u'",
        use_options_view ? "v2_router_options" : "v2_routers", router_id);
    return false;
  }

  options_str_ = ::get_string((*row)[0]);

  return true;
}

ReadReplicasMode RouterOptions::get_read_replicas_mode() const {
  ReadReplicasMode result = kDefaultReadReplicasMode;
  auto &log_suppressor = LogSuppressor::instance();
  std::string warning;
  const auto mode_op = MetadataJsonOptions::get_router_option_str(
      options_str_, "read_replicas_mode");

  if (!mode_op) {
    warning = "Error reading read_replicas_mode from the v2_routers.options: " +
              mode_op.error() + ". Using default mode.";
  } else {
    if (!mode_op.value()) {
      // value not in options, use default
    } else if (*mode_op.value() == "append") {
      result = ReadReplicasMode::append;
    } else if (*mode_op.value() == "replace") {
      result = ReadReplicasMode::replace;
    } else if (*mode_op.value() == "ignore") {
      result = ReadReplicasMode::ignore;
    } else {
      warning = "Unknown read_replicas_mode read from the metadata: '" +
                *mode_op.value() + "'. Using default mode. (" + options_str_ +
                ")";
    }
  }

  // we want to log the warning only when it's changing
  std::string message;
  if (!warning.empty()) {
    message = "Error parsing read_replicas_mode from options JSON string: " +
              warning + "; Using '" + to_string(result) + "' mode";
  } else {
    message = "Using read_replicas_mode='" + to_string(result) + "'";
  }

  log_suppressor.log_message(LogSuppressor::MessageId::kReadReplicasMode, "",
                             message, !warning.empty(),
                             mysql_harness::logging::LogLevel::kWarning,
                             mysql_harness::logging::LogLevel::kInfo, true);
  return result;
}

std::string to_string(const ReadReplicasMode mode) {
  switch (mode) {
    case ReadReplicasMode::append:
      return "append";
    case ReadReplicasMode::replace:
      return "replace";
    default:
      return "ignore";
  }
}