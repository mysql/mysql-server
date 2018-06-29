// Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include "plugin/x/ngs/include/ngs/protocol_decoder.h"

#include <stddef.h>
#include <new>

#include "plugin/x/ngs/include/ngs/ngs_error.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"

const uint32_t ON_IDLE_TIMEOUT_VALUE = 500;

namespace ngs {

Protocol_decoder::Decode_error::Decode_error() {}

Protocol_decoder::Decode_error::Decode_error(const bool disconnected)
    : m_disconnected(disconnected) {}

Protocol_decoder::Decode_error::Decode_error(const int sys_error)
    : m_sys_error(sys_error) {}

Protocol_decoder::Decode_error::Decode_error(const Error_code &error_code)
    : m_error_code(error_code) {}

bool Protocol_decoder::Decode_error::was_peer_disconnected() const {
  return m_disconnected;
}

int Protocol_decoder::Decode_error::get_io_error() const { return m_sys_error; }

Error_code Protocol_decoder::Decode_error::get_logic_error() const {
  return m_error_code;
}

bool Protocol_decoder::Decode_error::was_error() const {
  return get_logic_error() || was_peer_disconnected() || get_io_error() != 0;
}

bool Protocol_decoder::read_header(uint8 *message_type, uint32 *message_size,
                                   Waiting_for_io_interface *wait_for_io) {
  int header_copied = 0;
  int input_size = 0;
  const char *input = nullptr;
  union {
    char buffer[4];  // Must be properly aligned
    longlong dummy;
  };

  int copy_from_input = 0;
  const bool needs_idle_check = wait_for_io->has_to_report_idle_waiting();
  const uint64_t io_read_timeout =
      needs_idle_check ? ON_IDLE_TIMEOUT_VALUE : m_wait_timeout_in_ms;

  m_vio->set_timeout_in_ms(Vio_interface::Direction::k_read, io_read_timeout);

  uint64_t total_timeout = 0;

  m_vio_input_stream.mark_vio_as_idle();

  while (header_copied < 4) {
    if (needs_idle_check) wait_for_io->on_idle_or_before_read();

    if (!m_vio_input_stream.Next((const void **)&input, &input_size)) {
      int out_error_code = 0;
      if (m_vio_input_stream.was_io_error(&out_error_code)) {
        if ((out_error_code == SOCKET_ETIMEDOUT ||
             out_error_code == SOCKET_EAGAIN) &&
            needs_idle_check) {
          total_timeout += ON_IDLE_TIMEOUT_VALUE;
          if (total_timeout < m_wait_timeout_in_ms) {
            m_vio_input_stream.clear_io_error();

            continue;
          }
        }
      }
      return false;
    }

    copy_from_input = std::min(input_size, 4 - header_copied);
    std::copy(input, input + copy_from_input, buffer + header_copied);
    header_copied += copy_from_input;
  }

#ifdef WORDS_BIGENDIAN
  std::swap(buffer[0], buffer[3]);
  std::swap(buffer[1], buffer[2]);
#endif

  uint32 *message_size_ptr = (uint32 *)buffer;
  *message_size = *message_size_ptr;

  m_vio_input_stream.mark_vio_as_active();

  if (*message_size > 0) {
    if (input_size == copy_from_input) {
      copy_from_input = 0;
      m_vio->set_timeout_in_ms(Vio_interface::Direction::k_read,
                               m_read_timeout_in_ms);

      if (!m_vio_input_stream.Next((const void **)&input, &input_size)) {
        return false;
      }
    }

    *message_type = input[copy_from_input];

    ++copy_from_input;
  }

  m_vio_input_stream.BackUp(input_size - copy_from_input);

  return true;
}

Protocol_decoder::Decode_error Protocol_decoder::read_and_decode(
    Message_request *out_message, Waiting_for_io_interface *wait_for_io) {
  const auto result = read_and_decode_impl(out_message, wait_for_io);
  const auto received = static_cast<long>(m_vio_input_stream.ByteCount());

  if (received > 0) m_protocol_monitor->on_receive(received);

  return result;
}

Protocol_decoder::Decode_error Protocol_decoder::read_and_decode_impl(
    Message_request *out_message, Waiting_for_io_interface *wait_for_io) {
  uint8 message_type;
  uint32 message_size;
  int io_error = 0;

  m_vio_input_stream.reset_byte_count();

  if (!read_header(&message_type, &message_size, wait_for_io)) {
    m_vio_input_stream.was_io_error(&io_error);

    if (0 == io_error) return {true};

    return {io_error};
  }

  if (0 == message_size) {
    return {
        Error(ER_X_BAD_MESSAGE, "Messages without payload are not supported")};
  }

  if (m_config->max_message_size < message_size) {
    // Force disconnect
    return {true};
  }

  const auto protobuf_payload_size = message_size - 1;

  m_vio_input_stream.lock_data(protobuf_payload_size);

  const auto error_code = m_message_decoder.parse(
      message_type, protobuf_payload_size, &m_vio_input_stream, out_message);

  m_vio_input_stream.unlock_data();

  if (m_vio_input_stream.was_io_error(&io_error)) {
    if (0 == io_error) return {true};

    return {io_error};
  }

  // Skip rest of the data
  const auto bytes_to_skip =
      protobuf_payload_size + 5 - m_vio_input_stream.ByteCount();

  m_vio_input_stream.Skip(bytes_to_skip);

  if (error_code) {
    return {error_code};
  }

  return {};
}

void Protocol_decoder::set_wait_timeout(const uint32 wait_timeout_in_seconds) {
  m_wait_timeout_in_ms = wait_timeout_in_seconds * 1000;
}

void Protocol_decoder::set_read_timeout(const uint32 read_timeout_in_seconds) {
  m_read_timeout_in_ms = read_timeout_in_seconds * 1000;
}

}  // namespace ngs
