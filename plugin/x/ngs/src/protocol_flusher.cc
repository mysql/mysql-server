/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/ngs/include/ngs/protocol_flusher.h"

namespace ngs {

namespace details {

class Write_visitor : public Page_visitor {
 public:
  Write_visitor(Vio_interface *vio) : m_vio(vio) {}

  bool visit(const char *buffer, ssize_t size) override {
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
  Vio_interface *m_vio;
  ssize_t m_result{0};
};

}  // namespace details

void Protocol_flusher::mark_flush() { m_flush = true; }
void Protocol_flusher::on_message(const uint8_t type) {
  if (m_flush) return;

  const bool can_buffer =
      ((type == Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA) ||
       (type == Mysqlx::ServerMessages::RESULTSET_ROW) ||
       (type == Mysqlx::ServerMessages::NOTICE) ||
       (type == Mysqlx::ServerMessages::RESULTSET_FETCH_DONE));

  const bool buffer_too_big =
      m_page_output_stream->ByteCount() > BUFFER_PAGE_SIZE * 4;

  m_flush = !can_buffer || buffer_too_big;
}

bool Protocol_flusher::try_flush() {
  if (m_flush) {
    m_flush = false;
    return flush();
  }

  return true;
}

bool Protocol_flusher::flush() {
  const bool is_valid_socket = INVALID_SOCKET != m_socket->get_fd();

  if (is_valid_socket) {
    details::Write_visitor writter(m_socket.get());

    m_socket->set_timeout_in_ms(ngs::Vio_interface::Direction::k_write,
                                m_write_timeout * 1000);

    m_page_output_stream->visit_buffers(&writter);

    const ssize_t result = writter.get_result();
    if (result <= 0) {
      log_debug("Error writing to client: %s (%i)", strerror(errno), errno);
      m_on_error(errno);
      return false;
    }

    m_protocol_monitor->on_send(static_cast<long>(writter.get_result()));
  }

  m_page_output_stream->reset();

  return true;
}

}  // namespace ngs
