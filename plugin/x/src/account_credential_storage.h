/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_ACCOUNT_CREDENTIAL_STORAGE_H_
#define PLUGIN_X_SRC_ACCOUNT_CREDENTIAL_STORAGE_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "plugin/x/src/helper/multithread/rw_lock.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace xpl {

template <typename Entry>
class Account_credential_storage {
 public:
  explicit Account_credential_storage(bool accepting_input)
      : m_accepting_input(accepting_input) {}

  void enable() {
    RWLock_writelock guard(&m_storage_lock);
    m_accepting_input = true;
  }

  void disable() {
    RWLock_writelock guard(&m_storage_lock);
    m_accepting_input = false;
    m_storage.clear();
  }

  bool upsert(const std::string &user, const std::string &host,
              const Entry &value) {
    RWLock_writelock guard(&m_storage_lock);
    if (!m_accepting_input) return false;
    m_storage[create_key(user, host)] = value;
    return true;
  }

  bool remove(const std::string &user, const std::string &host) {
    RWLock_writelock guard(&m_storage_lock);
    return m_storage.erase(create_key(user, host));
  }

  const Entry *get_entry(const std::string &user,
                         const std::string &host) const {
    RWLock_readlock guard(&m_storage_lock);
    if (!m_accepting_input) return nullptr;
    const auto it = m_storage.find(create_key(user, host));
    return it == m_storage.end() ? nullptr : &(it->second);
  }

  std::size_t size() const {
    RWLock_readlock guard(&m_storage_lock);
    return m_storage.size();
  }

  void clear() {
    RWLock_writelock guard(&m_storage_lock);
    m_storage.clear();
  }

 private:
  std::string create_key(const std::string &user,
                         const std::string &host) const {
    return user + '\0' + host + '\0';
  }

  mutable RWLock m_storage_lock{KEY_rwlock_x_sha256_password_cache};
  std::unordered_map<std::string, Entry> m_storage;
  bool m_accepting_input = false;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ACCOUNT_CREDENTIAL_STORAGE_H_
