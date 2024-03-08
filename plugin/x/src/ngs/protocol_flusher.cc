/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/ngs/protocol_flusher.h"

#include "plugin/x/protocol/encoders/encoding_buffer.h"
#include "plugin/x/protocol/encoders/encoding_xmessages.h"
#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/variables/system_variables_defaults.h"

namespace ngs {

constexpr int k_number_of_pages_that_trigger_flush = 5;

// Alias for return types
using Result = xpl::iface::Protocol_flusher::Result;

namespace details {

class Write_visitor {
 public:
  explicit Write_visitor(xpl::iface::Vio *vio) : m_vio(vio) {}

  bool visit(const char *buffer, ssize_t size) {
    while (size > 0) {
      const ssize_t result =
          m_vio->write(reinterpret_cast<const uchar *>(buffer), size);

      if (result < 1) {
        m_result = result;
        return false;
      }

      size -= result;
      buffer += result;
      m_result += result;
    }

    return true;
  }

  ssize_t get_result() const { return m_result; }

 private:
  xpl::iface::Vio *m_vio;
  ssize_t m_result{0};
};

}  // namespace details

void Protocol_flusher::trigger_flush_required() { m_flush = true; }

template <uint8_t repeat>
bool check_pages_count(protocol::Page *page) {
  return page->m_next_page ? check_pages_count<repeat - 1>(page->m_next_page)
                           : false;
}

template <>
bool check_pages_count<0>(protocol::Page *) {
  return true;
}

void Protocol_flusher::trigger_on_message(const uint8_t type) {
  if (m_flush) return;

  const bool can_buffer =
      ((type == Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA) ||
       (type == Mysqlx::ServerMessages::RESULTSET_ROW) ||
       (type == Mysqlx::ServerMessages::NOTICE) ||
       (type == Mysqlx::ServerMessages::RESULTSET_FETCH_DONE) ||
       (type == Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS) ||
       (type == Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS) ||
       (type == Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED));

  // Let check if flusher holds `k_number_of_pages_that_trigger_flush` pages.
  //
  // Thus its good to guard if we don't use too many memory for buffering also
  // filling too large buffer might have a negative influence on performance.
  //
  // Number of page that trigger a flush should be benchmarked.
  const bool buffer_too_big =
      check_pages_count<k_number_of_pages_that_trigger_flush>(
          m_encoder->m_buffer->m_front);

  m_flush = !can_buffer || buffer_too_big;
}

Result Protocol_flusher::try_flush() {
  if (m_io_error) return Result::k_error;

  if (m_flush) {
    m_flush = false;
    return flush() ? Result::k_flushed : Result::k_error;
  }

  return Result::k_not_flushed;
}

bool Protocol_flusher::flush() {
  const bool is_valid_socket = INVALID_SOCKET != m_socket->get_fd();

  if (is_valid_socket) {
    details::Write_visitor writter(m_socket.get());

    m_socket->set_timeout_in_ms(xpl::iface::Vio::Direction::k_write,
                                m_write_timeout * 1000);

    auto page = m_encoder->m_buffer->m_front;

    if (0 == page->get_used_bytes()) {
      return true;
    }

    while (page) {
      if (!writter.visit(reinterpret_cast<const char *>(page->m_begin_data),
                         page->get_used_bytes()))
        break;
      page = page->m_next_page;
    }

    m_encoder->buffer_reset();

    const ssize_t result = writter.get_result();
    if (result <= 0) {
      log_debug("Error writing to client: %s (%i)", strerror(errno), errno);
      m_io_error = true;
      m_on_error(errno);
      return false;
    }

    m_protocol_monitor->on_send(static_cast<int64_t>(writter.get_result()));
  }

  return true;
}

}  // namespace ngs
