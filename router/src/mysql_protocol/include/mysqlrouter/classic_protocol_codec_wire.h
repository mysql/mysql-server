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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_WIRE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_WIRE_H_

// codecs for classic_protocol::wire::

#include <algorithm>     // find
#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <system_error>  // error_code
#include <type_traits>
#include <utility>  // move

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/type_traits.h"  // endian
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_wire.h"
#include "mysqlrouter/partial_buffer_sequence.h"

namespace classic_protocol {
/**
 * codec of a FixedInt.
 *
 * classic proto uses 1, 2, 3, 4, 8 for IntSize
 */
template <int IntSize>
class Codec<wire::FixedInt<IntSize>> {
 public:
  static constexpr size_t int_size{IntSize};

  using value_type = wire::FixedInt<int_size>;

  constexpr Codec(value_type v, capabilities::value_type /* unused */)
      : v_{v} {}

  /**
   * size of the encoded object.
   */
  constexpr size_t size() const noexcept { return int_size; }

  /**
   * encode value_type into buffer.
   */
  stdx::expected<size_t, std::error_code> encode(
      const net::mutable_buffer &buffer) const {
    auto v = v_.value();

    if (stdx::endian::native == stdx::endian::big) {
      v = stdx::byteswap(v);
    }

    return buffer_copy(buffer, net::const_buffer(&v, int_size));
  }

  /**
   * maximum bytes which may scanned by the decoder.
   */
  static constexpr size_t max_size() noexcept { return int_size; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type /* caps */) {
    typename value_type::value_type v{};

    size_t copied = buffer_copy(net::buffer(&v, int_size), buffers);

    if (copied != int_size) {
      // not enough data in buffers.
      return stdx::make_unexpected(
          make_error_code(codec_errc::not_enough_input));
    }

    if (stdx::endian::native == stdx::endian::big) {
      v = stdx::byteswap(v);
    }

    return std::make_pair(copied, value_type(v));
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
class Codec<wire::VarInt> : public impl::EncodeBase<Codec<wire::VarInt>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    if (v_.value() < 251) {
      return accu.step(wire::FixedInt<1>(v_.value())).result();
    } else if (v_.value() < 1 << 16) {
      return accu.step(wire::FixedInt<1>(varint_16))
          .step(wire::FixedInt<2>(v_.value()))
          .result();
    } else if (v_.value() < (1 << 24)) {
      return accu.step(wire::FixedInt<1>(varint_24))
          .step(wire::FixedInt<3>(v_.value()))
          .result();
    } else {
      return accu.step(wire::FixedInt<1>(varint_64))
          .step(wire::FixedInt<8>(v_.value()))
          .result();
    }
  }

 public:
  static constexpr uint8_t varint_16{0xfc};
  static constexpr uint8_t varint_24{0xfd};
  static constexpr uint8_t varint_64{0xfe};
  using value_type = wire::VarInt;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base{caps}, v_{v} {}

  static constexpr size_t max_size() noexcept { return 9; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    // length
    auto first_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!first_byte_res) return stdx::make_unexpected(first_byte_res.error());

    auto first_byte = first_byte_res->value();

    if (first_byte < 251) {
      return std::make_pair(accu.result().value(), value_type(first_byte));
    } else if (first_byte == varint_16) {
      auto value_res = accu.template step<wire::FixedInt<2>>();
      if (!value_res) return stdx::make_unexpected(value_res.error());
      return std::make_pair(accu.result().value(),
                            value_type(value_res->value()));
    } else if (first_byte == varint_24) {
      auto value_res = accu.template step<wire::FixedInt<3>>();
      if (!value_res) return stdx::make_unexpected(value_res.error());
      return std::make_pair(accu.result().value(),
                            value_type(value_res->value()));
    } else if (first_byte == varint_64) {
      auto value_res = accu.template step<wire::FixedInt<8>>();
      if (!value_res) return stdx::make_unexpected(value_res.error());
      return std::make_pair(accu.result().value(),
                            value_type(value_res->value()));
    }

    return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
  }

 private:
  const value_type v_;
};

/**
 * codec for a NULL value in the Resultset.
 */
template <>
class Codec<wire::Null> : public Codec<wire::FixedInt<1>> {
 public:
  using value_type = wire::Null;

  static constexpr uint8_t nul_byte{0xfb};

  Codec(value_type, capabilities::value_type caps)
      : Codec<wire::FixedInt<1>>(nul_byte, caps) {}

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type /* caps */) {
    uint8_t v;

    size_t copied = buffer_copy(net::buffer(&v, 1), buffers);

    if (copied != 1) {
      return stdx::make_unexpected(
          make_error_code(codec_errc::not_enough_input));
    } else if (v != nul_byte) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    return std::make_pair(copied, value_type());
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

  Codec(value_type v, capabilities::value_type caps)
      : v_{std::move(v)}, caps_{caps} {}

  size_t size() const noexcept { return v_; }

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  stdx::expected<size_t, std::error_code> encode(
      const net::mutable_buffer &buffer) const {
    return buffer_copy(buffer, net::buffer(std::vector<char>(size())));
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type /* caps */) {
    size_t buf_size = buffer_size(buffers);

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
template <>
class Codec<wire::String> {
 public:
  using value_type = wire::String;

  Codec(value_type v, capabilities::value_type caps)
      : v_{std::move(v)}, caps_{caps} {}

  size_t size() const noexcept { return v_.value().size(); }

  static size_t max_size() noexcept {
    // we actually don't know what the size of the null-term string is ... until
    // the end of the buffer
    return std::numeric_limits<size_t>::max();
  }

  stdx::expected<size_t, std::error_code> encode(
      const net::mutable_buffer &buffer) const {
    return buffer_copy(buffer, net::const_buffer(v_.value().data(), size()));
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type /* caps */) {
    size_t buf_size = buffer_size(buffers);

    // MUST handle the empty case as &s.front() for .empty() std::string is
    // undefined and may trigger an assert()ion on glibc's implementation
    if (0 == buf_size) {
      return std::make_pair(buf_size, value_type(std::string()));
    }
    std::string s;
    s.resize(buf_size);

    size_t len =
        buffer_copy(net::mutable_buffer(&s.front(), s.size()), buffers);

    return std::make_pair(len, value_type(s));
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
template <>
class Codec<wire::VarString> : public impl::EncodeBase<Codec<wire::VarString>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::VarInt(v_.value().size()))
        .step(wire::String(v_.value()))
        .result();
  }

 public:
  using value_type = wire::VarString;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    // we actually don't know what the size of the null-term string is ...
    // until the end of the buffer
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);
    // decode the length
    auto var_string_len_res = accu.template step<wire::VarInt>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    // decode string of length
    auto var_string_res =
        accu.template step<wire::String>(var_string_len_res->value());

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(var_string_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for 0-terminated string.
 */
template <>
class Codec<wire::NulTermString>
    : public impl::EncodeBase<Codec<wire::NulTermString>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.template step<wire::String>(v_)
        .template step<wire::FixedInt<1>>(0)
        .result();
  }

 public:
  using value_type = wire::NulTermString;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    // we actually don't know what the size of the null-term string is ...
    // until the end of the buffer
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type /* caps */) {
    // length of the string before the \0
    size_t len{};

    // we don't know where the \0 will be be, scan all buffers for the first
    // one.
    const auto bufend = buffer_sequence_end(buffers);
    for (auto bufcur = buffer_sequence_begin(buffers); bufcur != bufend;
         ++bufcur) {
      const auto first = static_cast<const uint8_t *>(bufcur->data());
      const auto last = first + bufcur->size();

      const auto pos = std::find(first, last, '\0');
      if (pos != last) {
        // \0 was found
        len += std::distance(first, pos);

        // builds a string from the buffer-sequence's content
        std::string s;
        if (len > 0) {
          // ensure we don't trigger undefined behaviour by using &s.front() if
          // s.size() is 0
          s.resize(len);
          buffer_copy(net::mutable_buffer(&s.front(), s.size()), buffers, len);
        }

        return std::make_pair(len + 1, value_type(s));  // consume the \0 too
      } else {
        len += buffer_size(*bufcur);
      }
    }

    // no 0-term found
    return stdx::make_unexpected(make_error_code(codec_errc::missing_nul_term));
  }

 private:
  const value_type v_;
};
}  // namespace classic_protocol

#endif
