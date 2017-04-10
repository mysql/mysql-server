/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_I_KEYS_CONTAINER_H
#define MYSQL_I_KEYS_CONTAINER_H

#include "i_keyring_key.h"
#include "i_keyring_io.h"

namespace keyring {

class IKeys_container : public Keyring_alloc
{
public:
  IKeys_container() : keyring_io(NULL)
  {}

  virtual bool init(IKeyring_io* keyring_io, std::string keyring_storage_url)= 0;
  virtual bool store_key(IKey *key)= 0;
  virtual IKey* fetch_key(IKey *key)= 0;
  virtual bool remove_key(IKey *key)= 0;
  virtual std::string get_keyring_storage_url()= 0;
  virtual void set_keyring_io(IKeyring_io *keyring_io)= 0;

  virtual ~IKeys_container() {};
protected:
  IKeyring_io *keyring_io;
};

}//namespace keyring

#endif //MYSQL_I_KEYS_CONTAINER_H
