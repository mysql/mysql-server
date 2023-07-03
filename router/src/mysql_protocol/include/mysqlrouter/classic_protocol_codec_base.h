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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_BASE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_BASE_H_

#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <system_error>  // error_code
#include <type_traits>
#include <utility>  // move

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/stdx/bit.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/partial_buffer_sequence.h"

namespace classic_protocol {

// bytes needed to encode x bits
//
// bits | bytes
// -----+------
//    0 |     0
//    1 |     1
//  ... |   ...
//    8 |     1
//    9 |     2
//  ... |   ...
//
constexpr size_t bytes_per_bits(size_t bits) { return (bits + 7) / 8; }

static_assert(bytes_per_bits(0) == 0);
static_assert(bytes_per_bits(1) == 1);
static_assert(bytes_per_bits(8) == 1);
static_assert(bytes_per_bits(9) == 2);

/**
 * Codec for a type.
 *
 * requirements for T:
 * - size_t size()
 * - stdx::expected<size_t, std::error_code> encode(net::mutable_buffer);
 * - static size_t max_size();
 * - static stdx::expected<T, std::error_code> decode(buffer_sequence,
 *   capabilities);
 */
template <class T>
class Codec;

/**
 * encode a message into a dynamic buffer.
 *
 * @param v message to encode
 * @param caps protocol capabilities
 * @param dyn_buffer dynamic buffer to write into
 * @returns number of bytes written into dynamic buffer or std::error_code on
 * error
 */
template <class T, class DynamicBuffer>
stdx::expected<size_t, std::error_code> encode(const T &v,
                                               capabilities::value_type caps,
                                               DynamicBuffer &&dyn_buffer) {
  //  static_assert(net::is_dynamic_buffer<DynamicBuffer>::value,
  //                "dyn_buffer MUST be a DynamicBuffer");

  Codec<T> codec(v, caps);

  const auto orig_size = dyn_buffer.size();
  const auto codec_size = codec.size();

  // reserve some space to write into
  dyn_buffer.grow(codec_size);

  const auto res = codec.encode(dyn_buffer.data(orig_size, codec_size));
  if (!res) {
    dyn_buffer.shrink(codec_size);
    return res;
  }

  dyn_buffer.shrink(codec_size - res.value());

  return res;
}

/**
 * decode a message from a buffer sequence.
 *
 * @param buffers buffer sequence to read from
 * @param caps protocol capabilities
 * @returns number of bytes read from 'buffers' and a T on success, or
 * std::error_code on error
 */
template <class T, class ConstBufferSequence>
stdx::expected<std::pair<size_t, T>, std::error_code> decode(
    const ConstBufferSequence &buffers, capabilities::value_type caps) {
  static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                "buffers MUST be a const buffer sequence");

  return Codec<T>::decode(buffers, caps);
}

namespace impl {

/**
 * Generator of decoded Types of a buffer-sequence.
 *
 * - .step<wire::VarInt>()
 */
template <class ConstBufferSequence>
class DecodeBufferAccumulator {
 public:
  using buffer_type = net::const_buffer;
  using result_type = stdx::expected<size_t, std::error_code>;

  /**
   * construct a DecodeBufferAccumulator.
   *
   * @param buffers a ConstBufferSequence
   * @param caps classic-protocol capabilities
   * @param consumed bytes to skip from the buffers
   */
  DecodeBufferAccumulator(const ConstBufferSequence &buffers,
                          capabilities::value_type caps, size_t consumed = 0)
      : buffers_{buffers}, caps_{caps} {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "buffers MUST be a const buffer sequence");
    buffers_.consume(consumed);
  }

  /**
   * decode a Type from the buffer sequence.
   *
   * if it succeeds, moves position in buffer-sequence forward and returns
   * decoded Type, otherwise returns error and updates the global error-code in
   * result()
   */
  template <class T>
  stdx::expected<typename Codec<T>::value_type, std::error_code> step(
      size_t max_size = classic_protocol::Codec<T>::max_size()) {
    return step_<T, false>(max_size);
  }

  /**
   * try decoding a Type from the buffer sequence.
   *
   * if it succeeds, moves position in buffer-sequence forward and returns
   * decoded Type, otherwise returns error and does NOT update the global
   * error-code in result()
   */
  template <class T>
  stdx::expected<typename Codec<T>::value_type, std::error_code> try_step(
      size_t max_size = classic_protocol::Codec<T>::max_size()) {
    return step_<T, true>(max_size);
  }

  /**
   * get result of the step().
   *
   * if a step() failed, result is the error-code of the first failed step()
   *
   * @returns consumed bytes by all steps(), or error of first failed step()
   *
   */
  result_type result() const {
    if (!res_) {
      return res_;
    } else {
      return buffers_.total_consumed();
    }
  }

 private:
  /**
   * execute a step.
   *
   * Note: 'Try' is used a compile time selector for step() and try_step() only.
   */
  template <class T, bool Try>
  stdx::expected<typename Codec<T>::value_type, std::error_code> step_(
      size_t max_size = std::numeric_limits<size_t>::max()) {
    if (!res_) return stdx::make_unexpected(res_.error());

    stdx::expected<std::pair<size_t, typename Codec<T>::value_type>,
                   std::error_code>
        decode_res = Codec<T>::decode(buffers_.prepare(max_size), caps_);
    if (decode_res) {
      buffers_.consume(decode_res->first);

      return decode_res->second;
    } else {
      if (!Try) {
        // capture the first failure
        res_ = stdx::make_unexpected(decode_res.error());
      }
      return stdx::make_unexpected(decode_res.error());
    }
  }

  PartialBufferSequence<ConstBufferSequence> buffers_;
  const capabilities::value_type caps_;

  result_type res_;
};

/**
 * accumulator of encoded buffers.
 *
 * writes the .step()ed encoded types into buffer.
 *
 * EncodeBufferAccumulator(buffer, caps)
 *   .step(wire::VarInt(42))
 *   .step(wire::VarInt(512))
 *   .result()
 *
 * The class should be used together with EncodeSizeAccumulator which shares
 * the same interface.
 */
class EncodeBufferAccumulator {
 public:
  using buffer_type = net::mutable_buffer;
  using result_type = stdx::expected<size_t, std::error_code>;

  /**
   * construct a encode-buffer-accumulator.
   *
   * @param buffer mutable-buffer to encode into
   * @param caps protocol capabilities
   * @param consumed bytes already used in the in buffer
   */
  EncodeBufferAccumulator(buffer_type buffer, capabilities::value_type caps,
                          size_t consumed = 0)
      : buffer_{std::move(buffer)}, caps_{caps}, consumed_{consumed} {}

  /**
   * encode a T into the buffer and move position forward.
   *
   * no-op of a previous step failed.
   */
  template <class T>
  EncodeBufferAccumulator &step(const T &v) {
    if (!res_) return *this;

    res_ = Codec<T>(v, caps_).encode(buffer_ + consumed_);
    if (res_) {
      consumed_ += res_.value();
    }

    return *this;
  }

  /**
   * get result the steps().
   *
   * @returns last used position in buffer, or first error in case of a step()
   * failed.
   */
  result_type result() const {
    if (!res_) {
      return res_;
    } else {
      return {consumed_};
    }
  }

 private:
  const buffer_type buffer_;
  const capabilities::value_type caps_;
  size_t consumed_{};

  result_type res_;
};

/**
 * accumulates the sizes of encoded T's.
 *
 * e.g. the size of tw
 *
 * EncodeSizeAccumulator(caps)
 *   .step(wire::VarInt(42))    // 1
 *   .step(wire::VarInt(512))   // 2
 *   .result()                  // = 3
 *
 * The class should be used together with EncodeBufferAccumulator which shares
 * the same interface.
 */
class EncodeSizeAccumulator {
 public:
  using result_type = size_t;

  /**
   * construct a EncodeSizeAccumulator.
   */
  constexpr explicit EncodeSizeAccumulator(capabilities::value_type caps)
      : caps_{caps} {}

  /**
   * accumulate the size() of encoded T.
   *
   * calls Codec<T>(v, caps).size()
   */
  template <class T>
  constexpr EncodeSizeAccumulator &step(const T &v) noexcept {
    consumed_ += Codec<T>(v, caps_).size();

    return *this;
  }

  /**
   * @returns size of all steps().
   */
  constexpr result_type result() const { return consumed_; }

 private:
  size_t consumed_{};
  const capabilities::value_type caps_;
};

/**
 * CRTP base for the Codec's encode part.
 *
 * derived classes must provide a 'accumulate_fields()' which
 * maps each field by the Mapper and returns the result
 *
 * used by .size() and .encode() as both have to process the same
 * fields in the same order, just with different mappers
 */
template <class T>
class EncodeBase {
 public:
  constexpr explicit EncodeBase(capabilities::value_type caps) : caps_{caps} {}

  constexpr size_t size() const noexcept {
    return static_cast<const T *>(this)->accumulate_fields(
        EncodeSizeAccumulator(caps_));
  }

  stdx::expected<size_t, std::error_code> encode(
      const net::mutable_buffer &buffer) const {
    return static_cast<const T *>(this)->accumulate_fields(
        EncodeBufferAccumulator(buffer, caps_));
  }

  constexpr capabilities::value_type caps() const noexcept { return caps_; }

 private:
  const capabilities::value_type caps_;
};

}  // namespace impl
}  // namespace classic_protocol

#endif
