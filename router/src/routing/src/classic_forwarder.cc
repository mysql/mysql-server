/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "classic_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/tls_error.h"

IMPORT_LOG_FUNCTIONS()

// forwarder

static bool has_frame_header(ClassicProtocolState *src_protocol) {
  return src_protocol->current_frame().has_value();
}

static stdx::expected<size_t, std::error_code> forward_frame_header_as_is(
    Channel *src_channel, Channel *dst_channel, size_t header_size) {
  auto &recv_buf = src_channel->recv_plain_view();

  return dst_channel->write(net::buffer(recv_buf, header_size));
}

static stdx::expected<size_t, std::error_code> write_frame_header(
    Channel *dst_channel, classic_protocol::frame::Header frame_header) {
  std::vector<uint8_t> dest_header;
  const auto encode_res =
      classic_protocol::encode<classic_protocol::frame::Header>(
          frame_header, {}, net::dynamic_buffer(dest_header));
  if (!encode_res) {
    return encode_res.get_unexpected();
  }

  return dst_channel->write(net::buffer(dest_header));
}

static stdx::expected<size_t, std::error_code> forward_header(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    size_t header_size, size_t payload_size) {
  if (src_protocol->seq_id() == dst_protocol->seq_id()) {
    return forward_frame_header_as_is(src_channel, dst_channel, header_size);
  } else {
    auto write_res =
        write_frame_header(dst_channel, {payload_size, dst_protocol->seq_id()});
    if (!write_res) return write_res.get_unexpected();

    // return the bytes that were skipped from the recv_buffer.
    return header_size;
  }
}

/**
 * @returns frame-is-done on success and std::error_code on error.
 */
static stdx::expected<bool, std::error_code> forward_frame_from_channel(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    Channel *dst_channel, ClassicProtocolState *dst_protocol) {
  if (!has_frame_header(src_protocol)) {
    auto read_res =
        ClassicFrame::ensure_frame_header(src_channel, src_protocol);
    if (!read_res) return read_res.get_unexpected();
  }

  auto &current_frame = src_protocol->current_frame().value();

  auto &recv_buf = src_channel->recv_plain_view();
  if (current_frame.forwarded_frame_size_ == 0) {
    const size_t header_size{4};

    const uint8_t seq_id = current_frame.seq_id_;
    const size_t payload_size = current_frame.frame_size_ - header_size;

    src_protocol->seq_id(seq_id);

    // if one side starts a new command, reset the sequence-id for the other
    // side too.
    if (seq_id == 0) {
      dst_protocol->seq_id(0);
    } else {
      ++dst_protocol->seq_id();
    }

    auto forward_res = forward_header(src_channel, src_protocol, dst_channel,
                                      dst_protocol, header_size, payload_size);
    if (!forward_res) return forward_res.get_unexpected();

    const size_t transferred = forward_res.value();

    current_frame.forwarded_frame_size_ = transferred;

    // skip the original header
    src_channel->consume_plain(transferred);
  }

#if 0
  std::cerr << __LINE__ << ": "
            << "seq-id: " << (int)current_frame.seq_id_ << ", "
            << "frame-size: " << current_frame.frame_size_ << ", "
            << "forwarded so far: " << current_frame.forwarded_frame_size_
            << "\n";
#endif

  // forward the (rest of the) payload.

  const size_t rest_of_frame_size =
      current_frame.frame_size_ - current_frame.forwarded_frame_size_;

  if (rest_of_frame_size > 0) {
    // try to fill the recv-buf up to the end of the frame
    //
    // ... not more than kMaxForwardSize to avoid reading all 16M at once.
    const size_t kMaxForwardSize{64UL * 1024};

    if (rest_of_frame_size > recv_buf.size()) {
      auto read_res = src_channel->read_to_plain(
          std::min(rest_of_frame_size - recv_buf.size(), kMaxForwardSize));
      if (!read_res) return read_res.get_unexpected();
    }

    if (recv_buf.empty()) {
      return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
    }

    const auto write_res =
        dst_channel->write(net::buffer(recv_buf, rest_of_frame_size));
    if (!write_res) return write_res.get_unexpected();

    size_t transferred = write_res.value();
    current_frame.forwarded_frame_size_ += transferred;

    src_channel->consume_plain(transferred);
  }

  bool src_side_is_done{false};
  if (current_frame.forwarded_frame_size_ == current_frame.frame_size_) {
#if 0
    std::cerr << __LINE__ << ": "
              << "seq-id: " << (int)(current_frame.seq_id_) << ", done"
              << "\n";
#endif
    // payload-size + frame-size
    bool is_overlong_packet = current_frame.frame_size_ == (0xffffff + 4);

    // frame is forwarded, reset for the next one.
    src_protocol->current_frame().reset();

    if (!is_overlong_packet) {
      src_side_is_done = true;
      src_protocol->current_msg_type().reset();
    }
  } else {
#if 0
    std::cerr << __LINE__ << ": partial-frame: "
              << "seq-id: " << (int)(current_frame.seq_id_) << ", "
              << "rest-of-frame: "
              << (current_frame.frame_size_ -
                  current_frame.forwarded_frame_size_)
              << "\n";
#endif
  }

  return src_side_is_done;
}

static stdx::expected<Forwarder::ForwardResult, std::error_code>
forward_frame_sequence(Channel *src_channel, ClassicProtocolState *src_protocol,
                       Channel *dst_channel,
                       ClassicProtocolState *dst_protocol) {
  const auto forward_res = forward_frame_from_channel(
      src_channel, src_protocol, dst_channel, dst_protocol);
  if (!forward_res) {
    auto ec = forward_res.error();

    if (ec == TlsErrc::kWantRead) {
      if (!dst_channel->send_buffer().empty()) {
        return Forwarder::ForwardResult::kWantSendDestination;
      }

      return Forwarder::ForwardResult::kWantRecvSource;
    }

    return forward_res.get_unexpected();
  }

  // if forward-frame succeeded, the send-plain-buffer should contain some data
  if (dst_channel->send_plain_buffer().empty()) {
    log_debug("%d: %s", __LINE__, "send-buffer is empty.");

    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  const auto src_is_done = forward_res.value();
  return src_is_done ? Forwarder::ForwardResult::kFinished
                     : Forwarder::ForwardResult::kWantSendDestination;
}

stdx::expected<Processor::Result, std::error_code>
ServerToClientForwarder::process() {
  switch (stage()) {
    case Stage::Forward:
      return forward();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ServerToClientForwarder::forward() {
  auto forward_res = forward_frame_sequence();
  if (!forward_res) return recv_server_failed(forward_res.error());

  switch (forward_res.value()) {
    case ForwardResult::kWantRecvDestination:
      return Result::RecvFromClient;
    case ForwardResult::kWantSendDestination:
      return Result::SendToClient;
    case ForwardResult::kWantRecvSource:
      return Result::RecvFromServer;
    case ForwardResult::kWantSendSource:
      return Result::SendToServer;
    case ForwardResult::kFinished: {
      stage(Stage::Done);

      auto *socket_splicer = connection()->socket_splicer();
      auto *dst_channel = socket_splicer->client_channel();

      // if flush is optional and send-buffer is not too full, skip the flush.
      //
      // force-send-buffer-size is a trade-off between latency,
      // syscall-latency and memory usage:
      //
      // - buffering more: less send()-syscalls which helps with small
      // resultset.
      // - buffering less: faster forwarding of smaller packets if the server
      // is send to generate packets.
      //
      // 64k is 4 TLS frames.
      constexpr const size_t kForceFlushAfterBytes{64UL * 1024};

      if (flush_before_next_func_optional_ &&
          dst_channel->send_plain_buffer().size() < kForceFlushAfterBytes) {
        return Result::Again;
      }

      // encrypt if there is something to encrypt.
      dst_channel->flush_to_send_buf();

      return dst_channel->send_buffer().empty() ? Result::Again
                                                : Result::SendToClient;
    }
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Forwarder::ForwardResult, std::error_code>
ServerToClientForwarder::forward_frame_sequence() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  return ::forward_frame_sequence(src_channel, src_protocol, dst_channel,
                                  dst_protocol);
}

stdx::expected<Processor::Result, std::error_code>
ClientToServerForwarder::process() {
  switch (stage()) {
    case Stage::Forward:
      return forward();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ClientToServerForwarder::forward() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *dst_channel = socket_splicer->server_channel();

  auto forward_res = forward_frame_sequence();
  if (!forward_res) return recv_client_failed(forward_res.error());

  switch (forward_res.value()) {
    case ForwardResult::kWantRecvSource:
      return Result::RecvFromClient;
    case ForwardResult::kWantSendSource:
      return Result::SendToClient;
    case ForwardResult::kWantRecvDestination:
      return Result::RecvFromServer;
    case ForwardResult::kWantSendDestination:
      return Result::SendToServer;
    case ForwardResult::kFinished:
      stage(Stage::Done);

      // if flush is optional and send-buffer is not too full, skip the flush.
      //
      // force-send-buffer-size is a trade-off between latency,
      // syscall-latency and memory usage:
      //
      // - buffering more: less send()-syscalls which helps with small
      // resultset.
      // - buffering less: faster forwarding of smaller packets if the server
      // is send to generate packets.
      constexpr const size_t kForceFlushAfterBytes{64L * 1024};

      if (flush_before_next_func_optional_ &&
          dst_channel->send_plain_buffer().size() < kForceFlushAfterBytes) {
        return Result::Again;
      }

      // encrypt the plaintext data if needed.
      dst_channel->flush_to_send_buf();

      return dst_channel->send_buffer().empty() ? Result::Again
                                                : Result::SendToServer;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Forwarder::ForwardResult, std::error_code>
ClientToServerForwarder::forward_frame_sequence() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  return ::forward_frame_sequence(src_channel, src_protocol, dst_channel,
                                  dst_protocol);
}
