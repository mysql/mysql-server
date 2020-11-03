/*
Copyright (c) 2016, 2020, Oracle and/or its affiliates.

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

#include "x_protocol.h"

#include <algorithm>
#include <cassert>
#include <memory>

#include <google/protobuf/io/coded_stream.h>

#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/buffer.h"  // net::stream_errc
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/routing.h"

#include "mysqlx.pb.h"
#include "mysqlx_connection.pb.h"
#include "mysqlx_session.pb.h"

using ProtobufMessage = google::protobuf::MessageLite;
IMPORT_LOG_FUNCTIONS()

constexpr size_t kMessageHeaderSize = 5;

static bool send_message(const std::string &log_prefix, int destination,
                         const int8_t type, const ProtobufMessage &msg,
                         mysql_harness::SocketOperationsBase *sock_ops) {
  using google::protobuf::io::CodedOutputStream;

  const size_t msg_size = message_byte_size(msg);
  std::vector<uint8_t> buffer(kMessageHeaderSize + msg_size);

  // first 4 bytes is the message size (plus type byte, without size bytes)
  CodedOutputStream::WriteLittleEndian32ToArray(
      static_cast<uint32_t>(msg_size + 1), &buffer[0]);
  // fifth byte is the message type
  buffer[kMessageHeaderSize - 1] = static_cast<uint8_t>(type);

  if ((msg_size > 0) &&
      (!msg.SerializeToArray(&buffer[kMessageHeaderSize], msg_size))) {
    log_error("[%s] error while serializing error message. Message size = %lu",
              log_prefix.c_str(), static_cast<ulong>(msg_size));
    return false;
  }

  const auto write_res =
      sock_ops->write_all(destination, &buffer[0], buffer.size());
  if (!write_res) {
    log_error("[%s] fd=%d write error: %s", log_prefix.c_str(), destination,
              write_res.error().message().c_str());
    return false;
  }

  return true;
}

static bool message_valid(const void *message_buffer, const int8_t message_type,
                          const uint32_t message_size) {
  std::unique_ptr<google::protobuf::MessageLite> msg;

  assert(message_type == Mysqlx::ClientMessages::SESS_AUTHENTICATE_START ||
         message_type == Mysqlx::ClientMessages::CON_CAPABILITIES_GET ||
         message_type == Mysqlx::ClientMessages::CON_CAPABILITIES_SET ||
         message_type == Mysqlx::ClientMessages::CON_CLOSE);

  switch (message_type) {
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      msg.reset(new Mysqlx::Session::AuthenticateStart());
      break;
    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
      msg.reset(new Mysqlx::Connection::CapabilitiesGet());
      break;
    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      msg.reset(new Mysqlx::Connection::CapabilitiesSet());
      break;
    default: /* Mysqlx::ClientMessages::CON_CLOSE */
      msg.reset(new Mysqlx::Connection::Close());
  }

  assert(msg.get() != nullptr);

  // sanity check deserializing the message
  if (!msg->ParseFromArray(message_buffer, static_cast<int>(message_size))) {
    return false;
  }

  return true;
}

static bool get_next_message(int sender, RoutingProtocolBuffer &buffer,
                             size_t &buffer_contents_size,
                             size_t &message_offset, int8_t &message_type,
                             uint32_t &message_size,
                             mysql_harness::SocketOperationsBase *sock_ops,
                             bool &error) {
  using google::protobuf::io::CodedInputStream;
  error = false;

  assert(buffer_contents_size >= message_offset);
  size_t bytes_left = buffer_contents_size - message_offset;

  // no more messages to process
  if (bytes_left == 0) {
    return false;
  }

  // we need at least 4 bytes to know the message size
  while (bytes_left < 4) {
    const auto read_res = sock_ops->read(
        sender, &buffer[message_offset + bytes_left], 4 - bytes_left);
    if (!read_res) {
      log_error("fd=%d failed reading size of the message: (%d %s)", sender,
                read_res.error().value(), read_res.error().message().c_str());
      error = true;
      return false;
    } else if (read_res.value() == 0) {
      // connection got closed on us

      error = true;
      return false;
    }

    buffer_contents_size += read_res.value();
    bytes_left += read_res.value();
  }

  // we got the message size, we can decode it
  CodedInputStream::ReadLittleEndian32FromArray(&buffer[message_offset],
                                                &message_size);

  // If not the whole message is in the buffer we need to read the remaining
  // part to be able to decode it. First let's check if the message will fit the
  // buffer. Currently we decode the messages ONLY in the handshake phase when
  // we expect relatively small messages: (AuthOk, AutCont, Notice, Error,
  // CapabilitiesGet...) In case the message does not fit the buffer, we just
  // return an error. This way we defend against the possibility of the client
  // sending huge messages while authenticating.
  size_t size_needed = message_offset + 4 + message_size;
  if (buffer.size() < size_needed) {
    log_error("X protocol message too big to fit the buffer: (%u, %lu, %lu)",
              message_size, static_cast<long unsigned>(buffer.size()),
              static_cast<long unsigned>(
                  message_offset));  // 32bit Linux requires casts
    error = true;
    return false;
  }
  // next read the remaining part of the message if needed
  while (message_size + 4 > bytes_left) {
    const auto read_res =
        sock_ops->read(sender, &buffer[message_offset + bytes_left],
                       message_size + 4 - bytes_left);
    if (!read_res) {
      log_error("fd=%d failed reading part of X protocol message: (%d %s)",
                sender, read_res.error().value(),
                read_res.error().message().c_str());

      error = true;
      return false;
    } else if (read_res.value() == 0) {
      // connection got closed on us.
      error = true;
      return false;
    }

    buffer_contents_size += read_res.value();
    bytes_left += read_res.value();
  }

  message_type = buffer[message_offset + kMessageHeaderSize - 1];

  return true;
}

stdx::expected<size_t, std::error_code> XProtocol::copy_packets(
    int sender, int receiver, bool sender_is_readable,
    std::vector<uint8_t> &buffer, int * /*curr_pktnr*/, bool &handshake_done,
    bool from_server) {
  auto buffer_length = buffer.size();
  size_t bytes_read = 0;

  mysql_harness::SocketOperationsBase *const so = sock_ops_;
  if (sender_is_readable) {
    const auto read_res = so->read(sender, &buffer.front(), buffer_length);
    if (!read_res) {
      if (read_res.error() != std::errc::resource_unavailable_try_again) {
        log_error("fd=%d sender read failed: (%d %s)", sender,
                  read_res.error().value(), read_res.error().message().c_str());
      }
      return stdx::make_unexpected(read_res.error());
    } else if (read_res.value() == 0) {
      // the caller assumes that errno == 0 on plain connection closes.
      return stdx::make_unexpected(make_error_code(net::stream_errc::eof));
    }
    bytes_read += read_res.value();
    if (!handshake_done) {
      // check packets integrity when handshaking.
      // we stop inspecting the messages when the client sends
      // AuthenticateStart or CapabilitesGet as a first message
      // that should be enough to prevent the MySQL Server from considering
      // the connection as an error even if it is terminated after that.
      int8_t message_type;
      size_t message_offset = 0;
      uint32_t message_size = 0;
      // the buffer can contain partial message or more than one message
      // the loop is to make sure that all messages are inspected
      // and that the whole message that is being processed is in the buffer
      bool msg_read_error = false;
      while (get_next_message(sender, buffer, bytes_read, message_offset,
                              message_type, message_size, so, msg_read_error) &&
             !msg_read_error) {
        if (!from_server) {
          // the first message from the client. We need to check if it's
          // correct.
          if (message_type == Mysqlx::ClientMessages::SESS_AUTHENTICATE_START ||
              message_type == Mysqlx::ClientMessages::CON_CAPABILITIES_GET ||
              message_type == Mysqlx::ClientMessages::CON_CAPABILITIES_SET ||
              message_type == Mysqlx::ClientMessages::CON_CLOSE) {
            // validate the message
            if (!message_valid(&buffer[message_offset + kMessageHeaderSize],
                               message_type, message_size - 1)) {
              log_warning("Invalid message content: type(%hhu), size(%u)",
                          message_type, message_size - 1);
              return stdx::make_unexpected(
                  make_error_code(std::errc::bad_message));
            }
            handshake_done = true;
            break;
          } else {
            // any other message at this point is not allowed by the x protocol
            // and would make MySQL Server consider this connection an error
            // which we need to prevent
            log_warning(
                "Received incorrect message type from the client while "
                "handshaking (was %hhu)",
                message_type);
            return stdx::make_unexpected(
                make_error_code(std::errc::bad_message));
          }
        }

        if (from_server && message_type == Mysqlx::ServerMessages::ERROR) {
          // if the server sends an error we don't consider it a failed
          // handshake. this is to have parity with how we behave in case of
          // classic protocol where error from the server (even ACCESS DENIED)
          // does not increment error connection counter
          handshake_done = true;
          break;
        }

        message_offset += (message_size + 4);
      }

      if (msg_read_error) {
        return stdx::make_unexpected(make_error_code(std::errc::bad_message));
      }
    }

    const auto write_res = so->write_all(receiver, &buffer[0], bytes_read);
    if (!write_res) {
      log_error("fd=%d write error: %s", receiver,
                write_res.error().message().c_str());
      return stdx::make_unexpected(write_res.error());
    }
  }

  return bytes_read;
}

bool XProtocol::send_error(int destination, unsigned short code,
                           const std::string &message,
                           const std::string &sql_state,
                           const std::string &log_prefix) {
  Mysqlx::Error error;
  error.set_code(code);
  error.set_sql_state(sql_state);
  error.set_msg(message);

  return send_message(log_prefix, destination, Mysqlx::ServerMessages::ERROR,
                      error, sock_ops_);
}

bool XProtocol::on_block_client_host(int server,
                                     const std::string &log_prefix) {
  // currently the MySQL Server (X-Plugin) does not have the feature of blocking
  // the client after reaching certain threshold of unsuccesfull connection
  // attemps (max_connect_errors) When this is done, the code here needs to be
  // revised to check if it prevents the server from considering the connection
  // as an error and blaming the router for it.

  // at the moment we send CapabilitiesGet message to the server assuming this
  // will prevent the MySQL Server from considering the connection as an error
  // and incrementing the counter.
  Mysqlx::Connection::CapabilitiesGet capabilities_get;

  return send_message(log_prefix, server,
                      Mysqlx::ClientMessages::CON_CAPABILITIES_GET,
                      capabilities_get, sock_ops_);
}

size_t message_byte_size(const google::protobuf::MessageLite &msg) {
#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  return msg.ByteSizeLong();
#else
  return msg.ByteSize();
#endif
}
