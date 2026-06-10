/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "mock_host_resolver_plugin_config.h"

#include <string>

#include "mysql/harness/string_utils.h"

const std::string k_ip4{"ip4:"};
const std::string k_ip6{"ip6:"};
const std::string k_wait{"wait:"};

static bool mark(const std::string attr, Entry *e) {
  if (attr == "log") {
    e->log_ = true;
  } else if (attr == "error") {
    e->error_ = true;
  } else if (attr == "not-found") {
    e->fail_not_found_ = true;
  } else if (attr.starts_with(k_ip4)) {
    auto addr = attr.substr(k_ip4.length());
    auto vaddr = net::ip::make_address_v4(addr.c_str());
    if (!vaddr) return false;
    e->addresses_.push_back(*vaddr);
  } else if (attr.starts_with(k_ip6)) {
    auto addr = attr.substr(k_ip6.length());
    auto vaddr = net::ip::make_address_v6(addr.c_str());
    if (!vaddr) return false;
    e->addresses_.push_back(*vaddr);
  } else if (attr.starts_with(k_wait)) {
    auto wait = attr.substr(k_wait.length());
    auto v = atoi(wait.c_str());
    if (v < 1) return false;
    e->wait_.set(v);
  } else
    return false;

  return true;
}

static bool parse_value(const std::string &value, Entry *e) {
  auto attributes = mysql_harness::split_string(value, ',', true);
  for (const auto &attr : attributes) {
    if (!mark(attr, e)) return false;
  }
  return true;
}

HostCachePluginConfig::HostCachePluginConfig(
    const mysql_harness::ConfigSection *section, ResolveActions &ra)
    : mysql_harness::BasePluginConfig(section), actions_{ra} {
  for (auto &[k, v] : section->get_options()) {
    if ("library" == k) continue;
    Entry e;
    if (!parse_value(v, &e))
      throw std::invalid_argument("Invalid value in parameter '" + k + "");
    actions_[k] = e;
  }
}

std::string HostCachePluginConfig::get_default(
    [[maybe_unused]] std::string_view option) const {
  return {};
}

[[nodiscard]] bool HostCachePluginConfig::is_required(
    [[maybe_unused]] std::string_view option) const {
  return false;
}
