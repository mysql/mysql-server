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

#ifndef MYSQL_KEYRING_H
#define MYSQL_KEYRING_H

#include <memory>

#include "my_rnd.h"
#include "mysql/plugin.h"
#include "plugin/keyring/common/keyring_memory.h"
#include "plugin/keyring/common/keys_container.h"
#include "plugin/keyring/common/keys_iterator.h"
#include "sql/mysqld.h"

using keyring::IKey;
using keyring::IKeyring_io;
using keyring::IKeys_container;
using keyring::ILogger;
using keyring::Keys_iterator;

namespace keyring {
/* Always defined. */
extern PSI_memory_key key_memory_KEYRING;
extern PSI_rwlock_key key_LOCK_keyring;
}  // namespace keyring

extern mysql_rwlock_t LOCK_keyring;

extern std::unique_ptr<IKeys_container> keys;
extern volatile bool is_keys_container_initialized;
extern std::unique_ptr<ILogger> logger;
extern std::unique_ptr<char[]> keyring_file_data;

#ifdef HAVE_PSI_INTERFACE
void keyring_init_psi_keys(void);
#endif  // HAVE_PSI_INTERFACE

bool init_keyring_locks();
bool create_keyring_dir_if_does_not_exist(const char *keyring_file_path);

void update_keyring_file_data(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                              SYS_VAR *var MY_ATTRIBUTE((unused)),
                              void *var_ptr MY_ATTRIBUTE((unused)),
                              const void *save_ptr);

bool mysql_key_fetch(std::unique_ptr<IKey> key_to_fetch, char **key_type,
                     void **key, size_t *key_len);
bool mysql_key_store(std::unique_ptr<IKey> key_to_store);
bool mysql_key_remove(std::unique_ptr<IKey> key_to_remove);

bool mysql_keyring_iterator_init(Keys_iterator *);
void mysql_keyring_iterator_deinit(Keys_iterator *);
bool mysql_keyring_iterator_get_key(Keys_iterator *, char *key_id,
                                    char *user_id);

bool check_key_for_writing(IKey *key, std::string error_for);

void log_operation_error(const char *failed_operation, const char *plugin_name);

bool is_key_length_and_type_valid(const char *key_type, size_t key_len);

template <typename T>
bool mysql_key_fetch(const char *key_id, char **key_type, const char *user_id,
                     void **key, size_t *key_len, const char *plugin_name) {
  try {
    std::unique_ptr<IKey> key_to_fetch(new T(key_id, NULL, user_id, NULL, 0));
    return mysql_key_fetch(std::move(key_to_fetch), key_type, key, key_len);
  } catch (...) {
    log_operation_error("fetch a key", plugin_name);
    return true;
  }
}

template <typename T>
bool mysql_key_store(const char *key_id, const char *key_type,
                     const char *user_id, const void *key, size_t key_len,
                     const char *plugin_name) {
  try {
    std::unique_ptr<IKey> key_to_store(
        new T(key_id, key_type, user_id, key, key_len));
    return mysql_key_store(std::move(key_to_store));
  } catch (...) {
    log_operation_error("store a key", plugin_name);
    return true;
  }
}

template <typename T>
bool mysql_key_remove(const char *key_id, const char *user_id,
                      const char *plugin_name) {
  try {
    std::unique_ptr<IKey> key_to_remove(new T(key_id, NULL, user_id, NULL, 0));
    return mysql_key_remove(std::move(key_to_remove));
  } catch (...) {
    log_operation_error("remove a key", plugin_name);
    return true;
  }
}

template <typename T>
bool mysql_key_iterator_init(Keys_iterator *key_iterator,
                             const char *plugin_name) {
  try {
    return mysql_keyring_iterator_init(key_iterator);
  } catch (...) {
    log_operation_error("iterator init", plugin_name);
    return true;
  }
}

template <typename T>
void mysql_key_iterator_deinit(Keys_iterator *key_iterator,
                               const char *plugin_name) {
  try {
    mysql_keyring_iterator_deinit(key_iterator);
  } catch (...) {
    log_operation_error("iterator deinit", plugin_name);
  }
}

template <typename T>
bool mysql_key_iterator_get_key(Keys_iterator *key_iterator, char *key_id,
                                char *user_id, const char *plugin_name) {
  try {
    return mysql_keyring_iterator_get_key(key_iterator, key_id, user_id);
  } catch (...) {
    log_operation_error("iterator get_key", plugin_name);
    return true;
  }
}

#endif  // MYSQL_KEYRING_H
