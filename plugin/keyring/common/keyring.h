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

#ifndef MYSQL_KEYRING_H
#define MYSQL_KEYRING_H

#include <memory>

#include "mysql/plugin.h"
#include <my_rnd.h>
#include <mysqld.h>
#include "keys_container.h"
#include "keyring_memory.h"

using keyring::IKeys_container;
using keyring::IKeyring_io;
using keyring::ILogger;
using keyring::IKey;

namespace keyring
{
/* Always defined. */
  extern PSI_memory_key key_memory_KEYRING;
  extern PSI_rwlock_key key_LOCK_keyring;
}

extern mysql_rwlock_t LOCK_keyring;

extern std::unique_ptr<IKeys_container> keys;
extern bool is_keys_container_initialized;
extern std::unique_ptr<ILogger> logger;
extern std::unique_ptr<char[]> keyring_file_data;

#ifdef HAVE_PSI_INTERFACE
void keyring_init_psi_keys(void);
#endif //HAVE_PSI_INTERFACE

bool init_keyring_locks();

void update_keyring_file_data(MYSQL_THD thd  MY_ATTRIBUTE((unused)),
                              struct st_mysql_sys_var *var  MY_ATTRIBUTE((unused)),
                              void *var_ptr MY_ATTRIBUTE((unused)),
                              const void *save_ptr);

bool mysql_key_fetch(std::unique_ptr<IKey> key_to_fetch, char **key_type,
                     void **key, size_t *key_len);
bool mysql_key_store(std::unique_ptr<IKey> key_to_store);
bool mysql_key_remove(std::unique_ptr<IKey> key_to_remove);

bool check_key_for_writting(IKey* key, std::string error_for);

template <typename T>
bool mysql_key_fetch(const char *key_id, char **key_type, const char *user_id,
                     void **key, size_t *key_len)
{
  try
  {
    std::unique_ptr<IKey> key_to_fetch(new T(key_id, NULL, user_id, NULL, 0));
    return mysql_key_fetch(std::move(key_to_fetch), key_type, key, key_len);
  }
  catch (...)
  {
    if (logger != NULL)
      logger->log(MY_ERROR_LEVEL, "Failed to fetch a key due to internal exception inside keyring_okv plugin");
    return TRUE;
  }
}

template <typename T>
bool mysql_key_store(const char *key_id, const char *key_type,
                     const char *user_id, const void *key, size_t key_len)
{
  try
  {
    std::unique_ptr<IKey> key_to_store(new T(key_id, key_type, user_id, key, key_len));
    return mysql_key_store(std::move(key_to_store));
  }
  catch (...)
  {
    if (logger != NULL)
      logger->log(MY_ERROR_LEVEL, "Failed to store a key due to internal exception inside keyring_okv plugin");
    return TRUE;
  }
}

template <typename T>
bool mysql_key_remove(const char *key_id, const char *user_id)
{
  try
  {
    std::unique_ptr<IKey> key_to_remove(new T(key_id, NULL, user_id, NULL, 0));
    return mysql_key_remove(std::move(key_to_remove));
  }
  catch (...)
  {
    if (logger != NULL)
      logger->log(MY_ERROR_LEVEL, "Failed to remove a key due to internal exception inside keyring_okv plugin");
    return TRUE;
  }
}

#endif //MYSQL_KEYRING_H
