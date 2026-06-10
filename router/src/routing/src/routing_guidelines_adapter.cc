/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "routing_guidelines_adapter.h"

#include "hostname_validator.h"         // is_valid_ip_address
#include "mysql/harness/destination.h"  // make_tcp_destination
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/ranges.h"   // enumerate
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqlrouter/routing_guidelines_version.h"
#include "mysqlrouter/utils.h"  // to_string

stdx::expected<std::string, std::error_code> create_routing_guidelines_document(
    const mysql_harness::Config::ConstSectionList &sections,
    net::io_context &io_ctx) {
  Guidelines_from_conf_adapter adapter{sections, io_ctx};
  return adapter.generate_guidelines_string();
}

Guidelines_from_conf_adapter::Guidelines_from_conf_adapter(
    const mysql_harness::Config::ConstSectionList &sections,
    net::io_context &io_ctx)
    : sections_{sections}, io_ctx_{io_ctx} {}

stdx::expected<void, std::error_code>
Guidelines_from_conf_adapter::fill_guidelines_doc() {
  json_guidelines_doc_.SetObject();
  allocator_ = json_guidelines_doc_.GetAllocator();
  add_guidelines_name();
  add_guidelines_version();
  for (auto [i, section] : stdx::views::enumerate(sections_)) {
    std::string section_name =
        section->key.empty() ? "__section_" + std::to_string(i) : section->key;

    if (section->name == "routing") {
      const auto role_info = get_role_info(section);
      if (!role_info) continue;  // static routing

      add_destinations(section_name, role_info.value());
      const auto route_add_result =
          add_routes(section_name, section, role_info.value());
      if (!route_add_result) {
        json_guidelines_doc_.RemoveAllMembers();
        return stdx::unexpected(route_add_result.error());
      } else {
        has_routes_ = true;
      }
    }
  }

  json_guidelines_doc_.AddMember("destinations", destinations_, allocator_);
  json_guidelines_doc_.AddMember("routes", routes_, allocator_);
  return {};
}
stdx::expected<std::string, std::error_code>
Guidelines_from_conf_adapter::generate_guidelines_string() {
  const auto fill_doc_result = fill_guidelines_doc();
  if (!fill_doc_result) return stdx::unexpected(fill_doc_result.error());
  if (!has_routes_) {
    return stdx::unexpected(make_error_code(std::errc::not_supported));
  }

  JsonStringBuffer out_buffer;
  rapidjson::PrettyWriter<JsonStringBuffer> out_writer{out_buffer};
  json_guidelines_doc_.Accept(out_writer);
  return out_buffer.GetString();
}

void Guidelines_from_conf_adapter::add_guidelines_name() {
  const std::string section_name = "name";
  json_guidelines_doc_.AddMember(JsonValue(section_name, allocator_),
                                 JsonValue(kDefaultName, allocator_),
                                 allocator_);
}

void Guidelines_from_conf_adapter::add_guidelines_version() {
  const std::string section_name = "version";
  const std::string version = mysqlrouter::to_string(
      mysqlrouter::get_routing_guidelines_supported_version());
  json_guidelines_doc_.AddMember(JsonValue(section_name, allocator_),
                                 JsonValue(version, allocator_), allocator_);
}

std::string Guidelines_from_conf_adapter::Role_info::role_str() const {
  switch (role_) {
    case Role::primary:
      return "PRIMARY";
    case Role::secondary:
      return "SECONDARY";
    default:
      return "PRIMARY_AND_SECONDARY";
  }
}

std::string Guidelines_from_conf_adapter::Role_info::strategy_str() const {
  switch (strategy_) {
    case Strategy::first_available:
      return "first-available";
    case Strategy::round_robin:
    case Strategy::round_robin_with_fallback:
    default:
      return "round-robin";
  }
}

Guidelines_from_conf_adapter::Role_info::Strategy
Guidelines_from_conf_adapter::Role_info::strategy_from_string(
    std::string_view strategy_str) {
  if (strategy_str == "first-available") {
    return Strategy::first_available;
  } else if (strategy_str == "round-robin") {
    return Strategy::round_robin;
  } else {
    return Strategy::round_robin_with_fallback;
  }
}

void Guidelines_from_conf_adapter::Role_info::set_strategy(
    const mysql_harness::ConfigSection *section) {
  if (section->has("routing_strategy")) {
    strategy_ = strategy_from_string(section->get("routing_strategy"));
  } else {
    switch (role_) {
      case Guidelines_from_conf_adapter::Role_info::Role::primary:
        strategy_ = Strategy::first_available;
        break;
      case Guidelines_from_conf_adapter::Role_info::Role::secondary:
        strategy_ = Strategy::round_robin_with_fallback;
        break;
      case Guidelines_from_conf_adapter::Role_info::Role::primary_and_secondary:
      default:
        strategy_ = Strategy::round_robin;
    }
  }
}

void Guidelines_from_conf_adapter::Role_info::set_protocol(
    const mysql_harness::ConfigSection *section) {
  if (section->has("protocol")) {
    protocol_ = Protocol::get_by_name(section->get("protocol"));
  } else {
    protocol_ = Protocol::get_default();
  }
}

std::optional<Guidelines_from_conf_adapter::Role_info>
Guidelines_from_conf_adapter::get_role_info(
    const mysql_harness::ConfigSection *section) const {
  mysqlrouter::URI uri;
  try {
    uri = mysqlrouter::URI(section->get("destinations"), false);
  } catch (const mysqlrouter::URIError &) {
    return std::nullopt;  // This is a static route, skip it
  }

  if (uri.query.count("role") == 0) return std::nullopt;

  Guidelines_from_conf_adapter::Role_info result;

  const auto role_str = uri.query.at("role");
  if (role_str == "PRIMARY") {
    result.role_ = Guidelines_from_conf_adapter::Role_info::Role::primary;
  } else if (role_str == "SECONDARY") {
    result.role_ = Guidelines_from_conf_adapter::Role_info::Role::secondary;
  } else {
    result.role_ =
        Guidelines_from_conf_adapter::Role_info::Role::primary_and_secondary;
  }
  result.set_strategy(section);
  result.set_protocol(section);
  result.host_ = uri.host;

  return result;
}

stdx::expected<std::string, std::error_code>
Guidelines_from_conf_adapter::get_route_match(
    const mysql_harness::ConfigSection *section) const {
  if (section->has("socket")) return "$.router.routeName = " + section->key;

  std::string match;
  std::string port_str;

  if (section->has("bind_port")) {
    port_str = section->get("bind_port");
  }

  if (section->has("bind_address")) {
    const auto bind_address_res =
        mysql_harness::make_tcp_destination(section->get("bind_address"));
    if (bind_address_res->port() > 0) {
      port_str = std::to_string(bind_address_res->port());
    }

    const auto bind_address = bind_address_res->hostname();
    if (bind_address != "0.0.0.0" && bind_address != "::") {
      if (!mysql_harness::is_valid_ip_address(bind_address)) {
        std::string addr_str;

        const auto resolve_res =
            net::ip::tcp::resolver{io_ctx_}.resolve(bind_address, port_str);
        if (!resolve_res) {
          return stdx::unexpected(resolve_res.error());
        } else if (resolve_res->empty()) {
          return stdx::unexpected(make_error_code(std::errc::invalid_argument));
        }

        bool loop_init{true};
        for (auto const &addr : *resolve_res) {
          if (!loop_init) {
            addr_str += ", ";
          } else {
            loop_init = false;
          }
          addr_str += "'" + addr.endpoint().address().to_string() + "'";
        }

        if (!addr_str.empty()) {
          addr_str += ", ";
        }
        addr_str += "'" + bind_address + "'";

        match += "$.session.targetIP IN (" + addr_str + ") AND ";
      } else {
        match += "$.session.targetIP IN ('" + bind_address + "') AND ";
      }
    }
  }

  match += "$.session.targetPort IN (" + port_str + ")";
  return match;
}

std::optional<std::string>
Guidelines_from_conf_adapter::get_fallback_destination(
    const Protocol::Type protocol, std::string_view host) const {
  for (auto [i, section] : stdx::views::enumerate(sections_)) {
    if (section->name == "routing") {
      const auto other_role_info_res = get_role_info(section);
      if (!other_role_info_res) continue;  // static routing

      const auto other_role = other_role_info_res.value();
      if (other_role.protocol_ == protocol && other_role.host_ == host &&
          other_role.role_ == Role_info::Role::primary) {
        return section->key.empty() ? "__section_" + std::to_string(i)
                                    : section->key;
        break;
      }
    }
  }
  return std::nullopt;
}

void Guidelines_from_conf_adapter::add_destinations(
    const std::string &section_name,
    const Guidelines_from_conf_adapter::Role_info &role_info) {
  std::string dest_match;

  if (role_info.role_ ==
      Guidelines_from_conf_adapter::Role_info::Role::primary_and_secondary) {
    dest_match +=
        "$.server.memberRole = PRIMARY OR $.server.memberRole = SECONDARY OR "
        "$.server.memberRole = READ_REPLICA";
  } else if (role_info.role_ ==
             Guidelines_from_conf_adapter::Role_info::Role::secondary) {
    dest_match +=
        "$.server.memberRole = SECONDARY OR $.server.memberRole = READ_REPLICA";
  } else {
    dest_match += "$.server.memberRole = " + role_info.role_str();
  }

  if (role_info.strategy_ == Role_info::Strategy::round_robin_with_fallback) {
    fallback_src_ =
        get_fallback_destination(role_info.protocol_, role_info.host_);
  }

  destinations_.PushBack(
      JsonValue(rapidjson::kObjectType)
          .AddMember("name", JsonValue(section_name, allocator_), allocator_)
          .AddMember("match", JsonValue(dest_match, allocator_), allocator_),
      allocator_);
}

stdx::expected<void, std::error_code> Guidelines_from_conf_adapter::add_routes(
    const std::string &section_name,
    const mysql_harness::ConfigSection *section,
    const Guidelines_from_conf_adapter::Role_info &role_info) {
  JsonValue destinations(rapidjson::kArrayType);
  JsonValue route_destination(rapidjson::kArrayType);
  JsonValue route_class(rapidjson::kStringType);
  route_class.SetString(section_name, allocator_);
  route_destination.PushBack(route_class, allocator_);

  const auto strategy_name = role_info.strategy_str();
  destinations.PushBack(
      JsonValue(rapidjson::kObjectType)
          .AddMember("strategy", JsonValue(strategy_name, allocator_),
                     allocator_)
          .AddMember("classes", route_destination, allocator_)
          .AddMember("priority", 0, allocator_),
      allocator_);

  JsonValue fallback_destination(rapidjson::kArrayType);
  JsonValue fallback_route(rapidjson::kStringType);
  if (role_info.strategy_ == Role_info::Strategy::round_robin_with_fallback &&
      fallback_src_) {
    const std::string &round_robin_str = "round-robin";

    fallback_route.SetString(fallback_src_.value(), allocator_);
    fallback_destination.PushBack(fallback_route, allocator_);
    destinations.PushBack(
        JsonValue(rapidjson::kObjectType)
            .AddMember("strategy", JsonValue(round_robin_str, allocator_),
                       allocator_)
            .AddMember("classes", fallback_destination, allocator_)
            .AddMember("priority", 1, allocator_),
        allocator_);
  }

  const auto match_res = get_route_match(section);
  if (!match_res) return stdx::unexpected(match_res.error());

  std::optional<std::string> access_mode;
  if (section->has("access_mode")) {
    access_mode = section->get("access_mode");
  }

  JsonValue route_json(rapidjson::kObjectType);
  route_json.AddMember("name", JsonValue(section_name, allocator_), allocator_)
      .AddMember("match", JsonValue(match_res.value(), allocator_), allocator_)
      .AddMember("destinations", destinations, allocator_);

  routes_.PushBack(route_json, allocator_);

  return {};
}
