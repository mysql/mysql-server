/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_I_SERIALIZED_OBJECT_H
#define MYSQL_I_SERIALIZED_OBJECT_H

#include "plugin/keyring/common/i_keyring_key.h"

namespace keyring {
enum Key_operation { STORE_KEY, REMOVE_KEY, ROTATE, NONE };

class ISerialized_object {
 public:
  ISerialized_object() : key_operation(NONE) {}

  virtual bool get_next_key(IKey **key) = 0;
  virtual bool has_next_key() = 0;
  virtual Key_operation get_key_operation() { return key_operation; }
  virtual void set_key_operation(Key_operation key_operation) {
    this->key_operation = key_operation;
  }

  virtual ~ISerialized_object() = default;

 protected:
  Key_operation key_operation;
};
}  // namespace keyring

#endif  // MYSQL_I_SERIALIZED_OBJECT_H
