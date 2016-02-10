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
#include "keyring.h"

namespace keyring
{
/* Always defined. */
  PSI_memory_key key_memory_KEYRING;
  PSI_rwlock_key key_LOCK_keyring;
}

mysql_rwlock_t LOCK_keyring;


boost::movelib::unique_ptr<IKeys_container> keys(NULL);
my_bool is_keys_container_initialized= FALSE;
boost::movelib::unique_ptr<ILogger> logger(NULL);
boost::movelib::unique_ptr<char[]> keyring_file_data(NULL);

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_info all_keyring_rwlocks[]=
{
  {&key_LOCK_keyring, "LOCK_keyring", 0}
};

static PSI_memory_info all_keyring_memory[]=
{
  {&key_memory_KEYRING, "KEYRING", 0}
};

void keyring_init_psi_keys(void)
{
  const char *category = "keyring";
  int count;

  count= array_elements(all_keyring_memory);
  mysql_memory_register(category, all_keyring_memory, count);

  count= array_elements(all_keyring_rwlocks);
  mysql_rwlock_register(category, all_keyring_rwlocks, count);
}
#endif //HAVE_PSI_INTERFACE

my_bool init_keyring_locks()
{
  if (mysql_rwlock_init(key_LOCK_keyring, &LOCK_keyring))
    return TRUE;
  return FALSE;
}

my_bool create_keyring_dir_if_does_not_exist(const char *keyring_file_path)
{
  if (!keyring_file_path || strlen(keyring_file_path) == 0)
    return TRUE;
  char keyring_dir[FN_REFLEN];
  size_t keyring_dir_length;
  dirname_part(keyring_dir, keyring_file_path, &keyring_dir_length);
  if (keyring_dir_length > 1 && (keyring_dir[keyring_dir_length-1] == FN_LIBCHAR ||
                                 keyring_dir[keyring_dir_length-1] == FN_LIBCHAR2) )
  {
    keyring_dir[keyring_dir_length-1]= '\0';
    --keyring_dir_length;
  }
  int flags=
#ifdef _WIN32
    0
#else
    S_IRWXU | S_IRGRP | S_IXGRP
#endif
    ;
  if (strlen(keyring_dir) == 0)
    return TRUE;
  my_mkdir(keyring_dir, flags, MYF(0));
  return FALSE;
}

int check_keyring_file_data(IKeyring_io* keyring_io, boost::movelib::unique_ptr<IKeys_container> new_keys,
                            MYSQL_THD thd  __attribute__((unused)),
                            struct st_mysql_sys_var *var  __attribute__((unused)),
                            void *save, st_mysql_value *value)
{
  char            buff[FN_REFLEN];
  const char      *keyring_filename;
  int             len = sizeof(buff);

  (*(const char **) save)= NULL;
  keyring_filename= value->val_str(value, buff, &len);
  mysql_rwlock_wrlock(&LOCK_keyring);
  if (create_keyring_dir_if_does_not_exist(keyring_filename))
  {
    mysql_rwlock_unlock(&LOCK_keyring);
    logger->log(MY_ERROR_LEVEL, "keyring_file_data cannot be set to new value"
      " as the keyring file cannot be created/accessed in the provided path");
    return 1;
  }
  try
  {
    if (new_keys->init(keyring_io, keyring_filename))
    {
      mysql_rwlock_unlock(&LOCK_keyring);
      return 1;
    }
    *reinterpret_cast<IKeys_container **>(save)= new_keys.get();
    new_keys.release();
    mysql_rwlock_unlock(&LOCK_keyring);
  }
  catch (...)
  {
    mysql_rwlock_unlock(&LOCK_keyring);
    return 1;
  }
  return(0);
}

void update_keyring_file_data(MYSQL_THD thd  __attribute__((unused)),
                              struct st_mysql_sys_var *var  __attribute__((unused)),
                              void *var_ptr __attribute__((unused)),
                              const void *save_ptr)
{
  mysql_rwlock_wrlock(&LOCK_keyring);
  IKeys_container *new_keys= *reinterpret_cast<IKeys_container**>(const_cast<void*>(save_ptr));
  keys.reset(new_keys);
  keyring_file_data.reset(new char[new_keys->get_keyring_storage_url().length()+1]);
  memcpy(keyring_file_data.get(), new_keys->get_keyring_storage_url().c_str(),
         new_keys->get_keyring_storage_url().length()+1);
  *reinterpret_cast<char **>(var_ptr)= keyring_file_data.get();
  is_keys_container_initialized= TRUE;
  mysql_rwlock_unlock(&LOCK_keyring);
}

my_bool mysql_key_store(IKeyring_io *keyring_io, const char *key_id,
                        const char *key_type, const char *user_id,
                        const void *key, size_t key_len)
{
  if (is_keys_container_initialized == FALSE)
    return TRUE;

  boost::movelib::unique_ptr<Key> key_to_store(new Key(key_id, key_type, user_id, key, key_len));
  if (key_to_store->is_key_type_valid() == FALSE)
  {
    logger->log(MY_ERROR_LEVEL, "Error while storing key: invalid key_type");
    return TRUE;
  }
  if (key_to_store->is_key_id_valid() == FALSE)
  {
    logger->log(MY_ERROR_LEVEL,
                "Error while storing key: key_id cannot be empty");
    return TRUE;
  }
  if (key_len > 0)
    key_to_store->xor_data();
  mysql_rwlock_wrlock(&LOCK_keyring);
  if (keys->store_key(keyring_io, key_to_store.get()))
  {
    mysql_rwlock_unlock(&LOCK_keyring);
    return TRUE;
  }
  mysql_rwlock_unlock(&LOCK_keyring);

  key_to_store.release();
  return FALSE;
}

my_bool mysql_key_remove(IKeyring_io *keyring_io, const char *key_id,
                         const char *user_id)
{
  bool retval= false;
  if (is_keys_container_initialized == FALSE)
    return TRUE;
  if (key_id == NULL || strlen(key_id) == 0)
  {
    logger->log(MY_ERROR_LEVEL,
                "Error while removing key: key_id cannot be empty");
    return TRUE;
  }
  Key key(key_id, NULL, user_id, NULL, 0);
  mysql_rwlock_wrlock(&LOCK_keyring);
  retval= keys->remove_key(keyring_io, &key);
  mysql_rwlock_unlock(&LOCK_keyring);
  return retval;
}

my_bool mysql_key_fetch(const char *key_id, char **key_type, const char *user_id,
                      void **key, size_t *key_len)
{
  if (is_keys_container_initialized == FALSE)
    return TRUE;

  Key key_to_fetch(key_id, NULL, user_id, NULL, 0);
  if (key_to_fetch.is_key_id_valid() == FALSE)
  {
    logger->log(MY_ERROR_LEVEL,
                "Error while fetching key: key_id cannot be empty");
    return TRUE;
  }
  mysql_rwlock_rdlock(&LOCK_keyring);
  IKey *fetched_key = keys->fetch_key(&key_to_fetch);
  mysql_rwlock_unlock(&LOCK_keyring);
  if (fetched_key)
  {
    *key_len = fetched_key->get_key_data_size();
    fetched_key->xor_data();
    *reinterpret_cast<uchar **>(key)=fetched_key->release_key_data();
    *key_type = my_strdup(key_memory_KEYRING,
                          fetched_key->get_key_type()->c_str(),
                          MYF(MY_WME));
  }
  else
    *key = NULL;
  return FALSE;
}

my_bool mysql_key_generate(IKeyring_io* keyring_io, const char *key_id,
                           const char *key_type, const char *user_id,
                           size_t key_len)
{
  if (is_keys_container_initialized == FALSE)
    return TRUE;
  Key key_candidate(key_id, key_type, user_id, NULL, 0);
  if (key_candidate.is_key_id_valid() == FALSE)
  {
    logger->log(MY_ERROR_LEVEL,
                "Error while generating key: key_id cannot be empty");
    return TRUE;
  }
  if (key_candidate.is_key_type_valid() == FALSE)
  {
    logger->log(MY_ERROR_LEVEL,
                "Error while generating key: invalid key_type");
    return TRUE;
  }

  boost::movelib::unique_ptr<uchar[]> key(new uchar[key_len]);
  if (my_rand_buffer(key.get(), key_len) == TRUE ||
      mysql_key_store(keyring_io, key_id, key_type, user_id, key.get(), key_len) == TRUE)
    return TRUE;
  return FALSE;
}

int keyring_deinit(void *arg __attribute__((unused)))
{
  //not taking a lock here as the calls to keyring_deinit are serialized by
  //the plugin framework
  keys.reset();
  logger.reset();
  keyring_file_data.reset();
  mysql_rwlock_destroy(&LOCK_keyring);
  return 0;
}
