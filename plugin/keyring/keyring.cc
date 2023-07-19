/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "my_config.h"

#include <mysql/plugin_keyring.h>
#include <memory>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "mysqld_error.h"
#include "plugin/keyring/buffered_file_io.h"
#include "plugin/keyring/common/keyring.h"

#ifdef _WIN32
#define MYSQL_DEFAULT_KEYRINGFILE MYSQL_KEYRINGDIR "\\keyring"
#else
#define MYSQL_DEFAULT_KEYRINGFILE MYSQL_KEYRINGDIR "/keyring"
#endif

using keyring::Buffered_file_io;
using keyring::Key;
using keyring::Keys_container;
using keyring::Keys_iterator;
using keyring::Logger;

mysql_rwlock_t LOCK_keyring;

int check_keyring_file_data(MYSQL_THD thd [[maybe_unused]],
                            SYS_VAR *var [[maybe_unused]], void *save,
                            st_mysql_value *value) {
  char buff[FN_REFLEN + 1];
  const char *keyring_filename;
  int len = sizeof(buff);
  std::unique_ptr<IKeys_container> new_keys(new Keys_container(logger.get()));

  (*(const char **)save) = nullptr;
  keyring_filename = value->val_str(value, buff, &len);
  mysql_rwlock_wrlock(&LOCK_keyring);
  if (create_keyring_dir_if_does_not_exist(keyring_filename)) {
    mysql_rwlock_unlock(&LOCK_keyring);
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_SET_KEYRING_FILE_DATA);
    return 1;
  }
  try {
    IKeyring_io *keyring_io(new Buffered_file_io(logger.get()));
    if (new_keys->init(keyring_io, keyring_filename)) {
      mysql_rwlock_unlock(&LOCK_keyring);
      return 1;
    }
    *reinterpret_cast<IKeys_container **>(save) = new_keys.get();
    new_keys.release();
    mysql_rwlock_unlock(&LOCK_keyring);
  } catch (...) {
    mysql_rwlock_unlock(&LOCK_keyring);
    return 1;
  }
  return (0);
}

static char *keyring_file_data_value = nullptr;
static MYSQL_SYSVAR_STR(
    data,                                              /* name       */
    keyring_file_data_value,                           /* value      */
    PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_NODEFAULT,        /* flags      */
    "The path to the keyring file. Must be specified", /* comment    */
    check_keyring_file_data,                           /* check()    */
    update_keyring_file_data,                          /* update()   */
    MYSQL_DEFAULT_KEYRINGFILE                          /* default    */
);

static MYSQL_SYSVAR_BOOL(open_mode, keyring_open_mode,
                         PLUGIN_VAR_INVISIBLE | PLUGIN_VAR_RQCMDARG,
                         "Mode in which keyring file should be opened", nullptr,
                         nullptr, true);

static SYS_VAR *keyring_file_system_variables[] = {
    MYSQL_SYSVAR(data), MYSQL_SYSVAR(open_mode), nullptr};

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static int keyring_init(MYSQL_PLUGIN plugin_info [[maybe_unused]]) {
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return true;

  logger.reset(new Logger());
  logger->log(WARNING_LEVEL, ER_SERVER_WARN_DEPRECATED, "keyring_file plugin",
              "component_keyring_file");

  try {
    SSL_library_init();  // always returns 1
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    ERR_load_BIO_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

#ifdef HAVE_PSI_INTERFACE
    keyring_init_psi_keys();
#endif

    DBUG_EXECUTE_IF("simulate_keyring_init_error", return true;);

    if (init_keyring_locks()) return true;

    if (create_keyring_dir_if_does_not_exist(keyring_file_data_value)) {
      logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_CREATE_KEYRING_DIR);
      return true;
    }
    keys.reset(new Keys_container(logger.get()));
    std::vector<std::string> allowedFileVersionsToInit;
    // this keyring will work with keyring files in the following versions:
    allowedFileVersionsToInit.push_back(keyring::keyring_file_version_2_0);
    allowedFileVersionsToInit.push_back(keyring::keyring_file_version_1_0);
    IKeyring_io *keyring_io =
        new Buffered_file_io(logger.get(), &allowedFileVersionsToInit);
    if (keys->init(keyring_io, keyring_file_data_value)) {
      is_keys_container_initialized = false;
      logger->log(ERROR_LEVEL, ER_KEYRING_FILE_INIT_FAILED);
      return true;
    }
    is_keys_container_initialized = true;
    return false;
  } catch (...) {
    if (logger != nullptr)
      logger->log(ERROR_LEVEL, ER_KEYRING_INTERNAL_EXCEPTION_FAILED_FILE_INIT);
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    return true;
  }
}

static int keyring_deinit(void *arg [[maybe_unused]]) {
// not taking a lock here as the calls to keyring_deinit are serialized by
// the plugin framework
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  ERR_free_strings();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  keys.reset();
  logger.reset();
  delete_keyring_file_data();
  mysql_rwlock_destroy(&LOCK_keyring);

  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  return 0;
}

static bool mysql_key_fetch(const char *key_id, char **key_type,
                            const char *user_id, void **key, size_t *key_len) {
  return mysql_key_fetch<keyring::Key>(key_id, key_type, user_id, key, key_len,
                                       "keyring_file");
}

static bool mysql_key_store(const char *key_id, const char *key_type,
                            const char *user_id, const void *key,
                            size_t key_len) {
  return mysql_key_store<keyring::Key>(key_id, key_type, user_id, key, key_len,
                                       "keyring_file");
}

static bool mysql_key_remove(const char *key_id, const char *user_id) {
  return mysql_key_remove<keyring::Key>(key_id, user_id, "keyring_file");
}

static bool mysql_key_generate(const char *key_id, const char *key_type,
                               const char *user_id, size_t key_len) {
  try {
    std::unique_ptr<IKey> key_candidate(
        new Key(key_id, key_type, user_id, nullptr, 0));

    std::unique_ptr<uchar[]> key(new uchar[key_len]);
    if (key.get() == nullptr) return true;
    memset(key.get(), 0, key_len);
    if (is_keys_container_initialized == false ||
        check_key_for_writing(key_candidate.get(), "generating") ||
        my_rand_buffer(key.get(), key_len))
      return true;

    return mysql_key_store(key_id, key_type, user_id, key.get(), key_len) ==
           true;
  } catch (...) {
    if (logger != nullptr)
      logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_GENERATE_KEY);
    return true;
  }
}

static void mysql_key_iterator_init(void **key_iterator) {
  *key_iterator = new Keys_iterator(logger.get());
  if (mysql_key_iterator_init<keyring::Key>(
          static_cast<Keys_iterator *>(*key_iterator), "keyring_file") ==
      true) {
    delete static_cast<Keys_iterator *>(*key_iterator);
    *key_iterator = nullptr;
  }
}

static void mysql_key_iterator_deinit(void *key_iterator) {
  mysql_key_iterator_deinit<keyring::Key>(
      static_cast<Keys_iterator *>(key_iterator), "keyring_file");
  delete static_cast<Keys_iterator *>(key_iterator);
}

static bool mysql_key_iterator_get_key(void *key_iterator, char *key_id,
                                       char *user_id) {
  return mysql_key_iterator_get_key<keyring::Key>(
      static_cast<Keys_iterator *>(key_iterator), key_id, user_id,
      "keyring_file");
}

/* Plugin type-specific descriptor */
static struct st_mysql_keyring keyring_descriptor = {
    MYSQL_KEYRING_INTERFACE_VERSION,
    mysql_key_store,
    mysql_key_fetch,
    mysql_key_remove,
    mysql_key_generate,
    mysql_key_iterator_init,
    mysql_key_iterator_deinit,
    mysql_key_iterator_get_key};

mysql_declare_plugin(keyring_file){
    MYSQL_KEYRING_PLUGIN, /*   type                            */
    &keyring_descriptor,  /*   descriptor                      */
    "keyring_file",       /*   name                            */
    PLUGIN_AUTHOR_ORACLE, /*   author                          */
    "store/fetch authentication data to/from a flat file", /*   description */
    PLUGIN_LICENSE_GPL,
    keyring_init,                  /*   init function (when loaded)     */
    nullptr,                       /*   check uninstall function        */
    keyring_deinit,                /*   deinit function (when unloaded) */
    0x0100,                        /*   version                         */
    nullptr,                       /*   status variables                */
    keyring_file_system_variables, /*   system variables                */
    nullptr,
    PLUGIN_OPT_ALLOW_EARLY,
} mysql_declare_plugin_end;
