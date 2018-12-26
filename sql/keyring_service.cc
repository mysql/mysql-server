/*  Copyright (c) 2016, 2017 Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "m_ctype.h"  /* my_charset_utf8_bin */
#include <mysql/plugin_keyring.h> /* keyring plugin */
#include "set_var.h"
#include "strfunc.h"
#include "sql_string.h"
#include "sql_plugin.h"
#include "mysqld.h"

struct Key_data
{
  Key_data() : result(TRUE)
  {}

  const char *key_id;
  const char *key_type_to_store;
  char **key_type_to_fetch;
  const char *user_id;
  const void *key_to_store;
  void **key_to_fetch;
  size_t key_len_to_store;
  size_t *key_len_to_fetch;
  my_bool result;
};

static my_bool key_fetch(THD *thd, plugin_ref plugin, void *arg)
{
  Key_data *key_data= reinterpret_cast<Key_data*>(arg);
  plugin= my_plugin_lock(NULL, &plugin);
  if (plugin)
  {
    st_mysql_keyring *keyring=
      (st_mysql_keyring *) plugin_decl(plugin)->info;
    key_data->result= keyring->mysql_key_fetch(key_data->key_id, key_data->key_type_to_fetch,
      key_data->user_id, key_data->key_to_fetch, key_data->key_len_to_fetch);
  }
  //this function should get executed only for the first plugin. This is why
  //it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return TRUE;
}

static my_bool key_store(THD *thd, plugin_ref plugin, void *arg)
{
  Key_data *key_data= reinterpret_cast<Key_data*>(arg);
  plugin= my_plugin_lock(NULL, &plugin);
  if (plugin)
  {
    st_mysql_keyring *keyring=
      (st_mysql_keyring *) plugin_decl(plugin)->info;
    key_data->result= keyring->mysql_key_store(key_data->key_id, key_data->key_type_to_store,
      key_data->user_id, key_data->key_to_store, key_data->key_len_to_store);
  }
  //this function should get executed only for the first plugin. This is why
  //it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return TRUE;
}

static my_bool key_remove(THD *thd, plugin_ref plugin, void *arg)
{
  Key_data *key_data= reinterpret_cast<Key_data*>(arg);
  plugin= my_plugin_lock(NULL, &plugin);
  if (plugin)
  {
    st_mysql_keyring *keyring=
      (st_mysql_keyring *) plugin_decl(plugin)->info;
    key_data->result= keyring->mysql_key_remove(key_data->key_id, key_data->user_id);
  }
  //this function should get executed only for the first plugin. This is why
  //it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return TRUE;
}

static my_bool key_generate(THD *thd, plugin_ref plugin, void *arg)
{
  Key_data *key_data= reinterpret_cast<Key_data*>(arg);
  plugin= my_plugin_lock(NULL, &plugin);
  if (plugin)
  {
    st_mysql_keyring *keyring=
      (st_mysql_keyring *) plugin_decl(plugin)->info;
    key_data->result= keyring->mysql_key_generate(key_data->key_id,
      key_data->key_type_to_store, key_data->user_id, key_data->key_len_to_store);
  }
  //this function should get executed only for the first plugin. This is why
  //it always returns error. plugin_foreach will stop after first iteration.
  plugin_unlock(NULL, plugin);
  return TRUE;
}

int my_key_fetch(const char *key_id, char **key_type, const char *user_id,
                 void **key, size_t *key_len)
{
  Key_data key_data;
  key_data.key_id= key_id;
  key_data.key_type_to_fetch= key_type;
  key_data.user_id= user_id;
  key_data.key_to_fetch= key;
  key_data.key_len_to_fetch= key_len;
  plugin_foreach(current_thd, key_fetch, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}

int my_key_store(const char *key_id, const char *key_type, const char *user_id,
                 const void *key, size_t key_len)
{
  Key_data key_data;
  key_data.key_id= key_id;
  key_data.key_type_to_store= key_type;
  key_data.user_id= user_id;
  key_data.key_to_store= key;
  key_data.key_len_to_store= key_len;
  if (keyring_access_test())
    return 1;
  plugin_foreach(current_thd, key_store, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}

int my_key_remove(const char *key_id, const char *user_id)
{
  Key_data key_data;
  key_data.key_id= key_id;
  key_data.user_id= user_id;
  if (keyring_access_test())
    return 1;
  plugin_foreach(current_thd, key_remove, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}

int my_key_generate(const char *key_id, const char *key_type,
                    const char *user_id, size_t key_len)
{

  Key_data key_data;
  key_data.key_id= key_id;
  key_data.key_type_to_store= key_type;
  key_data.user_id= user_id;
  key_data.key_len_to_store= key_len;
  if (keyring_access_test())
    return 1;
  plugin_foreach(current_thd, key_generate, MYSQL_KEYRING_PLUGIN, &key_data);
  return key_data.result;
}
