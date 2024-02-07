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

#ifndef MYSQL_HARNESS_KEYRING_FILE_INCLUDED
#define MYSQL_HARNESS_KEYRING_FILE_INCLUDED

#include "keyring_memory.h"

namespace mysql_harness {

/**
 * KeyringFile class.
 *
 * Implements Keyring interface and provides additional methods for loading and
 * saving keyring to file.
 */
class HARNESS_EXPORT KeyringFile : public KeyringMemory {
 public:
  KeyringFile() = default;

  /**
   * Sets additional data to be stored with the file but will not be
   * encrypted.
   *
   * @param[in] data to store in header
   */
  void set_header(const std::string &data);

  /**
   * Saves keyring to file.
   *
   * @param[in] file_name Keyring file name.
   * @param[in] key Key used for encryption.
   *
   * @exception std::exception Saving to file failed.
   */
  void save(const std::string &file_name, const std::string &key) const;

  /**
   * Load keyring from file.
   *
   * @param[in] file_name Keyring file name.
   * @param[in] key Key used for decryption.
   *
   * @exception std::exception Loading from file failed.
   */
  void load(const std::string &file_name, const std::string &key);

  /**
   * Read header data from file.
   *
   * @param[in] file_name Keyring file name.
   *
   * @exception std::exception Loading from file failed.
   */
  std::string read_header(const std::string &file_name);

 private:
  std::string header_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_KEYRING_FILE_INCLUDED
