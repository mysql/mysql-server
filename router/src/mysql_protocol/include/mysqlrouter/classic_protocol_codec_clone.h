/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_CLONE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_CLONE_H_

#include "mysqlrouter/classic_protocol_clone.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"

namespace classic_protocol {
namespace clone::client {
enum class CommandByte {
  Init = 0x01,
  Attach,
  Reinit,
  Execute,
  Ack,
  Exit,
};
}

/**
 * codec for clone::client::Init message.
 *
 * - Fixed<1> cmd_byte
 * - Fixed<4> protocol version
 * - Fixed<4> ddl_timeout
 * - 0-or-more
 *   - 1 SE type
 *   - Fixed<4> locator_len
 *   - String<locator_len> locator
 */
template <>
class Codec<clone::client::Init>
    : public impl::EncodeBase<Codec<clone::client::Init>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.protocol_version))
        .step(wire::FixedInt<4>(v_.ddl_timeout))
        .result();
  }

 public:
  using value_type = clone::client::Init;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::client::CommandByte::Init);
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto protocol_version_res = accu.template step<wire::FixedInt<4>>();
    auto ddl_timeout_res = accu.template step<wire::FixedInt<4>>();

    // TODO(jkneschk): if there is more data, 1-or-more Locators

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());
    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

// Reinit decodes the same way as Init
// Ack decodes the same way as Init

template <>
class Codec<clone::client::Execute>
    : public impl::EncodeBase<Codec<clone::client::Execute>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::client::Execute;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::client::CommandByte::Execute);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

template <>
class Codec<clone::client::Attach>
    : public impl::EncodeBase<Codec<clone::client::Attach>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::client::Attach;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::client::CommandByte::Attach);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

template <>
class Codec<clone::client::Reinit>
    : public impl::EncodeBase<Codec<clone::client::Reinit>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::client::Reinit;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::client::CommandByte::Reinit);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

template <>
class Codec<clone::client::Ack>
    : public impl::EncodeBase<Codec<clone::client::Ack>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::client::Ack;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::client::CommandByte::Ack);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

template <>
class Codec<clone::client::Exit>
    : public impl::EncodeBase<Codec<clone::client::Exit>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::client::Exit;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::client::CommandByte::Exit);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

namespace clone::server {
enum class CommandByte {
  Locators = 0x01,
  DataDescriptor,
  Data,
  Plugin,
  Config,
  Collation,
  PluginV2,  // version: 0x0101
  ConfigV3,  // version: 0x0102
  Complete = 99,
  Error = 100,
};
}

// clone::string:
//   Fixed<4> len
//   String<len> payload
//
// Plugin:
// - clone::string
// PluginV2:
// - key   clone::string
// - value clone::string
// Collation:
// - clone::string
// Config
// - key   clone::string
// - value clone::string

template <>
class Codec<clone::server::Complete>
    : public impl::EncodeBase<Codec<clone::server::Complete>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::server::Complete;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::server::CommandByte::Complete);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

template <>
class Codec<clone::server::Error>
    : public impl::EncodeBase<Codec<clone::server::Error>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte())).result();
  }

 public:
  using value_type = clone::server::Error;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(clone::server::CommandByte::Error);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type());
  }

 private:
  const value_type v_;
};

}  // namespace classic_protocol
#endif
