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

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_FLEXIBLE_BUFFER_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_FLEXIBLE_BUFFER_H_

#include "mysql/harness/net_ts/buffer.h"

namespace net {
namespace tls {

class FlexibleBuffer {
 public:
  FlexibleBuffer() noexcept {}
  FlexibleBuffer(void *p, size_t n) noexcept
      : data_{p}, size_{0}, full_size_{n} {}
  FlexibleBuffer(FlexibleBuffer &&fb) noexcept {
    data_ = fb.data_;
    size_ = fb.size_;
    full_size_ = fb.full_size_;

    fb.size_ = fb.full_size_ = 0;
    fb.data_ = nullptr;
  }
  FlexibleBuffer(const FlexibleBuffer &fb) noexcept {
    data_ = fb.data_;
    size_ = fb.size_;
    full_size_ = fb.full_size_;
  }

  FlexibleBuffer(const net::mutable_buffer &b) noexcept
      : data_{b.data()}, size_{0}, full_size_{b.size()} {}

  void *data_used() const noexcept { return data_; }
  void *data_free() const { return reinterpret_cast<uint8_t *>(data_) + size_; }
  size_t size_free() const { return full_size_ - size_; }
  size_t size_used() const { return size_; }
  size_t size_full() const { return full_size_; }

  void reset() { size_ = 0; }

  const FlexibleBuffer *begin() const { return this; }
  const FlexibleBuffer *end() const { return this + 1; }

  bool pop(size_t v) noexcept {
    if (size_ >= v) {
      size_ -= v;
      memmove(data_, reinterpret_cast<uint8_t *>(data_) + v, size_);
      return true;
    }

    size_ = 0;
    return false;
  }

  bool push(size_t v) noexcept {
    size_ += v;
    if (size_ <= full_size_) return true;
    size_ = full_size_;
    return false;
  }

 protected:
  void *data_{nullptr};
  size_t size_{0};
  size_t full_size_{0};
};

class FlexibleOutputBuffer : public FlexibleBuffer {
 public:
  using FlexibleBuffer::FlexibleBuffer;

  const FlexibleOutputBuffer *begin() const { return this; }
  const FlexibleOutputBuffer *end() const { return this + 1; }

  void *data() const noexcept { return data_; }
  size_t size() const noexcept { return size_used(); }

  // Return only the data that were pushed to the buffer
  operator net::const_buffer() const { return net::buffer(data_, size_); }
};

class FlexibleInputBuffer : public FlexibleBuffer {
 public:
  using FlexibleBuffer::FlexibleBuffer;
  const FlexibleInputBuffer *begin() const { return this; }
  const FlexibleInputBuffer *end() const { return this + 1; }

  void *data() const noexcept { return data_free(); }
  size_t size() const noexcept { return size_free(); }

  // Return the data that are not used inside the buffer
  operator net::mutable_buffer() const {
    return net::buffer(reinterpret_cast<char *>(data_free()), size_free());
  }
};

}  // namespace tls
}  // namespace net

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_FLEXIBLE_BUFFER_H_
