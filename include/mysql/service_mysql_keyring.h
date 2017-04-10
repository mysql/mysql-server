/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVICE_MYSQL_PLUGIN_KEYRING_INCLUDED
#define MYSQL_SERVICE_MYSQL_PLUGIN_KEYRING_INCLUDED

/**
  @file include/mysql/service_mysql_keyring.h
*/


#ifdef __cplusplus
extern "C" {
#endif

/**
  @ingroup group_ext_plugin_services

  This service allows plugins to interact with key store backends.

  A key currently is a blob of binary data, defined by a string
  key type, that's meanigfull to the relevant backend.
  Typical key_type values include "AES", "DES", "DSA" etc.
  There's no length in the type, since it's defined by the number of bytes
  the key takes.

  A key is uniquely identified by the key_id and the user_id values, i.e.
  all keys are assigned to a particular user.
  There's only one exception to that: a single category for instance keys
  that are not associated with any particular user id. This is signified
  to the APIs by supplying NULL for user_id.
  Plugins would typically pass user accounts to the user_id parameter, or
  NULL if there's no user account to associate the key with.

  Not all backends must implement all of the functions defined
  in this interface.

  The plugin service is a "bridge service", i.e. facade with no
  real functionality that just calls the actual keyring plugin APIs.
  This is needed to allow other plugins to call into the keyring
  plugins and thus overcome the limitation that only the server
  can call plugins.

  @sa st_mysql_keyring
*/
extern struct mysql_keyring_service_st
{
  /**
    Stores a key into the keyring.
    @sa my_key_store, st_mysql_keyring::mysql_key_store
  */
  int (*my_key_store_func)(const char *, const char *, const char *,
                           const void *, size_t);
  /**
    Receives a key from the keyring.
    @sa my_key_fetch, st_mysql_keyring::mysql_key_fetch
  */
  int (*my_key_fetch_func)(const char *, char **, const char *, void **,
                           size_t *);

  /**
    Removes a key from the keyring.
    @sa my_key_remove, st_mysql_keyring::mysql_key_remove
  */
  int (*my_key_remove_func)(const char *, const char *);
  /**
    Generates a new key inside the keyring backend
    @sa my_key_generate, st_mysql_keyring::mysql_key_generate
  */
  int (*my_key_generate_func)(const char *, const char *, const char *,
                              size_t);
} *mysql_keyring_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_key_store(key_id, key_type, user_id, key, key_len) \
  mysql_keyring_service->my_key_store_func(key_id, key_type, user_id, key, \
                                           key_len)
#define my_key_fetch(key_id, key_type, user_id, key, key_len) \
  mysql_keyring_service->my_key_fetch_func(key_id, key_type, user_id, key, \
                                           key_len)
#define my_key_remove(key_id, user_id) \
  mysql_keyring_service->my_key_remove_func(key_id, user_id)
#define my_key_generate(key_id, key_type, user_id, key_len) \
  mysql_keyring_service->my_key_generate_func(key_id, key_type, user_id, \
                                              key_len)
#else

int my_key_store(const char *, const char *, const char *, const void *, size_t);
int my_key_fetch(const char *, char **, const char *, void **,
                 size_t *);
int my_key_remove(const char *, const char *);
int my_key_generate(const char *, const char *, const char *, size_t);

#endif

#ifdef __cplusplus
}
#endif

#endif //MYSQL_SERVICE_MYSQL_PLUGIN_KEYRING_INCLUDED

