/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_I_SERIALIZER_H
#define MYSQL_I_SERIALIZER_H

#include <my_global.h>
#include <hash.h>
#include "i_serialized_object.h"

namespace keyring
{

  class ISerializer
  {
  public:
    /**
     * Purpose of this function is to serialize keys_hash or key (or both)
     * into an object implementing ISeriazlized_object interface.
     * @param keys_hash - HASH container of all the keys stored in keyring.
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
    virtual ISerialized_object* serialize(HASH *keys_hash, IKey *key,
                                          Key_operation operation)= 0;
    virtual ~ISerializer()
    {}
  };
} //namespace keyring

#endif //MYSQL_I_SERIALIZER_H
