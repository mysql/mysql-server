/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "routing_simulator.h"

#include <string_view>
#include <vector>

#include "utils.h"  // format_json_error

#include "mysql/harness/utility/string.h"  // mysql_harness::join

namespace routing_guidelines {

Routing_simulator::Routing_simulator(
    rpn::Context *external_context,
    Routing_guidelines_engine::ResolveCache *external_cache)
    : external_context_(external_context), external_cache_(external_cache) {}

stdx::expected<void, std::string> Routing_simulator::process_document(
    const std::string &s) {
  try {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(s);
    if (!ok) {
      return stdx::unexpected(format_json_error(s, ok, 15));
    }

    if (!d.IsObject()) {
      return stdx::unexpected("No JSON object found");
    }

    const auto &it = d.FindMember("type");
    if (it == d.MemberEnd()) {
      if (!rpd_) {
        rpd_ = std::make_unique<Routing_guidelines_engine>(
            Routing_guidelines_engine::create(s));
      } else {
        auto new_rp = Routing_guidelines_engine::create(s);
        rpd_->update_routing_guidelines(std::move(new_rp));
      }
      return {};
    }

    const std::string_view type = it->value.GetString();
    const auto colon_pos = type.find(":");
    auto expected_name =
        colon_pos == std::string::npos ? "" : type.substr(colon_pos + 1);

    if (type == "router") {
      return parse_router(d);
    } else if (type.starts_with("destination")) {
      return parse_destination(d, expected_name);
    } else if (type.starts_with("source")) {
      return parse_source(d, expected_name);
    } else if (type.starts_with("sql")) {
      return parse_sql(d, expected_name);
    } else if (type == "cache") {
      Routing_guidelines_engine::ResolveCache cache;
      for (const auto &member : d.GetObject()) {
        std::string name = member.name.GetString();
        if (name == "type") continue;
        cache.emplace(name,
                      net::ip::make_address(member.value.GetString()).value());
      }
      if (external_cache_) *external_cache_ = cache;
      if (rpd_) rpd_->update_resolve_cache(cache);
      return {};
    } else {
      return stdx::unexpected(
          std::string("Undefined object type: ").append(type));
    }
  } catch (const std::runtime_error &e) {
    return stdx::unexpected(
        std::string("Exception while processing document:\n") + e.what());
  }

  return {};
}

stdx::expected<void, std::string> Routing_simulator::parse_router(
    const rapidjson::Document &d) {
  for (const auto &member : d.GetObject()) {
    const std::string_view member_name = member.name.GetString();

    if (member_name == "port.rw") {
      router_.port_rw = member.value.GetInt();
    } else if (member_name == "port.ro") {
      router_.port_ro = member.value.GetInt();
    } else if (member_name == "port.rw_split") {
      router_.port_rw_split = member.value.GetInt();
    } else if (member_name == "address") {
      router_.bind_address = member.value.GetString();
    } else if (member_name == "hostname") {
      router_.hostname = member.value.GetString();
    } else if (member_name == "localCluster") {
      router_.local_cluster = member.value.GetString();
    } else if (member_name.starts_with("tags.")) {
      router_.tags.emplace(member.name.GetString() + 4,
                           member.value.GetString());
    } else if (member_name != "type") {
      return stdx::unexpected(
          std::string("Unrecognized member of router info: ")
              .append(member_name));
    }
  }

  if (external_context_) external_context_->set_router_info(router_);

  return {};
}

stdx::expected<void, std::string> Routing_simulator::parse_destination(
    const rapidjson::Document &d, std::string_view expected_name) {
  const auto &it = d.FindMember("uuid");
  Server_info &server =
      it == d.MemberEnd()
          ? (last_destination_.empty() ? server_
                                       : destinations_[last_destination_])
          : destinations_[it->value.GetString()];

  for (const auto &member : d.GetObject()) {
    const std::string_view member_name = member.name.GetString();

    if (member_name == "port") {
      server.port = member.value.GetInt();
    } else if (member_name == "label") {
      server.label = member.value.GetString();
    } else if (member_name == "address") {
      server.address = member.value.GetString();
    } else if (member_name == "uuid") {
      server.uuid = member.value.GetString();
      if (server.uuid.empty())
        throw std::runtime_error(
            "Destination uuid if providen cannot be an empty string");
      last_destination_ = server.uuid;
    } else if (member_name == "version") {
      server.version = member.value.GetInt();
    } else if (member_name == "memberRole") {
      server.member_role = member.value.GetString();
    } else if (member_name.starts_with("tags.")) {
      server.tags.emplace(member.name.GetString() + strlen("tags."),
                          member.value.GetString());
    } else if (member_name == "clusterName") {
      server.cluster_name = member.value.GetString();
    } else if (member_name == "clusterSetName") {
      server.cluster_set_name = member.value.GetString();
    } else if (member_name == "clusterRole") {
      server.cluster_role = member.value.GetString();
    } else if (member_name == "isClusterInvalidated") {
      server.cluster_is_invalidated = member.value.GetBool();
    } else if (member_name != "type") {
      return stdx::unexpected(
          std::string("Unrecognized member of destination info: ") +
          member.name.GetString());
    }
  }
  if (external_context_ && &server_ != &server) server_ = server;
  if (!rpd_) return {};

  const auto returned = rpd_->classify(server, router_);

  std::string classification_error_msg = "Error during classification: ";
  for (const auto &error : returned.errors) {
    classification_error_msg += error + '\n';
  }

  if (!returned.errors.empty()) {
    return stdx::unexpected(classification_error_msg);
  }

  if (!expected_name.empty() && rpd_) {
    std::vector<std::string> classes;

    size_t beg = 0;
    auto end = expected_name.find(',');
    while (end != std::string::npos) {
      classes.emplace_back(expected_name.substr(beg, end - beg));
      beg = end + 1;
      end = expected_name.find(',', beg);
    }
    classes.emplace_back(expected_name.substr(beg));

    const auto &dsts = rpd_->destination_classes();
    for (const auto &c : classes)
      if (std::find(dsts.begin(), dsts.end(), c) == dsts.end()) {
        return stdx::unexpected(
            std::string("Expected to return class '") + c +
            "' not defined in routing guidelines document: " +
            mysql_harness::join(dsts, ","));
      }
    if (classes != returned.class_names) {
      return stdx::unexpected(std::string("Expected destination classes '") +
                              mysql_harness::join(classes, ",") +
                              "' do not match returned ones: " +
                              mysql_harness::join(returned.class_names, ","));
    }
  }
  return {};
}

stdx::expected<void, std::string> Routing_simulator::parse_source(
    const rapidjson::Document &d, std::string_view expected_name) {
  const auto &it = d.FindMember("serial");
  Session_info &session =
      it == d.MemberEnd()
          ? (last_source_ < 0 ? session_ : sources_[last_source_])
          : sources_[it->value.GetInt()];

  for (const auto &member : d.GetObject()) {
    const std::string_view member_name = member.name.GetString();

    if (member_name == "targetPort") {
      session.target_port = member.value.GetInt();
    } else if (member_name == "targetIp") {
      session.target_ip = member.value.GetString();
    } else if (member_name == "sourceIp") {
      session.source_ip = member.value.GetString();
    } else if (member_name == "user") {
      session.user = member.value.GetString();
    } else if (member_name == "schema") {
      session.schema = member.value.GetString();
    } else if (member_name.starts_with("connectAttrs.")) {
      session.connect_attrs.emplace(
          member.name.GetString() + strlen("connectAttrs."),
          member.value.GetString());
    } else if (member_name != "type" && member_name != "serial") {
      return stdx::unexpected(
          std::string("Unrecognized member of source info: ")
              .append(member_name));
    }
  }
  if (external_context_ && &session_ != &session) session_ = session;
  if (!rpd_) return {};

  const auto cls = rpd_->classify(session, router_);

  std::string classification_error_msg = "Error during classification: ";
  for (const auto &error : cls.errors) {
    classification_error_msg += error + '\n';
  }

  if (!cls.errors.empty()) {
    return stdx::unexpected(classification_error_msg);
  }

  if (!expected_name.empty() && rpd_) {
    const auto &routes = rpd_->get_routes();
    if (std::find_if(routes.begin(), routes.end(),
                     [&expected_name](const auto &route) {
                       return expected_name == route.name;
                     }) == routes.end()) {
      return stdx::unexpected(
          std::string("Expected route '").append(expected_name) +
          "' not present in routing guidelines document");
    }

    if (expected_name != cls.route_name) {
      return stdx::unexpected(
          std::string("Expected class '").append(expected_name) +
          "' does not match route classification result: " + cls.route_name);
    }
  }
  return {};
}

stdx::expected<void, std::string> Routing_simulator::parse_sql(
    const rapidjson::Document &d, std::string_view expected_name) {
  int session = -1;

  for (const auto &member : d.GetObject()) {
    const std::string_view member_name = member.name.GetString();

    if (member_name == "defaultSchema") {
      sql_.default_schema = member.value.GetString();
    } else if (member_name == "isRead") {
      sql_.is_read = member.value.GetBool();
    } else if (member_name == "isUpdate") {
      sql_.is_update = member.value.GetBool();
    } else if (member_name == "isDDL") {
      sql_.is_ddl = member.value.GetBool();
    } else if (member_name.starts_with("queryTags.")) {
      sql_.query_tags.emplace(member.name.GetString() + strlen("queryTags."),
                              member.value.GetString());
    } else if (member_name.starts_with("queryHints.")) {
      sql_.query_hints.emplace(member.name.GetString() + strlen("queryHints."),
                               member.value.GetString());
    } else if (member_name == "route") {
      session = member.value.GetInt();
      if (sources_.find(session) == sources_.end()) {
        throw std::runtime_error("No defined source matches this serial");
      }
    } else {
      return stdx::unexpected(
          std::string("Unrecognized member of sql info: ").append(member_name));
    }
  }
  if (!rpd_) return {};

  Session_info &si =
      session < 0 ? (last_source_ < 0 ? session_ : sources_[last_source_])
                  : sources_[session];
  const auto cls = rpd_->classify(si, router_, &sql_);

  std::string classification_error_msg = "Error during classification: ";
  for (const auto &error : cls.errors) {
    classification_error_msg += error + '\n';
  }

  if (!cls.errors.empty()) {
    return stdx::unexpected(classification_error_msg);
  }

  if (!expected_name.empty() && rpd_) {
    const auto &routes = rpd_->get_routes();
    if (std::find_if(routes.begin(), routes.end(),
                     [&expected_name](const auto &route) {
                       return expected_name == route.name;
                     }) == routes.end()) {
      return stdx::unexpected(
          std::string("Expected sql class '").append(expected_name) +
          "' not present in routing guidelines document");
    }

    if (expected_name != cls.route_name) {
      return stdx::unexpected(
          std::string("Expected route '").append(expected_name) +
          "' does not match sql classification result: " + cls.route_name);
    }
  }
  return {};
}

}  // namespace routing_guidelines
