/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_FRAME_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_FRAME_H_

#include <cstddef>  // size_t
#include <cstdint>  // uint8_t
#include <memory>
#include <system_error>  // error_code
#include <utility>       // move

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_wire.h"

namespace classic_protocol {

/**
 * Codec of a Frame Header.
 */
template <>
class Codec<frame::Header> : public impl::EncodeBase<Codec<frame::Header>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<3>(v_.payload_size()))
        .step(wire::FixedInt<1>(v_.seq_id()))
        .result();
  }

 public:
  using value_type = frame::Header;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr size_t max_size() noexcept { return 4; }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto payload_size_res = accu.template step<wire::FixedInt<3>>();
    auto seq_id_res = accu.template step<wire::FixedInt<1>>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(payload_size_res->value(), seq_id_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * Codec of Compressed Header.
 */
template <>
class Codec<frame::CompressedHeader>
    : public impl::EncodeBase<Codec<frame::CompressedHeader>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<3>(v_.payload_size()))
        .step(wire::FixedInt<1>(v_.seq_id()))
        .step(wire::FixedInt<3>(v_.uncompressed_size()))
        .result();
  }

 public:
  using value_type = frame::CompressedHeader;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept { return 7; }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto payload_size_res = accu.template step<wire::FixedInt<3>>();
    auto seq_id_res = accu.template step<wire::FixedInt<1>>();
    auto uncompressed_size_res = accu.template step<wire::FixedInt<3>>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(payload_size_res->value(), seq_id_res->value(),
                   uncompressed_size_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * Codec for a Frame.
 *
 * Frame is
 *
 * - header
 * - payload
 */
template <class PayloadType>
class Codec<frame::Frame<PayloadType>>
    : public impl::EncodeBase<Codec<frame::Frame<PayloadType>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu
        .step(frame::Header(
            Codec<PayloadType>(v_.payload(), this->caps()).size(), v_.seq_id()))
        .step(PayloadType(v_.payload()))
        .result();
  }

 public:
  using value_type = frame::Frame<PayloadType>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      net::const_buffer buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto header_res = accu.template step<frame::Header>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    constexpr const size_t header_size{Codec<frame::Header>::max_size()};

    // check the payload is at least what we expect.
    if (buffer.size() < header_size + header_res->payload_size()) {
      return stdx::make_unexpected(
          make_error_code(classic_protocol::codec_errc::not_enough_input));
    }

    auto payload_res =
        accu.template step<PayloadType>(header_res->payload_size());

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(header_res->seq_id(), payload_res.value()));
  }

 private:
  const value_type v_;
};
}  // namespace classic_protocol

#endif
