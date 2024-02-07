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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_MESSAGE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_MESSAGE_H_

#include <cassert>
#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <system_error>  // error_code
#include <utility>       // move

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/ranges.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_binary.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/classic_protocol_wire.h"

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
template <bool Borrowed>
class Codec<borrowable::message::server::Greeting<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::Greeting<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    if (v_.protocol_version() == 0x09) {
      return accu.step(bw::FixedInt<1>(v_.protocol_version()))
          .step(bw::NulTermString<Borrowed>(v_.version()))
          .step(bw::FixedInt<4>(v_.connection_id()))
          .step(bw::NulTermString<Borrowed>(v_.auth_method_data().substr(0, 8)))
          .result();
    } else {
      uint8_t auth_method_data_size{0};
      if (v_.capabilities()[classic_protocol::capabilities::pos::plugin_auth]) {
        auth_method_data_size = v_.auth_method_data().size();
      }

      accu.step(bw::FixedInt<1>(v_.protocol_version()))
          .step(bw::NulTermString<Borrowed>(v_.version()))
          .step(bw::FixedInt<4>(v_.connection_id()))
          .step(bw::NulTermString<Borrowed>(v_.auth_method_data().substr(0, 8)))
          .step(bw::FixedInt<2>(v_.capabilities().to_ulong() & 0xffff));

      if ((v_.capabilities().to_ullong() >= (1 << 16)) ||
          v_.status_flags().any() || (v_.collation() != 0)) {
        accu.step(bw::FixedInt<1>(v_.collation()))
            .step(bw::FixedInt<2>(v_.status_flags().to_ulong()))
            .step(
                bw::FixedInt<2>((v_.capabilities().to_ulong() >> 16) & 0xffff))
            .step(bw::FixedInt<1>(auth_method_data_size))
            .step(bw::String<Borrowed>(std::string(10, '\0')));
        if (v_.capabilities()
                [classic_protocol::capabilities::pos::secure_connection]) {
          accu.step(bw::String<Borrowed>(v_.auth_method_data().substr(8)));
          if (v_.capabilities()
                  [classic_protocol::capabilities::pos::plugin_auth]) {
            accu.step(bw::NulTermString<Borrowed>(v_.auth_method_name()));
          }
        }
      }
      return accu.result();
    }
  }

 public:
  using value_type = borrowable::message::server::Greeting<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    // proto-version
    auto protocol_version_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (protocol_version_res->value() != 0x09 &&
        protocol_version_res->value() != 0x0a) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto version_res = accu.template step<bw::NulTermString<Borrowed>>();
    auto connection_id_res = accu.template step<bw::FixedInt<4>>();
    auto auth_method_data_res =
        accu.template step<bw::NulTermString<Borrowed>>();

    if (protocol_version_res->value() == 0x09) {
      return std::make_pair(
          accu.result().value(),
          value_type(protocol_version_res->value(), version_res->value(),
                     connection_id_res->value(), auth_method_data_res->value(),
                     0, 0, 0, {}));
    } else {
      // capabilities are split into two a lower-2-byte part and a
      // higher-2-byte
      auto cap_lower_res = accu.template step<bw::FixedInt<2>>();
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      // 3.21.x doesn't send more.
      if (buffer_size(buffer) <= accu.result().value()) {
        return std::make_pair(
            accu.result().value(),
            value_type(protocol_version_res->value(), version_res->value(),
                       connection_id_res->value(),
                       auth_method_data_res->value(), cap_lower_res->value(),
                       0x0, 0x0, {}));
      }

      // if there's more data
      auto collation_res = accu.template step<bw::FixedInt<1>>();
      auto status_flags_res = accu.template step<bw::FixedInt<2>>();
      auto cap_hi_res = accu.template step<bw::FixedInt<2>>();

      // before we use cap_hi|cap_low check they don't have an error
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      classic_protocol::capabilities::value_type capabilities(
          cap_lower_res->value() | (cap_hi_res->value() << 16));

      size_t auth_method_data_len{13};
      if (capabilities[classic_protocol::capabilities::pos::plugin_auth]) {
        auto auth_method_data_len_res = accu.template step<bw::FixedInt<1>>();
        if (!accu.result()) return stdx::unexpected(accu.result().error());

        // should be 21, but least 8
        if (auth_method_data_len_res->value() < 8) {
          return stdx::unexpected(make_error_code(codec_errc::invalid_input));
        }
        auth_method_data_len = auth_method_data_len_res->value() - 8;
      } else {
        accu.template step<void>(1);  // should be 0 ...
      }

      accu.template step<void>(10);  // skip the filler

      stdx::expected<bw::String<Borrowed>, std::error_code>
          auth_method_data_2_res;
      stdx::expected<bw::NulTermString<Borrowed>, std::error_code>
          auth_method_res;
      if (capabilities
              [classic_protocol::capabilities::pos::secure_connection]) {
        // auth-method-data
        auth_method_data_2_res =
            accu.template step<bw::String<Borrowed>>(auth_method_data_len);

        if (capabilities[classic_protocol::capabilities::pos::plugin_auth]) {
          // auth_method
          auth_method_res = accu.template step<bw::NulTermString<Borrowed>>();
        }
      }

      if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::AuthMethodSwitch<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::AuthMethodSwitch<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()));

    if (this->caps()[classic_protocol::capabilities::pos::plugin_auth]) {
      accu.step(bw::NulTermString<Borrowed>(v_.auth_method()))
          .step(bw::String<Borrowed>(v_.auth_method_data()));
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::server::AuthMethodSwitch<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0xfe; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::plugin_auth;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    // proto-version
    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    if (!caps[classic_protocol::capabilities::pos::plugin_auth]) {
      return std::make_pair(accu.result().value(), value_type());
    }

    auto auth_method_res = accu.template step<bw::NulTermString<Borrowed>>();
    auto auth_method_data_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::AuthMethodData<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::AuthMethodData<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::String<Borrowed>(v_.auth_method_data()))
        .result();
  }

 public:
  using value_type = borrowable::message::server::AuthMethodData<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0x01; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto auth_method_data_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(auth_method_data_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for server-side Ok message.
 */
template <bool Borrowed>
class Codec<borrowable::message::server::Ok<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::Ok<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::VarInt(v_.affected_rows()))
        .step(bw::VarInt(v_.last_insert_id()));

    if (this->caps()[capabilities::pos::protocol_41] ||
        this->caps()[capabilities::pos::transactions]) {
      accu.step(bw::FixedInt<2>(v_.status_flags().to_ulong()));
      if (this->caps().test(capabilities::pos::protocol_41)) {
        accu.step(bw::FixedInt<2>(v_.warning_count()));
      }
    }

    if (this->caps().test(capabilities::pos::session_track)) {
      accu.step(bw::VarString<Borrowed>(v_.message()));
      if (v_.status_flags().test(status::pos::session_state_changed)) {
        accu.step(bw::VarString<Borrowed>(v_.session_changes()));
      }
    } else {
      accu.step(bw::String<Borrowed>(v_.message()));
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::server::Ok<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0x00; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::session_track | capabilities::transactions |
           capabilities::protocol_41;
  }

  /**
   * decode a server::Ok message from a buffer-sequence.
   *
   * precondition:
   * - input starts with cmd_byte()
   *
   * @param buffer input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, message::server::Ok> on success, with bytes
   * processed
   * @retval codec_errc::invalid_input if preconditions aren't met
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto affected_rows_res = accu.template step<bw::VarInt>();
    auto last_insert_id_res = accu.template step<bw::VarInt>();

    stdx::expected<bw::FixedInt<2>, std::error_code> status_flags_res(0);
    stdx::expected<bw::FixedInt<2>, std::error_code> warning_count_res(0);
    if (caps[capabilities::pos::protocol_41] ||
        caps[capabilities::pos::transactions]) {
      status_flags_res = accu.template step<bw::FixedInt<2>>();
      if (caps[capabilities::pos::protocol_41]) {
        warning_count_res = accu.template step<bw::FixedInt<2>>();
      }
    }

    stdx::expected<bw::String<Borrowed>, std::error_code> message_res;
    stdx::expected<bw::VarString<Borrowed>, std::error_code>
        session_changes_res;
    if (caps[capabilities::pos::session_track]) {
      // if there is more data.
      const auto var_message_res =
          accu.template try_step<bw::VarString<Borrowed>>();
      if (var_message_res) {
        // set the message from the var-string
        message_res = var_message_res.value();
      }

      if (status_flags_res->value() &
          status::session_state_changed.to_ulong()) {
        session_changes_res = accu.template step<bw::VarString<Borrowed>>();
      }
    } else {
      message_res = accu.template step<bw::String<Borrowed>>();
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::Eof<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::Eof<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()));

    auto shared_caps = this->caps();

    if (shared_caps.test(
            capabilities::pos::text_result_with_session_tracking)) {
      accu.step(bw::VarInt(v_.affected_rows()))
          .step(bw::VarInt(v_.last_insert_id()));

      if (shared_caps[capabilities::pos::protocol_41] ||
          shared_caps[capabilities::pos::transactions]) {
        accu.step(bw::FixedInt<2>(v_.status_flags().to_ulong()));
        if (shared_caps[capabilities::pos::protocol_41]) {
          accu.step(bw::FixedInt<2>(v_.warning_count()));
        }
      }

      if (shared_caps[capabilities::pos::session_track]) {
        if (!v_.message().empty() ||
            v_.status_flags()[status::pos::session_state_changed]) {
          // only write message and session-changes if both of them aren't
          // empty.
          accu.step(bw::VarString<Borrowed>(v_.message()));
          if (v_.status_flags()[status::pos::session_state_changed]) {
            accu.step(bw::VarString<Borrowed>(v_.session_changes()));
          }
        }
      } else {
        accu.step(bw::String<Borrowed>(v_.message()));
      }
    } else if (shared_caps[capabilities::pos::protocol_41]) {
      accu.step(bw::FixedInt<2>(v_.warning_count()))
          .step(bw::FixedInt<2>(v_.status_flags().to_ulong()));
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::server::Eof<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0xfe; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::text_result_with_session_tracking |
           capabilities::session_track | capabilities::transactions |
           capabilities::protocol_41;
  }

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
   * @param buffer input buffser sequence
   * @param caps protocol capabilities
   *
   * @retval std::pair<size_t, message::server::Eof> on success, with bytes
   * processed
   * @retval codec_errc::invalid_input if preconditions aren't met
   * @retval codec_errc::not_enough_input not enough data to parse the whole
   * message
   */
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    const auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    if (caps[capabilities::pos::text_result_with_session_tracking]) {
      const auto affected_rows_res = accu.template step<bw::VarInt>();
      const auto last_insert_id_res = accu.template step<bw::VarInt>();

      stdx::expected<bw::FixedInt<2>, std::error_code> status_flags_res(0);
      stdx::expected<bw::FixedInt<2>, std::error_code> warning_count_res(0);
      if (caps[capabilities::pos::protocol_41] ||
          caps[capabilities::pos::transactions]) {
        status_flags_res = accu.template step<bw::FixedInt<2>>();
        if (caps[capabilities::pos::protocol_41]) {
          warning_count_res = accu.template step<bw::FixedInt<2>>();
        }
      }

      stdx::expected<bw::String<Borrowed>, std::error_code> message_res;
      stdx::expected<bw::VarString<Borrowed>, std::error_code>
          session_state_info_res;
      if (caps[capabilities::pos::session_track]) {
        // when session-track is supported, the 'message' part is a VarString.
        // But only if there is actually session-data and the message has data.
        const auto var_message_res =
            accu.template try_step<bw::VarString<Borrowed>>();
        if (var_message_res) {
          // set the message from the var-string
          message_res = var_message_res.value();
        }

        if (status_flags_res->value() &
            status::session_state_changed.to_ulong()) {
          session_state_info_res =
              accu.template step<bw::VarString<Borrowed>>();
        }
      } else {
        message_res = accu.template step<bw::String<Borrowed>>();
      }

      if (!accu.result()) return stdx::unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(affected_rows_res->value(), last_insert_id_res->value(),
                     status_flags_res->value(), warning_count_res->value(),
                     message_res->value(), session_state_info_res->value()));
    } else if (caps[capabilities::pos::protocol_41]) {
      const auto warning_count_res = accu.template step<bw::FixedInt<2>>();
      const auto status_flags_res = accu.template step<bw::FixedInt<2>>();

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
template <bool Borrowed>
class Codec<borrowable::message::server::Error<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::Error<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<2>(v_.error_code()));
    if (this->caps()[capabilities::pos::protocol_41]) {
      accu.step(bw::FixedInt<1>('#'))
          .step(bw::String<Borrowed>(v_.sql_state()));
    }

    return accu.step(bw::String<Borrowed>(v_.message())).result();
  }

 public:
  using value_type = borrowable::message::server::Error<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() { return 0xff; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::protocol_41;
  }

  static constexpr size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    // decode all fields, check result later before they are used.
    auto error_code_res = accu.template step<bw::FixedInt<2>>();
    stdx::expected<bw::String<Borrowed>, std::error_code> sql_state_res;
    if (caps[capabilities::pos::protocol_41]) {
      auto sql_state_hash_res = accu.template step<bw::FixedInt<1>>();
      if (!sql_state_hash_res) {
        return stdx::unexpected(sql_state_hash_res.error());
      }
      sql_state_res = accu.template step<bw::String<Borrowed>>(5);
    }
    auto message_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::server::ColumnCount>
    : public impl::EncodeBase<Codec<borrowable::message::server::ColumnCount>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::VarInt(v_.count())).result();
  }

 public:
  using value_type = borrowable::message::server::ColumnCount;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto count_res = accu.template step<bw::VarInt>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::ColumnMeta<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::ColumnMeta<Borrowed>>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    if (!this->caps()[capabilities::pos::protocol_41]) {
      accu.step(bw::VarString<Borrowed>(v_.table()))
          .step(bw::VarString<Borrowed>(v_.name()))
          .step(bw::VarInt(3))
          .step(bw::FixedInt<3>(v_.column_length()))
          .step(bw::VarInt(1))
          .step(bw::FixedInt<1>(v_.type()));

      if (this->caps()[capabilities::pos::long_flag]) {
        accu.step(bw::VarInt(3))
            .step(bw::FixedInt<2>(v_.flags().to_ulong()))
            .step(bw::FixedInt<1>(v_.decimals()));
      } else {
        accu.step(bw::VarInt(2))
            .step(bw::FixedInt<1>(v_.flags().to_ulong()))
            .step(bw::FixedInt<1>(v_.decimals()));
      }

      return accu.result();
    } else {
      return accu.step(bw::VarString<Borrowed>(v_.catalog()))
          .step(bw::VarString<Borrowed>(v_.schema()))
          .step(bw::VarString<Borrowed>(v_.table()))
          .step(bw::VarString<Borrowed>(v_.orig_table()))
          .step(bw::VarString<Borrowed>(v_.name()))
          .step(bw::VarString<Borrowed>(v_.orig_name()))
          .step(bw::VarInt(12))
          .step(bw::FixedInt<2>(v_.collation()))
          .step(bw::FixedInt<4>(v_.column_length()))
          .step(bw::FixedInt<1>(v_.type()))
          .step(bw::FixedInt<2>(v_.flags().to_ulong()))
          .step(bw::FixedInt<1>(v_.decimals()))
          .step(bw::FixedInt<2>(0))
          .result();
    }
  }

 public:
  using value_type = borrowable::message::server::ColumnMeta<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    if (!caps[capabilities::pos::protocol_41]) {
      // 3.2x protocol used up to 4.0.x

      // bit-size of the 'flags' field
      const uint8_t flags_size = caps[capabilities::pos::long_flag] ? 2 : 1;

      const auto table_res = accu.template step<bw::VarString<Borrowed>>();
      const auto name_res = accu.template step<bw::VarString<Borrowed>>();

      const auto column_length_len_res = accu.template step<bw::VarInt>();
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      if (column_length_len_res->value() != 3) {
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      const auto column_length_res = accu.template step<bw::FixedInt<3>>();
      const auto type_len_res = accu.template step<bw::VarInt>();
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      if (type_len_res->value() != 1) {
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      const auto type_res = accu.template step<bw::FixedInt<1>>();
      const auto flags_and_decimals_len_res = accu.template step<bw::VarInt>();
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      if (flags_and_decimals_len_res->value() != flags_size + 1) {
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      stdx::expected<bw::FixedInt<3>, std::error_code> flags_and_decimals_res(
          0);
      if (flags_size == 2) {
        flags_and_decimals_res = accu.template step<bw::FixedInt<3>>();
      } else {
        const auto small_flags_and_decimals_res =
            accu.template step<bw::FixedInt<2>>();
        if (small_flags_and_decimals_res) {
          flags_and_decimals_res =
              bw::FixedInt<3>(small_flags_and_decimals_res->value());
        }
      }

      if (!accu.result()) return stdx::unexpected(accu.result().error());

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
      const auto catalog_res = accu.template step<bw::VarString<Borrowed>>();
      const auto schema_res = accu.template step<bw::VarString<Borrowed>>();
      const auto table_res = accu.template step<bw::VarString<Borrowed>>();
      const auto orig_table_res = accu.template step<bw::VarString<Borrowed>>();
      const auto name_res = accu.template step<bw::VarString<Borrowed>>();
      const auto orig_name_res = accu.template step<bw::VarString<Borrowed>>();

      /* next is a collection of fields which is wrapped inside a varstring of
       * 12-bytes size */
      const auto other_len_res = accu.template step<bw::VarInt>();

      if (other_len_res->value() != 12) {
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      const auto collation_res = accu.template step<bw::FixedInt<2>>();
      const auto column_length_res = accu.template step<bw::FixedInt<4>>();
      const auto type_res = accu.template step<bw::FixedInt<1>>();
      const auto flags_res = accu.template step<bw::FixedInt<2>>();
      const auto decimals_res = accu.template step<bw::FixedInt<1>>();

      accu.template step<void>(2);  // fillers

      if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::SendFileRequest<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::SendFileRequest<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::String<Borrowed>(v_.filename()))
        .result();
  }

 public:
  using value_type = borrowable::message::server::SendFileRequest<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static constexpr uint8_t cmd_byte() noexcept { return 0xfb; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto filename_res = accu.template step<bw::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::server::StmtPrepareOk>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::StmtPrepareOk>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.statement_id()))
        .step(bw::FixedInt<2>(v_.column_count()))
        .step(bw::FixedInt<2>(v_.param_count()))
        .step(bw::FixedInt<1>(0))
        .step(bw::FixedInt<2>(v_.warning_count()));

    if (caps()[capabilities::pos::optional_resultset_metadata]) {
      accu.step(bw::FixedInt<1>(v_.with_metadata()));
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::server::StmtPrepareOk;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept { return 0x00; }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::optional_resultset_metadata;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!cmd_byte_res) return stdx::unexpected(cmd_byte_res.error());
    auto stmt_id_res = accu.template step<bw::FixedInt<4>>();
    auto column_count_res = accu.template step<bw::FixedInt<2>>();
    auto param_count_res = accu.template step<bw::FixedInt<2>>();
    auto filler_res = accu.template step<bw::FixedInt<1>>();
    if (!filler_res) return stdx::unexpected(filler_res.error());
    auto warning_count_res = accu.template step<bw::FixedInt<2>>();

    // by default, metadata isn't optional
    int8_t with_metadata{1};
    if (caps[capabilities::pos::optional_resultset_metadata]) {
      auto with_metadata_res = accu.template step<bw::FixedInt<1>>();

      if (with_metadata_res) {
        with_metadata = with_metadata_res->value();
      }
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::Row<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::Row<Borrowed>>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    for (const auto &field : v_) {
      if (field) {
        accu.step(bw::VarString<Borrowed>(*field));
      } else {
        accu.step(bw::Null());
      }
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::server::Row<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    std::vector<typename value_type::value_type> fields;

    const size_t buf_size = buffer_size(buffer);

    while (accu.result() && (accu.result().value() < buf_size)) {
      // field may other be a Null or a VarString
      auto null_res = accu.template try_step<bw::Null>();
      if (null_res) {
        fields.emplace_back(std::nullopt);
      } else {
        auto field_res = accu.template step<bw::VarString<Borrowed>>();
        if (!field_res) return stdx::unexpected(field_res.error());

        fields.emplace_back(field_res->value());
      }
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::server::StmtRow<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::StmtRow<Borrowed>>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(0));

    // null-bitmap starts with a 2-bit offset
    size_t bit_pos{2};
    uint8_t null_bit_byte{};

    for (auto const &field : v_) {
      if (!field) null_bit_byte |= (1 << bit_pos);

      if (++bit_pos > 7) {
        accu.step(bw::FixedInt<1>(null_bit_byte));

        bit_pos = 0;
        null_bit_byte = 0;
      }
    }

    if (bit_pos != 0) accu.step(bw::FixedInt<1>(null_bit_byte));

    for (auto [n, field] : stdx::views::enumerate(v_)) {
      if (!field) continue;

      switch (v_.types()[n]) {
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
          accu.step(bw::VarInt(field->size()));
          break;
        case field_type::Date:
        case field_type::DateTime:
        case field_type::Timestamp:
        case field_type::Time:
          accu.step(bw::FixedInt<1>(field->size()));
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
      accu.step(borrowed::wire::String(*field));
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::server::StmtRow<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static size_t max_size() noexcept {
    return std::numeric_limits<size_t>::max();
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps,
      std::vector<field_type::value_type> types) {
    namespace bw = borrowable::wire;

    impl::DecodeBufferAccumulator accu(buffer, caps);

    const auto row_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    // first byte is 0x00
    if (row_byte_res->value() != 0x00) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    const auto nullbits_res =
        accu.template step<bw::String<Borrowed>>(bytes_per_bits(types.size()));
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    const auto nullbits = nullbits_res->value();

    std::vector<typename value_type::value_type> values;

    for (size_t n{}, bit_pos{2}, byte_pos{}; n < types.size(); ++n, ++bit_pos) {
      if (bit_pos > 7) {
        bit_pos = 0;
        ++byte_pos;
      }

      if (!(nullbits[byte_pos] & (1 << bit_pos))) {
        stdx::expected<size_t, std::error_code> field_size_res(
            stdx::unexpected(make_error_code(std::errc::invalid_argument)));
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
            auto string_field_size_res = accu.template step<bw::VarInt>();
            if (!accu.result()) return stdx::unexpected(accu.result().error());

            field_size_res = string_field_size_res->value();
          } break;
          case field_type::Date:
          case field_type::DateTime:
          case field_type::Timestamp:
          case field_type::Time: {
            auto time_field_size_res = accu.template step<bw::FixedInt<1>>();
            if (!accu.result()) return stdx::unexpected(accu.result().error());

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

        if (!field_size_res) {
          return stdx::unexpected(
              make_error_code(codec_errc::field_type_unknown));
        }

        const auto value_res =
            accu.template step<bw::String<Borrowed>>(field_size_res.value());
        if (!accu.result()) return stdx::unexpected(accu.result().error());

        values.push_back(value_res->value());
      } else {
        values.emplace_back(std::nullopt);
      }
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(), value_type(types, values));
  }

 private:
  const value_type v_;
};

/**
 * codec for server::Statistics message.
 */
template <bool Borrowed>
class Codec<borrowable::message::server::Statistics<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::server::Statistics<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::String<Borrowed>(v_.stats())).result();
  }

 public:
  using value_type = borrowable::message::server::Statistics<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto stats_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(Base::cmd_byte())).result();
  }

 public:
  using __base = impl::EncodeBase<CodecSimpleCommand<Base, ValueType>>;

  friend __base;

  constexpr CodecSimpleCommand(capabilities::value_type caps) : __base(caps) {}

  static constexpr size_t max_size() noexcept { return 1; }

  static stdx::expected<std::pair<size_t, ValueType>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != Base::cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
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
class Codec<borrowable::message::client::Quit>
    : public CodecSimpleCommand<Codec<borrowable::message::client::Quit>,
                                borrowable::message::client::Quit> {
 public:
  using value_type = borrowable::message::client::Quit;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Quit);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }
};

/**
 * codec for client's ResetConnection command.
 */
template <>
class Codec<borrowable::message::client::ResetConnection>
    : public CodecSimpleCommand<
          Codec<borrowable::message::client::ResetConnection>,
          borrowable::message::client::ResetConnection> {
 public:
  using value_type = borrowable::message::client::ResetConnection;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ResetConnection);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }
};

/**
 * codec for client's Ping command.
 */
template <>
class Codec<borrowable::message::client::Ping>
    : public CodecSimpleCommand<Codec<borrowable::message::client::Ping>,
                                borrowable::message::client::Ping> {
 public:
  using value_type = borrowable::message::client::Ping;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Ping);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }
};

/**
 * codec for client's Statistics command.
 */
template <>
class Codec<borrowable::message::client::Statistics>
    : public CodecSimpleCommand<Codec<borrowable::message::client::Statistics>,
                                borrowable::message::client::Statistics> {
 public:
  using value_type = borrowable::message::client::Statistics;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Statistics);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }
};

/**
 * codec for client's Debug command.
 */
template <>
class Codec<borrowable::message::client::Debug>
    : public CodecSimpleCommand<Codec<borrowable::message::client::Debug>,
                                borrowable::message::client::Debug> {
 public:
  using value_type = borrowable::message::client::Debug;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Debug);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }
};

/**
 * codec for client's InitSchema command.
 */
template <bool Borrowed>
class Codec<borrowable::message::client::InitSchema<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::InitSchema<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::String<Borrowed>(v_.schema()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::InitSchema<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::InitSchema);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto schema_res = accu.template step<bw::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(schema_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Query command.
 */
template <bool Borrowed>
class Codec<borrowable::message::client::Query<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::Query<Borrowed>>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()));

    auto caps = this->caps();

    if (caps.test(capabilities::pos::query_attributes)) {
      uint64_t param_count = v_.values().size();
      accu.step(bw::VarInt(param_count));  // param_count
      accu.step(bw::VarInt(1));            // param_set_count: always 1

      if (param_count > 0) {
        // mark all that are NULL in the nullbits
        //
        // - one bit per parameter to send
        // - if a parameter is NULL, the bit is set, and later no value is
        // added.

        uint8_t null_bit_byte{};
        int bit_pos{};

        for (auto const &param : v_.values()) {
          if (!param.value) null_bit_byte |= 1 << bit_pos;

          if (++bit_pos > 7) {
            accu.step(bw::FixedInt<1>(null_bit_byte));

            bit_pos = 0;
            null_bit_byte = 0;
          }
        }

        if (bit_pos != 0) accu.step(bw::FixedInt<1>(null_bit_byte));

        accu.step(bw::FixedInt<1>(1));  // new_param_bind_flag: always 1

        for (const auto &param : v_.values()) {
          accu.step(bw::FixedInt<2>(param.type_and_flags))
              .step(bw::VarString<Borrowed>(param.name));
        }

        for (const auto &param : v_.values()) {
          if (!param.value) continue;

          auto type = param.type_and_flags & 0xff;
          switch (type) {
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
            case field_type::Json:
            case field_type::NewDecimal:
            case field_type::Geometry:
              accu.template step<bw::VarInt>(param.value->size());
              break;
            case field_type::Date:
            case field_type::DateTime:
            case field_type::Timestamp:
            case field_type::Time:
              assert(param.value->size() <= 255);

              accu.template step<bw::FixedInt<1>>(param.value->size());

              break;
            case field_type::LongLong:
            case field_type::Double:
              assert(param.value->size() == 8);

              break;
            case field_type::Long:
            case field_type::Int24:
            case field_type::Float:
              assert(param.value->size() == 4);

              break;
            case field_type::Short:
            case field_type::Year:
              assert(param.value->size() == 2);

              break;
            case field_type::Tiny:
              assert(param.value->size() == 1);

              break;
            default:
              assert(true || "unknown field-type");
              break;
          }
          accu.template step<bw::String<Borrowed>>(*param.value);
        }
      }
    }

    accu.step(bw::String<Borrowed>(v_.statement()));
    return accu.result();
  }

  template <class Accu>
  static stdx::expected<size_t, std::error_code> decode_field_size(
      Accu &accu, uint8_t type) {
    namespace bw = borrowable::wire;

    switch (type) {
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
      case field_type::Json:
      case field_type::NewDecimal:
      case field_type::Geometry: {
        auto string_field_size_res = accu.template step<bw::VarInt>();
        if (!accu.result()) {
          return stdx::unexpected(accu.result().error());
        }

        return string_field_size_res->value();
      }
      case field_type::Date:
      case field_type::DateTime:
      case field_type::Timestamp:
      case field_type::Time: {
        auto time_field_size_res = accu.template step<bw::FixedInt<1>>();
        if (!accu.result()) {
          return stdx::unexpected(accu.result().error());
        }

        return time_field_size_res->value();
      }
      case field_type::LongLong:
      case field_type::Double:
        return 8;
      case field_type::Long:
      case field_type::Int24:
      case field_type::Float:
        return 4;
      case field_type::Short:
      case field_type::Year:
        return 2;
      case field_type::Tiny:
        return 1;
    }

    return stdx::unexpected(make_error_code(codec_errc::invalid_input));
  }

 public:
  using value_type = borrowable::message::client::Query<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Query);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::query_attributes;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    std::vector<typename value_type::Param> params;
    if (caps.test(capabilities::pos::query_attributes)) {
      //
      auto param_count_res = accu.template step<bw::VarInt>();
      if (!param_count_res) return stdx::unexpected(param_count_res.error());

      // currently always 1.
      auto param_set_count_res = accu.template step<bw::VarInt>();
      if (!param_set_count_res)
        return stdx::unexpected(param_set_count_res.error());

      if (param_set_count_res->value() != 1) {
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      const auto param_count = param_count_res->value();
      if (param_count > 0) {
        // bit-map
        const auto nullbits_res = accu.template step<bw::String<Borrowed>>(
            bytes_per_bits(param_count));
        if (!accu.result()) return stdx::unexpected(accu.result().error());

        const auto nullbits = nullbits_res->value();

        // always 1
        auto new_params_bound_res = accu.template step<bw::FixedInt<1>>();
        if (!accu.result()) return stdx::unexpected(accu.result().error());

        auto new_params_bound = new_params_bound_res->value();
        if (new_params_bound != 1) {
          // Always 1, malformed packet error of not 1
          return stdx::unexpected(make_error_code(codec_errc::invalid_input));
        }

        // redundant, but protocol-docs says:
        //
        //   'if new-params-bind-flag == 1'
        //
        // therefore keep it.
        if (new_params_bound == 1) {
          for (long n{}; n < param_count; ++n) {
            auto param_type_res = accu.template step<bw::FixedInt<2>>();
            if (!accu.result()) {
              return stdx::unexpected(accu.result().error());
            }
            auto param_name_res = accu.template step<bw::VarString<Borrowed>>();
            if (!accu.result()) {
              return stdx::unexpected(accu.result().error());
            }

            params.emplace_back(param_type_res->value(),
                                param_name_res->value(),
                                std::optional<std::string>());
          }
        }

        for (long n{}, nullbit_pos{}, nullbyte_pos{}; n < param_count;
             ++n, ++nullbit_pos) {
          if (nullbit_pos > 7) {
            ++nullbyte_pos;
            nullbit_pos = 0;
          }

          if (!(nullbits[nullbyte_pos] & (1 << nullbit_pos))) {
            auto &param = params[n];

            auto field_size_res =
                decode_field_size(accu, param.type_and_flags & 0xff);
            if (!field_size_res) {
              return stdx::unexpected(field_size_res.error());
            }

            auto param_value_res = accu.template step<bw::String<Borrowed>>(
                field_size_res.value());
            if (!accu.result()) {
              return stdx::unexpected(accu.result().error());
            }

            param.value = param_value_res->value();
          }
        }
      }
    }

    auto statement_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(statement_res->value(), std::move(params)));
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
template <bool Borrowed>
class Codec<borrowable::message::client::SendFile<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::SendFile<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::String<Borrowed>(v_.payload())).result();
  }

 public:
  using value_type = borrowable::message::client::SendFile<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto payload_res = accu.template step<bw::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(payload_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's ListFields command.
 */
template <bool Borrowed>
class Codec<borrowable::message::client::ListFields<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::ListFields<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::NulTermString<Borrowed>(v_.table_name()))
        .step(bw::String<Borrowed>(v_.wildcard()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::ListFields<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ListFields);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto table_name_res = accu.template step<bw::NulTermString<Borrowed>>();
    auto wildcard_res = accu.template step<bw::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::Reload>
    : public impl::EncodeBase<Codec<borrowable::message::client::Reload>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<1>(v_.cmds().to_ulong()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::Reload;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Refresh);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto cmds_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::Kill>
    : public impl::EncodeBase<Codec<borrowable::message::client::Kill>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.connection_id()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::Kill;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ProcessKill);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto connection_id_res = accu.template step<bw::FixedInt<4>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(connection_id_res->value()));
  }

 private:
  value_type v_;
};

/**
 * codec for client's Prepared Statement command.
 */
template <bool Borrowed>
class Codec<borrowable::message::client::StmtPrepare<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::StmtPrepare<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::String<Borrowed>(v_.statement()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::StmtPrepare<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtPrepare);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_res = accu.template step<bw::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(statement_res->value()));
  }

 private:
  const value_type v_;
};

/**
 * codec for client's Execute Statement command.
 */
template <bool Borrowed>
class Codec<borrowable::message::client::StmtExecute<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::StmtExecute<Borrowed>>> {
  template <class Accumulator>
  auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    auto caps = this->caps();

    accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.statement_id()))
        .step(bw::FixedInt<1>(v_.flags().to_ullong()))
        .step(bw::FixedInt<4>(v_.iteration_count()));

    // num-params from the StmtPrepareOk
    auto num_params = v_.values().size();

    const bool supports_query_attributes =
        caps.test(capabilities::pos::query_attributes);

    if (supports_query_attributes &&
        v_.flags().test(cursor::pos::param_count_available)) {
      accu.step(bw::VarInt(num_params));
    }

    if (num_params == 0) return accu.result();

    // mark all that are NULL in the nullbits
    //
    // - one bit per parameter to send
    // - if a parameter is NULL, the bit is set, and later no value is added.
    uint8_t null_bit_byte{};
    int bit_pos{};

    for (auto const &param : v_.values()) {
      if (!param.has_value()) null_bit_byte |= 1 << bit_pos;

      if (++bit_pos > 7) {
        accu.step(bw::FixedInt<1>(null_bit_byte));

        bit_pos = 0;
        null_bit_byte = 0;
      }
    }

    if (bit_pos != 0) accu.step(bw::FixedInt<1>(null_bit_byte));

    accu.step(bw::FixedInt<1>(v_.new_params_bound()));

    if (v_.new_params_bound()) {
      for (const auto &param_def : v_.types()) {
        accu.step(bw::FixedInt<2>(param_def.type_and_flags));

        if (supports_query_attributes) {
          accu.step(borrowed::wire::VarString(param_def.name));
        }
      }
    }

    for (auto [n, v] : stdx::views::enumerate(v_.values())) {
      // add all the values that aren't NULL
      if (!v.has_value()) continue;

      // write length of the type is a variable length
      switch (v_.types()[n].type_and_flags & 0xff) {
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
          accu.step(bw::VarInt(v->size()));
          break;
        case field_type::Date:
        case field_type::DateTime:
        case field_type::Timestamp:
        case field_type::Time:
          accu.step(bw::FixedInt<1>(v->size()));
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
        default:
          assert(false || "Unknown Type");
      }
      accu.step(borrowed::wire::String(v.value()));
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::client::StmtExecute<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type val, capabilities::value_type caps)
      : __base(caps), v_{std::move(val)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtExecute);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::query_attributes;
  }

  /**
   * decode a buffer into a message::client::StmtExecute.
   *
   * @param buffer a buffer
   * @param caps protocol capabilities
   * @param metadata_lookup callable that expects a 'uint32_t statement_id'
   * that returns a result that's convertible to
   * 'stdx::expected<std::vector<ParamDef>, std::error_code>' representing the
   * parameter-definitions of the prepared statement
   *
   * decoding a StmtExecute message requires the parameter-definitions of the
   * prepared statement. The metadata_lookup function may be called to get
   * the parameter-definitions for the statement-id.
   *
   * The function may return a parameter-definitions directly
   *
   * \code
   * StmtExecute::decode(
   *   buffers,
   *   capabilities::protocol_41,
   *   [](uint32_t stmt_id) { return std::vector<ParamDef>{}; });
   * \endcode
   *
   * ... or a stdx::expected<std::vector<ParamDef>, std::error_code> if it wants
   * to signal that a statement-id wasn't found
   *
   * \code
   * StmtExecute::decode(
   *   buffers,
   *   capabilities::protocol_41,
   *   [](uint32_t stmt_id) ->
   *       stdx::expected<std::vector<ParamDef>, std::error_code> {
   *     bool found{true};
   *
   *     if (found) {
   *       return {};
   *     } else {
   *       return stdx::unexpected(make_error_code(
   *         codec_errc::statement_id_not_found));
   *     }
   *   });
   * \endcode
   */
  template <class Func>
  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps,
      Func &&metadata_lookup) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<bw::FixedInt<4>>();
    auto flags_res = accu.template step<bw::FixedInt<1>>();
    auto iteration_count_res = accu.template step<bw::FixedInt<4>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    const auto param_count_available{1 << cursor::pos::param_count_available};

    const bool supports_query_attributes =
        caps.test(capabilities::pos::query_attributes);

    stdx::expected<std::vector<typename value_type::ParamDef>, std::error_code>
        metadata_res = metadata_lookup(statement_id_res->value());
    if (!metadata_res) {
      return stdx::unexpected(
          make_error_code(codec_errc::statement_id_not_found));
    }

    size_t param_count = metadata_res->size();

    if (supports_query_attributes &&
        (flags_res->value() & param_count_available) != 0) {
      auto param_count_res = accu.template step<bw::VarInt>();
      if (!accu.result()) {
        return stdx::unexpected(accu.result().error());
      }

      if (static_cast<uint64_t>(param_count_res->value()) < param_count) {
        // can the param-count shrink?
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      param_count = param_count_res->value();
    }

    if (param_count == 0) {
      return std::make_pair(
          accu.result().value(),
          value_type(statement_id_res->value(), flags_res->value(),
                     iteration_count_res->value(), false, {}, {}));
    }

    auto nullbits_res =
        accu.template step<bw::String<Borrowed>>(bytes_per_bits(param_count));
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto new_params_bound_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    std::vector<typename value_type::ParamDef> types;

    auto new_params_bound = new_params_bound_res->value();
    if (new_params_bound == 0) {
      // no new params, use the last known params.
      types = *metadata_res;
    } else if (new_params_bound == 1) {
      // check that there is at least enough data for the types (a FixedInt<2>)
      // before reserving memory.
      if (param_count >= buffer.size() / 2) {
        return stdx::unexpected(make_error_code(codec_errc::invalid_input));
      }

      types.reserve(param_count);

      for (size_t n{}; n < param_count; ++n) {
        auto type_res = accu.template step<bw::FixedInt<2>>();
        if (!accu.result()) return stdx::unexpected(accu.result().error());

        if (supports_query_attributes) {
          auto name_res = accu.template step<bw::VarString<Borrowed>>();
          if (!accu.result()) {
            return stdx::unexpected(accu.result().error());
          }
          types.emplace_back(type_res->value(), name_res->value());
        } else {
          types.emplace_back(type_res->value(), "");
        }
      }
    } else {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    if (param_count != types.size()) {
      // param-count and available types doesn't match.
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    std::vector<std::optional<typename value_type::string_type>> values;
    values.reserve(param_count);

    const auto nullbits = nullbits_res->value();
    for (size_t n{}, bit_pos{}, byte_pos{}; n < param_count; ++n, ++bit_pos) {
      if (bit_pos > 7) {
        bit_pos = 0;
        ++byte_pos;
      }

      // if the data was sent via COM_STMT_SEND_LONG_DATA, there will be no data
      // for
      const auto param_already_sent =
          n < metadata_res->size() ? (*metadata_res)[n].param_already_sent
                                   : false;

      if (param_already_sent) {
        values.emplace_back("");  // empty
      } else if (!(nullbits[byte_pos] & (1 << bit_pos))) {
        stdx::expected<size_t, std::error_code> field_size_res(
            stdx::unexpected(make_error_code(std::errc::invalid_argument)));
        switch (types[n].type_and_flags & 0xff) {
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
            auto string_field_size_res = accu.template step<bw::VarInt>();
            if (!accu.result()) {
              return stdx::unexpected(accu.result().error());
            }

            field_size_res = string_field_size_res->value();
          } break;
          case field_type::Date:
          case field_type::DateTime:
          case field_type::Timestamp:
          case field_type::Time: {
            auto time_field_size_res = accu.template step<bw::FixedInt<1>>();
            if (!accu.result()) {
              return stdx::unexpected(accu.result().error());
            }

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

        if (!field_size_res) {
          return stdx::unexpected(
              make_error_code(codec_errc::field_type_unknown));
        }

        auto value_res =
            accu.template step<bw::String<Borrowed>>(field_size_res.value());
        if (!accu.result()) {
          return stdx::unexpected(accu.result().error());
        }

        values.push_back(value_res->value());
      } else {
        // NULL
        values.emplace_back(std::nullopt);
      }
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::client::StmtParamAppendData<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::StmtParamAppendData<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.statement_id()))
        .step(bw::FixedInt<2>(v_.param_id()))
        .step(bw::String<Borrowed>(v_.data()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::StmtParamAppendData<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtSendLongData);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<bw::FixedInt<4>>();
    auto param_id_res = accu.template step<bw::FixedInt<2>>();
    auto data_res = accu.template step<bw::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::StmtClose>
    : public impl::EncodeBase<Codec<borrowable::message::client::StmtClose>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.statement_id()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::StmtClose;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtClose);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<bw::FixedInt<4>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::StmtReset>
    : public impl::EncodeBase<Codec<borrowable::message::client::StmtReset>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.statement_id()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::StmtReset;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtReset);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<bw::FixedInt<4>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::SetOption>
    : public impl::EncodeBase<Codec<borrowable::message::client::SetOption>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<2>(v_.option()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::SetOption;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::SetOption);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto option_res = accu.template step<bw::FixedInt<2>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::StmtFetch>
    : public impl::EncodeBase<Codec<borrowable::message::client::StmtFetch>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.statement_id()))
        .step(bw::FixedInt<4>(v_.row_count()))
        .result();
  }

 public:
  using value_type = borrowable::message::client::StmtFetch;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::StmtFetch);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }

    auto statement_id_res = accu.template step<bw::FixedInt<4>>();
    auto row_count_res = accu.template step<bw::FixedInt<4>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::client::Greeting<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::Greeting<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    const auto shared_caps = v_.capabilities() & this->caps();

    if (shared_caps[classic_protocol::capabilities::pos::protocol_41]) {
      accu.step(bw::FixedInt<4>(v_.capabilities().to_ulong()))
          .step(bw::FixedInt<4>(v_.max_packet_size()))
          .step(bw::FixedInt<1>(v_.collation()))
          .step(bw::String<Borrowed>(std::string(23, '\0')));
      if (!(shared_caps[classic_protocol::capabilities::pos::ssl] &&
            v_.username().empty())) {
        // the username is empty and SSL is set, this is a short SSL-greeting
        // packet
        accu.step(bw::NulTermString<Borrowed>(v_.username()));

        if (shared_caps[classic_protocol::capabilities::pos::
                            client_auth_method_data_varint]) {
          accu.step(bw::VarString<Borrowed>(v_.auth_method_data()));
        } else if (shared_caps[classic_protocol::capabilities::pos::
                                   secure_connection]) {
          accu.step(bw::FixedInt<1>(v_.auth_method_data().size()))
              .step(bw::String<Borrowed>(v_.auth_method_data()));
        } else {
          accu.step(bw::NulTermString<Borrowed>(v_.auth_method_data()));
        }

        if (shared_caps
                [classic_protocol::capabilities::pos::connect_with_schema]) {
          accu.step(bw::NulTermString<Borrowed>(v_.schema()));
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
            accu.step(bw::NulTermString<Borrowed>(v_.auth_method_name()));
          }
        } else {
          if (shared_caps[classic_protocol::capabilities::pos::plugin_auth]) {
            accu.step(bw::NulTermString<Borrowed>(v_.auth_method_name()));
          }

          accu.step(bw::VarString<Borrowed>(v_.attributes()));
        }
      }
    } else {
      accu.step(bw::FixedInt<2>(v_.capabilities().to_ulong()))
          .step(bw::FixedInt<3>(v_.max_packet_size()))
          .step(bw::NulTermString<Borrowed>(v_.username()));
      if (shared_caps
              [classic_protocol::capabilities::pos::connect_with_schema]) {
        accu.step(bw::NulTermString<Borrowed>(v_.auth_method_data()))
            .step(bw::String<Borrowed>(v_.schema()));
      } else {
        accu.step(bw::String<Borrowed>(v_.auth_method_data()));
      }
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::client::Greeting<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::secure_connection | capabilities::protocol_41 |
           capabilities::ssl | capabilities::client_auth_method_data_varint |
           capabilities::connect_attributes | capabilities::connect_with_schema;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto capabilities_lo_res = accu.template step<bw::FixedInt<2>>();
    if (!capabilities_lo_res)
      return stdx::unexpected(capabilities_lo_res.error());

    auto client_capabilities = classic_protocol::capabilities::value_type(
        capabilities_lo_res->value());

    // decoding depends on the capabilities that both client and server have
    // in common
    auto shared_capabilities = caps & client_capabilities;

    if (shared_capabilities[classic_protocol::capabilities::pos::protocol_41]) {
      // if protocol_41 is set in the capabilities, we expected 2 more bytes
      // of capabilities
      auto capabilities_hi_res = accu.template step<bw::FixedInt<2>>();
      if (!capabilities_hi_res)
        return stdx::unexpected(capabilities_hi_res.error());

      client_capabilities |= classic_protocol::capabilities::value_type(
          capabilities_hi_res->value() << 16);

      shared_capabilities = caps & client_capabilities;

      auto max_packet_size_res = accu.template step<bw::FixedInt<4>>();
      auto collation_res = accu.template step<bw::FixedInt<1>>();

      accu.template step<bw::String<Borrowed>>(23);  // skip 23 bytes

      auto last_accu_res = accu.result();

      auto username_res = accu.template step<bw::NulTermString<Borrowed>>();
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

        return stdx::unexpected(accu.result().error());
      }

      // auth-method-data is either
      //
      // - varint length
      // - fixed-int-1 length
      // - null-term-string
      stdx::expected<bw::String<Borrowed>, std::error_code>
          auth_method_data_res;
      if (shared_capabilities[classic_protocol::capabilities::pos::
                                  client_auth_method_data_varint]) {
        auto res = accu.template step<bw::VarString<Borrowed>>();
        if (!res) return stdx::unexpected(res.error());

        auth_method_data_res = bw::String<Borrowed>(res->value());
      } else if (shared_capabilities
                     [classic_protocol::capabilities::pos::secure_connection]) {
        auto auth_method_data_len_res = accu.template step<bw::FixedInt<1>>();
        if (!auth_method_data_len_res)
          return stdx::unexpected(auth_method_data_len_res.error());
        auto auth_method_data_len = auth_method_data_len_res->value();

        auto res =
            accu.template step<bw::String<Borrowed>>(auth_method_data_len);
        if (!res) return stdx::unexpected(res.error());

        auth_method_data_res = bw::String<Borrowed>(res->value());
      } else {
        auto res = accu.template step<bw::NulTermString<Borrowed>>();
        if (!res) return stdx::unexpected(res.error());

        auth_method_data_res = bw::String<Borrowed>(res->value());
      }

      stdx::expected<bw::NulTermString<Borrowed>, std::error_code> schema_res;
      if (shared_capabilities
              [classic_protocol::capabilities::pos::connect_with_schema]) {
        schema_res = accu.template step<bw::NulTermString<Borrowed>>();
      }
      if (!schema_res) return stdx::unexpected(schema_res.error());

      stdx::expected<bw::NulTermString<Borrowed>, std::error_code>
          auth_method_res;
      if (shared_capabilities
              [classic_protocol::capabilities::pos::plugin_auth]) {
        if (net::buffer_size(buffer) == accu.result().value()) {
          // even with plugin_auth set, the server is fine, if no
          // auth_method_name is sent.
          auth_method_res = bw::NulTermString<Borrowed>{};
        } else {
          auth_method_res = accu.template step<bw::NulTermString<Borrowed>>();
        }
      }
      if (!auth_method_res) return stdx::unexpected(auth_method_res.error());

      stdx::expected<bw::VarString<Borrowed>, std::error_code> attributes_res;
      if (shared_capabilities
              [classic_protocol::capabilities::pos::connect_attributes]) {
        attributes_res = accu.template step<bw::VarString<Borrowed>>();
      }

      if (!accu.result()) return stdx::unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(client_capabilities, max_packet_size_res->value(),
                     collation_res->value(), username_res->value(),
                     auth_method_data_res->value(), schema_res->value(),
                     auth_method_res->value(), attributes_res->value()));

    } else {
      auto max_packet_size_res = accu.template step<bw::FixedInt<3>>();

      auto username_res = accu.template step<bw::NulTermString<Borrowed>>();

      stdx::expected<bw::String<Borrowed>, std::error_code>
          auth_method_data_res;
      stdx::expected<bw::String<Borrowed>, std::error_code> schema_res;

      if (shared_capabilities
              [classic_protocol::capabilities::pos::connect_with_schema]) {
        auto res = accu.template step<bw::NulTermString<Borrowed>>();
        if (!res) return stdx::unexpected(res.error());

        // auth_method_data is a wire::String, move it over
        auth_method_data_res = bw::String<Borrowed>(res->value());

        schema_res = accu.template step<bw::String<Borrowed>>();
      } else {
        auth_method_data_res = accu.template step<bw::String<Borrowed>>();
      }

      if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::client::AuthMethodData<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::AuthMethodData<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::String<Borrowed>(v_.auth_method_data())).result();
  }

 public:
  using value_type = borrowable::message::client::AuthMethodData<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto auth_method_data_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::client::ChangeUser<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::ChangeUser<Borrowed>>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::NulTermString<Borrowed>(v_.username()));

    if (this->caps()[classic_protocol::capabilities::pos::secure_connection]) {
      accu.step(bw::FixedInt<1>(v_.auth_method_data().size()))
          .step(bw::String<Borrowed>(v_.auth_method_data()));
    } else {
      accu.step(bw::NulTermString<Borrowed>(v_.auth_method_data()));
    }
    accu.step(bw::NulTermString<Borrowed>(v_.schema()));

    // 4.1 and later have a collation
    //
    // this could be checked via the protocol_41 capability, but that's not
    // what the server does
    if (v_.collation() != 0x00 ||
        this->caps()[classic_protocol::capabilities::pos::plugin_auth] ||
        this->caps()[classic_protocol::capabilities::pos::connect_attributes]) {
      accu.step(bw::FixedInt<2>(v_.collation()));
      if (this->caps()[classic_protocol::capabilities::pos::plugin_auth]) {
        accu.step(bw::NulTermString<Borrowed>(v_.auth_method_name()));
      }

      if (this->caps()
              [classic_protocol::capabilities::pos::connect_attributes]) {
        accu.step(bw::VarString<Borrowed>(v_.attributes()));
      }
    }

    return accu.result();
  }

 public:
  using value_type = borrowable::message::client::ChangeUser<Borrowed>;
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::ChangeUser);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return capabilities::secure_connection | capabilities::plugin_auth |
           capabilities::connect_attributes;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto username_res = accu.template step<bw::NulTermString<Borrowed>>();

    // auth-method-data is either
    //
    // - fixed-int-1 length
    // - null-term-string
    stdx::expected<bw::String<Borrowed>, std::error_code> auth_method_data_res;
    if (caps[classic_protocol::capabilities::pos::secure_connection]) {
      auto auth_method_data_len_res = accu.template step<bw::FixedInt<1>>();
      if (!auth_method_data_len_res)
        return stdx::unexpected(auth_method_data_len_res.error());
      auto auth_method_data_len = auth_method_data_len_res->value();

      auto res = accu.template step<bw::String<Borrowed>>(auth_method_data_len);
      if (!res) return stdx::unexpected(res.error());

      auth_method_data_res = bw::String<Borrowed>(res->value());
    } else {
      auto res = accu.template step<bw::NulTermString<Borrowed>>();
      if (!res) return stdx::unexpected(res.error());

      auth_method_data_res = bw::String<Borrowed>(res->value());
    }

    auto schema_res = accu.template step<bw::NulTermString<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    // 3.23.x-4.0 don't send more.
    if (buffer_size(buffer) <= accu.result().value()) {
      return std::make_pair(
          accu.result().value(),
          value_type(username_res->value(), auth_method_data_res->value(),
                     schema_res->value(), 0x00, {}, {}));
    }

    // added in 4.1
    auto collation_res = accu.template step<bw::FixedInt<2>>();

    stdx::expected<bw::NulTermString<Borrowed>, std::error_code>
        auth_method_name_res;
    if (caps[classic_protocol::capabilities::pos::plugin_auth]) {
      auth_method_name_res = accu.template step<bw::NulTermString<Borrowed>>();
    }

    stdx::expected<bw::VarString<Borrowed>, std::error_code> attributes_res;
    if (caps[classic_protocol::capabilities::pos::connect_attributes]) {
      attributes_res = accu.template step<bw::VarString<Borrowed>>();
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
class Codec<borrowable::message::client::Clone>
    : public CodecSimpleCommand<Codec<borrowable::message::client::Clone>,
                                borrowable::message::client::Clone> {
 public:
  using value_type = borrowable::message::client::Clone;
  using __base = CodecSimpleCommand<Codec<value_type>, value_type>;

  constexpr Codec(value_type, capabilities::value_type caps) : __base(caps) {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::Clone);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }
};

/**
 * codec for client side dump-binlog message.
 */
template <bool Borrowed>
class Codec<borrowable::message::client::BinlogDump<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::BinlogDump<Borrowed>>> {
 public:
  using value_type = borrowable::message::client::BinlogDump<Borrowed>;

 private:
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.position()))
        .step(bw::FixedInt<2>(v_.flags().underlying_value()))
        .step(bw::FixedInt<4>(v_.server_id()))
        .step(bw::String<Borrowed>(v_.filename()))
        .result();
  }

 public:
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::BinlogDump);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto position_res = accu.template step<bw::FixedInt<4>>();
    auto flags_res = accu.template step<bw::FixedInt<2>>();
    auto server_id_res = accu.template step<bw::FixedInt<4>>();

    auto filename_res = accu.template step<bw::String<Borrowed>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    stdx::flags<typename value_type::Flags> flags;
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
template <bool Borrowed>
class Codec<borrowable::message::client::RegisterReplica<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::RegisterReplica<Borrowed>>> {
 public:
  using value_type = borrowable::message::client::RegisterReplica<Borrowed>;

 private:
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    return accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<4>(v_.server_id()))
        .step(bw::FixedInt<1>(v_.hostname().size()))
        .step(bw::String<Borrowed>(v_.hostname()))
        .step(bw::FixedInt<1>(v_.username().size()))
        .step(bw::String<Borrowed>(v_.username()))
        .step(bw::FixedInt<1>(v_.password().size()))
        .step(bw::String<Borrowed>(v_.password()))
        .step(bw::FixedInt<2>(v_.port()))
        .step(bw::FixedInt<4>(v_.replication_rank()))
        .step(bw::FixedInt<4>(v_.master_id()))
        .result();
  }

 public:
  using __base = impl::EncodeBase<Codec<value_type>>;

  friend __base;

  constexpr Codec(value_type v, capabilities::value_type caps)
      : __base(caps), v_{std::move(v)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::RegisterReplica);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto server_id_res = accu.template step<bw::FixedInt<4>>();
    auto hostname_len_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto hostname_res =
        accu.template step<bw::String<Borrowed>>(hostname_len_res->value());

    auto username_len_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto username_res =
        accu.template step<bw::String<Borrowed>>(username_len_res->value());

    auto password_len_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto password_res =
        accu.template step<bw::String<Borrowed>>(password_len_res->value());

    auto port_res = accu.template step<bw::FixedInt<2>>();
    auto replication_rank_res = accu.template step<bw::FixedInt<4>>();
    auto master_id_res = accu.template step<bw::FixedInt<4>>();

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
template <bool Borrowed>
class Codec<borrowable::message::client::BinlogDumpGtid<Borrowed>>
    : public impl::EncodeBase<
          Codec<borrowable::message::client::BinlogDumpGtid<Borrowed>>> {
 public:
  using value_type = borrowable::message::client::BinlogDumpGtid<Borrowed>;

 private:
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    namespace bw = borrowable::wire;

    accu.step(bw::FixedInt<1>(cmd_byte()))
        .step(bw::FixedInt<2>(v_.flags().underlying_value()))
        .step(bw::FixedInt<4>(v_.server_id()))
        .step(bw::FixedInt<4>(v_.filename().size()))
        .step(bw::String<Borrowed>(v_.filename()))
        .step(bw::FixedInt<8>(v_.position()));

    if (v_.flags() & value_type::Flags::through_gtid) {
      accu.step(bw::FixedInt<4>(v_.sids().size()))
          .step(bw::String<Borrowed>(v_.sids()));
    }

    return accu.result();
  }

 public:
  using base_ = impl::EncodeBase<Codec<value_type>>;

  friend base_;

  constexpr Codec(value_type val, capabilities::value_type caps)
      : base_(caps), v_{std::move(val)} {}

  constexpr static uint8_t cmd_byte() noexcept {
    return static_cast<uint8_t>(CommandByte::BinlogDumpGtid);
  }

  /**
   * capabilities the codec depends on.
   */
  static constexpr capabilities::value_type depends_on_capabilities() noexcept {
    return {};
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    namespace bw = borrowable::wire;

    auto cmd_byte_res = accu.template step<bw::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    if (cmd_byte_res->value() != cmd_byte()) {
      return stdx::unexpected(make_error_code(codec_errc::invalid_input));
    }
    auto flags_res = accu.template step<bw::FixedInt<2>>();
    auto server_id_res = accu.template step<bw::FixedInt<4>>();
    auto filename_len_res = accu.template step<bw::FixedInt<4>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto filename_res =
        accu.template step<bw::String<Borrowed>>(filename_len_res->value());
    auto position_res = accu.template step<bw::FixedInt<8>>();

    stdx::flags<typename value_type::Flags> flags;
    flags.underlying_value(flags_res->value());

    if (!(flags & value_type::Flags::through_gtid)) {
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(flags, server_id_res->value(), filename_res->value(),
                     position_res->value(), {}));
    }

    auto sids_len_res = accu.template step<bw::FixedInt<4>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto sids_res =
        accu.template step<bw::String<Borrowed>>(sids_len_res->value());

    if (!accu.result()) return stdx::unexpected(accu.result().error());

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
