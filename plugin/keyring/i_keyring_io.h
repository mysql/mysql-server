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

#ifndef IKEYRINGIO_INCLUDED
#define IKEYRINGIO_INCLUDED

#include <my_global.h>
#include "keyring_key.h"

namespace keyring {

enum Flush_operation
{
  STORE_KEY,
  REMOVE_KEY,
  FULL
};

class IKeyring_io : public Keyring_alloc
{
public:
  virtual my_bool init(std::string *keyring_storage_url)= 0;
  virtual my_bool open(std::string *keyring_storage_url)= 0;
  virtual void reserve_buffer(size_t memory_size)= 0;
  virtual my_bool close()= 0;
  virtual my_bool flush_to_backup()= 0;
  virtual my_bool flush_to_keyring(IKey *key = NULL, Flush_operation operation= STORE_KEY)= 0;
  virtual my_bool operator<< (const IKey* key)= 0;
  virtual my_bool operator>> (IKey* key)= 0;

  virtual ~IKeyring_io() {}
};

} //namespace keyring

#endif //IKEYRINGIO_INCLUDED
