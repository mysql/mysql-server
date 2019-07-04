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

#include "plugin/x/src/io/vio_input_stream.h"

#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/src/operations_factory.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace xpl {

const uint32 k_buffer_size = 1024 * 4;

Vio_input_stream::Vio_input_stream(
    const std::shared_ptr<ngs::Vio_interface> &connection)
    : m_connection(connection) {
  ngs::allocate_array(m_buffer, k_buffer_size, KEY_memory_x_recv_buffer);
}

Vio_input_stream::~Vio_input_stream() { ngs::free_array(m_buffer); }

void Vio_input_stream::mark_vio_as_idle() {
  m_connection->set_state(PSI_SOCKET_STATE_IDLE);
  m_idle = true;
  m_idle_data = 0;
}

void Vio_input_stream::mark_vio_as_active() {
  m_connection->set_state(PSI_SOCKET_STATE_ACTIVE);
  m_idle = false;

  MYSQL_SOCKET_WAIT_VARIABLES(locker, state) /* no ';' */
  MYSQL_START_SOCKET_WAIT(locker, &state, m_connection->get_mysql_socket(),
                          PSI_SOCKET_RECV, 0);
  MYSQL_END_SOCKET_WAIT(locker, m_idle_data);
}

bool Vio_input_stream::was_io_error(int *error_code) const {
  if (0 == m_last_io_return_value) {
    *error_code = 0;
    return true;
  }

  if (m_last_io_return_value < 0) {
    Operations_factory operations_factory;

    *error_code =
        operations_factory.create_system_interface()->get_socket_errno();

    return true;
  }

  return false;
}

void Vio_input_stream::reset_byte_count() { m_bytes_count = 0; }

void Vio_input_stream::lock_data(const int count) {
  m_locked_data_pos = 0;
  m_locked_data_count = count;
}

void Vio_input_stream::unlock_data() { m_locked_data_count = 0; }

bool Vio_input_stream::Next(const void **data, int *size) {
  if (m_locked_data_count > 0) {
    if (m_locked_data_count == m_locked_data_pos) {
      return false;
    }
  }

  if (peek_data(data, size)) {
    if (m_locked_data_count > 0) {
      const int delta = m_locked_data_count - m_locked_data_pos - *size;

      if (delta < 0) {
        *size = std::max(*size + delta, 0);
      }
    }
    m_buffer_data_pos += *size;
    m_bytes_count += *size;
    m_locked_data_pos += *size;

    return true;
  }

  return false;
}

void Vio_input_stream::BackUp(int count) {
  DBUG_ASSERT(m_buffer_data_pos >= count);
  m_buffer_data_pos -= count;
  m_bytes_count -= count;

  if (m_locked_data_count > 0) {
    m_locked_data_pos -= count;
  }
}

bool Vio_input_stream::Skip(int count) {
  while (count > (m_buffer_data_count - m_buffer_data_pos)) {
    const void *ptr;
    int size;

    count -= (m_buffer_data_count - m_buffer_data_pos);
    m_buffer_data_count = m_buffer_data_pos = 0;

    if (!peek_data(&ptr, &size)) return false;
  }

  m_buffer_data_pos += count;
  m_bytes_count += count;

  return true;
}

Vio_input_stream::gint64 Vio_input_stream::ByteCount() const {
  return m_bytes_count;
}

bool Vio_input_stream::peek_data(const void **data, int *size) {
  if (m_buffer_data_pos < m_buffer_data_count) {
    *data = m_buffer + m_buffer_data_pos;
    *size = m_buffer_data_count - m_buffer_data_pos;

    return true;
  }

  if (!read_more_data()) return false;

  *size = m_buffer_data_count;
  *data = m_buffer;

  return true;
}

bool Vio_input_stream::read_more_data() {
  const ssize_t result =
      m_connection->read(reinterpret_cast<uchar *>(m_buffer), k_buffer_size);

  if (result <= 0) {
    m_last_io_return_value = result;

    return false;
  }

  if (m_idle) {
    m_idle_data += result;
  }

  m_buffer_data_count = result;
  m_buffer_data_pos = 0;

  return true;
}

}  // namespace xpl
