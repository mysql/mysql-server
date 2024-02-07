/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_WIRE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_WIRE_H_

// codecs for classic_protocol::wire::

#include <algorithm>     // find
#include <bit>           // endian
#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <system_error>  // error_code
#include <type_traits>
#include <utility>  // move

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/stdx/bit.h"  // byteswap
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_wire.h"

namespace classic_protocol {
/**
 * codec of a FixedInt.
 *
 * classic proto uses 1, 2, 3, 4, 8 for IntSize
 */
template <int IntSize>
class Codec<borrowable::wire::FixedInt<IntSize>> {
 public:
  static constexpr size_t int_size{IntSize};

  using value_type = borrowable::wire::FixedInt<int_size>;

  constexpr Codec(value_type v, capabilities::value_type /* caps */) : v_{v} {}

  /**
   * size of the encoded object.
   */
  static constexpr size_t size() noexcept { return int_size; }

  /**
   * encode value_type into buffer.
   */
  stdx::expected<size_t, std::error_code> encode(
      net::mutable_buffer buffer) const {
    if (buffer.size() < int_size) {
      return stdx::unexpected(make_error_code(std::errc::no_buffer_space));
    }

    auto int_val = v_.value();

    if (std::endian::native == std::endian::big) {
      int_val = stdx::byteswap(int_val);
    }

    std::copy_n(reinterpret_cast<const std::byte *>(&int_val), int_size,
                static_cast<std::byte *>(buffer.data()));

    return int_size;
  }

  /**
   * maximum bytes which may scanned by the decoder.
   */
  static constexpr size_t max_size() noexcept { return int_size; }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type /* caps */) {
    if (buffer.size() < int_size) {
      // not enough data in buffers.
      return stdx::unexpected(make_error_code(codec_errc::not_enough_input));
    }

    typename value_type::value_type value{};

    std::copy_n(static_cast<const std::byte *>(buffer.data()), int_size,
                reinterpret_cast<std::byte *>(&value));

    if (std::endian::native == std::endian::big) {
      value = stdx::byteswap(value);
    }

    return std::make_pair(int_size, value_type(value));
  }

 private:
  const value_type v_;
};

/**
 * codec for variable length integers.
 *
 * note: encoded as little endian
 *
 *     0x00
 *     ...
 *     0xfa -> 0xfa
 *     0xfb [undefined]
 *     0xfc 0x.. 0x..
 *     0xfd 0x.. 0x.. 0x..
 *
 *     3.21:
 *     0xfe 0x.. 0x.. 0x.. 0x.. 0x00
 *     [1 + 5 bytes read, only 4 bytes used]
 *
 *     4.0:
 *     0xfe 0x.. 0x.. 0x.. 0x.. 0x.. 0x.. 0x.. 0x..
 *     [1 + 8 bytes read, only 4 bytes used]
 */
template <>
class Codec<borrowable::wire::VarInt>
    : public impl::EncodeBase<Codec<borrowable::wire::VarInt>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    if (v_.value() < 251) {
      return accu.step(borrowable::wire::FixedInt<1>(v_.value())).result();
    } else if (v_.value() < 1 << 16) {
      return accu.step(borrowable::wire::FixedInt<1>(varint_16))
          .step(borrowable::wire::FixedInt<2>(v_.value()))
          .result();
    } else if (v_.value() < (1 << 24)) {
      return accu.step(borrowable::wire::FixedInt<1>(varint_24))
          .step(borrowable::wire::FixedInt<3>(v_.value()))
          .result();
    } else {
      return accu.step(borrowable::wire::FixedInt<1>(varint_64))
          .step(borrowable::wire::FixedInt<8>(v_.value()))
          .result();
    }
  }

 public:
  static constexpr uint8_t varint_16{0xfc};
  static constexpr uint8_t varint_24{0xfd};
  static constexpr uint8_t varint_64{0xfe};
  using value_type = borrowable::wire::VarInt;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base{caps}, v_{v} {}

  static constexpr size_t max_size() noexcept { return 9; }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    // length
    auto first_byte_res = accu.template step<borrowable::wire::FixedInt<1>>();
    if (!first_byte_res) return stdx::unexpected(first_byte_res.error());

    auto first_byte = first_byte_res->value();

    if (first_byte < 251) {
      return std::make_pair(accu.result().value(), value_type(first_byte));
    } else if (first_byte == varint_16) {
      auto value_res = accu.template step<borrowable::wire::FixedInt<2>>();
      if (!value_res) return stdx::unexpected(value_res.error());
      return std::make_pair(accu.result().value(),
                            value_type(value_res->value()));
    } else if (first_byte == varint_24) {
      auto value_res = accu.template step<borrowable::wire::FixedInt<3>>();
      if (!value_res) return stdx::unexpected(value_res.error());
      return std::make_pair(accu.result().value(),
                            value_type(value_res->value()));
    } else if (first_byte == varint_64) {
      auto value_res = accu.template step<borrowable::wire::FixedInt<8>>();
      if (!value_res) return stdx::unexpected(value_res.error());
      return std::make_pair(accu.result().value(),
                            value_type(value_res->value()));
    }

    return stdx::unexpected(make_error_code(codec_errc::invalid_input));
  }

 private:
  const value_type v_;
};

/**
 * codec for a NULL value in the Resultset.
 */
template <>
class Codec<borrowable::wire::Null>
    : public Codec<borrowable::wire::FixedInt<1>> {
 public:
  using value_type = borrowable::wire::Null;

  static constexpr uint8_t nul_byte{0xfb};

  Codec(value_type, capabilities::value_type caps)
      : Codec<borrowable::wire::FixedInt<1>>(nul_byte, caps) {}

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type /* caps */) {
    if (buffer.size() < 1) {
      return stdx::unexpected(make_error_code(codec_errc::not_enough_input));
    }

    const uint8_t nul_val = *static_cast<const uint8_t *>(buffer.data());

    if (nul_val != nul_byte) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    return std::make_pair(1, value_type());
  }
};

/**
 * codec for ignorable bytes.
 *
 * limited by length or buffer.size()
 */
template <>
class Codec<void> {
 public:
  using value_type = size_t;

  Codec(value_type val, capabilities::value_type caps) : v_(val), caps_{caps} {}

  size_t size() const noexcept { return v_; }

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  stdx::expected<size_t, std::error_code> encode(
      const net::mutable_buffer &buffer) const {
    if (buffer.size() < size()) {
      return stdx::unexpected(make_error_code(std::errc::no_buffer_space));
    }

    auto *first = static_cast<std::uint8_t *>(buffer.data());
    auto *last = first + size();

    // fill with 0
    std::fill(first, last, 0);

    return size();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type /* caps */) {
    size_t buf_size = buffer.size();

    return std::make_pair(buf_size, buf_size);
  }

 private:
  const value_type v_;
  const capabilities::value_type caps_;
};

/**
 * codec for wire::String.
 *
 * limited by length or buffer.size()
 */
template <bool Borrowed>
class Codec<borrowable::wire::String<Borrowed>> {
 public:
  using value_type = borrowable::wire::String<Borrowed>;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : v_{std::move(v)}, caps_{caps} {}

  constexpr size_t size() const noexcept { return v_.value().size(); }

  static size_t max_size() noexcept {
    // we actually don't know what the size of the null-term string is ... until
    // the end of the buffer
    return std::numeric_limits<size_t>::max();
  }

  stdx::expected<size_t, std::error_code> encode(
      const net::mutable_buffer &buffer) const {
    if (buffer.size() < size()) {
      return stdx::unexpected(make_error_code(std::errc::no_buffer_space));
    }

    // in -> out
    std::copy_n(reinterpret_cast<const std::byte *>(v_.value().data()), size(),
                static_cast<std::byte *>(buffer.data()));

    return size();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type /* caps */) {
    const size_t buf_size = buffer_size(buffer);

    if (0 == buf_size) return std::make_pair(buf_size, value_type());

    return std::make_pair(
        buf_size,
        value_type({static_cast<const char *>(buffer.data()), buffer.size()}));
  }

 private:
  const value_type v_;
  const capabilities::value_type caps_;
};

/**
 * codec for string with known length.
 *
 * - varint of string length
 * - string of length
 */
template <bool Borrowed>
class Codec<borrowable::wire::VarString<Borrowed>>
    : public impl::EncodeBase<Codec<borrowable::wire::VarString<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(borrowable::wire::VarInt(v_.value().size()))
        .step(borrowable::wire::String<Borrowed>(v_.value()))
        .result();
  }

 public:
  using value_type = borrowable::wire::VarString<Borrowed>;
  using base_type = impl::EncodeBase<Codec<value_type>>;

  friend base_type;

  constexpr Codec(value_type val, capabilities::value_type caps)
      : base_type(caps), v_{std::move(val)} {}

  static size_t max_size() noexcept {
    // we actually don't know what the size of the null-term string is ...
    // until the end of the buffer
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);
    // decode the length
    auto var_string_len_res = accu.template step<borrowable::wire::VarInt>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    // decode string of length
    auto var_string_res =
        accu.template step<borrowable::wire::String<Borrowed>>(
            var_string_len_res->value());

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(var_string_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for 0-terminated string.
 */
template <bool Borrowed>
class Codec<borrowable::wire::NulTermString<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::wire::NulTermString<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.template step<borrowable::wire::String<Borrowed>>(v_)
        .template step<borrowable::wire::FixedInt<1>>(0)
        .result();
  }

 public:
  using value_type = borrowable::wire::NulTermString<Borrowed>;
  using base_type = impl::EncodeBase<Codec<value_type>>;

  friend base_type;

  constexpr Codec(value_type val, capabilities::value_type caps)
      : base_type(caps), v_{std::move(val)} {}

  static size_t max_size() noexcept {
    // we actually don't know what the size of the null-term string is ...
    // until the end of the buffer
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type /* caps */) {
    // length of the string before the \0

    const auto *first = static_cast<const uint8_t *>(buffer.data());
    const auto *last = first + buffer.size();

    const auto *pos = std::find(first, last, '\0');
    if (pos == last) {
      // no 0-term found
      return stdx::unexpected(make_error_code(codec_errc::missing_nul_term));
    }

    // \0 was found
    size_t len = std::distance(first, pos);
    if (len == 0) {
      return std::make_pair(len + 1, value_type());  // consume the \0 too
    }

    return std::make_pair(len + 1,
                          value_type({static_cast<const char *>(buffer.data()),
                                      len}));  // consume the \0 too
  }

 private:
  const value_type v_;
};

}  // namespace classic_protocol

#endif
