/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef KEYRING_INFO_INCLUDED
#define KEYRING_INFO_INCLUDED

#include "mysqlrouter/router_export.h"

#include <chrono>
#include <stdexcept>
#include <string>

namespace mysql_harness {
class Config;
}

/**
 * @brief MasterKeyWriteError class represents error during writing
 * master key using master-key-writer. More detail about the nature
 * of the error can be accessed using what() member function.
 */
class MasterKeyWriteError : public std::runtime_error {
 public:
  explicit MasterKeyWriteError(const std::string &msg)
      : std::runtime_error(msg) {}
};

/**
 * @brief MasterKeyReadError class represents error during reading
 * master key using master-key-reader. More detail about the nature
 * of the error can be accessed using what() member function.
 */
class MasterKeyReadError : public std::runtime_error {
 public:
  explicit MasterKeyReadError(const std::string &msg)
      : std::runtime_error(msg) {}
};

/**
 * @brief SetRouterIdEnvVariableError class represents error duing
 * adding ROUTER_ID variable to environment. More detail about the
 * nature of the error can be accessed using what() member function.
 */
class SetRouterIdEnvVariableError : public std::runtime_error {
 public:
  explicit SetRouterIdEnvVariableError(const std::string &msg)
      : std::runtime_error(msg) {}
};

/**
 * @brief KeyringInfo class encapsulates loading and storing master key
 * using master-key-reader and master-key-writer.
 */
class ROUTER_LIB_EXPORT KeyringInfo {
 private:
  /** @brief The path to keyring file */
  std::string keyring_file_;

  /** @brief The path to master key file, empty if master key file is not used
   */
  std::string master_key_file_;

  /** @brief The path to master-key-reader that is used to read master key */
  std::string master_key_reader_;

  /** @brief The path to master-key-writer that is used to store master key */
  std::string master_key_writer_;

  /** @brief The master key that is used to encode/decode keyring content */
  std::string master_key_;

  /** @brief The maximum time to write master key using master-key-writer or
   * read master key using master-key-fetcher. */
  std::chrono::milliseconds rw_timeout_ = std::chrono::milliseconds(30000);

  /** @brief If true then log verbose error messages */
  bool verbose_ = true;

 public:
  /**
   * Default constructor.
   *
   * @param verbose IF true then log verbose error messages
   */
  KeyringInfo(bool verbose = true) noexcept : verbose_(verbose) {}

  /**
   * Constructs KeyringInfo and assigns keyring file and master key file
   *
   * @param keyring_file The path to keyring file
   * @param master_key_file The path to master key file
   */
  KeyringInfo(const std::string &keyring_file,
              const std::string &master_key_file)
      : keyring_file_(keyring_file), master_key_file_(master_key_file) {}

  void set_keyring_file(const std::string &keyring_file) {
    keyring_file_ = keyring_file;
  }

  const std::string &get_keyring_file() const noexcept { return keyring_file_; }

  void set_master_key_file(const std::string &master_key_file) {
    master_key_file_ = master_key_file;
  }

  const std::string &get_master_key_file() const noexcept {
    return master_key_file_;
  }

  void set_master_key_reader(const std::string &master_key_reader) {
    master_key_reader_ = master_key_reader;
  }

  const std::string &get_master_key_reader() const noexcept {
    return master_key_reader_;
  }

  void set_master_key_writer(const std::string &master_key_writer) {
    master_key_writer_ = master_key_writer;
  }

  const std::string &get_master_key_writer() const noexcept {
    return master_key_writer_;
  }

  void set_master_key(const std::string &master_key) {
    master_key_ = master_key;
  }

  const std::string &get_master_key() const noexcept { return master_key_; }

  /**
   * @brief Initializes KeyringInfo using data read from Config. It initializes
   * keyring_file, master_key_file_, master_key_reader_ and master_key_writer.
   *
   * @param config The Config that is used to initialize KeyringInfo
   */
  void init(mysql_harness::Config &config);

  /**
   * @brief Reads master key using master_key_reader_;
   *
   * @return true if successfully read master key, false otherwise.
   */
  bool read_master_key() noexcept;

  /**
   * @brief Writes master key using master_key_writer_;
   *
   * @return true if write was successful, false otherwise.
   */
  bool write_master_key() const noexcept;

  /*
   * @brief Generate master key and store it in KeyringInfo. Generated
   * master key can be accessed using master_key_ attribute.
   */
  void generate_master_key() noexcept;

  /**
   * @brief Adds ROUTER_ID variable to environment.
   *
   * @throw SetRouterIdEnvVariableError if adding ROUTER_ID to environment
   * fails.
   */
  void add_router_id_to_env(uint32_t router_id) const;

  /**
   * @brief Checks if master-key-reader/master-key-writer should be
   * used to load/store master key.
   *
   * @return true if master-key-reader/master-key-writer should be used
   * to load/store master key, false otherwise.
   */
  bool use_master_key_external_facility() const noexcept;

  /**
   * @brief Checks if mysqlrouter.key and keyring files should be used to
   * store master key.
   *
   * @return true if master key should be used to store master key, false
   * otherwise.
   */
  bool use_master_key_file() const noexcept;

  /**
   * @brief Checks if master key is correct: it cannot be empty, and cannot
   * be longer than mysql_harness::kMaxKeyringKeyLength.
   *
   * @throw std::runtime_error if master key is empty or is longer than
   * mysql_harness::kMaxKeyringKeyLength
   */
  void validate_master_key() const;

  /**
   * @brief Returns path to keyring file based on data read from config or
   * bootstrap directory.
   *
   * @return The path to keyring file
   */
  std::string get_keyring_file(const mysql_harness::Config &config) const;
};

#endif /* KEYRING_INFO_INCLUDED */
