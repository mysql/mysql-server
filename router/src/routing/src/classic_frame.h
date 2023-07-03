/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_FRAME_INCLUDED
#define ROUTING_CLASSIC_FRAME_INCLUDED

#include <system_error>

#include "channel.h"
#include "classic_connection.h"
#include "mysql/harness/stdx/expected.h"

class ClassicFrame {
 public:
  static stdx::expected<void, std::error_code> ensure_has_msg_prefix(
      Channel *src_channel, ClassicProtocolState *src_protocol);

  [[nodiscard]] static stdx::expected<void, std::error_code>
  ensure_has_full_frame(Channel *src_channel,
                        ClassicProtocolState *src_protocol);

  /**
   * recv a full message sequence into the channel's recv_plain_buffer()
   */
  [[nodiscard]] static stdx::expected<void, std::error_code>
  recv_frame_sequence(Channel *src_channel, ClassicProtocolState *src_protocol);

  static stdx::expected<void, std::error_code> ensure_server_greeting(
      Channel *src_channel, ClassicProtocolState *src_protocol);

  static stdx::expected<void, std::error_code> ensure_frame_header(
      Channel *src_channel, ClassicProtocolState *src_protocol);

  template <class T>
  static constexpr uint8_t cmd_byte() {
    return classic_protocol::Codec<T>::cmd_byte();
  }

  /**
   * receive a classic protocol message frame from a channel.
   */
  template <class Msg>
  static inline stdx::expected<Msg, std::error_code> recv_msg(
      Channel *src_channel, ClassicProtocolState *src_protocol,
      classic_protocol::capabilities::value_type caps) {
    auto read_res =
        ClassicFrame::recv_frame_sequence(src_channel, src_protocol);
    if (!read_res) return stdx::make_unexpected(read_res.error());

    auto &recv_buf = src_channel->recv_plain_buffer();

    auto decode_res =
        classic_protocol::decode<classic_protocol::frame::Frame<Msg>>(
            net::buffer(recv_buf), caps);
    if (!decode_res) return stdx::make_unexpected(decode_res.error());

    src_protocol->seq_id(decode_res->second.seq_id());

    return decode_res->second.payload();
  }

  template <class Msg>
  static inline stdx::expected<Msg, std::error_code> recv_msg(
      Channel *src_channel, ClassicProtocolState *src_protocol) {
    return recv_msg<Msg>(src_channel, src_protocol,
                         src_protocol->shared_capabilities());
  }

  template <class Msg>
  static inline stdx::expected<size_t, std::error_code> send_msg(
      Channel *dst_channel, ClassicProtocolState *dst_protocol, Msg msg,
      classic_protocol::capabilities::value_type caps) {
    std::vector<uint8_t> frame_buf;
    auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<Msg>(++dst_protocol->seq_id(),
                                            std::forward<Msg>(msg)),
        caps, net::dynamic_buffer(frame_buf));
    if (!encode_res) return encode_res;

    auto write_res = dst_channel->write_plain(net::buffer(frame_buf));
    if (!write_res) return write_res;
    return dst_channel->flush_to_send_buf();
  }

  template <class Msg>
  static inline stdx::expected<size_t, std::error_code> send_msg(
      Channel *dst_channel, ClassicProtocolState *dst_protocol, Msg msg) {
    return send_msg<Msg>(dst_channel, dst_protocol, std::forward<Msg>(msg),
                         dst_protocol->shared_capabilities());
  }
};

#endif
