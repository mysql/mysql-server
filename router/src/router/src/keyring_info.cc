/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include "mysqlrouter/keyring_info.h"

#include <array>
#include <chrono>
#include <stdexcept>  // runtime_error
#include <string>

#include "dim.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/supported_router_options.h"
#include "process_launcher.h"
#include "random_generator.h"

IMPORT_LOG_FUNCTIONS()

static const unsigned kKeyLength = 32;
static const char *kDefaultKeyringFileName = "keyring";

using mysql_harness::ProcessLauncher;

std::string KeyringInfo::get_keyring_file(
    const mysql_harness::Config &config) const {
  std::string keyring_file;

  if (config.has_default(router::options::kKeyringPath)) {
    keyring_file = config.get_default(router::options::kKeyringPath);
  }

  if (keyring_file.empty()) {
    keyring_file = mysql_harness::Path(config.get_default("data_folder"))
                       .join(kDefaultKeyringFileName)
                       .str();
  }

  return keyring_file;
}

void KeyringInfo::init(mysql_harness::Config &config) {
  keyring_file_ = get_keyring_file(config);

  if (config.has_default(router::options::kMasterKeyPath)) {
    master_key_file_ = config.get_default(router::options::kMasterKeyPath);
  }

  if (config.has_default(router::options::kMasterKeyReader)) {
    master_key_reader_ = config.get_default(router::options::kMasterKeyReader);
  }

  if (config.has_default(router::options::kMasterKeyWriter)) {
    master_key_writer_ = config.get_default(router::options::kMasterKeyWriter);
  }
}

bool KeyringInfo::read_master_key() noexcept {
  auto timeout = std::chrono::steady_clock::now() + rw_timeout_;

  try {
    ProcessLauncher process_launcher(master_key_reader_, {}, {});
    process_launcher.start();
    while (std::chrono::steady_clock::now() < timeout) {
      char output[1024] = {0};
      int bytes_read =
          process_launcher.read(output, sizeof(output) - 1, rw_timeout_);
      if (bytes_read > 0) {
        master_key_ += output;
      } else {
        // encountered end of input
        break;
      }
    }

    auto wait_for_exit = std::chrono::duration_cast<std::chrono::milliseconds>(
        timeout - std::chrono::steady_clock::now());

    int exit_code = 0;
    if ((exit_code = process_launcher.wait(wait_for_exit))) {
      master_key_ = "";
      if (verbose_) {
        log_error("Cannot execute master key reader '%s'",
                  get_master_key_reader().c_str());
#if !defined(_WIN32) && !defined(__APPLE__)
        if (exit_code == EACCES || exit_code == EPERM) {
          log_error(
              "This may be caused by insufficient rights or AppArmor "
              "settings.\n"
              "If you have AppArmor enabled try adding MySQLRouter rights to "
              "execute your keyring reader in the mysqlrouter profile file:\n"
              "/etc/apparmor.d/usr.bin.mysqlrouter\n\n"
              "Example:\n\n"
              "  /path/to/your/master-key-reader Ux,\n");
        }
#endif
      }
      return false;
    }
  } catch (...) {
    return false;
  }

  return true;
}

bool KeyringInfo::write_master_key() const noexcept {
  try {
    ProcessLauncher process_launcher(master_key_writer_, {}, {});
    process_launcher.start();
    process_launcher.write(master_key_.c_str(), master_key_.size());
    process_launcher.end_of_write();
    int exit_code = 0;
    if ((exit_code = process_launcher.wait(rw_timeout_))) {
      if (verbose_) {
        log_error("Cannot execute master key writer '%s'",
                  get_master_key_writer().c_str());
#if !defined(_WIN32) && !defined(__APPLE__)
        if (exit_code == EACCES || exit_code == EPERM) {
          log_error(
              "This may be caused by insufficient rights or AppArmor "
              "settings.\n"
              "If you have AppArmor enabled try adding MySQLRouter rights to "
              "execute your keyring writer in the mysqlrouter profile file:\n"
              "/etc/apparmor.d/usr.bin.mysqlrouter\n\n"
              "Example:\n\n"
              "  /path/to/your/master-key-writer Ux,\n");
        }
#endif
      }
      return false;
    }
  } catch (...) {
    return false;
  }
  return true;
}

void KeyringInfo::generate_master_key() noexcept {
  mysql_harness::RandomGeneratorInterface &rg =
      mysql_harness::DIM::instance().get_RandomGenerator();
  master_key_ = rg.generate_strong_password(kKeyLength);
}

void KeyringInfo::add_router_id_to_env(uint32_t router_id) const {
  int err_code;
#ifdef _WIN32
  err_code = _putenv_s("ROUTER_ID", std::to_string(router_id).c_str());
#else
  err_code = ::setenv("ROUTER_ID", std::to_string(router_id).c_str(), 1);
#endif
  if (err_code)
    throw SetRouterIdEnvVariableError(
        "Failed to add ROUTER_ID=" + std::to_string(router_id) +
        " to Environment, error_code=" + std::to_string(err_code));
}

bool KeyringInfo::use_master_key_external_facility() const noexcept {
  return !master_key_reader_.empty();
}

bool KeyringInfo::use_master_key_file() const noexcept {
  return !use_master_key_external_facility() && !master_key_file_.empty();
}

void KeyringInfo::validate_master_key() const {
  if (master_key_.empty()) throw std::runtime_error("Encryption key is empty");
  if (master_key_.length() > mysql_harness::kMaxKeyringKeyLength)
    throw std::runtime_error(
        "Encryption key can't be longer than " +
        std::to_string(mysql_harness::kMaxKeyringKeyLength) +
        ". Master key length: " + std::to_string(master_key_.length()));
}
