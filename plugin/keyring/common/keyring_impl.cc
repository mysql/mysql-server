/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stddef.h>
#include <memory>
#include <sstream>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_socket.h"
#include "mysqld_error.h"
#include "plugin/keyring/common/keyring.h"

namespace keyring {
/* Always defined. */
PSI_memory_key key_memory_KEYRING;
PSI_rwlock_key key_LOCK_keyring;
}  // namespace keyring

std::unique_ptr<IKeys_container> keys(nullptr);
volatile bool is_keys_container_initialized = false;
std::unique_ptr<ILogger> logger(nullptr);
char *keyring_file_data(nullptr);
bool keyring_open_mode = false;  // 0 - Read|Write|Create; 1 - Read only

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_info all_keyring_rwlocks[] = {
    {&keyring::key_LOCK_keyring, "LOCK_keyring", 0, 0, PSI_DOCUMENT_ME}};

static PSI_memory_info all_keyring_memory[] = {
    {&keyring::key_memory_KEYRING, "KEYRING", 0, 0, PSI_DOCUMENT_ME}};

void delete_keyring_file_data() {
  free(keyring_file_data);
  keyring_file_data = nullptr;
}

void keyring_init_psi_keys(void) {
  const char *category = "keyring";
  int count;

  count = static_cast<int>(array_elements(all_keyring_memory));
  mysql_memory_register(category, all_keyring_memory, count);

  count = static_cast<int>(array_elements(all_keyring_rwlocks));
  mysql_rwlock_register(category, all_keyring_rwlocks, count);
}
#endif  // HAVE_PSI_INTERFACE

bool init_keyring_locks() {
  return mysql_rwlock_init(keyring::key_LOCK_keyring, &LOCK_keyring);
}

bool is_key_length_and_type_valid(const char *key_type, size_t key_len) {
  const std::string key_type_str(key_type);
  bool is_key_len_valid = false;
  bool is_type_valid = true;

  if (key_type_str == keyring::AES)
    is_key_len_valid = (key_len == 16 || key_len == 24 || key_len == 32);
  else if (key_type_str == keyring::RSA)
    is_key_len_valid = (key_len == 128 || key_len == 256 || key_len == 512);
  else if (key_type_str == keyring::DSA)
    is_key_len_valid = (key_len == 128 || key_len == 256 || key_len == 384);
  else if (key_type_str == keyring::SECRET)
    is_key_len_valid = (key_len > 0 && key_len <= 16384);
  else {
    is_type_valid = false;
    logger->log(ERROR_LEVEL, ER_KEYRING_INVALID_KEY_TYPE);
  }

  if (is_type_valid && !is_key_len_valid)
    logger->log(ERROR_LEVEL, ER_KEYRING_INVALID_KEY_LENGTH);

  return is_type_valid && is_key_len_valid;
}

void log_operation_error(const char *failed_operation,
                         const char *plugin_name) {
  if (logger != nullptr) {
    logger->log(ERROR_LEVEL, ER_KEYRING_OPERATION_FAILED_DUE_TO_INTERNAL_ERROR,
                failed_operation, plugin_name);
  }
}

bool create_keyring_dir_if_does_not_exist(const char *keyring_file_path) {
  if (!keyring_file_path || strlen(keyring_file_path) == 0) return true;
  char keyring_dir[FN_REFLEN];
  size_t keyring_dir_length;
  dirname_part(keyring_dir, keyring_file_path, &keyring_dir_length);
  if (keyring_dir_length > 1 &&
      (keyring_dir[keyring_dir_length - 1] == FN_LIBCHAR)) {
    keyring_dir[keyring_dir_length - 1] = '\0';
    --keyring_dir_length;
  }
  constexpr int flags =
#ifdef _WIN32
      0
#else
      S_IRWXU | S_IRGRP | S_IXGRP
#endif
      ;
  /*
    If keyring_dir_length is 0, it means file
    is being created current working directory
  */
  if (strlen(keyring_dir) != 0) my_mkdir(keyring_dir, flags, MYF(0));
  return false;
}

void log_opearation_error(const char *failed_operation,
                          const char *plugin_name) {
  if (logger != nullptr) {
    logger->log(ERROR_LEVEL, ER_KEYRING_OPERATION_FAILED_DUE_TO_INTERNAL_ERROR,
                failed_operation, plugin_name);
  }
}

void update_keyring_file_data(MYSQL_THD thd [[maybe_unused]],
                              SYS_VAR *var [[maybe_unused]],
                              void *var_ptr [[maybe_unused]],
                              const void *save_ptr) {
  mysql_rwlock_wrlock(&LOCK_keyring);
  IKeys_container *new_keys =
      *reinterpret_cast<IKeys_container **>(const_cast<void *>(save_ptr));
  keys.reset(new_keys);
  free(keyring_file_data);
  keyring_file_data = (static_cast<char *>(
      malloc(new_keys->get_keyring_storage_url().length() + 1)));
  memcpy(keyring_file_data, new_keys->get_keyring_storage_url().c_str(),
         new_keys->get_keyring_storage_url().length() + 1);
  *reinterpret_cast<char **>(var_ptr) = keyring_file_data;
  is_keys_container_initialized = true;
  mysql_rwlock_unlock(&LOCK_keyring);
}

bool mysql_key_fetch(std::unique_ptr<IKey> key_to_fetch, char **key_type,
                     void **key, size_t *key_len) {
  if (!is_keys_container_initialized) return true;

  if (!key_to_fetch->is_key_id_valid()) {
    logger->log(ERROR_LEVEL, ER_KEYRING_KEY_FETCH_FAILED_DUE_TO_EMPTY_KEY_ID);
    return true;
  }
  mysql_rwlock_rdlock(&LOCK_keyring);
  IKey *fetched_key = keys->fetch_key(key_to_fetch.get());
  mysql_rwlock_unlock(&LOCK_keyring);
  if (fetched_key) {
    *key_len = fetched_key->get_key_data_size();
    fetched_key->xor_data();
    *key = static_cast<void *>(fetched_key->release_key_data());
    *key_type =
        my_strdup(keyring::key_memory_KEYRING,
                  fetched_key->get_key_type_as_string()->c_str(), MYF(MY_WME));
  } else
    *key = nullptr;
  return false;
}

bool check_key_for_writing(IKey *key, const std::string &error_for) {
  if (!key->is_key_type_valid()) {
    logger->log(ERROR_LEVEL, ER_KEYRING_CHECK_KEY_FAILED_DUE_TO_INVALID_KEY,
                error_for.c_str());
    return true;
  }
  if (!key->is_key_id_valid()) {
    logger->log(ERROR_LEVEL, ER_KEYRING_CHECK_KEY_FAILED_DUE_TO_EMPTY_KEY_ID,
                error_for.c_str());
    return true;
  }
  return false;
}

bool mysql_key_store(std::unique_ptr<IKey> key_to_store) {
  if (!is_keys_container_initialized) return true;

  if (check_key_for_writing(key_to_store.get(), "storing")) return true;

  if (key_to_store->get_key_data_size() > 0) key_to_store->xor_data();
  mysql_rwlock_wrlock(&LOCK_keyring);
  if (keys->store_key(key_to_store.get())) {
    mysql_rwlock_unlock(&LOCK_keyring);
    return true;
  }
  mysql_rwlock_unlock(&LOCK_keyring);

  (void)key_to_store.release();
  return false;
}

bool mysql_key_remove(std::unique_ptr<IKey> key_to_remove) {
  if (!is_keys_container_initialized) return true;
  if (!key_to_remove->is_key_id_valid()) {
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_REMOVE_KEY_DUE_TO_EMPTY_ID);
    return true;
  }
  mysql_rwlock_wrlock(&LOCK_keyring);
  const bool retval = keys->remove_key(key_to_remove.get());
  mysql_rwlock_unlock(&LOCK_keyring);
  return retval;
}

bool mysql_keyring_iterator_init(Keys_iterator *key_iterator) {
  if (!is_keys_container_initialized) return true;
  mysql_rwlock_rdlock(&LOCK_keyring);
  key_iterator->init();
  mysql_rwlock_unlock(&LOCK_keyring);
  return false;
}

void mysql_keyring_iterator_deinit(Keys_iterator *key_iterator) {
  key_iterator->deinit();
}

bool mysql_keyring_iterator_get_key(Keys_iterator *key_iterator, char *key_id,
                                    char *user_id) {
  keyring::Key_metadata *key_loaded = nullptr;
  const bool error = key_iterator->get_key(&key_loaded);
  if (!error && key_loaded != nullptr) {
    if (key_id) strcpy(key_id, key_loaded->id->c_str());
    if (user_id) strcpy(user_id, key_loaded->user->c_str());
    delete key_loaded;
  } else if (!error && key_loaded == nullptr) {
    /* no keys exists or all keys are read */
    return true;
  }
  return error;
}
