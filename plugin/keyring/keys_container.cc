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

#include <my_global.h>
#include "keys_container.h"

namespace keyring {

extern PSI_memory_key key_memory_KEYRING;

static uchar *get_hash_key(const uchar *key, size_t *length)
{
  std::string *key_signature= reinterpret_cast<const IKey *>(key)->get_key_signature();
  *length= key_signature->length();
  return reinterpret_cast<uchar *>(const_cast<char*>(key_signature->c_str()));
}


void free_hash_key(void* key)
{
  IKey *key_to_free= reinterpret_cast<IKey*>(key);
  delete key_to_free;
}

Keys_container::Keys_container(ILogger *logger)
 : memory_needed_to_flush_to_disk(0)
 , logger(logger)
{
  my_hash_clear(&keys_hash);
}

Keys_container::~Keys_container()
{
  free_keys_hash();
}

my_bool Keys_container::init(IKeyring_io* keyring_io, std::string keyring_storage_url)
{
  this->keyring_storage_url= keyring_storage_url;
  if (my_hash_init(&keys_hash, system_charset_info, 0, 0,
                   (hash_get_key_function) get_hash_key, free_hash_key, HASH_UNIQUE,
                   key_memory_KEYRING) ||
      keyring_io->init(&this->keyring_storage_url) ||
      load_keys_from_keyring_storage(keyring_io))
  {
    free_keys_hash();
    return TRUE;
  }
  return FALSE;
}

std::string Keys_container::get_keyring_storage_url()
{
  return keyring_storage_url;
}

my_bool Keys_container::store_key_in_hash(IKey *key)
{
  if (my_hash_insert(&keys_hash, (uchar *) key))
    return TRUE;
  memory_needed_to_flush_to_disk+= key->get_key_pod_size();
  return FALSE;
}

my_bool Keys_container::store_key(IKeyring_io* keyring_io, IKey* key)
{
  keyring_io->open(&keyring_storage_url);
  if (flush_to_backup(keyring_io) || store_key_in_hash(key))
    return TRUE;//keyring_io destructor will take care of removing the backup(if exists)
  if (flush_to_keyring(keyring_io, key, STORE_KEY) || keyring_io->close())
  {
    remove_key_from_hash(key);
    return TRUE;
  }
  return FALSE;
}

IKey* Keys_container::get_key_from_hash(IKey *key)
{
  return reinterpret_cast<IKey*>(my_hash_search(&keys_hash,
    reinterpret_cast<const uchar*>(key->get_key_signature()->c_str()),
    key->get_key_signature()->length()));
}

IKey*Keys_container::fetch_key(IKey *key)
{
  DBUG_ASSERT(key->get_key_data() == NULL);
  DBUG_ASSERT(key->get_key_type()->empty());

  IKey *fetched_key= get_key_from_hash(key);

  if (fetched_key == NULL)
    return NULL;

  if (fetched_key->get_key_type()->empty())
    return NULL;
  key->set_key_type(fetched_key->get_key_type());
  uchar *key_data= keyring_malloc<uchar*>(fetched_key->get_key_data_size());
  memcpy(key_data, fetched_key->get_key_data(), fetched_key->get_key_data_size());
  key->set_key_data(key_data, fetched_key->get_key_data_size());
  return key;
}

my_bool Keys_container::remove_key_from_hash(IKey *key)
{
  my_bool retVal= TRUE;
  keys_hash.free_element= NULL; //Prevent my_hash_delete from removing key from memory
  retVal= my_hash_delete(&keys_hash, reinterpret_cast<uchar*>(key));
  if (retVal == FALSE)
    memory_needed_to_flush_to_disk-= key->get_key_pod_size();
  keys_hash.free_element= free_hash_key;
  return retVal;
}

my_bool Keys_container::remove_key(IKeyring_io *keyring_io, IKey *key)
{
  keyring_io->open(&keyring_storage_url);
  IKey* fetched_key_to_delete= get_key_from_hash(key);
  if(fetched_key_to_delete == NULL)
    return TRUE;
  if (flush_to_backup(keyring_io) || remove_key_from_hash(fetched_key_to_delete))
    return TRUE;//keyring_io destructor will take care of removing the backup(if exists)
  if (flush_to_keyring(keyring_io, fetched_key_to_delete, REMOVE_KEY) || keyring_io->close())
  {
    //reinsert the key
    store_key_in_hash(fetched_key_to_delete);
    return TRUE;
  }
  //successfully removed the key from hash and flushed to disk, safely remove
  //the key
  delete fetched_key_to_delete;

  return FALSE;
}

void Keys_container::free_keys_hash()
{
  if (my_hash_inited(&keys_hash))
    my_hash_free(&keys_hash);
}

my_bool Keys_container::load_keys_from_keyring_storage(IKeyring_io *keyring_io)
{
  my_bool was_error= FALSE;
  Key *key_loaded= new Key();
  while(*keyring_io >> key_loaded)
  {
    if (key_loaded->is_key_valid() == FALSE || store_key_in_hash(key_loaded))
    {
      was_error= TRUE;
      break;
    }
    key_loaded= new Key();
  }
  if(was_error)
  {
    logger->log(MY_ERROR_LEVEL, "Error while loading keyring content. "
                                "The keyring might be malformed");
    memory_needed_to_flush_to_disk= 0;
  }
  delete key_loaded;
  keyring_io->close();
  return was_error;
}

my_bool Keys_container::upload_keys_into_output_buffer(IKeyring_io *keyring_io)
{
  keyring_io->reserve_buffer(memory_needed_to_flush_to_disk);

  for (uint i= 0 ; i < keys_hash.records ; ++i)
  {
    if((*keyring_io <<
          reinterpret_cast<IKey *>(my_hash_element(&keys_hash, i))) == FALSE)
    {
      keyring_io->close();
      return TRUE;
    }
  }
  return FALSE;
}

my_bool Keys_container::flush_to_keyring(IKeyring_io *keyring_io, IKey *key,
                                         Flush_operation operation)
{
  if (upload_keys_into_output_buffer(keyring_io) ||
      keyring_io->flush_to_keyring(key, operation))
  {
    logger->log(MY_ERROR_LEVEL, "Could not flush keys to keyring");
    return TRUE;
  }
  return FALSE;
}

my_bool Keys_container::flush_to_backup(IKeyring_io *keyring_io)
{
  if (upload_keys_into_output_buffer(keyring_io) ||
      keyring_io->flush_to_backup())
  {
    logger->log(MY_ERROR_LEVEL, "Could not flush keys to keyring's backup");
    return TRUE;
  }
  return FALSE;
}

} //namespace keyring
