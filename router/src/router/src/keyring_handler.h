/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_BOOTSTRAP_SRC_KEYRING_HANDLER_H_
#define ROUTER_SRC_BOOTSTRAP_SRC_KEYRING_HANDLER_H_

#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/utils.h"

class KeyringHandler {
 public:
  bool init(mysql_harness::Config &config, const bool is_service) {
    ki_.init(config);
    if (ki_.use_master_key_external_facility()) {
      init_keyring_using_external_facility(config);
    } else if (ki_.use_master_key_file()) {
      init_keyring_using_master_key_file();
    } else {  // prompt password
      if (is_service) return false;
      init_keyring_using_prompted_password();
    }

    return true;
  }

  KeyringInfo &get_ki() { return ki_; }

 private:
  static uint32_t get_router_id(const mysql_harness::Config &config) {
    uint32_t result = 0;  // TODO

    if (config.has_any("metadata_cache")) {
      const auto &metadata_caches = config.get("metadata_cache");
      for (const auto &section : metadata_caches) {
        if (section->has("router_id")) {
          std::istringstream iss(section->get("router_id"));
          iss >> result;
          break;
        }
      }
    }
    return result;
  }

  void init_keyring_using_prompted_password() {
    std::string master_key =
        mysqlrouter::prompt_password("Encryption key for router keyring");
    if (master_key.length() > mysql_harness::kMaxKeyringKeyLength)
      throw std::runtime_error("Encryption key is too long");
    mysql_harness::init_keyring_with_key(ki_.get_keyring_file(), master_key,
                                         false);
  }

  void init_keyring_using_master_key_file() {
    mysql_harness::init_keyring(ki_.get_keyring_file(),
                                ki_.get_master_key_file(), false);
  }

  void init_keyring_using_external_facility(
      const mysql_harness::Config &config) {
    ki_.add_router_id_to_env(get_router_id(config));
    if (!ki_.read_master_key()) {
      throw MasterKeyReadError(
          "Cannot fetch master key using master key reader:" +
          ki_.get_master_key_reader());
    }
    ki_.validate_master_key();
    mysql_harness::init_keyring_with_key(ki_.get_keyring_file(),
                                         ki_.get_master_key(), false);
  }

  KeyringInfo ki_;
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_KEYRING_HANDLER_H_
