/*  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>

#include "my_inttypes.h"
#include "mysql/plugin.h"
#include "mysql/plugin_keyring.h" /* keyring plugin */
#include "sql/current_thd.h"
#include "sql/set_var.h"
#include "sql/sql_plugin.h"
#include "sql/sql_plugin_ref.h"

class THD;

struct Key_data {
  Key_data() : result(true) {}

  const char *key_id;
  const char *key_type_to_store;
  char **key_type_to_fetch;
  const char *user_id;
  const void *key_to_store;
  void **key_to_fetch;
  size_t key_len_to_store;
  size_t *key_len_to_fetch;
  bool result;
};

static bool key_fetch(THD *, plugin_ref plugin, void *arg) {
  Key_data *key_data = reinterpret_cast<Key_data *>(arg);
  plugin = my_plugin_lock(NULL, &plugin);
  if (plugin) {
    st_mysql_keyring *keyring = (st_mysql_keyring *)plugin_decl(plugin)->info;
    key_data->result = keyring->mysql_key_fetch(
        key_data->key_id, key_data->key_type_to_fetch, key_data->user_id,
        key_data->key_to_fetch, key_data->key_len_to_fetch);
  }
  // this function should get executed only for the first plugin. This is why
  // it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return true;
}

static bool key_store(THD *, plugin_ref plugin, void *arg) {
  Key_data *key_data = reinterpret_cast<Key_data *>(arg);
  plugin = my_plugin_lock(NULL, &plugin);
  if (plugin) {
    st_mysql_keyring *keyring = (st_mysql_keyring *)plugin_decl(plugin)->info;
    key_data->result = keyring->mysql_key_store(
        key_data->key_id, key_data->key_type_to_store, key_data->user_id,
        key_data->key_to_store, key_data->key_len_to_store);
  }
  // this function should get executed only for the first plugin. This is why
  // it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return true;
}

static bool key_remove(THD *, plugin_ref plugin, void *arg) {
  Key_data *key_data = reinterpret_cast<Key_data *>(arg);
  plugin = my_plugin_lock(NULL, &plugin);
  if (plugin) {
    st_mysql_keyring *keyring = (st_mysql_keyring *)plugin_decl(plugin)->info;
    key_data->result =
        keyring->mysql_key_remove(key_data->key_id, key_data->user_id);
  }
  // this function should get executed only for the first plugin. This is why
  // it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return true;
}

static bool key_generate(THD *, plugin_ref plugin, void *arg) {
  Key_data *key_data = reinterpret_cast<Key_data *>(arg);
  plugin = my_plugin_lock(NULL, &plugin);
  if (plugin) {
    st_mysql_keyring *keyring = (st_mysql_keyring *)plugin_decl(plugin)->info;
    key_data->result = keyring->mysql_key_generate(
        key_data->key_id, key_data->key_type_to_store, key_data->user_id,
        key_data->key_len_to_store);
  }
  // this function should get executed only for the first plugin. This is why
  // it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return true;
}

/**
  Iterates over all active keyring plugins and calls the mysql_key_fetch API
  for the first one found.

  @sa st_mysql_keyring::mysql_key_fetch, mysql_keyring_service_st
*/
int my_key_fetch(const char *key_id, char **key_type, const char *user_id,
                 void **key, size_t *key_len) {
  Key_data key_data;
  key_data.key_id = key_id;
  key_data.key_type_to_fetch = key_type;
  key_data.user_id = user_id;
  key_data.key_to_fetch = key;
  key_data.key_len_to_fetch = key_len;
  plugin_foreach(current_thd, key_fetch, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}

/**
  Iterates over all active keyring plugins calls the mysql_key_store API
  for the first one found.

  @sa st_mysql_keyring::mysql_key_store, mysql_keyring_service_st
*/
int my_key_store(const char *key_id, const char *key_type, const char *user_id,
                 const void *key, size_t key_len) {
  Key_data key_data;
  key_data.key_id = key_id;
  key_data.key_type_to_store = key_type;
  key_data.user_id = user_id;
  key_data.key_to_store = key;
  key_data.key_len_to_store = key_len;
  if (keyring_access_test()) return 1;
  plugin_foreach(current_thd, key_store, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}

/**
  Iterates over all active keyring plugins and calls the mysql_key_remove API
  for the first one found.

  @sa st_mysql_keyring::mysql_key_remove, mysql_keyring_service_st
*/
int my_key_remove(const char *key_id, const char *user_id) {
  Key_data key_data;
  key_data.key_id = key_id;
  key_data.user_id = user_id;
  if (keyring_access_test()) return 1;
  plugin_foreach(current_thd, key_remove, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}

/**
  Iterates over all active keyring plugins and calls the mysql_key_generate API
  for the first one found.

  @sa st_mysql_keyring::mysql_key_generate, mysql_keyring_service_st
*/
int my_key_generate(const char *key_id, const char *key_type,
                    const char *user_id, size_t key_len) {
  Key_data key_data;
  key_data.key_id = key_id;
  key_data.key_type_to_store = key_type;
  key_data.user_id = user_id;
  key_data.key_len_to_store = key_len;
  if (keyring_access_test()) return 1;
  plugin_foreach(current_thd, key_generate, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}
