/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_BASE_DETAILS_OWNED_BUFFER_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_BASE_DETAILS_OWNED_BUFFER_H_

#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "mysql/harness/net_ts/buffer.h"

namespace http {
namespace base {
namespace details {

template <typename T>
class ref_buffer {
 public:
  using This = ref_buffer<T>;
  ref_buffer(T &ref) : ref_{ref} {}
  ref_buffer(const ref_buffer &other) : ref_{other.ref_} {}

  void *data() const noexcept { return ref_.data(); }
  size_t size() const noexcept { return ref_.size(); }

  void reset() { ref_.reset(); }

  This &operator+=(size_t n) {
    ref_ += n;
    return *this;
  }

  operator net::const_buffer() const {
    return net::const_buffer(data(), size());
  }

 private:
  T &ref_;
};

template <typename T>
class ref_buffers {
 public:
  using This = ref_buffers<T>;
  ref_buffers(T &ref) : ref_{&ref} {}
  ref_buffers(const ref_buffers &other) : ref_{other.ref_} {}

  auto begin() const { return ref_->begin(); }
  auto end() const { return ref_->end(); }

  This &operator=(const This &t) { ref_ = t.ref_; }

 private:
  T *ref_;
};

class owned_buffer {
 public:
  owned_buffer(size_t n = 0)
      : buffer_{n ? new uint8_t[n] : nullptr},
        buffer_size_{n},
        data_movable_{buffer_.get()},
        data_size_{0} {}

  owned_buffer(owned_buffer &&other)
      : buffer_{std::move(other.buffer_)},
        buffer_size_{other.buffer_size_},
        data_movable_{other.data_movable_},
        data_size_{other.data_size_} {}

 public:  // Methods needed by net_ts stream template
  void *data() const noexcept { return data_movable_; }
  size_t size() const noexcept { return data_size_; }

  void reset() {
    data_movable_ = buffer_.get();
    data_size_ = 0;
  }

  owned_buffer &operator+=(size_t n) {
    data_movable_ = data_movable_ + n;
    data_size_ = data_size_ > n ? data_size_ - n : 0;
    return *this;
  }

  operator net::const_buffer() const {
    return net::const_buffer(data(), size());
  }

 public:  // Other methods
  size_t space_left() const { return buffer_size_ - data_size_; }
  bool empty() const { return 0 == data_size_; }

  size_t write(const uint8_t *source, size_t source_size) {
    const auto bytes_to_copy = std::min(source_size, space_left());
    memcpy(buffer_.get() + data_size_, source, bytes_to_copy);
    data_size_ += bytes_to_copy;

    return bytes_to_copy;
  }

 private:
  std::unique_ptr<uint8_t[]> buffer_;
  const size_t buffer_size_;

  uint8_t *data_movable_;
  size_t data_size_;
};

}  // namespace details
}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_BASE_DETAILS_OWNED_BUFFER_H_
