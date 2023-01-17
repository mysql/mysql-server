/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_KEYRING_MEMORY_INCLUDED
#define MYSQL_HARNESS_KEYRING_MEMORY_INCLUDED

#include <map>
#include <stdexcept>
#include <vector>
#include "keyring.h"

namespace mysql_harness {

class decryption_error : public std::runtime_error {
 public:
  decryption_error(const char *_what) : std::runtime_error(_what) {}
};

/**
 * KeyringMemory class.
 *
 * Implements Keyring interface and provides additional methods for parsing
 * and serialization using a simple binary format. Also, handles AES encryption.
 * Used primarily for testing and as a base for KeyringFile.
 */
class HARNESS_EXPORT KeyringMemory : public Keyring {
 public:
  constexpr static unsigned int kFormatVersion = 0;

  KeyringMemory() = default;

  /**
   * Serializes and encrypts keyring data to memory buffer.
   *
   * @param[in] key Key used for encryption.
   *
   * @return Serialized keyring data.
   *
   * @exception std::exception Serialization failed.
   */
  std::vector<char> serialize(const std::string &key) const;

  /**
   * Parses and decrypts keyring data.
   *
   * @param[in] key Key used for decryption.
   * @param[in] buffer Serialized keyring data.
   * @param[in] buffer_size Size of the data.
   *
   * @exception std::exception Parsing failed.
   */
  void parse(const std::string &key, const char *buffer,
             std::size_t buffer_size);

  // Keyring interface.
  void store(const std::string &uid, const std::string &attribute,
             const std::string &value) override;

  std::string fetch(const std::string &uid,
                    const std::string &attribute) const override;

  bool remove(const std::string &uid) override;

  bool remove_attribute(const std::string &uid,
                        const std::string &attribute) override;

  const std::map<std::string, std::map<std::string, std::string>> &entries()
      const {
    return entries_;
  }

 private:
  std::map<std::string, std::map<std::string, std::string>> entries_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_KEYRING_MEMORY_INCLUDED
