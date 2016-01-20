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

#ifndef MYSQL_PLUGIN_KEYRING_INCLUDED
#define MYSQL_PLUGIN_KEYRING_INCLUDED

/**
  API for keyring plugin. (MYSQL_KEYRING_PLUGIN)
*/

#include "plugin.h"
#define MYSQL_KEYRING_INTERFACE_VERSION 0x0100

/**
  The descriptor structure for the plugin, that is referred from
  st_mysql_plugin.
*/

struct st_mysql_keyring
{
  int interface_version;
  /*!
    Add key to the keyring.

    Obfuscates and adds the key to the keyring. The key is associated with
    key_id and user_id (unique key identifier).

    @param[in] key_id   id of the key to store
    @param[in] key_type type of the key to store
    @param[in] user_id  id of the owner of the key
    @param[in] key      the key itself to be stored. The memory of the key is
                        copied by the keyring, thus the key itself can be freed
                        after it was stored in the keyring.
    @param[in] key_len  the length of the key to be stored

    @return Operation status
      @retval 0 OK
      @retval 1 ERROR
  */
  my_bool (*mysql_key_store)(const char *key_id, const char *key_type,
                             const char* user_id, const void *key, size_t key_len);
  /*!
    Fetches key from the keyring.

    De-obfuscates and retrieves key associated with key_id and user_id from the
    keyring.

    @param[in]  key_id   id of the key to fetch
    @param[out] key_type type of the fetched key
    @param[in]  user_id  id of the owner of the key
    @param[out] key      the fetched key itself. The memory for this key is
                         allocated by the keyring and needs to be freed by the
                         user when no longer needed. Prior to freeing the memory
                         it needs to be obfuscated or zeroed.
    @param[out] key_len  the length of the fetched key

    @return Operation status
      @retval 0 OK
      @retval 1 ERROR
  */
  my_bool (*mysql_key_fetch)(const char *key_id, char **key_type,
                             const char *user_id, void **key, size_t *key_len);

  /*!
    Removes key from the keyring.

    Removes the key associated with key_id and user_id from the
    keyring.

    @param[in]  key_id   id of the key to remove
    @param[in]  user_id  id of the owner of the key to remove

    @return Operation status
      @retval 0 OK
      @retval 1 ERROR
  */
  my_bool (*mysql_key_remove)(const char *key_id, const char *user_id);

  /*!
    Generates and stores the key.

    Generates a random key of length key_len, associates it with key_id, user_id
    and stores it in the keyring.

    @param[in] key_id   id of the key to generate
    @param[in] key_type type of the key to generate
    @param[in] user_id  id of the owner of the generated key
    @param[in] key_len  length of the key to generate

    @return Operation status
      @retval 0 OK
      @retval 1 ERROR
  */
  my_bool (*mysql_key_generate)(const char *key_id, const char *key_type,
                                const char *user_id, size_t key_len);
};
#endif
