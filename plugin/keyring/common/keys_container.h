/* Copyright (c) 2016, 2021, Oracle and/or its affiliates.

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

#ifndef KEYS_CONTAINER_INCLUDED
#define KEYS_CONTAINER_INCLUDED

#include <my_global.h>
#include <hash.h>
#include "keyring_key.h"
#include "i_keyring_io.h"
#include "logger.h"
#include <sys_vars_shared.h> //For PolyLock, AutoWLock, AutoRLock
#include "i_keys_container.h"
#include "keyring_memory.h"

#include <vector>

namespace keyring {

extern "C" MYSQL_PLUGIN_IMPORT CHARSET_INFO *system_charset_info;

class Keys_container : public IKeys_container
{
private:
  bool remove_keys_metadata(IKey *key);
  void store_keys_metadata(IKey *key);
public:
  Keys_container(ILogger* logger);
  my_bool init(IKeyring_io* keyring_io, std::string keyring_storage_url);
  my_bool store_key(IKey *key);
  IKey* fetch_key(IKey *key);
  my_bool remove_key(IKey *key);
  std::string get_keyring_storage_url();
  std::vector<Key_metadata> get_keys_metadata()
  {
    return keys_metadata;
  }


  ~Keys_container();

  ulong get_number_of_keys()
  {
    return keys_hash->records;
  };
protected:
  Keys_container(const Keys_container &);
  virtual void allocate_and_set_data_for_key(IKey *key,
                                             std::string *source_key_type,
                                             uchar *source_key_data,
                                             size_t source_key_data_size);
  my_bool load_keys_from_keyring_storage();
  void free_keys_hash();
  IKey *get_key_from_hash(IKey *key);
  my_bool store_key_in_hash(IKey *key);
  my_bool remove_key_from_hash(IKey *key);
  virtual my_bool flush_to_backup();
  virtual my_bool flush_to_storage(IKey *key, Key_operation operation);

  HASH *keys_hash;
  std::vector<Key_metadata> keys_metadata;
  ILogger *logger;
  IKeyring_io *keyring_io;
  std::string keyring_storage_url;
};

} //namespace keyring

#endif //KEYS_CONTAINER_INCLUDED
