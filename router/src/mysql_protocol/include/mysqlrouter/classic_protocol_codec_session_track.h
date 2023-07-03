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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_SESSION_TRACK_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_SESSION_TRACK_H_

// codecs for classic_protocol::session_track:: messages

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlrouter/classic_protocol_session_track.h"
#include "mysqlrouter/classic_protocol_wire.h"

namespace classic_protocol {

/**
 * codec for session_track::TransactionState.
 *
 * part of session_track::Field
 */
template <>
class Codec<session_track::TransactionState>
    : public impl::EncodeBase<Codec<session_track::TransactionState>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu
        .step(wire::FixedInt<1>(0x08))  // length
        .step(wire::FixedInt<1>(v_.trx_type()))
        .step(wire::FixedInt<1>(v_.read_unsafe()))
        .step(wire::FixedInt<1>(v_.read_trx()))
        .step(wire::FixedInt<1>(v_.write_unsafe()))
        .step(wire::FixedInt<1>(v_.write_trx()))
        .step(wire::FixedInt<1>(v_.stmt_unsafe()))
        .step(wire::FixedInt<1>(v_.resultset()))
        .step(wire::FixedInt<1>(v_.locked_tables()))
        .result();
  }

 public:
  using value_type = session_track::TransactionState;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t type_byte() { return 0x05; }

  /**
   * decode a session_track::TransactionState from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, message::server::Ok> on success, with bytes
   * processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "buffers MUST be a const buffer sequence");
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    const auto payload_length_res = accu.template step<wire::VarInt>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (payload_length_res->value() != 0x08) {
      // length of the payload that follows.
      return stdx::make_unexpected(make_error_code(std::errc::bad_message));
    }

    const auto trx_type_res = accu.template step<wire::FixedInt<1>>();
    const auto read_unsafe_res = accu.template step<wire::FixedInt<1>>();
    const auto read_trx_res = accu.template step<wire::FixedInt<1>>();
    const auto write_unsafe_res = accu.template step<wire::FixedInt<1>>();
    const auto write_trx_res = accu.template step<wire::FixedInt<1>>();
    const auto stmt_unsafe_res = accu.template step<wire::FixedInt<1>>();
    const auto resultset_res = accu.template step<wire::FixedInt<1>>();
    const auto locked_tables_res = accu.template step<wire::FixedInt<1>>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(trx_type_res->value(), read_unsafe_res->value(),
                   read_trx_res->value(), write_unsafe_res->value(),
                   write_trx_res->value(), stmt_unsafe_res->value(),
                   resultset_res->value(), locked_tables_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for session_track::TransactionCharacteristics.
 *
 * part of session_track::Field
 */
template <>
class Codec<session_track::TransactionCharacteristics>
    : public impl::EncodeBase<
          Codec<session_track::TransactionCharacteristics>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::VarString(v_.characteristics())).result();
  }

 public:
  using value_type = session_track::TransactionCharacteristics;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t type_byte() { return 0x04; }

  /**
   * decode a session_track::TransactionCharacteristics from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, session_track::TransactionCharacteristics> on
   * success, with bytes processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "buffers MUST be a const buffer sequence");
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    const auto characteristics_res = accu.template step<wire::VarString>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(characteristics_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for session_track::State.
 *
 * part of session_track::Field
 */
template <>
class Codec<session_track::State>
    : public impl::EncodeBase<Codec<session_track::State>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(v_.state())).result();
  }

 public:
  using value_type = session_track::State;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t type_byte() { return 0x02; }

  /**
   * decode a session_track::State from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, session_track::State> on success, with bytes
   * processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto state_res = accu.template step<wire::FixedInt<1>>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(state_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for session_track::Schema.
 *
 * part of session_track::Field
 */
template <>
class Codec<session_track::Schema>
    : public impl::EncodeBase<Codec<session_track::Schema>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::VarString(v_.schema())).result();
  }

 public:
  using value_type = session_track::Schema;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t type_byte() { return 0x01; }

  /**
   * decode a session_track::Schema from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, session_track::Schema> on success, with bytes
   * processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "buffers MUST be a const buffer sequence");
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto schema_res = accu.template step<wire::VarString>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(schema_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for session_track::SystemVariable.
 *
 * part of session_track::Field
 */
template <>
class Codec<session_track::SystemVariable>
    : public impl::EncodeBase<Codec<session_track::SystemVariable>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::VarString(v_.key()))
        .step(wire::VarString(v_.value()))
        .result();
  }

 public:
  using value_type = session_track::SystemVariable;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t type_byte() { return 0x00; }

  /**
   * decode a session_track::SystemVariable from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, session_track::SystemVariable> on success, with
   * bytes processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "buffers MUST be a const buffer sequence");
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto key_res = accu.template step<wire::VarString>();
    auto value_res = accu.template step<wire::VarString>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(key_res->value(), value_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for session_track::Gtid.
 *
 * format:
 *
 * - FixedInt<int> spec (only 0 is in use for now)
 * - VarString     payload (payload according to spec).
 *
 * payload for spec 0:
 * - GTID in human-readable form like 4dd0f9d5-3b00-11eb-ad70-003093140e4e:23929
 *
 * part of session_track::Field
 */
template <>
class Codec<session_track::Gtid>
    : public impl::EncodeBase<Codec<session_track::Gtid>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(v_.spec()))
        .step(wire::VarString(v_.gtid()))
        .result();
  }

 public:
  using value_type = session_track::Gtid;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t type_byte() { return 0x03; }

  /**
   * decode a session_track::Gtid from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, session_track::Gtid> on success, with bytes
   * processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto spec_res = accu.template step<wire::FixedInt<1>>();
    auto gtid_res = accu.template step<wire::VarString>();

    if (!accu.result()) return accu.result().get_unexpected();

    return std::make_pair(accu.result().value(),
                          value_type(spec_res->value(), gtid_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for session-track's Field.
 *
 * sent as part of a server::Ok and server::Eof message.
 *
 * - FixedInt<1> type
 * - VarString data
 *
 * data is encoded according type:
 *
 * - 0x00 session_track::SystemVariable
 * - 0x01 session_track::Schema
 * - 0x02 session_track::StateChanged
 * - 0x03 session_track::Gtid
 * - 0x04 session_track::TransactionCharacteristics
 * - 0x05 session_track::TransactionState
 */
template <>
class Codec<session_track::Field>
    : public impl::EncodeBase<Codec<session_track::Field>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(v_.type()))
        .step(wire::VarString(v_.data()))
        .result();
  }

 public:
  using value_type = session_track::Field;

  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  /**
   * decode a session_track::Field from a buffer-sequence.
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, session_track::Field> on success, with bytes
   * processed
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value);
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto type_res = accu.template step<wire::FixedInt<1>>();
    auto data_res = accu.template step<wire::VarString>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(type_res->value(), data_res->value()));
  }

 private:
  const value_type v_;
};

}  // namespace classic_protocol

#endif
