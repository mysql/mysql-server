/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_I_SERIALIZER_H
#define MYSQL_I_SERIALIZER_H

#include <memory>
#include <string>

#include "map_helpers.h"
#include "plugin/keyring/common/i_serialized_object.h"

namespace keyring {

class ISerializer {
 public:
  /**
   * Purpose of this function is to serialize keys_hash or key (or both)
   * into an object implementing ISeriazlized_object interface.
   * @param keys_hash - hash container of all the keys stored in keyring.
   *                    This parameter is usually used when all the keys
   *                    need to be serialized.
   * @param key - key to be added or removed.
   * @param operation - The operation which is performed on key. It shows
   *                    whether we serialize key that is to be removed or
   *                    stored.
   * @return ISerialized_object on success returns serialized object, ie. object
   *                            which implements ISeriazlied_object interface
   *                            on failure returns NULL.
   */
  virtual ISerialized_object *serialize(
      const collation_unordered_map<std::string, std::unique_ptr<IKey>>
          &keys_hash,
      IKey *key, const Key_operation operation) = 0;
  virtual ~ISerializer() {}
};
}  // namespace keyring

#endif  // MYSQL_I_SERIALIZER_H
