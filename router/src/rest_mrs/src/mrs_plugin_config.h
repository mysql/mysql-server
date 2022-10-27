/*
  Copyright (c)froM 2021, 2022, Oracle and/or its affiliates.

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

namespace mrs {

class PluginConfig : public ::mysql_harness::BasePluginConfig,
                     public ::mrs::Configuration {
 public:
  using ConfigSection = mysql_harness::ConfigSection;

 public:
  explicit PluginConfig(const ConfigSection *section,
                        const std::vector<std::string> &routing_sections)
      : mysql_harness::BasePluginConfig(section) {
    static const char *kKeyringAttributePassword = "password";
    using StringOption = mysql_harness::StringOption;

    mysql_user_ = get_option(section, "mysql_user", StringOption{});
    mysql_user_data_access_ =
        get_option(section, "mysql_user_data_access", StringOption{});

    auto routing = get_option(section, "routing",
                              mysql_harness::ArrayOption<StringOption>{});

    if (mysql_user_data_access_.empty()) {
      mysql_user_data_access_ = mysql_user_;
    }

    mysql_user_password_ =
        get_keyring_value(mysql_user_, kKeyringAttributePassword);
    mysql_user_data_access_password_ =
        get_keyring_value(mysql_user_data_access_, kKeyringAttributePassword);
    jwt_secret_ = get_keyring_value("rest-user", "jwt_secret");

    for (auto el : routing) {
      routing_names_.insert(el);
    }

    if (!std::all_of(
            routing.begin(), routing.end(), [&routing_sections](const auto &v) {
              return std::find(routing_sections.begin(), routing_sections.end(),
                               v) != routing_sections.end();
            }))
      throw std::logic_error(
          "Routing name specified for `routing` option, doesn't exists.");
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
    if (option == "routing") return true;
    if (option == "authentication") return true;

    return false;
  }

  std::string get_default(const std::string & /*option*/) const override {
    return "";
  }

 private:
  static std::string get_keyring_value(const std::string &user,
                                       const char *attr) {
    try {
      return mysql_harness::get_keyring()
                 ? mysql_harness::get_keyring()->fetch(user.c_str(), attr)
                 : "";
    } catch (const std::exception &e) {
      return "";
    }
  }
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_REST_MRS_CONFIG_H_
