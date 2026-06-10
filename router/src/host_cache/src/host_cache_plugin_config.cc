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

#include "host_cache_plugin_config.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include "mysql/harness/section_config_exposer.h"
#include "mysqlrouter/supported_host_cache_options.h"

template <class T>
using IntOption = mysql_harness::IntOption<T>;
using BoolOption = mysql_harness::BoolOption;

HostCachePluginConfig::HostCachePluginConfig(
    const mysql_harness::ConfigSection *section)
    : mysql_harness::BasePluginConfig(section) {
  namespace options = host_cache::options;
  enabled_ = get_option(section, options::kOptionEnabled, BoolOption{});
  ttl_success_ = get_option(section, options::kOptionTtlSuccess,
                            IntOption<uint32_t>{1, 86400});
  ttl_negative_ = get_option(section, options::kOptionTtlNegative,
                             IntOption<uint32_t>{1, 86400});
  ttl_jitter_ratio_ = get_option(section, options::kOptionTtlJitter,
                                 mysql_harness::DoubleOption{0.0, 0.5});
  max_entries_ = get_option(section, options::kOptionMaxEntries,
                            IntOption<uint32_t>{1, 10000});
}

std::string HostCachePluginConfig::get_default(std::string_view option) const {
  namespace options = host_cache::options;
  const static std::map<std::string_view, std::string> s_defaults{
      {options::kOptionEnabled, options::kDefaultEnabled ? "1" : "0"},
      {options::kOptionTtlSuccess, std::to_string(options::kDefaultTtlSuccess)},
      {options::kOptionTtlNegative,
       std::to_string(options::kDefaultTtlNegative)},
      {options::kOptionTtlJitter, std::to_string(options::kDefaultTtlJitter)},
      {options::kOptionMaxEntries, std::to_string(options::kDefaultMaxEntries)},
  };

  auto it = s_defaults.find(option);
  return it == s_defaults.end() ? std::string() : it->second;
}

[[nodiscard]] bool HostCachePluginConfig::is_required(
    std::string_view /* option */) const {
  return false;
}

namespace {

class HostCachingConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  HostCachingConfigExposer(const bool initial,
                           const HostCachePluginConfig &plugin_config,
                           const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(initial, default_section,
                                            {"host_cache", ""}),
        plugin_config_(plugin_config) {}

  void expose() override {
    namespace options = host_cache::options;
    expose_option(options::kOptionEnabled, plugin_config_.enabled_,
                  options::kDefaultEnabled, false);
    expose_option(options::kOptionMaxEntries, plugin_config_.max_entries_,
                  options::kDefaultMaxEntries, false);
    expose_option(options::kOptionTtlSuccess, plugin_config_.ttl_success_,
                  options::kDefaultTtlSuccess, false);
    expose_option(options::kOptionTtlNegative, plugin_config_.ttl_negative_,
                  options::kDefaultTtlNegative, false);
    expose_option(options::kOptionTtlJitter, plugin_config_.ttl_jitter_ratio_,
                  options::kDefaultTtlJitter, false);
  }

 private:
  const HostCachePluginConfig &plugin_config_;
};

}  // namespace

void HostCachePluginConfig::expose_configuration(
    const mysql_harness::ConfigSection &default_section,
    const bool initial) const {
  HostCachingConfigExposer(initial, *this, default_section).expose();
}
