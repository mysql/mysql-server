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

#include "router_cs_options.h"

#include <rapidjson/writer.h>

#include "mysql/harness/event_state_tracker.h"
#include "mysql/harness/logging/logging.h"

using mysql_harness::EventStateTracker;
using mysql_harness::logging::LogLevel;
using mysqlrouter::MySQLSession;

IMPORT_LOG_FUNCTIONS()

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

  options_str_ = as_string((*row)[0]);

  return true;
}

std::optional<mysqlrouter::TargetCluster>
RouterClusterSetOptions::get_target_cluster(const unsigned router_id) const {
  std::string out_error;
  // check if we have a target cluster assigned in the metadata
  std::string target_cluster_str =
      get_router_option_str(options_str_, "target_cluster", "", out_error);
  mysqlrouter::TargetCluster target_cluster;

  if (!out_error.empty()) {
    log_error("Error reading target_cluster from the router.options: %s",
              out_error.c_str());
    return {};
  }

  const std::string invalidated_cluster_routing_policy_str =
      get_router_option_str(options_str_, "invalidated_cluster_policy", "",
                            out_error);

  if (invalidated_cluster_routing_policy_str == "accept_ro") {
    target_cluster.invalidated_cluster_routing_policy(
        mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::AcceptRO);
  } else {
    // this is the default strategy
    target_cluster.invalidated_cluster_routing_policy(
        mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy::DropAll);
  }

  const bool target_cluster_in_options = !target_cluster_str.empty();
  const bool target_cluster_in_options_changed =
      EventStateTracker::instance().state_changed(
          target_cluster_in_options,
          EventStateTracker::EventId::TargetClusterPresentInOptions);

  if (!target_cluster_in_options) {
    const auto log_level = target_cluster_in_options_changed
                               ? LogLevel::kWarning
                               : LogLevel::kDebug;
    log_custom(log_level,
               "Target cluster for router_id=%d not set, using 'primary' as "
               "a target cluster",
               router_id);
    target_cluster_str = "primary";
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
  std::string out_error;
  auto stats_updates_frequency = std::chrono::seconds(get_router_option_uint(
      options_str_, "stats_updates_frequency", 0, out_error));
  if (!out_error.empty()) {
    log_warning(
        "Error parsing stats_updates_frequency from the router.options: %s. "
        "Using default value %u",
        out_error.c_str(), 0);
    return 0s;
  }

  return stats_updates_frequency;
}

bool RouterClusterSetOptions::get_use_replica_primary_as_rw() const {
  std::string out_error;
  auto result = get_router_option_bool(
      options_str_, "use_replica_primary_as_rw", false, out_error);
  if (!out_error.empty()) {
    log_warning(
        "Error parsing use_replica_primary_as_rw from the router.options: "
        "%s. Using default value 'false'",
        out_error.c_str());
    return false;
  }

  return result;
}

std::string RouterClusterSetOptions::get_router_option_str(
    const std::string &options, const std::string &name,
    const std::string &default_value, std::string &out_error) const {
  out_error = "";
  if (options.empty()) return default_value;

  rapidjson::Document json_doc;
  json_doc.Parse(options);

  if (json_doc.HasParseError() || !json_doc.IsObject()) {
    out_error = "not a valid JSON object";
    return default_value;
  }

  const auto it = json_doc.FindMember(name);
  if (it == json_doc.MemberEnd()) {
    return default_value;
  }

  if (!it->value.IsString()) {
    out_error = "options." + name + " not a string";
    return default_value;
  }

  return it->value.GetString();
}

uint32_t RouterClusterSetOptions::get_router_option_uint(
    const std::string &options, const std::string &name,
    const uint32_t &default_value, std::string &out_error) const {
  out_error = "";
  if (options.empty()) return default_value;

  rapidjson::Document json_doc;
  json_doc.Parse(options);

  if (json_doc.HasParseError() || !json_doc.IsObject()) {
    out_error = "not a valid JSON object";
    return default_value;
  }

  const auto it = json_doc.FindMember(name);
  if (it == json_doc.MemberEnd()) {
    return default_value;
  }

  if (!it->value.IsUint()) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    it->value.Accept(writer);
    out_error =
        "options." + name + "='" + sb.GetString() + "'; not an unsigned int";
    return default_value;
  }

  return it->value.GetUint();
}

uint32_t RouterClusterSetOptions::get_router_option_bool(
    const std::string &options, const std::string &name,
    const bool &default_value, std::string &out_error) const {
  out_error = "";
  if (options.empty()) return default_value;

  rapidjson::Document json_doc;
  json_doc.Parse(options);

  if (json_doc.HasParseError() || !json_doc.IsObject()) {
    out_error = "not a valid JSON object";
    return default_value;
  }

  const auto it = json_doc.FindMember(name);
  if (it == json_doc.MemberEnd()) {
    return default_value;
  }

  if (!it->value.IsBool()) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    it->value.Accept(writer);
    out_error = "options." + name + "='" + sb.GetString() + "'; not a boolean";
    return default_value;
  }

  return it->value.GetBool();
}
