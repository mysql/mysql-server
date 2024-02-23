/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

/**
 * destination status plugin.
 */

#include <array>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <system_error>

// Harness interface include files
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/section_config_exposer.h"

#include "mysqlrouter/destination_status_component.h"
#include "mysqlrouter/destination_status_plugin_export.h"
#include "mysqlrouter/supported_destination_status_options.h"

template <class T>
using IntOption = mysql_harness::IntOption<T>;

static const std::string kSectionName{"destination_status"};
static const std::string kOptionThreshold{"error_quarantine_threshold"};
static const std::string kOptionInterval{"error_quarantine_interval"};
static constexpr uint32_t kDefaultErrorQuarantineThreshold{1};
static constexpr uint32_t kDefaultErrorQuarantineInterval{1};

class DestinationStatusPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  uint32_t error_quarantine_threshold;
  std::chrono::seconds error_quarantine_interval;

  explicit DestinationStatusPluginConfig(
      const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section),
        error_quarantine_threshold(get_option(section, kOptionThreshold,
                                              IntOption<uint32_t>{1, 65535})),
        error_quarantine_interval(get_option(section, kOptionInterval,
                                             IntOption<uint32_t>{1, 3600})) {}

  std::string get_default(std::string_view option) const override {
    const std::map<std::string_view, std::string> defaults{
        {kOptionThreshold, std::to_string(kDefaultErrorQuarantineThreshold)},
        {kOptionInterval,
         std::to_string(kDefaultErrorQuarantineInterval)},  // in seconds
    };

    auto it = defaults.find(option);
    return it == defaults.end() ? std::string() : it->second;
  }

  [[nodiscard]] bool is_required(std::string_view /* option */) const override {
    return false;
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  // assume there is only one section for us
  try {
    int sections{0};

    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) continue;

      if (sections != 0) {
        throw std::invalid_argument("[" + section->name +
                                    (section->key.empty() ? "" : ":") +
                                    section->key + "] already loaded.");
      }

      ++sections;

      DestinationStatusPluginConfig config{section};

      DestinationStatusComponent::get_instance().init(
          config.error_quarantine_interval, config.error_quarantine_threshold);
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());

  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

const static std::array<const char *, 2> required = {{
    "logger",
    "io",
}};

namespace {

class DestinationStatusConfigExposer
    : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  DestinationStatusConfigExposer(
      const bool initial, const DestinationStatusPluginConfig &plugin_config,
      const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(initial, default_section,
                                            DC::SectionId{kSectionName, ""}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option(kOptionThreshold, plugin_config_.error_quarantine_threshold,
                  kDefaultErrorQuarantineThreshold);
    expose_option(kOptionInterval,
                  plugin_config_.error_quarantine_interval.count(),
                  kDefaultErrorQuarantineInterval);
  }

 private:
  const DestinationStatusPluginConfig &plugin_config_;
};

}  // namespace

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char * /*key*/, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name == kSectionName) {
      DestinationStatusPluginConfig config{section};

      DestinationStatusConfigExposer(initial, config,
                                     info->config->get_default_section())
          .expose();
    }
  }
}

extern "C" {
mysql_harness::Plugin DESTINATION_STATUS_PLUGIN_EXPORT
    harness_plugin_destination_status = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "destination status",                    // name
        VERSION_NUMBER(0, 0, 1),
        // requires
        required.size(),
        required.data(),
        // conflicts
        0,
        nullptr,
        init,
        nullptr,  // deinit
        nullptr,  // start
        nullptr,  // stop
        false,    // declares_readiness
        destination_status_supported_options.size(),
        destination_status_supported_options.data(),
        expose_configuration,
};
}
