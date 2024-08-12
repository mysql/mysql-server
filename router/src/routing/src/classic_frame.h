/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_FRAME_INCLUDED
#define ROUTING_CLASSIC_FRAME_INCLUDED

#include <system_error>

#include "basic_protocol_splicer.h"
#include "classic_connection_base.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/channel.h"

class ClassicFrame {
 public:
  static stdx::expected<void, std::error_code> ensure_has_msg_prefix(
      Channel &src_channel, ClassicProtocolState &src_protocol);

  template <class Proto>
  static stdx::expected<void, std::error_code> ensure_has_msg_prefix(
      TlsSwitchableConnection<Proto> &conn) {
    return ensure_has_msg_prefix(conn.channel(), conn.protocol());
  }

  [[nodiscard]] static stdx::expected<void, std::error_code>
  ensure_has_full_frame(Channel &src_channel,
                        ClassicProtocolState &src_protocol);

  template <class Proto>
  [[nodiscard]] static stdx::expected<void, std::error_code>
  ensure_has_full_frame(TlsSwitchableConnection<Proto> &conn) {
    return ensure_has_full_frame(conn.channel(), conn.protocol());
  }

  /**
   * recv a full message sequence into the channel's recv_plain_buffer()
   */
  [[nodiscard]] static stdx::expected<size_t, std::error_code>
  recv_frame_sequence(Channel &src_channel, ClassicProtocolState &src_protocol);

  static stdx::expected<void, std::error_code> ensure_server_greeting(
      Channel &src_channel, ClassicProtocolState &src_protocol);

  static stdx::expected<void, std::error_code> ensure_frame_header(
      Channel &src_channel, ClassicProtocolState &src_protocol);

  template <class Proto>
  static stdx::expected<void, std::error_code> ensure_frame_header(
      TlsSwitchableConnection<Proto> &conn) {
    return ensure_frame_header(conn.channel(), conn.protocol());
  }

  template <class T>
  static constexpr uint8_t cmd_byte() {
    return classic_protocol::Codec<T>::cmd_byte();
  }

  /**
   * receive a classic protocol message frame from a channel.
   */
  template <class Msg, class Proto>
  static inline stdx::expected<Msg, std::error_code> recv_msg(
      Channel &src_channel, Proto &src_protocol,
      classic_protocol::capabilities::value_type caps) {
    auto read_res =
        ClassicFrame::recv_frame_sequence(src_channel, src_protocol);
    if (!read_res) return stdx::unexpected(read_res.error());

    auto num_of_frames = *read_res;
    if (num_of_frames > 1) {
      // more than one frame.
      auto frame_sequence_buf = src_channel.recv_plain_view();

      // assemble the payload from multiple frames.

      auto &payload_buf = src_channel.payload_buffer();
      payload_buf.clear();

      while (!frame_sequence_buf.empty()) {
        auto hdr_res =
            classic_protocol::decode<classic_protocol::frame::Header>(
                net::buffer(frame_sequence_buf), caps);
        if (!hdr_res) return stdx::unexpected(hdr_res.error());

        // skip the hdr.
        frame_sequence_buf =
            frame_sequence_buf.last(frame_sequence_buf.size() - hdr_res->first);

        auto frame_payload =
            frame_sequence_buf.first(hdr_res->second.payload_size());

        payload_buf.insert(payload_buf.end(), frame_payload.begin(),
                           frame_payload.end());

        frame_sequence_buf = frame_sequence_buf.last(
            frame_sequence_buf.size() - hdr_res->second.payload_size());
      }

      auto decode_res =
          classic_protocol::decode<Msg>(net::buffer(payload_buf), caps);
      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second;
    } else {
      auto &recv_buf = src_channel.recv_plain_view();

      auto decode_res =
          classic_protocol::decode<classic_protocol::frame::Frame<Msg>>(
              net::buffer(recv_buf), caps);
      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second.payload();
    }
  }

  template <class Msg, class Proto>
  static inline stdx::expected<Msg, std::error_code> recv_msg(
      Channel &src_channel, Proto &src_protocol) {
    return recv_msg<Msg>(src_channel, src_protocol,
                         src_protocol.shared_capabilities());
  }

  template <class Msg, class Proto>
  static inline stdx::expected<Msg, std::error_code> recv_msg(
      TlsSwitchableConnection<Proto> &conn) {
    return recv_msg<Msg>(conn.channel(), conn.protocol());
  }

  /**
   * receive a StmtExecute message from a channel.
   *
   * specialization of recv_msg<> as StmtExecute needs a the data from the
   * StmtPrepareOk.
   */
  template <class Msg, class Proto>
    requires(
        (std::is_same_v<
             Msg, classic_protocol::borrowed::message::client::StmtExecute> &&
         std::is_same_v<Proto, ClientSideClassicProtocolState>))
  static inline stdx::expected<Msg, std::error_code> recv_msg(
      Channel &src_channel, Proto &src_protocol,
      classic_protocol::capabilities::value_type caps) {
    auto read_res =
        ClassicFrame::recv_frame_sequence(src_channel, src_protocol);
    if (!read_res) return stdx::unexpected(read_res.error());

    const auto &recv_buf = src_channel.recv_plain_view();

    auto frame_decode_res =
        classic_protocol::decode<classic_protocol::frame::Frame<
            classic_protocol::borrowed::wire::String>>(net::buffer(recv_buf),
                                                       caps);
    if (!frame_decode_res) {
      return stdx::unexpected(frame_decode_res.error());
    }

    src_protocol.seq_id(frame_decode_res->second.seq_id());

    auto decode_res = classic_protocol::decode<Msg>(
        net::buffer(frame_decode_res->second.payload().value()), caps,
        [src_protocol](
            auto stmt_id) -> stdx::expected<std::vector<typename Msg::ParamDef>,
                                            std::error_code> {
          const auto it = src_protocol.prepared_statements().find(stmt_id);
          if (it == src_protocol.prepared_statements().end()) {
            return stdx::unexpected(make_error_code(
                classic_protocol::codec_errc::statement_id_not_found));
          }

          std::vector<typename Msg::ParamDef> params;
          params.reserve(it->second.parameters.size());

          for (const auto &param : it->second.parameters) {
            params.emplace_back(param.type_and_flags, std::string_view{},
                                param.param_already_sent);
          }

          return params;
        });
    if (!decode_res) return stdx::unexpected(decode_res.error());

    return decode_res->second;
  }

  template <class Msg>
  static inline stdx::expected<size_t, std::error_code> send_msg(
      Channel &dst_channel, ClassicProtocolState &dst_protocol, Msg msg,
      classic_protocol::capabilities::value_type caps) {
    auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<Msg>(++dst_protocol.seq_id(),
                                            std::forward<Msg>(msg)),
        caps, net::dynamic_buffer(dst_channel.send_plain_buffer()));
    if (!encode_res) return encode_res;

    return dst_channel.flush_to_send_buf();
  }

  template <class Msg>
  static inline stdx::expected<size_t, std::error_code> send_msg(
      Channel &dst_channel, ClassicProtocolState &dst_protocol, Msg msg) {
    return send_msg<Msg>(dst_channel, dst_protocol, std::forward<Msg>(msg),
                         dst_protocol.shared_capabilities());
  }

  template <class Msg, class Proto>
  static inline stdx::expected<size_t, std::error_code> send_msg(
      TlsSwitchableConnection<Proto> &conn, Msg msg,
      classic_protocol::capabilities::value_type caps) {
    return send_msg<Msg>(conn.channel(), conn.protocol(),
                         std::forward<Msg>(msg), caps);
  }

  template <class Msg, class Proto>
  static inline stdx::expected<size_t, std::error_code> send_msg(
      TlsSwitchableConnection<Proto> &conn, Msg msg) {
    return send_msg<Msg>(conn.channel(), conn.protocol(),
                         std::forward<Msg>(msg));
  }

  /**
   * set attributes from the Ok message in the TraceEvent.
   */
  static void trace_set_attributes(
      TraceEvent *ev, ClassicProtocolState &src_protocol,
      const classic_protocol::borrowed::message::server::Ok &msg);

  /**
   * set attributes from the Eof message in the TraceEvent.
   */
  static void trace_set_attributes(
      TraceEvent *ev, ClassicProtocolState &src_protocol,
      const classic_protocol::borrowed::message::server::Eof &msg);

  /**
   * set attributes from the Eof message in the TraceEvent.
   */
  static void trace_set_attributes(
      TraceEvent *ev, ClassicProtocolState &src_protocol,
      const classic_protocol::borrowed::message::server::Error &msg);
};

#endif
