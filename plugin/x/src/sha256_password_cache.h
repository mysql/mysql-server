/*
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_SHA256_PASSWORD_CACHE_H_
#define PLUGIN_X_SRC_SHA256_PASSWORD_CACHE_H_

#include <string>
#include <utility>

#include "plugin/x/src/account_credential_storage.h"
#include "plugin/x/src/interface/sha256_password_cache.h"

namespace xpl {

/**
  Class used for storing hashed passwords for each authenticated user. This
  allows for fast authentication.
*/
class SHA256_password_cache final : public iface::SHA256_password_cache {
 public:
  using Cache_entry = std::string;

  SHA256_password_cache() = default;
  SHA256_password_cache(SHA256_password_cache &) = delete;
  SHA256_password_cache &operator=(const SHA256_password_cache &) = delete;
  SHA256_password_cache(SHA256_password_cache &&) = delete;
  SHA256_password_cache &operator=(SHA256_password_cache &&) = delete;

  void enable() override;
  void disable() override;

  bool upsert(const std::string &user, const std::string &host,
              const std::string &value) override;
  bool remove(const std::string &user, const std::string &host) override;
  const Cache_entry *get_entry(const std::string &user,
                               const std::string &host) const override;
  bool contains(const std::string &user, const std::string &host,
                const std::string &value) const override;
  std::size_t size() const override { return m_password_cache.size(); }
  void clear() override;

 private:
  std::pair<bool, Cache_entry> create_hash(const std::string &value) const;

  Account_credential_storage<Cache_entry> m_password_cache{false};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SHA256_PASSWORD_CACHE_H_
