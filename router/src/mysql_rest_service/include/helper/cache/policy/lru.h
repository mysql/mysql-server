/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_CACHE_POLICY_LRU_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_CACHE_POLICY_LRU_H_

#include <algorithm>

#include "helper/container/cyclic_buffer.h"

namespace helper {
namespace cache {
namespace policy {

class Lru {
 public:
  template <typename Key, typename Value, uint32_t size>
  class Algorithm {
   public:
    using Buffer = container::CycleBufferArray<Key, size>;

    void access(const Key key) {
      auto it = std::remove(buffer_.begin(), buffer_.end(), key);

      *it = key;
    }

    void remove(const Key key) {
      std::remove(buffer_.begin(), buffer_.end(), key);
      buffer_.pop_back();
    }

    void push(const Key key, Key **out_key = nullptr) {
      if (size == buffer_.size()) {
        key_ = buffer_.front();
        if (out_key) *out_key = &key_;
      }

      buffer_.push_back(key);
    }

    Key *pop() {
      if (buffer_.empty()) return nullptr;

      key_ = buffer_.front();
      buffer_.pop_front();
      return &key_;
    }

    const Buffer &get_container() const { return buffer_; }

   private:
    Buffer buffer_;
    Key key_;
  };
};

}  // namespace policy
}  // namespace cache
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_CACHE_POLICY_LRU_H_
