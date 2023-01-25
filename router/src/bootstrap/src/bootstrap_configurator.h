/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_
#define ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bootstrap_credentials.h"
#include "keyring_handler.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/vt100.h"
#include "mysqlrouter/uri.h"
#include "router/src/router/src/config_generator.h"
#include "router/src/router/src/router_conf.h"

using String = std::string;
using Strings = std::vector<std::string>;
using UniqueStrings = std::set<std::string>;

class BootstrapConfigurator {
 public:
  BootstrapConfigurator();

  void init(int argc, char **argv);
  void run();

  static void init_main_logger(mysql_harness::LoaderConfig &config,
                               bool raw_mode = false);

 private:
  std::string router_program_name_;
  mysql_harness::Path origin_;
  CmdArgHandler arg_handler_;

  KeyringHandler keyring_;
  MySQLRouterConf bootstrapper_;

  bool bootstrap_mrs_ = false;
  BootstrapCredentials mrs_metadata_account_;
  BootstrapCredentials mrs_data_account_;
  std::string mrs_secret_;

  bool showing_info_ = false;

  struct RoutingConfig {
    std::string key;
    bool is_metadata_cache;
  };
  std::pair<RoutingConfig, RoutingConfig> get_config_classic_sections();

  mysql_harness::Config config_{mysql_harness::Config::allow_keys};

  void parse_command_options(std::vector<std::string> arguments);
  void prepare_command_options(const std::string &bootstrap_uri);

  std::string get_version_line() noexcept;
  void show_help();
  void show_usage() noexcept;

  void configure_mrs(mysqlrouter::MySQLSession *session,
                     const std::string &config_path);

  void check_mrs_metadata(mysqlrouter::MySQLSession *session) const;

  void load_configuration(const std::string &path);

  bool can_configure_mrs(const std::string &config_path) const;

  void create_mrs_users(mysqlrouter::MySQLSession *session,
                        uint64_t mrs_router_id);
  void store_mrs_data_in_keyring();
  void store_mrs_configuration(const std::string &config_path,
                               uint64_t mrs_router_id);
  uint64_t register_mrs_router_instance(mysqlrouter::MySQLSession *session);

  std::string get_configured_router_name() const;
  std::string get_configured_rest_endpoint() const;

  void store_mrs_account_metadata(mysqlrouter::MySQLSession *session,
                                  uint64_t mrs_router_id,
                                  const std::string &key,
                                  const std::string &user,
                                  const std::vector<std::string> &hosts);
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_
