/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_KEYRING_MASTER_KEY_FILE_INCLUDED
#define ROUTER_KEYRING_MASTER_KEY_FILE_INCLUDED

#include <stdexcept>
#include <string>
#include <utility>  // std::pair
#include <vector>

#include "harness_export.h"
#include "my_compiler.h"

namespace mysql_harness {

MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4275)
class HARNESS_EXPORT invalid_master_keyfile : public std::runtime_error {
 public:
  invalid_master_keyfile(const std::string &w) : std::runtime_error(w) {}
};
MY_COMPILER_DIAGNOSTIC_POP()

class HARNESS_EXPORT MasterKeyFile {
 public:
  MasterKeyFile(const std::string &file) : path_(file) {}

  /**
   * load master-key-file from disk.
   *
   * @throws std::runtime_error on failure
   */
  void load();

  /**
   * save master-key-file to disk.
   *
   * @throws std::runtime_error on failure
   */
  void save();

  /**
   * add value-key pair to 'id'.
   *
   * encrypts the value-key pair.
   *
   * @throws std::runtime_error on failure
   */
  void add(const std::string &id, const std::string &value,
           const std::string &key);

  /**
   * add encrypted buffer to 'id'.
   *
   * @throws std::runtime_error on failure
   */
  void add_encrypted(const std::string &id, const std::string &buf);

  /**
   * get value for 'key' for 'id'.
   *
   * @returns value for 'key' of 'id'.
   * @retval empty if key or id aren't found
   */
  std::string get(const std::string &id, const std::string &key) const;

  /**
   * get value encrypted 'key-value' pair for 'id'.
   *
   * @returns encrypted buffer of 'id'.
   * @retval empty if key or id aren't found
   */
  std::string get_encrypted(const std::string &id) const;

  /**
   * remove id from master-key-dict.
   *
   * @returns success
   * @retval true id removed
   * @retval false id not removed (not found, ...)
   */
  bool remove(const std::string &id);

  /**
   * get entries.
   */
  const auto &entries() const { return entries_; }

 private:
  std::string path_;
  std::vector<std::pair<std::string, std::string>> entries_;
};
}  // namespace mysql_harness

#endif
