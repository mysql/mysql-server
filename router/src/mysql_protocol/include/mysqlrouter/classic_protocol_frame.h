/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_FRAME_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_FRAME_H_

#include <cstddef>  // size_t
#include <cstdint>  // uint8_t
#include <utility>  // move

namespace classic_protocol {

namespace frame {
class Header {
 public:
  constexpr Header(size_t payload_size, uint8_t seq_id) noexcept
      : payload_size_{payload_size}, seq_id_{seq_id} {}

  constexpr size_t payload_size() const noexcept { return payload_size_; }
  constexpr uint8_t seq_id() const noexcept { return seq_id_; }

 private:
  size_t payload_size_{0};
  uint8_t seq_id_{0};
};

constexpr bool operator==(const Header &a, const Header &b) {
  return a.payload_size() == b.payload_size() && a.seq_id() == b.seq_id();
}

/**
 * header of a compressed frame.
 *
 * used if client and server negotiated compression.
 */
class CompressedHeader {
 public:
  constexpr CompressedHeader(size_t payload_size, uint8_t seq_id,
                             size_t uncompressed_size)
      : payload_size_{payload_size},
        seq_id_{seq_id},
        uncompressed_size_{uncompressed_size} {}

  constexpr size_t payload_size() const noexcept { return payload_size_; }
  constexpr uint8_t seq_id() const noexcept { return seq_id_; }
  constexpr size_t uncompressed_size() const noexcept {
    return uncompressed_size_;
  }

 private:
  size_t payload_size_;
  uint8_t seq_id_;
  size_t uncompressed_size_;
};

constexpr bool operator==(const CompressedHeader &a,
                          const CompressedHeader &b) {
  return a.payload_size() == b.payload_size() && a.seq_id() == b.seq_id() &&
         a.uncompressed_size() == b.uncompressed_size();
}

template <class PayloadType>
class Frame {
 public:
  using value_type = PayloadType;

  constexpr Frame(uint8_t seq_id, value_type v)
      : seq_id_{seq_id}, payload_{std::move(v)} {}

  constexpr uint8_t seq_id() const { return seq_id_; }
  constexpr value_type payload() const { return payload_; }

 private:
  uint8_t seq_id_;
  value_type payload_;
};

template <class T>
bool operator==(const Frame<T> &a, const Frame<T> &b) {
  return a.seq_id() == b.seq_id() && a.payload() == b.payload();
}

}  // namespace frame

}  // namespace classic_protocol

#endif
