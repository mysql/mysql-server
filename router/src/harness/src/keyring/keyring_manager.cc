/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "keyring/keyring_manager.h"

#include <cstring>
#include <random>
#include <stdexcept>
#include <system_error>

#include "dim.h"
#include "keyring/keyring_file.h"
#include "keyring/master_key_file.h"
#include "mysql/harness/filesystem.h"
#include "random_generator.h"

/*
 * Keyring Management
 *
 * One or more passwords can be stored in the keyring, which is persisted on
 * disk in the keyring file.
 * The encryption key of the keyring can be fed to the keyring through an
 * auto-generated and persisted on a master key file
 *
 * The keyring's encryption key will be itself
 * encrypted by a second key, which is generated automatically and stored
 * in the keyring file. The location of the master key file is selected by the
 * user and the same key file can be shared by multiple keyrings.
 *
 * File Layout:
 *
 *  Keyring File                 KeyFile
 * +-------------+             +-------------------+
 * | KeyFile Key |             | Keyring File Name |
 * |-------------|             | Keyring Key       |
 * | Password    |             | Keyring File Name |
 * | Password    |             | Keyring Key       |
 * | ...         |             +-------------------+
 * +-------------+
 */

namespace mysql_harness {

static std::unique_ptr<KeyringFile> g_keyring;
static std::string g_keyring_file_path;
static std::string g_keyring_key;

static const unsigned kKeyLength = 32;

/**
 * Gets the master_key for the specified keyring_file from the master key store.
 * If the master key store file does not exist, it will be created along with
 * a new master_key, which will be stored and also returned.
 * If the master key store already exists, but does not have an entry for the
 * master key, it will be generated and then stored.
 *
 * Returns the master_key and the scramble for the master_key
 */
static std::pair<std::string, std::string> get_master_key(
    MasterKeyFile &mkf, const std::string &keyring_file_path,
    bool create_if_needed) {
  KeyringFile kf;

  // get the scramble for the master key file from the keyring file itself
  std::string master_scramble;

  try {
    master_scramble = kf.read_header(keyring_file_path);
    if (master_scramble.empty()) {
      throw std::runtime_error(
          "Keyring file '" + keyring_file_path +
          "' was created in an old version and needs to be recreated. Please "
          "delete and bootstrap again.");
    }
  } catch (const std::system_error &e) {
    if (e.code() != std::errc::no_such_file_or_directory || !create_if_needed) {
      throw;
    }

    return {};
  }

  // look up for the master_key for this given keyring file
  std::string master_key =
      mkf.get(mysql_harness::Path(keyring_file_path).real_path().str(),
              master_scramble);

  // if there is no master key for the "read-path' based keyring path, try to
  // lookup via the path itself.
  if (master_key.empty()) {
    master_key = mkf.get(keyring_file_path, master_scramble);
  }

  return {master_key, master_scramble};
}

static std::pair<std::string, std::string> create_initial_keyring_pair(
    MasterKeyFile &mkf, const std::string &keyring_file_path,
    std::string master_scramble) {
  // if the master key doesn't exist anywhere yet, generate one and store it
  mysql_harness::RandomGeneratorInterface &rg =
      mysql_harness::DIM::instance().get_RandomGenerator();
  std::string master_key = rg.generate_strong_password(kKeyLength);

  // scramble to encrypt the master key with, which should be stored in the
  // keyring
  if (master_scramble.empty()) {
    master_scramble = rg.generate_strong_password(kKeyLength);

    KeyringFile kf;
    kf.set_header(master_scramble);
    kf.save(keyring_file_path, master_key);
  }

  // use the real-path to store/lookup the keyring
  mkf.add(mysql_harness::Path(keyring_file_path).real_path().str(), master_key,
          master_scramble);

  mkf.save();

  return {master_key, master_scramble};
}

bool init_keyring(const std::string &keyring_file_path,
                  const std::string &master_key_path, bool create_if_needed) {
  MasterKeyFile mkf(master_key_path);

  try {
    mkf.load();
  } catch (const std::system_error &e) {
    if (e.code() == std::errc::no_such_file_or_directory && create_if_needed) {
      // ignore the error and proceed to create the file
    } else
      throw;
  }

  // throws std::runtime_error (anything else?)
  auto [master_key, master_scramble] =
      get_master_key(mkf, keyring_file_path, create_if_needed);

  // if there is a master-scramble, the
  const bool keyring_existed{!master_scramble.empty()};

  if (master_key.empty()) {
    if (!create_if_needed) {
      throw std::runtime_error("Master key for keyring at '" +
                               keyring_file_path + "' could not be read");
    }

    try {
      std::tie(master_key, master_scramble) =
          create_initial_keyring_pair(mkf, keyring_file_path, master_scramble);
    } catch (const std::system_error &e) {
      throw std::system_error(
          e.code(), "Unable to save master key to " + master_key_path);
    }
  }

  // load the keyring.
  init_keyring_with_key(keyring_file_path, master_key, false);

  return keyring_existed;
}

bool init_keyring_with_key(const std::string &keyring_file_path,
                           const std::string &master_key,
                           bool create_if_needed) {
  if (g_keyring) throw std::logic_error("Keyring already initialized");
  bool existed = false;

  auto key_store = std::make_unique<KeyringFile>();
  try {
    key_store->load(keyring_file_path, master_key);
    existed = true;
  } catch (const std::exception &) {
    if (!create_if_needed) throw;
    // force initial creation
    key_store->save(keyring_file_path, master_key);
  }
  g_keyring = std::move(key_store);
  g_keyring_file_path = keyring_file_path;
  g_keyring_key = master_key;
  return existed;
}

void flush_keyring() {
  if (!g_keyring) throw std::logic_error("No keyring loaded");
  g_keyring->save(g_keyring_file_path, g_keyring_key);
}

Keyring *get_keyring() noexcept { return g_keyring.get(); }

void reset_keyring() noexcept { g_keyring.reset(); }

}  // namespace mysql_harness
