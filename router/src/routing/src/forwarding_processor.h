/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_FORWARDING_PROCESSOR_INCLUDED
#define ROUTING_CLASSIC_FORWARDING_PROCESSOR_INCLUDED

#include <chrono>

#include "basic_protocol_splicer.h"
#include "classic_connection_base.h"
#include "processor.h"

/**
 * a processor base class with helper functions.
 */
class ForwardingProcessor : public Processor {
 public:
  using Processor::Processor;

  /**
   * interval between connect-retries.
   */
  static constexpr const std::chrono::milliseconds kConnectRetryInterval{
      std::chrono::milliseconds(100)};

 protected:
  /**
   * forward the current packet from the server-side to the client-side.
   *
   * use 'noflush' if the next message is from the server side too to allow
   * merging of multiple server-side packets into one "send-to-client".
   *
   * Useful for resultsets which is split into multiple packets.
   *
   * Pushes a ServerToClientForwarder to the processor-stack.
   *
   * @param noflush if true, it isn't required to wait until the packet is sent
   * to the client.
   */
  stdx::expected<Result, std::error_code> forward_server_to_client(
      bool noflush = false);

  /**
   * forward the current packet from the client-side to the server-side.
   *
   * Pushes a ClientToServerForwarder to the processor-stack.
   *
   * @param noflush if true, it isn't required to wait until the packet is sent
   * to the server.
   */
  stdx::expected<Result, std::error_code> forward_client_to_server(
      bool noflush = false);

  /**
   * check of the capabilities of the source and the destination are the same
   * for this message.
   *
   * @param src_protocol the source protocol state
   * @param dst_protocol the destination protocol state
   * @param msg the message that shall be forwarded
   *
   * @retval true if the msg can be forwarded as is.
   */
  template <class T>
  static bool message_can_be_forwarded_as_is(ClassicProtocolState &src_protocol,
                                             ClassicProtocolState &dst_protocol,
                                             const T &msg [[maybe_unused]]) {
    const auto mask = classic_protocol::Codec<T>::depends_on_capabilities();

    return (src_protocol.shared_capabilities() & mask) ==
           (dst_protocol.shared_capabilities() & mask);
  }

  /**
   * adjust the end-of-columns packet.
   *
   * if source and destination don't have the same CLIENT_DEPRECATE_EOF, the Eof
   * packet has to be add/removed between columns and rows.
   *
   * @param no_flush if the packet is forwarded, don't force a send as there is
   * more data coming.
   */
  stdx::expected<Processor::Result, std::error_code>
  skip_or_inject_end_of_columns(bool no_flush = false);

  /**
   * move the server connection to the pool.
   */
  stdx::expected<bool, std::error_code> pool_server_connection();

  /**
   * reconnect a socket.
   *
   * pushes a ConnectProcessor on the processor stack.
   *
   * when finished, a socket is established.
   *
   * @retval Result::Again on success.
   */
  stdx::expected<Processor::Result, std::error_code> socket_reconnect_start(
      TraceEvent *parent_event);

  /**
   * reconnect a mysql classic connection.
   *
   * pushes a LazyConnector on the processor stack.
   *
   * when finished, a mysql connection is authenticated.
   */
  stdx::expected<Processor::Result, std::error_code> mysql_reconnect_start(
      TraceEvent *parent_event);

  /**
   * handle error-code of a failed receive() from the server-socket and check
   * the status of the client socket.
   */
  stdx::expected<Result, std::error_code>
  recv_server_failed_and_check_client_socket(std::error_code ec);

  /**
   * send a Error msg based on the reconnect_error().
   *
   * @retval Result::SendToClient on success.
   */
  stdx::expected<Processor::Result, std::error_code> reconnect_send_error_msg(
      Channel &src_channel, ClassicProtocolState &src_protocol);

  template <class Proto>
  stdx::expected<Processor::Result, std::error_code> reconnect_send_error_msg(
      TlsSwitchableConnection<Proto> &conn) {
    return reconnect_send_error_msg(conn.channel(), conn.protocol());
  }

  /**
   * set the reconnect error.
   *
   * may be called from handlers.
   */
  void reconnect_error(classic_protocol::message::server::Error err) {
    reconnect_error_ = std::move(err);
  }

  /**
   * get the reconnect error.
   */
  classic_protocol::message::server::Error reconnect_error() const {
    return reconnect_error_;
  }

  /**
   * check if the error is a transient error.
   */
  static bool connect_error_is_transient(
      const classic_protocol::message::server::Error &err);

 private:
  /**
   * reconnect error set by lazy_reconnect_start().
   */
  classic_protocol::message::server::Error reconnect_error_{};
};

#endif
