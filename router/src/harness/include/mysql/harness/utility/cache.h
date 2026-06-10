/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_UTILITY_CACHE_CACHE_H_
#define MYSQL_HARNESS_UTILITY_CACHE_CACHE_H_

#include <map>
#include <utility>

#include "mysql/harness/utility/cache/lru.h"

namespace mysql_harness {
namespace utility {
namespace cache {

template <typename Key, typename Value, typename Algorithm>
class Cache {
  using Container = std::map<Key, Value>;

 public:
  template <typename... Args>
  explicit Cache(Args &&...args) : key_cache_(std::forward<Args>(args)...) {}

  void remove(const Key &key) {
    auto it = container_.find(key);

    if (container_.end() != it) {
      key_cache_.remove(key);
      container_.erase(it);
    }
  }

  Value *get_cached_value(const Key &key) {
    auto it = container_.find(key);
    if (container_.end() == it) {
      return nullptr;
    }

    key_cache_.access(key);

    return &it->second;
  }

  Value *set(const Key &key, Value &&value) {
    return set_impl(key, std::move(value));
  }

  Value *set(const Key &key, const Value &value) {
    return set_impl(key, value);
  }

  Container &get_container() { return container_; }

  bool is_full() const { return key_cache_.is_full(); }

 private:
  template <typename V>
  Value *set_impl(const Key &key, V &&value) {
    auto cached_value = get_cached_value(key);
    if (cached_value) {
      *cached_value = std::forward<V>(value);
      return cached_value;
    }

    Key *removed_key = nullptr;
    key_cache_.push(key, &removed_key);

    if (removed_key) {
      auto node = container_.extract(*removed_key);
      node.key() = key;
      node.mapped() = std::forward<V>(value);
      container_.insert(std::move(node));
      return &container_[key];
    }

    return &container_.emplace(std::make_pair(key, std::forward<V>(value)))
                .first->second;
  }

  Algorithm key_cache_;
  Container container_;
};

template <typename Key, typename Value, uint32_t size>
using FixedLruCache =
    Cache<Key, Value,
          typename mysql_harness::utility::cache::policy::Lru::AlgorithmFixed<
              Key, size>>;

template <typename Key, typename Value>
using DynamicLruCache = Cache<
    Key, Value,
    typename mysql_harness::utility::cache::policy::Lru::AlgorithmDynamic<Key>>;

}  // namespace cache
}  // namespace utility
}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_UTILITY_CACHE_CACHE_H_
