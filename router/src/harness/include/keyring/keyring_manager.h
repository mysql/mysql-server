/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_KEYRING_MANAGER_INCLUDED
#define MYSQL_HARNESS_KEYRING_MANAGER_INCLUDED

#include <stdexcept>
#include <string>
#include "keyring.h"

namespace mysql_harness {

class invalid_master_keyfile : public std::runtime_error {
 public:
  invalid_master_keyfile(const std::string &w) : std::runtime_error(w) {}
};

static const int kMaxKeyringKeyLength = 255;

/**
 * Initialize an instance of a keyring to be used in the application
 * from the contents of a file, using the given master key file.
 *
 * @param keyring_file_path path to the file where keyring is stored
 * @param master_key_path path to the file keyring master keys are stored
 * @param create_if_needed creates the keyring if it doesn't exist yet
 *
 * @return false if the keyring had to be created
 */
HARNESS_EXPORT bool init_keyring(const std::string &keyring_file_path,
                                 const std::string &master_key_path,
                                 bool create_if_needed);

/**
 * Initialize an instance of a keyring to be used in the application
 * from the contents of a file, using the given master key.
 *
 * @param keyring_file_path path to the file where keyring is stored
 * @param master_key master key for the keyring
 * @param create_if_needed creates the keyring if it doesn't exist yet
 *
 * @return false if the keyring had to be created
 */
HARNESS_EXPORT bool init_keyring_with_key(const std::string &keyring_file_path,
                                          const std::string &master_key,
                                          bool create_if_needed);

/**
 * Saves the keyring contents to disk.
 */
HARNESS_EXPORT void flush_keyring();

/**
 * Gets a previously initialized singleton instance of the keyring
 */
HARNESS_EXPORT Keyring *get_keyring();

/**
 * Clears the keyring singleton.
 */
HARNESS_EXPORT void reset_keyring();
}  // namespace mysql_harness

#endif
