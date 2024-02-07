/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_CLIENT_STREAM_CONNECTION_INPUT_STREAM_H_
#define PLUGIN_X_CLIENT_STREAM_CONNECTION_INPUT_STREAM_H_

#include <google/protobuf/io/zero_copy_stream.h>
#include <algorithm>
#include <memory>

#include "my_dbug.h"

#include "mysqlxclient/xconnection.h"
#include "mysqlxclient/xerror.h"

namespace xcl {

class Connection_input_stream
    : public google::protobuf::io::ZeroCopyInputStream {
 public:
  using pb_int64 = google::protobuf::int64;

 public:
  explicit Connection_input_stream(XConnection *connection)
      : m_connection(connection) {}

  /*
    XConnection supports only blocking IO-calls, to make it work
    correctly with CodeInputStream, this class must be X Protocol
    message format aware.

    To minimize the impact, the user of this class must set how
    long the current byte sequence is.

    In most cases it will be:
    1. Read 5 bytes header ->AllowRead(5);
    2. Read rest to X Message payload ->AllowRead(Payload -1);
   */
  void AllowedRead(const pb_int64 io_read) {
    DBUG_PRINT("info", ("AllowedRead(%i)", (int)io_read));
    m_allowed_io_size = io_read;
  }

  pb_int64 GetLeftAllowedToRead() const { return m_allowed_io_size; }

  void ClearIOError() {
    DBUG_TRACE;
    m_io_error = {};
  }
  XError GetIOError() const {
    if (m_io_error) {
      DBUG_LOG("debug", "Connection_input_stream::GetIOError() = {error_code:"
                            << m_io_error.error() << ", msg: \""
                            << m_io_error.what() << "\"");
    }
    return m_io_error;
  }

  bool Next(const void **data, int *size) override {
    DBUG_TRACE;
    if (m_io_error) {
      m_buffer_data_size = m_buffer_offset = 0;
      DBUG_PRINT("info",
                 ("IOError %i - %s", m_io_error.error(), m_io_error.what()));
      return false;
    }

    if (m_buffer_data_size == m_buffer_offset) {
      if (0 == m_allowed_io_size) return false;

      m_all += m_buffer_data_size;
      m_buffer_data_size = std::min(k_buffer_max_size, m_allowed_io_size);
      m_allowed_io_size -= m_buffer_data_size;
      m_buffer_offset = 0;
      m_io_error = m_connection->read(m_buffer.get(), m_buffer_data_size);

      DBUG_DUMP("debug", m_buffer.get(), m_buffer_data_size);

      return Next(data, size);
    }

    *data = m_buffer.get() + m_buffer_offset;
    *size = m_buffer_data_size - m_buffer_offset;
    m_buffer_offset = m_buffer_data_size;

    return true;
  }

  void BackUp(int count) override {
    DBUG_PRINT("info", ("BackUp(%i)", count));
    m_buffer_offset -= count;
  }

  bool Skip(int count) override {
    DBUG_PRINT("info", ("Skip(%i)", count));
    const auto left = m_buffer_data_size - m_buffer_offset;

    if (left > count) {
      m_buffer_offset += count;
      return true;
    }

    m_buffer_offset = m_buffer_data_size;

    {
      const void *data = nullptr;
      int data_size = 0;

      if (!Next(&data, &data_size)) return false;
    }

    return Skip(count - left);
  }

  pb_int64 ByteCount() const override { return m_all + m_buffer_offset; }

 private:
  XError m_io_error;
  const pb_int64 k_buffer_max_size = 1024 * 4;
  pb_int64 m_buffer_data_size = 0;
  pb_int64 m_allowed_io_size = 0;
  std::unique_ptr<uint8_t[]> m_buffer{new uint8_t[k_buffer_max_size]};
  int m_buffer_offset = 0;
  int m_all = 0;

  XConnection *m_connection;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_STREAM_CONNECTION_INPUT_STREAM_H_
