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
#include <mysql/plugin_keyring.h>
#include "keyring.h"

#ifdef _WIN32
#define MYSQL_DEFAULT_KEYRINGFILE MYSQL_KEYRINGDIR"\\keyring"
#else
#define MYSQL_DEFAULT_KEYRINGFILE MYSQL_KEYRINGDIR"/keyring"
#endif

static int check_keyring_file_data(MYSQL_THD thd  __attribute__((unused)),
                    struct st_mysql_sys_var *var  __attribute__((unused)),
                    void *save, st_mysql_value *value)
{
  Buffered_file_io keyring_io(logger.get());

  boost::movelib::unique_ptr<IKeys_container> new_keys(new Keys_container(logger.get()));
  return check_keyring_file_data(&keyring_io, ::boost::move(new_keys), thd, var, save, value);
}

static char *keyring_file_data_value= NULL;
static MYSQL_SYSVAR_STR(
  data,                                                        /* name       */
  keyring_file_data_value,                                     /* value      */
  PLUGIN_VAR_RQCMDARG,                                         /* flags      */
  "The path to the keyring file. Must be specified",           /* comment    */
  check_keyring_file_data,                                     /* check()    */
  update_keyring_file_data,                                    /* update()   */
  MYSQL_DEFAULT_KEYRINGFILE                                    /* default    */
);

static struct st_mysql_sys_var *keyring_system_variables[]= {
  MYSQL_SYSVAR(data),
  NULL
};

static int keyring_init(MYSQL_PLUGIN plugin_info)
{
  try
  {
#ifdef HAVE_PSI_INTERFACE
    keyring_init_psi_keys();
#endif

    if (init_keyring_locks())
      return TRUE;

    logger.reset(new Logger(plugin_info));
    if (create_keyring_dir_if_does_not_exist(keyring_file_data_value))
    {
      logger->log(MY_ERROR_LEVEL, "Could not create keyring directory "
        "The keyring_file will stay unusable until correct path to the keyring "
        "directory gets provided");
      return FALSE;
    }
    Buffered_file_io keyring_io(logger.get());
    keys.reset(new Keys_container(logger.get()));
    if (keys->init(&keyring_io, keyring_file_data_value))
    {
      is_keys_container_initialized = FALSE;
      logger->log(MY_ERROR_LEVEL, "keyring_file initialization failure. Please check"
        " if the keyring_file_data points to readable keyring file or keyring file"
        " can be created in the specified location. "
        "The keyring_file will stay unusable until correct path to the keyring file "
        "gets provided");
      return FALSE;
    }
    is_keys_container_initialized = TRUE;
    return FALSE;
  }
  catch (...)
  {
    return TRUE;
  }
}

my_bool mysql_key_store(const char *key_id, const char *key_type,
                        const char *user_id, const void *key, size_t key_len)
{
  try
  {
    Buffered_file_io keyring_io(logger.get());
    return mysql_key_store(&keyring_io, key_id, key_type, user_id, key,
                           key_len);
  }
  catch (...)
  {
    return TRUE;
  }
}

my_bool mysql_key_remove(const char *key_id, const char *user_id)
{
  try
  {
    Buffered_file_io keyring_io(logger.get());
    return mysql_key_remove(&keyring_io, key_id, user_id);
  }
  catch (...)
  {
    return TRUE;
  }
}

my_bool mysql_key_generate(const char *key_id, const char *key_type,
                           const char *user_id, size_t key_len)
{
  try
  {
    Buffered_file_io keyring_io(logger.get());
    return mysql_key_generate(&keyring_io, key_id, key_type, user_id, key_len);
  }
  catch (...)
  {
    return TRUE;
  }

}

/* Plugin type-specific descriptor */
static struct st_mysql_keyring_file keyring_descriptor=
{
  MYSQL_KEYRING_INTERFACE_VERSION,
  mysql_key_store,
  mysql_key_fetch,
  mysql_key_remove,
  mysql_key_generate
};

mysql_declare_plugin(keyring_file)
{
  MYSQL_KEYRING_PLUGIN,                                   /*   type                            */
  &keyring_descriptor,                                    /*   descriptor                      */
  "keyring_file",                                         /*   name                            */
  "Oracle Corporation",                                   /*   author                          */
  "store/fetch authentication data to/from a flat file",  /*   description                     */
  PLUGIN_LICENSE_GPL,
  keyring_init,                                           /*   init function (when loaded)     */
  keyring_deinit,                                         /*   deinit function (when unloaded) */
  0x0100,                                                 /*   version                         */
  NULL,                                                   /*   status variables                */
  keyring_system_variables,                               /*   system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;
