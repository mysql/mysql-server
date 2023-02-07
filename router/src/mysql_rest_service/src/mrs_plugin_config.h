/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_
#define ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_

#include <algorithm>
#include <string>

#include "mysql/harness/config_option.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin_config.h"
#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/routing_component.h"

#include "mrs/configuration.h"

IMPORT_LOG_FUNCTIONS()

class UserConfigurationInfo {
 public:
  void operator()(const char *variable) {
    log_error(
        "MySQL Server account: '%s', set in configuration file "
        "must have configured password in `MySQLRouters` keyring.",
        variable);
    log_info(
        "Please consult the MRS documentation on: how to configure MySQL "
        "Server accounts for MRS");
  }
};

namespace mrs {

class PluginConfig : public ::mysql_harness::BasePluginConfig,
                     public ::mrs::Configuration {
 public:
  using ConfigSection = mysql_harness::ConfigSection;
  using SecondsOption = mysql_harness::SecondsOption;
  using StringOption = mysql_harness::StringOption;
  template <typename T>
  using IntOption = mysql_harness::IntOption<T>;

  const std::string k_option_metadata_refresh = "metadata_refresh_interval";

 public:
  explicit PluginConfig(const ConfigSection *section,
                        const std::vector<std::string> &routing_sections,
                        const std::vector<std::string> &metadatacaches_sections,
                        const std::string &router_name)
      : mysql_harness::BasePluginConfig(section) {
    static const char *kKeyringAttributePassword = "password";
    mysql_user_ = get_option(section, "mysql_user", StringOption{});
    mysql_user_data_access_ =
        get_option(section, "mysql_user_data_access", StringOption{});
    auto rw_route =
        get_option(section, "mysql_read_write_route", StringOption{});
    auto ro_route =
        get_option(section, "mysql_read_only_route", StringOption{});
    router_id_ =
        get_option_no_default(section, "router_id", IntOption<uint64_t>{});
    metadata_refresh_interval_ =
        get_option(section, k_option_metadata_refresh, SecondsOption{});
    router_name_ = router_name;

    if (mysql_user_data_access_.empty()) {
      mysql_user_data_access_ = mysql_user_;
    }

    if (metadata_refresh_interval_.count() == 0)
      throw std::logic_error(
          "`metadata_refresh_interval` option, must be greater than zero.");

    mysql_user_password_ = get_keyring_value<UserConfigurationInfo>(
        mysql_user_, kKeyringAttributePassword);
    mysql_user_data_access_password_ =
        get_keyring_value(mysql_user_data_access_, kKeyringAttributePassword);
    jwt_secret_ = get_keyring_value("rest-user", "jwt_secret");

    for (auto el : routing_sections) {
      routing_names_.insert(el);
    }

    for (auto el : metadatacaches_sections) {
      metada_names_.insert(el);
    }

    if (std::find(routing_sections.begin(), routing_sections.end(), rw_route) ==
        routing_sections.end())
      throw std::logic_error(
          "Route name '" + rw_route +
          "' specified for `mysql_read_write_route` option, doesn't exist.");
    if (!ro_route.empty() &&
        std::find(routing_sections.begin(), routing_sections.end(), ro_route) ==
            routing_sections.end())
      throw std::logic_error(
          "Route name '" + ro_route +
          "' specified for `mysql_read_only_route` option, doesn't exist.");
  }

  void init_runtime_configuration() {
    auto &routing = MySQLRoutingComponent::get_instance();

    auto r = routing.api(*routing_names_.begin());
    auto desitnations = r.get_destinations();
    auto ssl = r.get_destination_ssl_options();
    // TODO(lkotula): investigate `n.mode` to divide the host set (Shouldn't
    // be in review)
    for (const auto &n : desitnations) {
      nodes_.emplace_back(n.address(), n.port());
    }

    // This is going to happen for metadata-cache, lets connect to router.
    if (desitnations.empty()) {
      nodes_.emplace_back(r.get_bind_address(), r.get_bind_port());
    }

    is_https_ = HttpServerComponent::get_instance().is_ssl_configured();

    ssl_.ssl_mode_ = ssl.ssl_mode;
    ssl_.ssl_ca_file_ = ssl.ca;
    ssl_.ssl_ca_path_ = ssl.capath;
    ssl_.ssl_crl_file_ = ssl.crl;
    ssl_.ssl_crl_path_ = ssl.crlpath;
    ssl_.ssl_curves_ = ssl.curves;
    ssl_.ssl_ciphers_ = ssl.ssl_cipher;
  }

  bool is_required(const std::string &option) const override {
    if (option == "mysql_user") return true;
    if (option == "mysql_read_write_route") return true;
    if (option == "authentication") return true;

    return false;
  }

  std::string get_default(const std::string &option) const override {
    if (option == k_option_metadata_refresh) return "5";

    return {};
  }

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
      return "";
    }
  }
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_
