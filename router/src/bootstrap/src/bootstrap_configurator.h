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
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/uri.h"

using String = std::string;
using Strings = std::vector<std::string>;
using UniqueStrings = std::set<std::string>;

class BootstrapArguments;

class BootstrapConfigurator {
 public:
  explicit BootstrapConfigurator(BootstrapArguments *arguments);

  void connect(std::string *out_password);

  void configure_mrs(bool accounts_if_not_exists);

  bool has_innodb_cluster_metadata() const;
  void check_mrs_metadata() const;

  void load_configuration();
  bool needs_configure_routing() const;
  bool can_configure_mrs() const;

  void create_mrs_users();
  void store_mrs_data_in_keyring();
  void store_mrs_configuration();
  void register_mrs_router_instance();

  String get_generated_configuration_file(bool *file_exists = nullptr) const;

 private:
  std::unique_ptr<mysqlrouter::MySQLSession> session_;

  struct RoutingConfig {
    std::string key;
    bool is_metadata_cache;
  };
  std::pair<RoutingConfig, RoutingConfig> get_config_classic_sections();
  BootstrapCredentials get_config_mrs_metadata_user();
  BootstrapCredentials get_config_mrs_data_user();
  std::string get_config_master_key_path();

  uint64_t get_config_router_id();
  UniqueStrings get_account_host_args();

  BootstrapArguments *arguments_;
  KeyringHandler ki_handler_;
  mysql_harness::Config config_{mysql_harness::Config::allow_keys};
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_
