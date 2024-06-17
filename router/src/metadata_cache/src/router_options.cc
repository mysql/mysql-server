/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include "router_options.h"

#include <rapidjson/document.h>
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
      return stdx::unexpected("not a valid JSON object");
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return std::nullopt;
    }

    if (!it->value.IsString()) {
      return stdx::unexpected("options." + name + " not a string");
    }

    return it->value.GetString();
  }

  static stdx::expected<std::optional<uint32_t>, std::string>
  get_router_option_uint(const std::string &options, const std::string &name,
                         const std::optional<uint32_t> default_value) {
    if (options.empty()) return default_value;

    rapidjson::Document json_doc;
    json_doc.Parse(options);

    if (!json_doc.IsObject()) {
      return stdx::unexpected("not a valid JSON object");
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsUint()) {
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      it->value.Accept(writer);
      return stdx::unexpected("options." + name + "='" + sb.GetString() +
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
      return stdx::unexpected("not a valid JSON object");
    }

    const auto it = json_doc.FindMember(name);
    if (it == json_doc.MemberEnd()) {
      return default_value;
    }

    if (!it->value.IsBool()) {
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      it->value.Accept(writer);

      return stdx::unexpected("options." + name + "='" + sb.GetString() +
                              "'; not a boolean");
    }

    return it->value.GetBool();
  }
};

}  // namespace

std::optional<mysqlrouter::TargetCluster> RouterOptions::get_target_cluster()
    const {
  std::string out_error;
  // check if we have a target cluster assigned in the metadata
  const auto target_cluster_op = MetadataJsonOptions::get_router_option_str(
      options_str_, "target_cluster");
  mysqlrouter::TargetCluster target_cluster;

  if (!target_cluster_op) {
    log_error("Error reading target_cluster from the router_options: %s",
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
               router_id_);
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

std::optional<std::chrono::seconds> RouterOptions::get_stats_updates_frequency()
    const {
  using namespace std::chrono_literals;
  using SecOptional = std::optional<std::chrono::seconds>;
  // the default in case of ClusterSet is 0s (never update), for standalone
  // Cluster it is std::nullopt (update every 10th TTL)
  using UintOptional = std::optional<uint32_t>;
  const UintOptional default_value_uint =
      cluster_type_ == mysqlrouter::ClusterType::GR_CS
          ? UintOptional{0}
          : UintOptional{std::nullopt};
  const auto stats_updates_frequency_op =
      MetadataJsonOptions::get_router_option_uint(
          options_str_, "stats_updates_frequency", default_value_uint);

  if (!stats_updates_frequency_op) {
    const SecOptional default_value_sec =
        cluster_type_ == mysqlrouter::ClusterType::GR_CS
            ? SecOptional{0s}
            : SecOptional{std::nullopt};
    log_warning(
        "Error parsing stats_updates_frequency from the router_options: %s. "
        "Using default value",
        stats_updates_frequency_op.error().c_str());
    return default_value_sec;
  }

  if (!stats_updates_frequency_op.value()) {
    return SecOptional{std::nullopt};
  }

  return SecOptional{std::chrono::seconds(*stats_updates_frequency_op.value())};
}

bool RouterOptions::get_use_replica_primary_as_rw() const {
  auto result = MetadataJsonOptions::get_router_option_bool(
      options_str_, "use_replica_primary_as_rw", false);
  if (!result) {
    log_warning(
        "Error parsing use_replica_primary_as_rw from the router_options: "
        "%s. Using default value 'false'",
        result.error().c_str());
    return false;
  }

  return result.value();
}

bool RouterOptions::read_from_metadata(
    mysqlrouter::MySQLSession &session, const unsigned router_id,
    const mysqlrouter::MetadataSchemaVersion schema_version,
    const mysqlrouter::ClusterType cluster_type) {
  router_id_ = router_id;
  cluster_type_ = cluster_type;
  const bool router_options_view_exists =
      schema_version >= mysqlrouter::MetadataSchemaVersion{2, 2, 0};

  std::string options_view;
  if (router_options_view_exists) {
    options_view = "v2_router_options";
  } else if (cluster_type == mysqlrouter::ClusterType::GR_CS) {
    // before the v2_router_options view was added (metadata 2.2),
    // ClusterSet-related options were read from v2_cs_router_options, now
    // v2_router_options is the superset of it and should be used instead
    options_view = "v2_cs_router_options";
  } else {
    options_str_ = "";
    return true;
  }

  const std::string query =
      "SELECT router_options FROM mysql_innodb_cluster_metadata." +
      options_view + " WHERE router_id = " + std::to_string(router_id);

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    log_error(
        "Error reading options from %s : did not find router entry for "
        "router_id '%u'",
        options_view.c_str(), router_id);
    return false;
  }

  const std::string options = (*row)[0] == nullptr ? "" : (*row)[0];

  if (options != options_str_) {
    log_info("New router options read from the metadata '%s', was '%s'",
             options.c_str(), options_str_.c_str());
    options_str_ = options;
  }

  return true;
}

ReadOnlyTargets RouterOptions::get_read_only_targets() const {
  ReadOnlyTargets result = kDefaultReadOnlyTargets;
  auto &log_suppressor = LogSuppressor::instance();
  std::string warning;
  const auto mode_op = MetadataJsonOptions::get_router_option_str(
      options_str_, "read_only_targets");

  if (!mode_op) {
    warning = "Error reading read_only_targets from the router_options: " +
              mode_op.error() + ". Using default value.";
  } else {
    if (!mode_op.value()) {
      // value not in options, use default
    } else if (*mode_op.value() == "all") {
      result = ReadOnlyTargets::all;
    } else if (*mode_op.value() == "read_replicas") {
      result = ReadOnlyTargets::read_replicas;
    } else if (*mode_op.value() == "secondaries") {
      result = ReadOnlyTargets::secondaries;
    } else {
      warning = "Unknown read_only_targets read from the metadata: '" +
                *mode_op.value() + "'. Using default value. (" + options_str_ +
                ")";
    }
  }

  // we want to log the warning only when it's changing
  std::string message;
  if (!warning.empty()) {
    message =
        "Error parsing read_only_targets from options JSON string: " + warning +
        "; Using '" + to_string(result) + "' value";
  } else {
    message = "Using read_only_targets='" + to_string(result) + "'";
  }

  log_suppressor.log_message(LogSuppressor::MessageId::kReadOnlyTargets, "",
                             message, !warning.empty(),
                             mysql_harness::logging::LogLevel::kWarning,
                             mysql_harness::logging::LogLevel::kInfo, true);
  return result;
}

std::string to_string(const ReadOnlyTargets mode) {
  switch (mode) {
    case ReadOnlyTargets::all:
      return "all";
    case ReadOnlyTargets::read_replicas:
      return "read_replicas";
    default:
      return "secondaries";
  }
}

QuorumConnectionLostAllowTraffic
RouterOptions::get_unreachable_quorum_allowed_traffic() const {
  QuorumConnectionLostAllowTraffic result =
      kDefaultQuorumConnectionLostAllowTraffic;
  auto &log_suppressor = LogSuppressor::instance();
  std::string warning;
  const auto mode_op = MetadataJsonOptions::get_router_option_str(
      options_str_, "unreachable_quorum_allowed_traffic");

  if (!mode_op) {
    warning =
        "Error reading unreachable_quorum_allowed_traffic from the "
        "router_options: " +
        mode_op.error() + ". Using default value.";
  } else {
    if (!mode_op.value()) {
      // value not in options, use default
    } else if (*mode_op.value() == "none") {
      result = QuorumConnectionLostAllowTraffic::none;
    } else if (*mode_op.value() == "read") {
      result = QuorumConnectionLostAllowTraffic::read;
    } else if (*mode_op.value() == "all") {
      result = QuorumConnectionLostAllowTraffic::all;
    } else {
      warning =
          "Unknown unreachable_quorum_allowed_traffic read from the "
          "metadata: '" +
          *mode_op.value() + "'. Using default value. (" + options_str_ + ")";
    }
  }

  // we want to log the warning only when it's changing
  std::string message;
  if (!warning.empty()) {
    message =
        "Error parsing unreachable_quorum_allowed_traffic from options JSON "
        "string: " +
        warning + "; Using '" + to_string(result) + "' value";
  } else {
    message =
        "Using unreachable_quorum_allowed_traffic='" + to_string(result) + "'";
  }

  log_suppressor.log_message(
      LogSuppressor::MessageId::kUnreachableQuorumAllowedTraffic, "", message,
      !warning.empty(), mysql_harness::logging::LogLevel::kWarning,
      mysql_harness::logging::LogLevel::kInfo, true);
  return result;
}

std::string to_string(const QuorumConnectionLostAllowTraffic allow) {
  switch (allow) {
    case QuorumConnectionLostAllowTraffic::read:
      return "read";
    case QuorumConnectionLostAllowTraffic::all:
      return "all";
    default:
      return "none";
  }
}