/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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
#include <unordered_map>
#include <utility>

#include "plugin/x/src/helper/multithread/rw_lock.h"
#include "plugin/x/src/interface/sha256_password_cache.h"
#include "plugin/x/src/ngs/thread.h"
#include "plugin/x/src/xpl_performance_schema.h"
#include "sql/auth/i_sha2_password_common.h"

namespace xpl {

/**
  Class used for storing hashed passwords for each authenticated user. This
  allows for fast authentication.
*/
class SHA256_password_cache final : public iface::SHA256_password_cache {
 public:
  using sha2_cache_entry_t = std::string;
  using password_cache_t = std::unordered_map<std::string, sha2_cache_entry_t>;

  SHA256_password_cache();
  SHA256_password_cache(SHA256_password_cache &) = delete;
  SHA256_password_cache &operator=(const SHA256_password_cache &) = delete;
  SHA256_password_cache(SHA256_password_cache &&) = delete;
  SHA256_password_cache &operator=(SHA256_password_cache &&) = delete;

  void enable() override;
  void disable() override;

  bool upsert(const std::string &user, const std::string &host,
              const std::string &value) override;
  bool remove(const std::string &user, const std::string &host) override;
  std::pair<bool, std::string> get_entry(
      const std::string &user, const std::string &host) const override;
  bool contains(const std::string &user, const std::string &host,
                const std::string &value) const override;
  std::size_t size() const override { return m_password_cache.size(); }
  void clear() override;

 private:
  std::string create_key(const std::string &user,
                         const std::string &host) const;
  std::pair<bool, sha2_cache_entry_t> create_hash(
      const std::string &value) const;

  mutable RWLock m_cache_lock;
  password_cache_t m_password_cache;
  bool m_accepting_input = false;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SHA256_PASSWORD_CACHE_H_
