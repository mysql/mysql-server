/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/sha256_password_cache.h"

#include "helper/multithread/rw_lock.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

SHA256_password_cache::SHA256_password_cache()
    : m_cache_lock(KEY_rwlock_x_sha256_password_cache) {}

/**
  Start caching.

  "Upsert" operation is going to cache all account informations passed to it.
*/
void SHA256_password_cache::enable() {
  RWLock_writelock guard(m_cache_lock);
  m_accepting_input = true;
}

/**
  Stop caching.

  Remove all already cached accounts and prevent the "Upsert" operation from
  caching account informations.
*/
void SHA256_password_cache::disable() {
  RWLock_writelock guard(m_cache_lock);
  m_accepting_input = false;

  m_password_cache.clear();
}

/**
  Update a cache entry or add a new entry if there is no entry with the given
  key. Key is constucted from username and hostname.

  @param [in] user Username which will be used as a part of the cache entry key
  @param [in] host Hostname which will be used as a part of the cache entry key
  @param [in] value Value to be cached

  @return Result of upsert operation
    @retval true Value added or updated successfully
    @retval false Upsert operation failed
*/
bool SHA256_password_cache::upsert(const std::string &user,
                                   const std::string &host,
                                   const std::string &value) {
  auto key = create_key(user, host);
  auto optional_hash = create_hash(value);
  RWLock_writelock guard(m_cache_lock);

  if (!m_accepting_input) return false;

  if (!optional_hash.first) return false;

  m_password_cache[key] = optional_hash.second;
  return true;
}

/**
  Remove an entry from the cache

  @param [in] user Username which will be used as a part of the cache entry key
  @param [in] host Hostname which will be used as a part of the cache entry key

  @return result of the deletion
    @retval true Entry successfully removed
    @retval false Error while removing the entry
*/
bool SHA256_password_cache::remove(const std::string &user,
                                   const std::string &host) {
  RWLock_writelock guard(m_cache_lock);
  return m_password_cache.erase(create_key(user, host));
}

/**
  Try to get an entry from the cache.

  @param [in] user Username which will be used as a part of the cache entry key
  @param [in] host Hostname which will be used as a part of the cache entry key

  @returns Pair containing flag and a string. When search was successful
  returns true and cache entry value. Otherwise returns false and an empty
  string
*/
std::pair<bool, std::string> SHA256_password_cache::get_entry(
    const std::string &user, const std::string &host) const {
  RWLock_readlock guard(m_cache_lock);

  if (!m_accepting_input) return {false, ""};

  const auto it = m_password_cache.find(create_key(user, host));

  if (it == m_password_cache.end()) return {false, ""};

  return {true, it->second};
}

/**
  Check if hash of the given value is stored in the cache.

  @param [in] user Username which will be used as a part of the cache entry key
  @param [in] host Hostname which will be used as a part of the cache entry key
  @param [in] value Value used as base for a hash which will be searched for

  @return
    @retval true Value hash is stored in the cache
    @retval false Value hash is not in the cache
*/
bool SHA256_password_cache::contains(const std::string &user,
                                     const std::string &host,
                                     const std::string &value) const {
  const auto search_result = get_entry(user, host);

  if (!search_result.first) return false;

  const auto optional_hash = create_hash(value);

  return !optional_hash.first ? false
                              : search_result.second == optional_hash.second;
}

/**
  Remove all cache entries.
*/
void SHA256_password_cache::clear() {
  RWLock_writelock guard(m_cache_lock);

  m_password_cache.clear();
}

/**
  Create a key which will be used in the cache.

  @param [in] user Username which will be used as a part of the cache entry key
  @param [in] host Hostname which will be used as a part of the cache entry key

  @returns Key string.
*/
std::string SHA256_password_cache::create_key(const std::string &user,
                                              const std::string &host) const {
  return user + '\0' + host + '\0';
}

/**
  Create SHA256(SHA256(value)) hash from the given value. It will be used as
  a cache entry

  @param[in] value Value which will be used to calculate the hash

  @returns Pair containing flag and a cache entry. Flag is returned so that in
  case when hash could not be generated we will not insert empty string into
  the password cache
*/
std::pair<bool, SHA256_password_cache::sha2_cache_entry_t>
SHA256_password_cache::create_hash(const std::string &value) const {
  // Locking not needed since SHA256_digest does not contain any shared state
  sha2_password::SHA256_digest sha256_digest;

  const auto length = sha2_password::CACHING_SHA2_DIGEST_LENGTH;
  unsigned char digest_buffer[length];

  auto one_digest_round = [&](const std::string &value) {
    if (sha256_digest.update_digest(value.c_str(), value.length()) ||
        sha256_digest.retrieve_digest(digest_buffer, length)) {
      return false;
    }
    return true;
  };

  // First digest round
  if (!one_digest_round(value)) return {false, ""};

  sha256_digest.scrub();

  std::string first_digest_round = {std::begin(digest_buffer),
                                    std::end(digest_buffer)};

  // Second digest round
  if (!one_digest_round(first_digest_round)) return {false, ""};

  return {true, {std::begin(digest_buffer), std::end(digest_buffer)}};
}

}  // namespace xpl
