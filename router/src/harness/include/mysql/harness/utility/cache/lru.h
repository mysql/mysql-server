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

#ifndef MYSQL_HARNESS_UTILITY_CACHE_LRU_H_
#define MYSQL_HARNESS_UTILITY_CACHE_LRU_H_

#include <algorithm>

#include <concepts>
#include <utility>
#include "mysql/harness/utility/container/cyclic_buffer.h"

namespace mysql_harness {
namespace utility {
namespace cache {
namespace policy {

class Lru {
 public:
  template <typename Key, uint32_t size>
  class AlgorithmFixed {
   public:
    using Buffer = container::CycleBufferArray<Key, size>;

    void access(const Key &key) {
      auto it = std::remove(buffer_.begin(), buffer_.end(), key);

      if (it != buffer_.end()) *it = key;
    }

    void remove(const Key &key) {
      if (std::remove(buffer_.begin(), buffer_.end(), key) != buffer_.end()) {
        buffer_.pop_back();
      }
    }

    void push(const Key &key, Key **out_key = nullptr) {
      if (size == buffer_.size()) {
        auto key = pop();
        if (out_key) *out_key = key;
      }

      buffer_.push_back(key);
    }

    Key *pop() {
      if (buffer_.empty()) return nullptr;

      if constexpr (std::movable<Key>) {
        key_ = std::move(buffer_.front());
      } else {
        key_ = buffer_.front();
      }
      buffer_.pop_front();
      return &key_;
    }

    const Buffer &get_container() const { return buffer_; }

    bool is_full() const { return size == buffer_.size(); }

   private:
    Buffer buffer_;
    Key key_;
  };

  template <typename Key>
  class AlgorithmDynamic {
   public:
    using Buffer = std::list<Key>;

    explicit AlgorithmDynamic(size_t max_size) : max_size_{max_size} {}

    void access(const Key &key) {
      if (buffer_.empty()) {
        return;
      }

      if (buffer_.front() != key) {
        buffer_.remove(key);
        buffer_.push_front(key);
      }
    }

    void remove(const Key &key) { buffer_.remove(key); }

    void push(const Key &key, Key **out_key = nullptr) {
      if (max_size_ == buffer_.size()) {
        auto key = pop();
        if (out_key) *out_key = key;
      }

      buffer_.push_front(key);
    }

    Key *pop() {
      if (buffer_.empty()) return nullptr;

      if constexpr (std::movable<Key>) {
        key_ = std::move(buffer_.back());
      } else {
        key_ = buffer_.back();
      }

      buffer_.pop_back();
      return &key_;
    }

    const Buffer &get_container() const { return buffer_; }

    bool is_full() const { return max_size_ == buffer_.size(); }

   private:
    Buffer buffer_;
    Key key_;
    size_t max_size_;
  };
};

}  // namespace policy
}  // namespace cache
}  // namespace utility
}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_UTILITY_CACHE_LRU_H_
