/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_
#define ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_

#include <algorithm>
#include <string>

#include "keyring/keyring_manager.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysqlrouter/component/http_server_component.h"
#include "mysqlrouter/routing_component.h"

#include "mrs/configuration.h"

class UserConfigurationInfo {
 public:
  void operator()(const char *variable);
};

namespace mrs {

class PluginConfig : public ::mysql_harness::BasePluginConfig,
                     public ::mrs::Configuration {
 public:
  using ConfigSection = mysql_harness::ConfigSection;
  using MilliSecondsOption = mysql_harness::MilliSecondsOption;
  using SecondsOption = mysql_harness::SecondsOption;
  using StringOption = mysql_harness::StringOption;
  template <typename T>
  using IntOption = mysql_harness::IntOption<T>;

  const std::string k_option_metadata_refresh = "metadata_refresh_interval";

 public:
  explicit PluginConfig(const ConfigSection *section,
                        const std::vector<std::string> &routing_sections,
                        const std::optional<std::string> &router_name,
                        const uint32_t http_port);

  bool init_runtime_configuration();
  std::set<std::string> get_waiting_for_routing_plugins();

  bool is_required(std::string_view option) const override;
  std::string get_default(std::string_view option) const override;

 private:
  class NoReporting {
   public:
    void operator()(const char *) {}
  };

  template <typename ErrorReport = NoReporting>
  static std::string get_keyring_value(const std::string &user,
                                       const char *attr) {
    try {
      if (!mysql_harness::get_keyring())
        throw std::runtime_error("Keyring not running");
      return mysql_harness::get_keyring()->fetch(user.c_str(), attr);

    } catch (const std::exception &e) {
      ErrorReport()(user.c_str());
      throw std::runtime_error(std::string("Could not fetch value for '") +
                               user + "' from the keyring: " + e.what());
    }
  }
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_
