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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_MESSAGE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_MESSAGE_H_

#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <system_error>  // error_code
#include <utility>       // move

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/classic_protocol_wire.h"
#include "mysqlrouter/partial_buffer_sequence.h"

namespace classic_protocol {

/**
 * codec for server Greeting message.
 *
 * 3.21 (protocol_version 9)
 *
 *     FixedInt<1>     protocol_version [0x09]
 *     NulTermString   server_version
 *     FixedInt<4>     connection_id
 *     NulTermString   auth-method-data
 *
 * 3.21 and later (protocol_version 10)
 *
 *     FixedInt<1>     protocol_version [0x0a]
 *     NulTermString   server_version
 *     FixedInt<4>     connection_id
 *     NulTermString   auth-method-data
 *     FixedInt<2>     capabilities (lower 16bit)
 *
 * 3.23 and later add:
 *
 *     FixedInt<1>     collation
 *     FixedInt<2>     status flags
 *     FixedInt<2>     capabilities (upper 16bit)
 *     FixedInt<1>     length of auth-method-data or 0x00
 *     String<10>      reserved
 *
 * if capabilities.secure_connection is set, adds
 *
 *     String<len>     auth-method-data-2
 *
 * if capabilities.plugin_auth is set, adds
 *
 *     NulTermString   auth-method
 */
template <>
class Codec<message::server::Greeting>
    : public impl::EncodeBase<Codec<message::server::Greeting>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    if (v_.protocol_version() == 0x09) {
      return accu.step(wire::FixedInt<1>(v_.protocol_version()))
          .step(wire::NulTermString(v_.version()))
          .step(wire::FixedInt<4>(v_.connection_id()))
          .step(wire::NulTermString(v_.auth_method_data().substr(0, 8)))
          .result();
    } else {
      uint8_t auth_method_data_size{0};
      if (v_.capabilities()[classic_protocol::capabilities::pos::plugin_auth]) {
        auth_method_data_size = v_.auth_method_data().size();
      }

      accu.step(wire::FixedInt<1>(v_.protocol_version()))
          .step(wire::NulTermString(v_.version()))
          .step(wire::FixedInt<4>(v_.connection_id()))
          .step(wire::NulTermString(v_.auth_method_data().substr(0, 8)))
          .step(wire::FixedInt<2>(v_.capabilities().to_ulong() & 0xffff));

      if ((v_.capabilities().to_ullong() >= (1 << 16)) ||
          v_.status_flags().any() || (v_.collation() != 0)) {
        accu.step(wire::FixedInt<1>(v_.collation()))
            .step(wire::FixedInt<2>(v_.status_flags().to_ulong()))
            .step(wire::FixedInt<2>((v_.capabilities().to_ulong() >> 16) &
                                    0xffff))
            .step(wire::FixedInt<1>(auth_method_data_size))
            .step(wire::String(std::string(10, '\0')));
        if (v_.capabilities()
                [classic_protocol::capabilities::pos::secure_connection]) {
          accu.step(wire::String(v_.auth_method_data().substr(8)));
          if (v_.capabilities()
                  [classic_protocol::capabilities::pos::plugin_auth]) {
            accu.step(wire::NulTermString(v_.auth_method_name()));
          }
        }
      }
      return accu.result();
    }
  }

 public:
  using value_type = message::server::Greeting;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    // proto-version
    auto protocol_version_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (protocol_version_res->value() != 0x09 &&
        protocol_version_res->value() != 0x0a) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto version_res = accu.template step<wire::NulTermString>();
    auto connection_id_res = accu.template step<wire::FixedInt<4>>();
    auto auth_method_data_res = accu.template step<wire::NulTermString>();

    if (protocol_version_res->value() == 0x09) {
      return std::make_pair(
          accu.result().value(),
          value_type(protocol_version_res->value(), version_res->value(),
                     connection_id_res->value(), auth_method_data_res->value(),
                     0, 0, 0, {}));
    } else {
      // capabilities are split into two a lower-2-byte part and a
      // higher-2-byte
      auto cap_lower_res = accu.template step<wire::FixedInt<2>>();
      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      // 3.21.x doesn't send more.
      if (buffer_size(buffers) <= accu.result().value()) {
        return std::make_pair(
            accu.result().value(),
            value_type(protocol_version_res->value(), version_res->value(),
                       connection_id_res->value(),
                       auth_method_data_res->value(), cap_lower_res->value(),
                       0x0, 0x0, {}));
      }

      // if there's more data
      auto collation_res = accu.template step<wire::FixedInt<1>>();
      auto status_flags_res = accu.template step<wire::FixedInt<2>>();
      auto cap_hi_res = accu.template step<wire::FixedInt<2>>();

      // before we use cap_hi|cap_low check they don't have an error
      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      classic_protocol::capabilities::value_type capabilities(
          cap_lower_res->value() | (cap_hi_res->value() << 16));

      size_t auth_method_data_len{13};
      if (capabilities[classic_protocol::capabilities::pos::plugin_auth]) {
        auto auth_method_data_len_res = accu.template step<wire::FixedInt<1>>();
        if (!accu.result()) return stdx::make_unexpected(accu.result().error());

        // should be 21, but least 8
        if (auth_method_data_len_res->value() < 8) {
          return stdx::make_unexpected(
              make_error_code(codec_errc::invalid_input));
        }
        auth_method_data_len = auth_method_data_len_res->value() - 8;
      } else {
        accu.template step<void>(1);  // should be 0 ...
      }

      accu.template step<void>(10);  // skip the filler

      stdx::expected<wire::String, std::error_code> auth_method_data_2_res;
      stdx::expected<wire::NulTermString, std::error_code> auth_method_res;
      if (capabilities
              [classic_protocol::capabilities::pos::secure_connection]) {
        // auth-method-data
        auth_method_data_2_res =
            accu.template step<wire::String>(auth_method_data_len);

        if (capabilities[classic_protocol::capabilities::pos::plugin_auth]) {
          // auth_method
          auth_method_res = accu.template step<wire::NulTermString>();
        }
      }

      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(
              protocol_version_res->value(), version_res->value(),
              connection_id_res->value(),
              auth_method_data_res->value() + auth_method_data_2_res->value(),
              capabilities, collation_res->value(), status_flags_res->value(),
              auth_method_res->value()));
    }
  }

 private:
  const value_type v_;
};

/**
 * codec for server::AuthMethodSwitch message.
 */
template <>
class Codec<message::server::AuthMethodSwitch>
    : public impl::EncodeBase<Codec<message::server::AuthMethodSwitch>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()));
    if (caps()[classic_protocol::capabilities::pos::plugin_auth]) {
      accu.step(wire::NulTermString(v_.auth_method()))
          .step(wire::String(v_.auth_method_data()));
    }

    return accu.result();
  }

 public:
  using value_type = message::server::AuthMethodSwitch;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0xfe; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    // proto-version
    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    if (!caps[classic_protocol::capabilities::pos::plugin_auth]) {
      return std::make_pair(accu.result().value(), value_type());
    }

    auto auth_method_res = accu.template step<wire::NulTermString>();
    auto auth_method_data_res = accu.template step<wire::String>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(auth_method_res->value(), auth_method_data_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for server::AuthMethodData message.
 */
template <>
class Codec<message::server::AuthMethodData>
    : public impl::EncodeBase<Codec<message::server::AuthMethodData>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::String(v_.auth_method_data()))
        .result();
  }

 public:
  using value_type = message::server::AuthMethodData;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0x01; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto auth_method_data_res = accu.template step<wire::String>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(auth_method_data_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for server-side Ok message.
 */
template <>
class Codec<message::server::Ok>
    : public impl::EncodeBase<Codec<message::server::Ok>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::VarInt(v_.affected_rows()))
        .step(wire::VarInt(v_.last_insert_id()));

    if (caps()[capabilities::pos::protocol_41] ||
        caps()[capabilities::pos::transactions]) {
      accu.step(wire::FixedInt<2>(v_.status_flags().to_ulong()));
      if (caps()[capabilities::pos::protocol_41]) {
        accu.step(wire::FixedInt<2>(v_.warning_count()));
      }
    }

    if (caps()[capabilities::pos::session_track]) {
      accu.step(wire::VarString(v_.message()));
      if (v_.status_flags()[status::pos::session_state_changed]) {
        accu.step(wire::VarString(v_.session_changes()));
      }
    } else {
      accu.step(wire::String(v_.message()));
    }

    return accu.result();
  }

 public:
  using value_type = message::server::Ok;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0x00; }

  /**
   * decode a server::Ok message from a buffer-sequence.
   *
   * precondition:
   * - input starts with cmd_byte()
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, message::server::Ok> on success, with bytes
   * processed
   * @retval codec_errc::invalid_input if preconditions aren't met
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto affected_rows_res = accu.template step<wire::VarInt>();
    auto last_insert_id_res = accu.template step<wire::VarInt>();

    stdx::expected<wire::FixedInt<2>, std::error_code> status_flags_res(0);
    stdx::expected<wire::FixedInt<2>, std::error_code> warning_count_res(0);
    if (caps[capabilities::pos::protocol_41] ||
        caps[capabilities::pos::transactions]) {
      status_flags_res = accu.template step<wire::FixedInt<2>>();
      if (caps[capabilities::pos::protocol_41]) {
        warning_count_res = accu.template step<wire::FixedInt<2>>();
      }
    }

    stdx::expected<wire::String, std::error_code> message_res;
    stdx::expected<wire::VarString, std::error_code> session_changes_res;
    if (caps[capabilities::pos::session_track]) {
      // if there is more data.
      const auto var_message_res = accu.template try_step<wire::VarString>();
      if (var_message_res) {
        // set the message from the var-string
        message_res = var_message_res.value();
      }

      if (status_flags_res->value() &
          status::session_state_changed.to_ulong()) {
        session_changes_res = accu.template step<wire::VarString>();
      }
    } else {
      message_res = accu.template step<wire::String>();
    }

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(affected_rows_res->value(), last_insert_id_res->value(),
                   status_flags_res->value(), warning_count_res->value(),
                   message_res->value(), session_changes_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for server-side Eof message.
 *
 * Eof message is encoded differently dependending on protocol capabiltiies,
 * but always starts with:
 *
 * - 0xef
 *
 * If capabilities has text_result_with_session_tracking, it is followed by
 * - [rest of Ok packet]
 *
 * otherwise, if capabilities has protocol_41
 * - FixedInt<2> warning-count
 * - FixedInt<2> status flags
 *
 * otherwise
 * - nothing
 */
template <>
class Codec<message::server::Eof>
    : public impl::EncodeBase<Codec<message::server::Eof>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()));

    if (caps()[capabilities::pos::text_result_with_session_tracking]) {
      accu.step(wire::VarInt(v_.affected_rows()))
          .step(wire::VarInt(v_.last_insert_id()));

      if (caps()[capabilities::pos::protocol_41] ||
          caps()[capabilities::pos::transactions]) {
        accu.step(wire::FixedInt<2>(v_.status_flags().to_ulong()));
        if (caps()[capabilities::pos::protocol_41]) {
          accu.step(wire::FixedInt<2>(v_.warning_count()));
        }
      }

      if (caps()[capabilities::pos::session_track]) {
        if (!v_.message().empty() ||
            v_.status_flags()[status::pos::session_state_changed]) {
          // only write message and session-changes if both of them aren't
          // empty.
          accu.step(wire::VarString(v_.message()));
          if (v_.status_flags()[status::pos::session_state_changed]) {
            accu.step(wire::VarString(v_.session_changes()));
          }
        }
      } else {
        accu.step(wire::String(v_.message()));
      }
    } else if (caps()[capabilities::pos::protocol_41]) {
      accu.step(wire::FixedInt<2>(v_.warning_count()))
          .step(wire::FixedInt<2>(v_.status_flags().to_ulong()));
    }

    return accu.result();
  }

 public:
  using value_type = message::server::Eof;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0xfe; }

  /**
   * decode a server::Eof message from a buffer-sequence.
   *
   * capabilities checked:
   * - protocol_41
   * - text_resultset_with_session_tracking
   *
   * precondition:
   * - input starts with cmd_byte()
   *
   * @param buffers input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, message::server::Eof> on success, with bytes
   * processed
   * @retval codec_errc::invalid_input if preconditions aren't met
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    const auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    if (caps[capabilities::pos::text_result_with_session_tracking]) {
      const auto affected_rows_res = accu.template step<wire::VarInt>();
      const auto last_insert_id_res = accu.template step<wire::VarInt>();

      stdx::expected<wire::FixedInt<2>, std::error_code> status_flags_res(0);
      stdx::expected<wire::FixedInt<2>, std::error_code> warning_count_res(0);
      if (caps[capabilities::pos::protocol_41] ||
          caps[capabilities::pos::transactions]) {
        status_flags_res = accu.template step<wire::FixedInt<2>>();
        if (caps[capabilities::pos::protocol_41]) {
          warning_count_res = accu.template step<wire::FixedInt<2>>();
        }
      }

      stdx::expected<wire::String, std::error_code> message_res;
      stdx::expected<wire::VarString, std::error_code> session_state_info_res;
      if (caps[capabilities::pos::session_track]) {
        // when session-track is supported, the 'message' part is a VarString.
        // But only if there is actually session-data and the message has data.
        const auto var_message_res = accu.template try_step<wire::VarString>();
        if (var_message_res) {
          // set the message from the var-string
          message_res = var_message_res.value();
        }

        if (status_flags_res->value() &
            status::session_state_changed.to_ulong()) {
          session_state_info_res = accu.template step<wire::VarString>();
        }
      } else {
        message_res = accu.template step<wire::String>();
      }

      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(affected_rows_res->value(), last_insert_id_res->value(),
                     status_flags_res->value(), warning_count_res->value(),
                     message_res->value(), session_state_info_res->value()));
    } else if (caps[capabilities::pos::protocol_41]) {
      const auto warning_count_res = accu.template step<wire::FixedInt<2>>();
      const auto status_flags_res = accu.template step<wire::FixedInt<2>>();

      return std::make_pair(
          accu.result().value(),
          value_type(status_flags_res->value(), warning_count_res->value()));
    } else {
      return std::make_pair(accu.result().value(), value_type());
    }
  }

 private:
  const value_type v_;
};

/**
 * codec for Error message.
 *
 * note: Format overview:
 *
 * 3.21: protocol_version <= 9 [not supported]
 *
 *     FixedInt<1> 0xff
 *     String      message
 *
 * 3.21: protocol_version > 9
 *
 *     FixedInt<1> 0xff
 *     FixedInt<2> error_code
 *     String      message
 *
 * 4.1 and later:
 *
 *     FixedInt<1> 0xff
 *     FixedInt<2> error_code
 *     '#'
 *     String<5>   sql_state
 *     String      message
 */
template <>
class Codec<message::server::Error>
    : public impl::EncodeBase<Codec<message::server::Error>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<2>(v_.error_code()));
    if (caps()[capabilities::pos::protocol_41]) {
      accu.step(wire::FixedInt<1>('#')).step(wire::String(v_.sql_state()));
    }

    return accu.step(wire::String(v_.message())).result();
  }

 public:
  using value_type = message::server::Error;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() { return 0xff; }

  static constexpr size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    // decode all fields, check result later before they are used.
    auto error_code_res = accu.template step<wire::FixedInt<2>>();
    stdx::expected<wire::String, std::error_code> sql_state_res;
    if (caps[capabilities::pos::protocol_41]) {
      auto sql_state_hash_res = accu.template step<wire::FixedInt<1>>();
      sql_state_res = accu.template step<wire::String>(5);
    }
    auto message_res = accu.template step<wire::String>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(error_code_res->value(), message_res->value(),
                   sql_state_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for ColumnCount message.
 */
template <>
class Codec<message::server::ColumnCount>
    : public impl::EncodeBase<Codec<message::server::ColumnCount>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::VarInt(v_.count())).result();
  }

 public:
  using value_type = message::server::ColumnCount;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto count_res = accu.template step<wire::VarInt>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(count_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * Codec of ColumnMeta.
 *
 * capabilities checked:
 * - protocol_41
 */
template <>
class Codec<message::server::ColumnMeta>
    : public impl::EncodeBase<Codec<message::server::ColumnMeta>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    if (!caps()[capabilities::pos::protocol_41]) {
      accu.step(wire::VarString(v_.table()))
          .step(wire::VarString(v_.name()))
          .step(wire::VarInt(3))
          .step(wire::FixedInt<3>(v_.column_length()))
          .step(wire::VarInt(1))
          .step(wire::FixedInt<1>(v_.type()));

      if (caps()[capabilities::pos::long_flag]) {
        accu.step(wire::VarInt(3))
            .step(wire::FixedInt<2>(v_.flags().to_ulong()))
            .step(wire::FixedInt<1>(v_.decimals()));
      } else {
        accu.step(wire::VarInt(2))
            .step(wire::FixedInt<1>(v_.flags().to_ulong()))
            .step(wire::FixedInt<1>(v_.decimals()));
      }

      return accu.result();
    } else {
      return accu.step(wire::VarString(v_.catalog()))
          .step(wire::VarString(v_.schema()))
          .step(wire::VarString(v_.table()))
          .step(wire::VarString(v_.orig_table()))
          .step(wire::VarString(v_.name()))
          .step(wire::VarString(v_.orig_name()))
          .step(wire::VarInt(12))
          .step(wire::FixedInt<2>(v_.collation()))
          .step(wire::FixedInt<4>(v_.column_length()))
          .step(wire::FixedInt<1>(v_.type()))
          .step(wire::FixedInt<2>(v_.flags().to_ulong()))
          .step(wire::FixedInt<1>(v_.decimals()))
          .step(wire::FixedInt<2>(0))
          .result();
    }
  }

 public:
  using value_type = message::server::ColumnMeta;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    if (!caps[capabilities::pos::protocol_41]) {
      // 3.2x protocol used up to 4.0.x

      // bit-size of the 'flags' field
      const uint8_t flags_size = caps[capabilities::pos::long_flag] ? 2 : 1;

      const auto table_res = accu.template step<wire::VarString>();
      const auto name_res = accu.template step<wire::VarString>();

      const auto column_length_len_res = accu.template step<wire::VarInt>();
      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      if (column_length_len_res->value() != 3) {
        return stdx::make_unexpected(
            make_error_code(codec_errc::invalid_input));
      }

      const auto column_length_res = accu.template step<wire::FixedInt<3>>();
      const auto type_len_res = accu.template step<wire::VarInt>();
      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      if (type_len_res->value() != 1) {
        return stdx::make_unexpected(
            make_error_code(codec_errc::invalid_input));
      }

      const auto type_res = accu.template step<wire::FixedInt<1>>();
      const auto flags_and_decimals_len_res =
          accu.template step<wire::VarInt>();
      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      if (flags_and_decimals_len_res->value() != flags_size + 1) {
        return stdx::make_unexpected(
            make_error_code(codec_errc::invalid_input));
      }

      stdx::expected<wire::FixedInt<3>, std::error_code> flags_and_decimals_res(
          0);
      if (flags_size == 2) {
        flags_and_decimals_res = accu.template step<wire::FixedInt<3>>();
      } else {
        const auto small_flags_and_decimals_res =
            accu.template step<wire::FixedInt<2>>();
        if (small_flags_and_decimals_res) {
          flags_and_decimals_res =
              wire::FixedInt<3>(small_flags_and_decimals_res->value());
        }
      }

      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      const uint16_t flags =
          flags_and_decimals_res->value() & ((1 << (flags_size * 8)) - 1);
      const uint8_t decimals =
          flags_and_decimals_res->value() >> (flags_size * 8);

      return std::make_pair(
          accu.result().value(),
          value_type({}, {}, table_res->value(), {}, name_res->value(), {}, {},
                     column_length_res->value(), type_res->value(), flags,
                     decimals));
    } else {
      const auto catalog_res = accu.template step<wire::VarString>();
      const auto schema_res = accu.template step<wire::VarString>();
      const auto table_res = accu.template step<wire::VarString>();
      const auto orig_table_res = accu.template step<wire::VarString>();
      const auto name_res = accu.template step<wire::VarString>();
      const auto orig_name_res = accu.template step<wire::VarString>();

      /* next is a collection of fields which is wrapped inside a varstring of
       * 12-bytes size */
      const auto other_len_res = accu.template step<wire::VarInt>();

      if (other_len_res->value() != 12) {
        return stdx::make_unexpected(
            make_error_code(codec_errc::invalid_input));
      }

      const auto collation_res = accu.template step<wire::FixedInt<2>>();
      const auto column_length_res = accu.template step<wire::FixedInt<4>>();
      const auto type_res = accu.template step<wire::FixedInt<1>>();
      const auto flags_res = accu.template step<wire::FixedInt<2>>();
      const auto decimals_res = accu.template step<wire::FixedInt<1>>();

      accu.template step<void>(2);  // fillers

      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(catalog_res->value(), schema_res->value(),
                     table_res->value(), orig_table_res->value(),
                     name_res->value(), orig_name_res->value(),
                     collation_res->value(), column_length_res->value(),
                     type_res->value(), flags_res->value(),
                     decimals_res->value()));
    }
  }

 private:
  const value_type v_;
};

/**
 * codec for server's SendFileRequest response.
 *
 * sent as response after client::Query
 *
 * layout:
 *
 *     0xfb<filename>
 */
template <>
class Codec<message::server::SendFileRequest>
    : public impl::EncodeBase<Codec<message::server::SendFileRequest>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::String(v_.filename()))
        .result();
  }

 public:
  using value_type = message::server::SendFileRequest;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0xfb; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto filename_res = accu.template step<wire::String>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(filename_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for server::StmtPrepareOk message.
 *
 * format:
 *
 * - FixedInt<1> == 0x00 [ok]
 * - FixedInt<4> stmt-id
 * - FixedInt<2> column-count
 * - FixedInt<2> param-count
 * - FixedInt<1> == 0x00 [filler]
 * - FixedInt<2> warning-count
 *
 * If caps contains optional_resultset_metadata:
 *
 * - FixedInt<1> with_metadata
 *
 * sent as response after a client::StmtPrepare
 */
template <>
class Codec<message::server::StmtPrepareOk>
    : public impl::EncodeBase<Codec<message::server::StmtPrepareOk>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.statement_id()))
        .step(wire::FixedInt<2>(v_.column_count()))
        .step(wire::FixedInt<2>(v_.param_count()))
        .step(wire::FixedInt<1>(0))
        .step(wire::FixedInt<2>(v_.warning_count()));

    if (caps()[capabilities::pos::optional_resultset_metadata]) {
      accu.step(wire::FixedInt<1>(v_.with_metadata()));
    }

    return accu.result();
  }

 public:
  using value_type = message::server::StmtPrepareOk;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept { return 0x00; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    auto stmt_id_res = accu.template step<wire::FixedInt<4>>();
    auto column_count_res = accu.template step<wire::FixedInt<2>>();
    auto param_count_res = accu.template step<wire::FixedInt<2>>();
    auto filler_res = accu.template step<wire::FixedInt<1>>();
    auto warning_count_res = accu.template step<wire::FixedInt<2>>();

    // by default, metadata isn't optional
    int8_t with_metadata{1};
    if (caps[capabilities::pos::optional_resultset_metadata]) {
      auto with_metadata_res = accu.template step<wire::FixedInt<1>>();

      if (with_metadata_res) {
        with_metadata = with_metadata_res->value();
      }
    }

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(stmt_id_res->value(), column_count_res->value(),
                   param_count_res->value(), warning_count_res->value(),
                   with_metadata));
  }

 private:
  const value_type v_;
};

/**
 * codec for a Row from the server.
 */
template <>
class Codec<message::server::Row>
    : public impl::EncodeBase<Codec<message::server::Row>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    for (const auto &field : v_) {
      if (field) {
        accu.step(wire::VarString(*field));
      } else {
        accu.step(wire::Null());
      }
    }

    return accu.result();
  }

 public:
  using value_type = message::server::Row;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    std::vector<value_type::value_type> fields;

    const size_t buf_size = buffer_size(buffers);

    while (accu.result() && (accu.result().value() < buf_size)) {
      // field may other be a Null or a VarString
      auto null_res = accu.template try_step<wire::Null>();
      if (null_res) {
        fields.emplace_back(std::nullopt);
      } else {
        auto field_res = accu.template step<wire::VarString>();
        if (!field_res) return stdx::make_unexpected(field_res.error());

        fields.emplace_back(std::move(field_res->value()));
      }
    }

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type(fields));
  }

 private:
  const value_type v_;
};

/**
 * codec for a StmtRow from the server.
 *
 * StmtRow is the Row of a StmtExecute's resultset.
 *
 * - 0x00
 * - NULL bitmap
 * - non-NULL-values in binary encoding
 *
 * both encode and decode require type information to know:
 *
 * - size the NULL bitmap
 * - length of each field
 */
template <>
class Codec<message::server::StmtRow>
    : public impl::EncodeBase<Codec<message::server::StmtRow>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(0));

    std::string nullbits;
    nullbits.resize(bytes_per_bits(v_.types().size()));

    // null-bitmap starts with a 2-bit offset
    size_t bit_pos{2};
    size_t byte_pos{};
    for (const auto &field : v_) {
      if (bit_pos > 7) {
        bit_pos = 0;
        ++byte_pos;
      }

      if (!field) {
        nullbits[byte_pos] |= 1 << bit_pos;
      }
    }

    accu.step(wire::String(nullbits));

    size_t n{};
    for (const auto &field : v_) {
      if (field) {
        switch (v_.types()[n++]) {
          case field_type::Bit:
          case field_type::Blob:
          case field_type::Varchar:
          case field_type::VarString:
          case field_type::Set:
          case field_type::String:
          case field_type::Enum:
          case field_type::TinyBlob:
          case field_type::MediumBlob:
          case field_type::LongBlob:
          case field_type::Decimal:
          case field_type::NewDecimal:
          case field_type::Geometry:
            accu.step(wire::VarInt(field->size()));
            break;
          case field_type::Date:
          case field_type::DateTime:
          case field_type::Timestamp:
          case field_type::Time:
            accu.step(wire::FixedInt<1>(field->size()));
            break;
          case field_type::LongLong:
          case field_type::Double:
          case field_type::Long:
          case field_type::Int24:
          case field_type::Float:
          case field_type::Short:
          case field_type::Year:
          case field_type::Tiny:
            // fixed size
            break;
        }
        accu.step(wire::String(*field));
      }
    }

    return accu.result();
  }

 public:
  using value_type = message::server::StmtRow;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps,
      std::vector<field_type::value_type> types) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    const auto row_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    // first byte is 0x00
    if (row_byte_res->value() != 0x00) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    const auto nullbits_res =
        accu.template step<wire::String>(bytes_per_bits(types.size()));
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    const auto nullbits = nullbits_res->value();

    std::vector<value_type::value_type> values;

    for (size_t n{}, bit_pos{2}, byte_pos{}; n < types.size(); ++n, ++bit_pos) {
      if (bit_pos > 7) {
        bit_pos = 0;
        ++byte_pos;
      }

      if (!(nullbits[byte_pos] & (1 << bit_pos))) {
        stdx::expected<size_t, std::error_code> field_size_res(
            stdx::make_unexpected(
                make_error_code(std::errc::invalid_argument)));
        switch (types[n]) {
          case field_type::Bit:
          case field_type::Blob:
          case field_type::Varchar:
          case field_type::VarString:
          case field_type::Set:
          case field_type::String:
          case field_type::Enum:
          case field_type::TinyBlob:
          case field_type::MediumBlob:
          case field_type::LongBlob:
          case field_type::Decimal:
          case field_type::NewDecimal:
          case field_type::Geometry: {
            auto string_field_size_res = accu.template step<wire::VarInt>();
            if (!accu.result())
              return stdx::make_unexpected(accu.result().error());

            field_size_res = string_field_size_res->value();
          } break;
          case field_type::Date:
          case field_type::DateTime:
          case field_type::Timestamp:
          case field_type::Time: {
            auto time_field_size_res = accu.template step<wire::FixedInt<1>>();
            if (!accu.result())
              return stdx::make_unexpected(accu.result().error());

            field_size_res = time_field_size_res->value();
          } break;
          case field_type::LongLong:
          case field_type::Double:
            field_size_res = 8;
            break;
          case field_type::Long:
          case field_type::Int24:
          case field_type::Float:
            field_size_res = 4;
            break;
          case field_type::Short:
          case field_type::Year:
            field_size_res = 2;
            break;
          case field_type::Tiny:
            field_size_res = 1;
            break;
          default:
            return stdx::make_unexpected(
                make_error_code(codec_errc::field_type_unknown));
        }
        const auto value_res =
            accu.template step<wire::String>(field_size_res.value());
        if (!accu.result()) return stdx::make_unexpected(accu.result().error());

        values.push_back(value_res->value());
      } else {
        values.emplace_back(std::nullopt);
      }
    }

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type(types, values));
  }

 private:
  const value_type v_;
};

/**
 * codec for server::Statistics message.
 */
template <>
class Codec<message::server::Statistics>
    : public impl::EncodeBase<Codec<message::server::Statistics>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::String(v_.stats())).result();
  }

 public:
  using value_type = message::server::Statistics;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto stats_res = accu.template step<wire::String>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(stats_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * CRTP base for client-side commands that are encoded as a single byte.
 */
template <class Base, class ValueType>
class CodecSimpleCommand
    : public impl::EncodeBase<CodecSimpleCommand<Base, ValueType>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(Base::cmd_byte())).result();
  }

 public:
  using __base = impl::EncodeBase<CodecSimpleCommand<Base, ValueType>>;

  friend __base;

  constexpr CodecSimpleCommand(capabilities::value_type caps) : __base(caps) {}

  static constexpr size_t max_size() noexcept { return 1; }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, ValueType>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != Base::cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    return std::make_pair(accu.result().value(), ValueType());
  }
};

enum class CommandByte {
  Quit = 0x01,
  InitSchema,
  Query,
  ListFields,
  CreateDb,
  DropDb,
  Refresh,
  Shutdown,
  Statistics,
  ProcessInfo,
  Connect,
  ProcessKill,
  Debug,
  Ping,
  Time,
  DelayedInsert,
  ChangeUser,
  BinlogDump,
  TableDump,
  ConnectOut,
  RegisterReplica,
  StmtPrepare,
  StmtExecute,
  StmtSendLongData,
  StmtClose,
  StmtReset,
  SetOption,
  StmtFetch,
  Deamon,
  BinlogDumpGtid,
  ResetConnection,
  Clone
};

/**
 * codec for client's Quit command.
 */
template <>
class Codec<message::client::Quit>
    : public CodecSimpleCommand<Codec<message::client::Quit>,
                                message::client::Quit> {
 public:
  using value_type = message::client::Quit;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Quit);
  }
};

/**
 * codec for client's ResetConnection command.
 */
template <>
class Codec<message::client::ResetConnection>
    : public CodecSimpleCommand<Codec<message::client::ResetConnection>,
                                message::client::ResetConnection> {
 public:
  using value_type = message::client::ResetConnection;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ResetConnection);
  }
};

/**
 * codec for client's Ping command.
 */
template <>
class Codec<message::client::Ping>
    : public CodecSimpleCommand<Codec<message::client::Ping>,
                                message::client::Ping> {
 public:
  using value_type = message::client::Ping;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Ping);
  }
};

/**
 * codec for client's Statistics command.
 */
template <>
class Codec<message::client::Statistics>
    : public CodecSimpleCommand<Codec<message::client::Statistics>,
                                message::client::Statistics> {
 public:
  using value_type = message::client::Statistics;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Statistics);
  }
};

/**
 * codec for client's InitSchema command.
 */
template <>
class Codec<message::client::InitSchema>
    : public impl::EncodeBase<Codec<message::client::InitSchema>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::String(v_.schema()))
        .result();
  }

 public:
  using value_type = message::client::InitSchema;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::InitSchema);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto schema_res = accu.template step<wire::String>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(schema_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Query command.
 */
template <>
class Codec<message::client::Query>
    : public impl::EncodeBase<Codec<message::client::Query>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::String(v_.statement()))
        .result();
  }

 public:
  using value_type = message::client::Query;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Query);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_res = accu.template step<wire::String>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(statement_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client::SendFile message.
 *
 * sent by client as response to server::SendFileRequest
 *
 * format:
 *
 * - String payload
 */
template <>
class Codec<message::client::SendFile>
    : public impl::EncodeBase<Codec<message::client::SendFile>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::String(v_.payload())).result();
  }

 public:
  using value_type = message::client::SendFile;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto payload_res = accu.template step<wire::String>();
    if (!accu.result()) return accu.result().get_unexpected();

    return std::make_pair(accu.result().value(),
                          value_type(payload_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's ListFields command.
 */
template <>
class Codec<message::client::ListFields>
    : public impl::EncodeBase<Codec<message::client::ListFields>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::NulTermString(v_.table_name()))
        .step(wire::String(v_.wildcard()))
        .result();
  }

 public:
  using value_type = message::client::ListFields;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ListFields);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto table_name_res = accu.template step<wire::NulTermString>();
    auto wildcard_res = accu.template step<wire::String>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(table_name_res->value(), wildcard_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Reload command.
 */
template <>
class Codec<message::client::Reload>
    : public impl::EncodeBase<Codec<message::client::Reload>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<1>(v_.cmds().to_ulong()))
        .result();
  }

 public:
  using value_type = message::client::Reload;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Refresh);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto cmds_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type(cmds_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Kill command.
 *
 * format:
 *
 * - FixedInt<1> == 0x0c, ProcessKill
 * - FixedInt<4> id
 */
template <>
class Codec<message::client::Kill>
    : public impl::EncodeBase<Codec<message::client::Kill>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.connection_id()))
        .result();
  }

 public:
  using value_type = message::client::Kill;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ProcessKill);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto connection_id_res = accu.template step<wire::FixedInt<4>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(connection_id_res->value()));
  }

 private:
  value_type v_;
};

/**
 * codec for client's Prepared Statement command.
 */
template <>
class Codec<message::client::StmtPrepare>
    : public impl::EncodeBase<Codec<message::client::StmtPrepare>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::String(v_.statement()))
        .result();
  }

 public:
  using value_type = message::client::StmtPrepare;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtPrepare);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_res = accu.template step<wire::String>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(statement_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Execute Statement command.
 */
template <>
class Codec<message::client::StmtExecute>
    : public impl::EncodeBase<Codec<message::client::StmtExecute>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.statement_id()))
        .step(wire::FixedInt<1>(v_.flags().to_ullong()))
        .step(wire::FixedInt<4>(v_.iteration_count()));

    // values.size() and types.size() MUST be the same
    if (!v_.values().empty()) {
      // mark all that are NULL in the nullbits
      //
      // - one bit per parameter to send
      // - if a parameter is NULL, the bit is set, and later no value is added.
      std::vector<uint8_t> nullbits(bytes_per_bits(v_.values().size()));

      {
        size_t byte_pos{}, bit_pos{};
        for (auto const &v : v_.values()) {
          if (bit_pos > 7) {
            bit_pos = 0;
            ++byte_pos;
          }

          if (!v) {
            nullbits[byte_pos] |= 1 << bit_pos;
          }

          ++bit_pos;
        }
      }

      accu.step(wire::String(
                    std::string(reinterpret_cast<const char *>(nullbits.data()),
                                nullbits.size())))
          .step(wire::FixedInt<1>(v_.new_params_bound()));
      if (v_.new_params_bound()) {
        for (const auto &t : v_.types()) {
          accu.step(wire::FixedInt<2>(t));
        }
        size_t n{};
        for (const auto &v : v_.values()) {
          // add all the values that aren't NULL
          if (v.has_value()) {
            // write length of the type is a variable length
            switch (v_.types()[n++]) {
              case field_type::Bit:
              case field_type::Blob:
              case field_type::Varchar:
              case field_type::VarString:
              case field_type::Set:
              case field_type::String:
              case field_type::Enum:
              case field_type::TinyBlob:
              case field_type::MediumBlob:
              case field_type::LongBlob:
              case field_type::Decimal:
              case field_type::NewDecimal:
              case field_type::Geometry:
                accu.step(wire::VarInt(v->size()));
                break;
              case field_type::Date:
              case field_type::DateTime:
              case field_type::Timestamp:
              case field_type::Time:
                accu.step(wire::FixedInt<1>(v->size()));
                break;
              case field_type::LongLong:
              case field_type::Double:
              case field_type::Long:
              case field_type::Int24:
              case field_type::Float:
              case field_type::Short:
              case field_type::Year:
              case field_type::Tiny:
                // fixed size
                break;
            }
            accu.step(wire::String(v.value()));
          }
        }
      }
    }

    return accu.result();
  }

 public:
  using value_type = message::client::StmtExecute;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtExecute);
  }

  /**
   * decode a sequence of buffers into a message::client::ExecuteStmt.
   *
   * @param buffers sequence of buffers
   * @param caps protocol capabilities
   * @param param_count_lookup callable that expects a 'uint32_t statement_id'
   * that returns and integer that's convertible to 'stdx::expected<uint16_t,
   * std::error_code>' representing the parameter count of the prepared
   * statement
   *
   * decoding a ExecuteStmt message requires a parameter count of the prepared
   * statement. The param_count_lookup function may be called to get the param
   * count for the statement-id.
   *
   * The function may return a param-count directly
   *
   * \code
   * ExecuteStmt::decode(
   *   buffers,
   *   capabilities::protocol_41,
   *   [](uint32_t stmt_id) { return 1; });
   * \endcode
   *
   * ... or a stdx::expected<uint16_t, std::error_code> if it wants to signal
   * that a statement-id wasn't found
   *
   * \code
   * ExecuteStmt::decode(
   *   buffers,
   *   capabilities::protocol_41,
   *   [](uint32_t stmt_id) -> stdx::expected<uint16_t, std::error_code> {
   *     bool found{true};
   *
   *     if (found) {
   *       return 1;
   *     } else {
   *       return stdx::make_unexpected(make_error_code(
   *         codec_errc::statement_id_not_found));
   *     }
   *   });
   * \endcode
   */
  template <class ConstBufferSequence, class Func>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps,
      Func &&param_count_lookup) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<wire::FixedInt<4>>();
    auto flags_res = accu.template step<wire::FixedInt<1>>();
    auto iteration_count_res = accu.template step<wire::FixedInt<4>>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    stdx::expected<uint64_t, std::error_code> param_count_res =
        param_count_lookup(statement_id_res->value());
    if (!param_count_res) {
      return stdx::make_unexpected(
          make_error_code(codec_errc::statement_id_not_found));
    }

    const size_t param_count = param_count_res.value();

    if (param_count == 0) {
      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(statement_id_res->value(), flags_res->value(),
                     iteration_count_res->value(), false, {}, {}));
    }

    auto nullbits_res =
        accu.template step<wire::String>(bytes_per_bits(param_count));
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto new_params_bound_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    std::vector<classic_protocol::field_type::value_type> types;
    std::vector<std::optional<std::string>> values;

    if (new_params_bound_res->value()) {
      const auto nullbits = nullbits_res->value();

      types.reserve(param_count);
      values.reserve(param_count);

      for (size_t n{}; n < param_count; ++n) {
        auto type_res = accu.template step<wire::FixedInt<2>>();
        if (!accu.result()) return stdx::make_unexpected(accu.result().error());

        types.push_back(type_res->value());
      }
      for (size_t n{}, bit_pos{}, byte_pos{}; n < param_count; ++n, ++bit_pos) {
        if (bit_pos > 7) {
          bit_pos = 0;
          ++byte_pos;
        }

        if (!(nullbits[byte_pos] & (1 << bit_pos))) {
          stdx::expected<size_t, std::error_code> field_size_res(
              stdx::make_unexpected(
                  make_error_code(std::errc::invalid_argument)));
          switch (types[n]) {
            case field_type::Bit:
            case field_type::Blob:
            case field_type::Varchar:
            case field_type::VarString:
            case field_type::Set:
            case field_type::String:
            case field_type::Enum:
            case field_type::TinyBlob:
            case field_type::MediumBlob:
            case field_type::LongBlob:
            case field_type::Decimal:
            case field_type::NewDecimal:
            case field_type::Geometry: {
              auto string_field_size_res = accu.template step<wire::VarInt>();
              if (!accu.result())
                return stdx::make_unexpected(accu.result().error());

              field_size_res = string_field_size_res->value();
            } break;
            case field_type::Date:
            case field_type::DateTime:
            case field_type::Timestamp:
            case field_type::Time: {
              auto time_field_size_res =
                  accu.template step<wire::FixedInt<1>>();
              if (!accu.result())
                return stdx::make_unexpected(accu.result().error());

              field_size_res = time_field_size_res->value();
            } break;
            case field_type::LongLong:
            case field_type::Double:
              field_size_res = 8;
              break;
            case field_type::Long:
            case field_type::Int24:
            case field_type::Float:
              field_size_res = 4;
              break;
            case field_type::Short:
            case field_type::Year:
              field_size_res = 2;
              break;
            case field_type::Tiny:
              field_size_res = 1;
              break;
          }
          auto value_res =
              accu.template step<wire::String>(field_size_res.value());
          if (!accu.result()) {
            return stdx::make_unexpected(accu.result().error());
          }

          values.push_back(value_res->value());
        } else {
          values.emplace_back(std::nullopt);
        }
      }
    }

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(statement_id_res->value(), flags_res->value(),
                   iteration_count_res->value(), new_params_bound_res->value(),
                   types, values));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's append data Statement command.
 */
template <>
class Codec<message::client::StmtParamAppendData>
    : public impl::EncodeBase<Codec<message::client::StmtParamAppendData>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.statement_id()))
        .step(wire::FixedInt<2>(v_.param_id()))
        .step(wire::String(v_.data()))
        .result();
  }

 public:
  using value_type = message::client::StmtParamAppendData;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtSendLongData);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<wire::FixedInt<4>>();
    auto param_id_res = accu.template step<wire::FixedInt<2>>();
    auto data_res = accu.template step<wire::String>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(statement_id_res->value(),
                                     param_id_res->value(), data_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Close Statement command.
 */
template <>
class Codec<message::client::StmtClose>
    : public impl::EncodeBase<Codec<message::client::StmtClose>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.statement_id()))
        .result();
  }

 public:
  using value_type = message::client::StmtClose;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtClose);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<wire::FixedInt<4>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(statement_id_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Reset Statement command.
 */
template <>
class Codec<message::client::StmtReset>
    : public impl::EncodeBase<Codec<message::client::StmtReset>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.statement_id()))
        .result();
  }

 public:
  using value_type = message::client::StmtReset;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtReset);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<wire::FixedInt<4>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(statement_id_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's SetOption command.
 */
template <>
class Codec<message::client::SetOption>
    : public impl::EncodeBase<Codec<message::client::SetOption>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<2>(v_.option()))
        .result();
  }

 public:
  using value_type = message::client::SetOption;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::SetOption);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto option_res = accu.template step<wire::FixedInt<2>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(option_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Fetch Cursor command.
 */
template <>
class Codec<message::client::StmtFetch>
    : public impl::EncodeBase<Codec<message::client::StmtFetch>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.statement_id()))
        .step(wire::FixedInt<4>(v_.row_count()))
        .result();
  }

 public:
  using value_type = message::client::StmtFetch;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtFetch);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<wire::FixedInt<4>>();
    auto row_count_res = accu.template step<wire::FixedInt<4>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(statement_id_res->value(), row_count_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client side greeting message.
 *
 *
 * in 3.21 ... 4.0:
 *
 *     FixedInt<2>    capabilities [protocol_41 not set]
 *     FixedInt<3>    max-allowed-packet
 *     NulTermString  username
 *     NulTermString  auth-method-data
 *
 *     [if not connect_with_schema, there may be no trailing Nul-byte]
 *
 *     if connect_with_schema {
 *       String         schema
 *     }
 *
 * the auth-method is "old_password" if "protocol_version == 10 &&
 * (capabilities & long_password)", it is "older_password" otherwise
 *
 *     FixedInt<2>    capabilities_lo [protocol_41 set]
 *     FixedInt<2>    capabilities_hi
 *     FixedInt<4>    max_allowed-packet
 *     ...
 *
 * The capabilities that are part of the message are the client's capabilities
 * (which may announce more than what the server supports). The codec
 * uses the capabilities that are shared between client and server to decide
 * which parts and how they are understood, though.
 *
 * checked capabilities:
 * - protocol_41
 * - ssl
 * - client_auth_method_data_varint
 * - secure_connection
 * - connect_with_schema
 * - plugin_auth
 * - connect_attributes
 */
template <>
class Codec<message::client::Greeting>
    : public impl::EncodeBase<Codec<message::client::Greeting>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    const auto shared_caps = v_.capabilities() & caps();

    if (shared_caps[classic_protocol::capabilities::pos::protocol_41]) {
      accu.step(wire::FixedInt<4>(v_.capabilities().to_ulong()))
          .step(wire::FixedInt<4>(v_.max_packet_size()))
          .step(wire::FixedInt<1>(v_.collation()))
          .step(wire::String(std::string(23, '\0')));
      if (!(shared_caps[classic_protocol::capabilities::pos::ssl] &&
            v_.username().empty())) {
        // the username is empty and SSL is set, this is a short SSL-greeting
        // packet
        accu.step(wire::NulTermString(v_.username()));

        if (shared_caps[classic_protocol::capabilities::pos::
                            client_auth_method_data_varint]) {
          accu.step(wire::VarString(v_.auth_method_data()));
        } else if (shared_caps[classic_protocol::capabilities::pos::
                                   secure_connection]) {
          accu.step(wire::FixedInt<1>(v_.auth_method_data().size()))
              .step(wire::String(v_.auth_method_data()));
        } else {
          accu.step(wire::NulTermString(v_.auth_method_data()));
        }

        if (shared_caps
                [classic_protocol::capabilities::pos::connect_with_schema]) {
          accu.step(wire::NulTermString(v_.schema()));
        }

        if (!shared_caps
                [classic_protocol::capabilities::pos::connect_attributes]) {
          // special handling for off-spec client/server implementations.
          //
          // 1. older clients may set ::plugin_auth, but
          //    ::connection_attributes which means nothing follows the
          //    "auth-method-name" field
          // 2. auth-method-name is empty, it MAY be skipped.
          if (shared_caps[classic_protocol::capabilities::pos::plugin_auth] &&
              !v_.auth_method_name().empty()) {
            accu.step(wire::NulTermString(v_.auth_method_name()));
          }
        } else {
          if (shared_caps[classic_protocol::capabilities::pos::plugin_auth]) {
            accu.step(wire::NulTermString(v_.auth_method_name()));
          }

          accu.step(wire::VarString(v_.attributes()));
        }
      }
    } else {
      accu.step(wire::FixedInt<2>(v_.capabilities().to_ulong()))
          .step(wire::FixedInt<3>(v_.max_packet_size()))
          .step(wire::NulTermString(v_.username()));
      if (shared_caps
              [classic_protocol::capabilities::pos::connect_with_schema]) {
        accu.step(wire::NulTermString(v_.auth_method_data()))
            .step(wire::String(v_.schema()));
      } else {
        accu.step(wire::String(v_.auth_method_data()));
      }
    }

    return accu.result();
  }

 public:
  using value_type = message::client::Greeting;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto capabilities_lo_res = accu.template step<wire::FixedInt<2>>();
    if (!capabilities_lo_res)
      return stdx::make_unexpected(capabilities_lo_res.error());

    auto client_capabilities = classic_protocol::capabilities::value_type(
        capabilities_lo_res->value());

    // decoding depends on the capabilities that both client and server have in
    // common
    auto shared_capabilities = caps & client_capabilities;

    if (shared_capabilities[classic_protocol::capabilities::pos::protocol_41]) {
      // if protocol_41 is set in the capabilities, we expected 2 more bytes
      // of capabilities
      auto capabilities_hi_res = accu.template step<wire::FixedInt<2>>();
      if (!capabilities_hi_res)
        return stdx::make_unexpected(capabilities_hi_res.error());

      client_capabilities |= classic_protocol::capabilities::value_type(
          capabilities_hi_res->value() << 16);

      shared_capabilities = caps & client_capabilities;

      auto max_packet_size_res = accu.template step<wire::FixedInt<4>>();
      auto collation_res = accu.template step<wire::FixedInt<1>>();

      accu.template step<wire::String>(23);  // skip 23 bytes

      auto last_accu_res = accu.result();

      auto username_res = accu.template step<wire::NulTermString>();
      if (!accu.result()) {
        // if there isn't enough data for the nul-term-string, but we had the
        // 23-bytes ...
        if (last_accu_res &&
            shared_capabilities[classic_protocol::capabilities::pos::ssl]) {
          return std::make_pair(
              last_accu_res.value(),
              value_type(client_capabilities, max_packet_size_res->value(),
                         collation_res->value(), {}, {}, {}, {}, {}));
        }

        return stdx::make_unexpected(accu.result().error());
      }

      // auth-method-data is either
      //
      // - varint length
      // - fixed-int-1 length
      // - null-term-string
      stdx::expected<wire::String, std::error_code> auth_method_data_res;
      if (shared_capabilities[classic_protocol::capabilities::pos::
                                  client_auth_method_data_varint]) {
        auto res = accu.template step<wire::VarString>();
        if (!res) return stdx::make_unexpected(res.error());

        auth_method_data_res = wire::String(res->value());
      } else if (shared_capabilities
                     [classic_protocol::capabilities::pos::secure_connection]) {
        auto auth_method_data_len_res = accu.template step<wire::FixedInt<1>>();
        if (!auth_method_data_len_res)
          return stdx::make_unexpected(auth_method_data_len_res.error());
        auto auth_method_data_len = auth_method_data_len_res->value();

        auto res = accu.template step<wire::String>(auth_method_data_len);
        if (!res) return stdx::make_unexpected(res.error());

        auth_method_data_res = wire::String(res->value());
      } else {
        auto res = accu.template step<wire::NulTermString>();
        if (!res) return stdx::make_unexpected(res.error());

        auth_method_data_res = wire::String(res->value());
      }

      stdx::expected<wire::NulTermString, std::error_code> schema_res;
      if (shared_capabilities
              [classic_protocol::capabilities::pos::connect_with_schema]) {
        schema_res = accu.template step<wire::NulTermString>();
      }
      if (!schema_res) return stdx::make_unexpected(schema_res.error());

      stdx::expected<wire::NulTermString, std::error_code> auth_method_res;
      if (shared_capabilities
              [classic_protocol::capabilities::pos::plugin_auth]) {
        if (net::buffer_size(buffers) == accu.result().value()) {
          // even with plugin_auth set, the server is fine, if no
          // auth_method_name is sent.
          auth_method_res = wire::NulTermString{};
        } else {
          auth_method_res = accu.template step<wire::NulTermString>();
        }
      }
      if (!auth_method_res)
        return stdx::make_unexpected(auth_method_res.error());

      stdx::expected<wire::VarString, std::error_code> attributes_res;
      if (shared_capabilities
              [classic_protocol::capabilities::pos::connect_attributes]) {
        attributes_res = accu.template step<wire::VarString>();
      }

      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(client_capabilities, max_packet_size_res->value(),
                     collation_res->value(), username_res->value(),
                     auth_method_data_res->value(), schema_res->value(),
                     auth_method_res->value(), attributes_res->value()));

    } else {
      auto max_packet_size_res = accu.template step<wire::FixedInt<3>>();

      auto username_res = accu.template step<wire::NulTermString>();

      stdx::expected<wire::String, std::error_code> auth_method_data_res;
      stdx::expected<wire::String, std::error_code> schema_res;

      if (shared_capabilities
              [classic_protocol::capabilities::pos::connect_with_schema]) {
        auto res = accu.template step<wire::NulTermString>();
        if (!res) return stdx::make_unexpected(res.error());

        // auth_method_data is a wire::String, move it over
        auth_method_data_res = wire::String(res->value());

        schema_res = accu.template step<wire::String>();
      } else {
        auth_method_data_res = accu.template step<wire::String>();
      }

      if (!accu.result()) return stdx::make_unexpected(accu.result().error());

      // idea: benchmark in-place constructor where all parameters are passed
      // down to the lowest level.
      //
      // It should involve less copy-construction.
      //
      // - stdx::in_place is for in-place construction of stdx::expected's
      // value
      // - std::piecewise_construct for the parts of the std::pair that is
      //   returned as value of stdx::expected
      //
      // return {stdx::in_place, std::piecewise_construct,
      //         std::forward_as_tuple(accu.result().value()),
      //         std::forward_as_tuple(capabilities,
      //         max_packet_size_res->value(),
      //                               0x00, username_res->value(),
      //                               auth_method_data_res->value(),
      //                               schema_res->value(), {},
      //                               {})};
      return std::make_pair(
          accu.result().value(),
          value_type(client_capabilities, max_packet_size_res->value(), 0x00,
                     username_res->value(), auth_method_data_res->value(),
                     schema_res->value(), {}, {}));
    }
  }

 private:
  const value_type v_;
};

/**
 * codec for client::AuthMethodData message.
 *
 * format:
 *
 * - String auth_method_data
 *
 * sent after server::AuthMethodData or server::AuthMethodContinue
 */
template <>
class Codec<message::client::AuthMethodData>
    : public impl::EncodeBase<Codec<message::client::AuthMethodData>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::String(v_.auth_method_data())).result();
  }

 public:
  using value_type = message::client::AuthMethodData;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto auth_method_data_res = accu.template step<wire::String>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(auth_method_data_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client side change-user message.
 *
 * checked capabilities:
 * - protocol_41
 * - secure_connection
 * - plugin_auth
 * - connect_attributes
 */
template <>
class Codec<message::client::ChangeUser>
    : public impl::EncodeBase<Codec<message::client::ChangeUser>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::NulTermString(v_.username()));

    if (caps()[classic_protocol::capabilities::pos::secure_connection]) {
      accu.step(wire::FixedInt<1>(v_.auth_method_data().size()))
          .step(wire::String(v_.auth_method_data()));
    } else {
      accu.step(wire::NulTermString(v_.auth_method_data()));
    }
    accu.step(wire::NulTermString(v_.schema()));

    // 4.1 and later have a collation
    //
    // this could be checked via the protocol_41 capability, but that's not
    // what the server does
    if (v_.collation() != 0x00 ||
        caps()[classic_protocol::capabilities::pos::plugin_auth] ||
        caps()[classic_protocol::capabilities::pos::connect_attributes]) {
      accu.step(wire::FixedInt<2>(v_.collation()));
      if (caps()[classic_protocol::capabilities::pos::plugin_auth]) {
        accu.step(wire::NulTermString(v_.auth_method_name()));
      }

      if (caps()[classic_protocol::capabilities::pos::connect_attributes]) {
        accu.step(wire::VarString(v_.attributes()));
      }
    }

    return accu.result();
  }

 public:
  using value_type = message::client::ChangeUser;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ChangeUser);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto username_res = accu.template step<wire::NulTermString>();

    // auth-method-data is either
    //
    // - fixed-int-1 length
    // - null-term-string
    stdx::expected<wire::String, std::error_code> auth_method_data_res;
    if (caps[classic_protocol::capabilities::pos::secure_connection]) {
      auto auth_method_data_len_res = accu.template step<wire::FixedInt<1>>();
      if (!auth_method_data_len_res)
        return stdx::make_unexpected(auth_method_data_len_res.error());
      auto auth_method_data_len = auth_method_data_len_res->value();

      auto res = accu.template step<wire::String>(auth_method_data_len);
      if (!res) return stdx::make_unexpected(res.error());

      auth_method_data_res = wire::String(res->value());
    } else {
      auto res = accu.template step<wire::NulTermString>();
      if (!res) return stdx::make_unexpected(res.error());

      auth_method_data_res = wire::String(res->value());
    }

    auto schema_res = accu.template step<wire::NulTermString>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    // 3.23.x-4.0 don't send more.
    if (buffer_size(buffers) <= accu.result().value()) {
      return std::make_pair(
          accu.result().value(),
          value_type(username_res->value(), auth_method_data_res->value(),
                     schema_res->value(), 0x00, {}, {}));
    }

    // added in 4.1
    auto collation_res = accu.template step<wire::FixedInt<2>>();

    stdx::expected<wire::NulTermString, std::error_code> auth_method_name_res;
    if (caps[classic_protocol::capabilities::pos::plugin_auth]) {
      auth_method_name_res = accu.template step<wire::NulTermString>();
    }

    stdx::expected<wire::VarString, std::error_code> attributes_res;
    if (caps[classic_protocol::capabilities::pos::connect_attributes]) {
      attributes_res = accu.template step<wire::VarString>();
    }

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(username_res->value(), auth_method_data_res->value(),
                   schema_res->value(), collation_res->value(),
                   auth_method_name_res->value(), attributes_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Clone command.
 *
 * response: server::Ok or server::Error
 */
template <>
class Codec<message::client::Clone>
    : public CodecSimpleCommand<Codec<message::client::Clone>,
                                message::client::Clone> {
 public:
  using value_type = message::client::Clone;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Clone);
  }
};

/**
 * codec for client side dump-binlog message.
 */
template <>
class Codec<message::client::BinlogDump>
    : public impl::EncodeBase<Codec<message::client::BinlogDump>> {
 public:
  using value_type = message::client::BinlogDump;

 private:
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.position()))
        .step(wire::FixedInt<2>(v_.flags().underlying_value()))
        .step(wire::FixedInt<4>(v_.server_id()))
        .step(wire::String(v_.filename()))
        .result();
  }

 public:
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::BinlogDump);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto position_res = accu.template step<wire::FixedInt<4>>();
    auto flags_res = accu.template step<wire::FixedInt<2>>();
    auto server_id_res = accu.template step<wire::FixedInt<4>>();

    auto filename_res = accu.template step<wire::String>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    stdx::flags<value_type::Flags> flags;
    flags.underlying_value(flags_res->value());

    return std::make_pair(
        accu.result().value(),
        value_type(flags, server_id_res->value(), filename_res->value(),
                   position_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client side register-replica message.
 */
template <>
class Codec<message::client::RegisterReplica>
    : public impl::EncodeBase<Codec<message::client::RegisterReplica>> {
 public:
  using value_type = message::client::RegisterReplica;

 private:
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    return accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<4>(v_.server_id()))
        .step(wire::FixedInt<1>(v_.hostname().size()))
        .step(wire::String(v_.hostname()))
        .step(wire::FixedInt<1>(v_.username().size()))
        .step(wire::String(v_.username()))
        .step(wire::FixedInt<1>(v_.password().size()))
        .step(wire::String(v_.password()))
        .step(wire::FixedInt<2>(v_.port()))
        .step(wire::FixedInt<4>(v_.replication_rank()))
        .step(wire::FixedInt<4>(v_.master_id()))
        .result();
  }

 public:
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::RegisterReplica);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto server_id_res = accu.template step<wire::FixedInt<4>>();
    auto hostname_len_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto hostname_res =
        accu.template step<wire::String>(hostname_len_res->value());

    auto username_len_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto username_res =
        accu.template step<wire::String>(username_len_res->value());

    auto password_len_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto password_res =
        accu.template step<wire::String>(password_len_res->value());

    auto port_res = accu.template step<wire::FixedInt<2>>();
    auto replication_rank_res = accu.template step<wire::FixedInt<4>>();
    auto master_id_res = accu.template step<wire::FixedInt<4>>();

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(server_id_res->value(), hostname_res->value(),
                   username_res->value(), password_res->value(),
                   port_res->value(), replication_rank_res->value(),
                   master_id_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client side dump-binlog-with-gtid message.
 */
template <>
class Codec<message::client::BinlogDumpGtid>
    : public impl::EncodeBase<Codec<message::client::BinlogDumpGtid>> {
 public:
  using value_type = message::client::BinlogDumpGtid;

 private:
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<1>(cmd_byte()))
        .step(wire::FixedInt<2>(v_.flags().underlying_value()))
        .step(wire::FixedInt<4>(v_.server_id()))
        .step(wire::FixedInt<4>(v_.filename().size()))
        .step(wire::String(v_.filename()))
        .step(wire::FixedInt<8>(v_.position()))
        //
        ;

    if (v_.flags() & value_type::Flags::through_gtid) {
      accu.step(wire::FixedInt<4>(v_.sids().size()))
          .step(wire::String(v_.sids()));
    }

    return accu.result();
  }

 public:
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::BinlogDumpGtid);
  }

  template <class ConstBufferSequence>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const ConstBufferSequence &buffers, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator<ConstBufferSequence> accu(buffers, caps);

    auto cmd_byte_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::make_unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto flags_res = accu.template step<wire::FixedInt<2>>();
    auto server_id_res = accu.template step<wire::FixedInt<4>>();
    auto filename_len_res = accu.template step<wire::FixedInt<4>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto filename_res =
        accu.template step<wire::String>(filename_len_res->value());
    auto position_res = accu.template step<wire::FixedInt<8>>();
    auto sids_len_res = accu.template step<wire::FixedInt<4>>();
    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    auto sids_res = accu.template step<wire::String>(sids_len_res->value());

    if (!accu.result()) return stdx::make_unexpected(accu.result().error());

    stdx::flags<value_type::Flags> flags;
    flags.underlying_value(flags_res->value());

    return std::make_pair(
        accu.result().value(),
        value_type(flags, server_id_res->value(), filename_res->value(),
                   position_res->value(), sids_res->value()));
  }

 private:
  const value_type v_;
};

}  // namespace classic_protocol

#endif
