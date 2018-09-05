/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_I_KEYS_CONTAINER_H
#define MYSQL_I_KEYS_CONTAINER_H

#include "plugin/keyring/common/i_keyring_io.h"
#include "plugin/keyring/common/i_keyring_key.h"

namespace keyring {

struct Key_metadata {
  std::string *id;
  std::string *user;

  Key_metadata() {}
  Key_metadata(std::string *id, std::string *user) {
    this->id = id;
    this->user = user;
  }
};

class IKeys_container : public Keyring_alloc {
 public:
  IKeys_container() : keyring_io(NULL) {}

  virtual bool init(IKeyring_io *keyring_io,
                    std::string keyring_storage_url) = 0;
  virtual bool store_key(IKey *key) = 0;
  virtual IKey *fetch_key(IKey *key) = 0;
  virtual bool remove_key(IKey *key) = 0;
  virtual std::string get_keyring_storage_url() = 0;
  virtual void set_keyring_io(IKeyring_io *keyring_io) = 0;
  virtual std::vector<Key_metadata> get_keys_metadata() = 0;

  virtual ~IKeys_container() {}

 protected:
  IKeyring_io *keyring_io;
};

}  // namespace keyring

#endif  // MYSQL_I_KEYS_CONTAINER_H
