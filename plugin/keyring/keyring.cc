/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#define LOG_SUBSYSTEM_TAG "keyring_file"

#include "my_config.h"

#include <mysql/plugin_keyring.h>
#include <memory>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include "plugin/keyring/buffered_file_io.h"
#include "plugin/keyring/common/keyring.h"

#ifdef _WIN32
#define MYSQL_DEFAULT_KEYRINGFILE MYSQL_KEYRINGDIR"\\keyring"
#else
#define MYSQL_DEFAULT_KEYRINGFILE MYSQL_KEYRINGDIR"/keyring"
#endif

using keyring::Buffered_file_io;
using keyring::Key;
using keyring::Keys_container;
using keyring::Keys_iterator;
using keyring::Logger;

mysql_rwlock_t LOCK_keyring;

int check_keyring_file_data(MYSQL_THD thd  MY_ATTRIBUTE((unused)),
                            struct st_mysql_sys_var *var  MY_ATTRIBUTE((unused)),
                            void *save, st_mysql_value *value)
{
  char            buff[FN_REFLEN+1];
  const char      *keyring_filename;
  int             len = sizeof(buff);
  std::unique_ptr<IKeys_container> new_keys(new Keys_container(logger.get()));

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
    IKeyring_io *keyring_io(new Buffered_file_io(logger.get()));
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

static struct st_mysql_sys_var *keyring_file_system_variables[]= {
  MYSQL_SYSVAR(data),
  NULL
};


static SERVICE_TYPE(registry) *reg_srv= nullptr;
SERVICE_TYPE(log_builtins) *log_bi= nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs= nullptr;

static int keyring_init(MYSQL_PLUGIN plugin_info)
{
  if (init_logging_service_for_plugin(&reg_srv))
    return TRUE;

  try
  {
    SSL_library_init(); //always returns 1
#ifndef HAVE_YASSL
    ERR_load_BIO_strings();
#endif
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

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
    keys.reset(new Keys_container(logger.get()));
    std::vector<std::string> allowedFileVersionsToInit;
    //this keyring will work with keyring files in the following versions:
    allowedFileVersionsToInit.push_back(keyring::keyring_file_version_2_0);
    allowedFileVersionsToInit.push_back(keyring::keyring_file_version_1_0);
    IKeyring_io *keyring_io= new Buffered_file_io(logger.get(), &allowedFileVersionsToInit);
    if (keys->init(keyring_io, keyring_file_data_value))
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
    if (logger != NULL)
      logger->log(MY_ERROR_LEVEL, "keyring_file initialization failure due to internal"
                                  " exception inside the plugin");
    deinit_logging_service_for_plugin(&reg_srv);
    return TRUE;
  }
}

static int keyring_deinit(void *arg MY_ATTRIBUTE((unused)))
{
  //not taking a lock here as the calls to keyring_deinit are serialized by
  //the plugin framework
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  ERR_free_strings();
  EVP_cleanup();
#ifndef HAVE_YASSL
  CRYPTO_cleanup_all_ex_data();
#endif
  keys.reset();
  logger.reset();
  keyring_file_data.reset();
  mysql_rwlock_destroy(&LOCK_keyring);

  deinit_logging_service_for_plugin(&reg_srv);

  return 0;
}

static bool mysql_key_fetch(const char *key_id, char **key_type, const char *user_id,
                            void **key, size_t *key_len)
{
  return mysql_key_fetch<keyring::Key>(key_id, key_type, user_id, key, key_len,
                                       "keyring_file");
}

static bool mysql_key_store(const char *key_id, const char *key_type,
                            const char *user_id, const void *key, size_t key_len)
{
  return mysql_key_store<keyring::Key>(key_id, key_type, user_id, key, key_len,
                                       "keyring_file");
}

static bool mysql_key_remove(const char *key_id, const char *user_id)
{
  return mysql_key_remove<keyring::Key>(key_id, user_id, "keyring_file");
}


static bool mysql_key_generate(const char *key_id, const char *key_type,
                               const char *user_id, size_t key_len)
{
  try
  {
    std::unique_ptr<IKey> key_candidate(new Key(key_id, key_type, user_id, NULL, 0));

    std::unique_ptr<uchar[]> key(new uchar[key_len]);
    if (key.get() == NULL)
      return TRUE;
    memset(key.get(), 0, key_len);
    if (is_keys_container_initialized == FALSE || check_key_for_writing(key_candidate.get(), "generating") ||
        my_rand_buffer(key.get(), key_len))
      return TRUE;

    return mysql_key_store(key_id, key_type, user_id, key.get(), key_len) == TRUE;
  }
  catch (...)
  {
    if (logger != NULL)
      logger->log(MY_ERROR_LEVEL, "Failed to generate a key due to internal exception inside keyring_file plugin");
    return TRUE;
  }
}

static void mysql_key_iterator_init(void **key_iterator)
{
  *key_iterator= new Keys_iterator(logger.get());
  mysql_key_iterator_init<keyring::Key>(static_cast<Keys_iterator*>(*key_iterator),
                                               "keyring_file");
}

static void mysql_key_iterator_deinit(void *key_iterator)
{
  mysql_key_iterator_deinit<keyring::Key>(static_cast<Keys_iterator*>(key_iterator),
                                          "keyring_file");
  delete static_cast<Keys_iterator*>(key_iterator);
}

static bool mysql_key_iterator_get_key(void *key_iterator,
                                       char *key_id, char *user_id)
{
  return mysql_key_iterator_get_key<keyring::Key>(static_cast<Keys_iterator*>(key_iterator),
                                                  key_id, user_id, "keyring_file");
}

/* Plugin type-specific descriptor */
static struct st_mysql_keyring keyring_descriptor=
{
  MYSQL_KEYRING_INTERFACE_VERSION,
  mysql_key_store,
  mysql_key_fetch,
  mysql_key_remove,
  mysql_key_generate,
  mysql_key_iterator_init,
  mysql_key_iterator_deinit,
  mysql_key_iterator_get_key
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
  NULL,                                                   /*   check uninstall function        */
  keyring_deinit,                                         /*   deinit function (when unloaded) */
  0x0100,                                                 /*   version                         */
  NULL,                                                   /*   status variables                */
  keyring_file_system_variables,                          /*   system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;
