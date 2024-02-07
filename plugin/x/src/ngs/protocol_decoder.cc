// Copyright (c) 2015, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include "plugin/x/src/ngs/protocol_decoder.h"

#include <stddef.h>
#include <algorithm>
#include <new>
#include <utility>

#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

#ifdef _WIN32
#define SOCKET_TIMEOUT_ROUNDUP(X) (X < 1000 ? 1000 : X)
#else
#define SOCKET_TIMEOUT_ROUNDUP(X) X
#endif

const uint32_t k_on_idle_timeout_value = SOCKET_TIMEOUT_ROUNDUP(500);

namespace ngs {
Protocol_decoder::Protocol_decoder(
    Message_dispatcher_interface *dispatcher,
    std::shared_ptr<xpl::iface::Vio> vio,
    xpl::iface::Protocol_monitor *protocol_monitor,
    std::shared_ptr<Protocol_config> config)
    : m_vio(std::move(vio)),
      m_protocol_monitor(protocol_monitor),
      m_vio_input_stream(m_vio),
      m_config(std::move(config)),
      m_message_decoder(dispatcher, m_protocol_monitor, m_config) {
  set_wait_timeout(m_config->m_global->m_timeouts.m_wait_timeout);
  set_read_timeout(m_config->m_global->m_timeouts.m_read_timeout);
}

bool Protocol_decoder::read_header(uint8_t *message_type,
                                   uint32_t *message_size,
                                   xpl::iface::Waiting_for_io *wait_for_io) {
  int header_copied = 0;
  int input_size = 0;
  const uint8_t *input = nullptr;
  uint8_t buffer[4];

  int copy_from_input = 0;
  const bool needs_idle_check = wait_for_io->has_to_report_idle_waiting();
  const uint64_t io_read_timeout =
      needs_idle_check ? k_on_idle_timeout_value : m_wait_timeout_in_ms;

  m_vio->set_timeout_in_ms(xpl::iface::Vio::Direction::k_read, io_read_timeout);

  uint64_t total_timeout = 0;

  m_vio_input_stream.mark_vio_as_idle();

  while (header_copied < 4) {
    if (needs_idle_check)
      if (!wait_for_io->on_idle_or_before_read()) return false;

    if (!m_vio_input_stream.Next((const void **)&input, &input_size)) {
      int out_error_code = 0;
      if (m_vio_input_stream.was_io_error(&out_error_code)) {
        if ((out_error_code == SOCKET_ETIMEDOUT ||
             out_error_code == SOCKET_EAGAIN) &&
            needs_idle_check) {
          total_timeout += k_on_idle_timeout_value;
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

  google::protobuf::io::CodedInputStream::ReadLittleEndian32FromArray(
      buffer, message_size);

  m_vio_input_stream.mark_vio_as_active();

  if (*message_size > 0) {
    if (input_size == copy_from_input) {
      copy_from_input = 0;
      m_vio->set_timeout_in_ms(xpl::iface::Vio::Direction::k_read,
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
    xpl::iface::Waiting_for_io *wait_for_io) {
  const auto result = read_and_decode_impl(wait_for_io);
  const auto received = m_vio_input_stream.ByteCount();

  if (received > 0) m_protocol_monitor->on_receive(received);

  return result;
}

Protocol_decoder::Decode_error Protocol_decoder::read_and_decode_impl(
    xpl::iface::Waiting_for_io *wait_for_io) {
  uint8_t message_type = 0;
  uint32_t message_size;
  int io_error = 0;

  m_vio_input_stream.reset_byte_count();

  if (!read_header(&message_type, &message_size, wait_for_io)) {
    m_vio_input_stream.was_io_error(&io_error);

    if (0 == io_error) return Decode_error(true);

    return Decode_error{io_error};
  }

  if (0 == message_size) {
    return Decode_error{
        Error(ER_X_BAD_MESSAGE, "Messages without payload are not supported")};
  }

  if (m_config->m_global->max_message_size < message_size) {
    // Force disconnect
    return Decode_error{true};
  }

  const auto protobuf_payload_size = message_size - 1;

  m_vio_input_stream.lock_data(protobuf_payload_size);

  const auto error_code = m_message_decoder.parse_and_dispatch(
      message_type, protobuf_payload_size, &m_vio_input_stream);

  m_vio_input_stream.unlock_data();

  if (m_vio_input_stream.was_io_error(&io_error)) {
    if (0 == io_error) return Decode_error{true};

    return Decode_error{io_error};
  }

  const int k_header = 4;

  // Skip rest of the data
  const auto bytes_to_skip =
      message_size + k_header - m_vio_input_stream.ByteCount();

  m_vio_input_stream.Skip(bytes_to_skip);

  return {error_code};
}

void Protocol_decoder::set_wait_timeout(
    const uint32_t wait_timeout_in_seconds) {
  m_wait_timeout_in_ms = wait_timeout_in_seconds * 1000;
}

void Protocol_decoder::set_read_timeout(
    const uint32_t read_timeout_in_seconds) {
  m_read_timeout_in_ms = read_timeout_in_seconds * 1000;
}

}  // namespace ngs
