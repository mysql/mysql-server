/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include <string>
#include <vector>

#include "mysql/harness/loader_config.h"

#include "bootstrap_credentials.h"
#include "mysqlrouter/keyring_info.h"

using String = std::string;
using Strings = std::vector<std::string>;

class BootstrapArguments;

class BootstrapConfigurator {
 public:
  BootstrapConfigurator(BootstrapArguments *arguments);

  void create_mrs_users();
  void store_mrs_data_in_keyring();
  void store_configuration();

  String get_generated_configuration_file() const;

 private:
  void load_configuration();

  BootstrapCredentials get_config_mrs_metadata_user();
  BootstrapCredentials get_config_mrs_data_user();
  std::string get_config_keyring_path();
  std::string get_config_master_key_path();

  uint64_t get_config_router_id();
  Strings get_account_host_args();

  BootstrapArguments *arguments_;
  KeyringInfo keyring_info_;
  mysql_harness::LoaderConfig config_{mysql_harness::Config::allow_keys};
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_
